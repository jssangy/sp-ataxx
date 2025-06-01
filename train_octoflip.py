#!/usr/bin/env python3
"""
OctaFlip NNUE 고효율 학습 시스템 v4.0 - 디버깅 개선 버전
"""

import numpy as np
import random
import time
import multiprocessing as mp
from collections import defaultdict, deque
import json
import pickle
import gzip
import os
import traceback  # 추가
from datetime import datetime, timedelta
import warnings
warnings.filterwarnings('ignore')

# 게임 상수
BOARD_SIZE = 8
RED = 'R'
BLUE = 'B' 
EMPTY = '.'

# 학습 설정 (M3 최적화)
WORKERS = min(mp.cpu_count() - 2, 6)  # 최대 6 워커
BATCH_SIZE = 512  # 더 작은 배치
GAMES_PER_GENERATION = 200  # 세대당 게임 수 대폭 줄임
TARGET_DURATION_HOURS = 10

# NNUE 구조
INPUT_SIZE = 192
HIDDEN1_SIZE = 64
HIDDEN2_SIZE = 32
OUTPUT_SIZE = 1

# 테스트 설정
TEST_INTERVAL_MINUTES = 30  # 30분마다 테스트
QUICK_TEST_GAMES = 4  # 각 AI당 4게임으로 줄임
SAVE_INTERVAL_GENERATIONS = 20  # 20세대마다 저장

# 학습 데이터 설정
MAX_BUFFER_SIZE = 200000  # 30000에서 10000으로 줄임
HALL_OF_FAME_SIZE = 10    # 10에서 5로 줄임

# 출력 스케일 (centipawn)
OUTPUT_SCALE = 200


class CompactNNUE:
    """메모리 효율적인 NNUE (완전 안정화)"""
    
    def __init__(self, init_from_file=None):
        if init_from_file and os.path.exists(init_from_file):
            self.load_weights(init_from_file)
        else:
            # 매우 작은 초기화
            self.w1 = self._init_weights((INPUT_SIZE, HIDDEN1_SIZE), 'relu') * 0.1
            self.b1 = np.zeros(HIDDEN1_SIZE, dtype=np.float32)
            
            self.w2 = self._init_weights((HIDDEN1_SIZE, HIDDEN2_SIZE), 'relu') * 0.1
            self.b2 = np.zeros(HIDDEN2_SIZE, dtype=np.float32)
            
            self.w3 = self._init_weights((HIDDEN2_SIZE, OUTPUT_SIZE), 'linear') * 0.1
            self.b3 = np.zeros(OUTPUT_SIZE, dtype=np.float32)
        
        # 학습 파라미터
        self.learning_rate = 0.001
        self.beta1 = 0.9
        self.beta2 = 0.999
        self.epsilon = 1e-8
        self.t = 0
        
        self.lr_decay_rate = 0.995
        self.min_lr = 0.00001

        # Momentum 초기화
        self._init_momentum()
        
        # Loss 추적
        self.loss_history = deque(maxlen=100)
        self.best_loss = float('inf')
        self.patience_counter = 0
        
    def _init_weights(self, shape, activation='relu'):
        """적절한 가중치 초기화"""
        fan_in = shape[0]
        if activation == 'relu':
            # He initialization
            std = np.sqrt(2.0 / fan_in)
        else:
            # Xavier initialization
            std = np.sqrt(1.0 / fan_in)
        return np.random.randn(*shape).astype(np.float32) * std
    
    def _init_momentum(self):
        """Adam momentum 초기화"""
        self.m_w1 = np.zeros_like(self.w1)
        self.m_w2 = np.zeros_like(self.w2)
        self.m_w3 = np.zeros_like(self.w3)
        self.m_b1 = np.zeros_like(self.b1)
        self.m_b2 = np.zeros_like(self.b2)
        self.m_b3 = np.zeros_like(self.b3)
        
        self.v_w1 = np.zeros_like(self.w1)
        self.v_w2 = np.zeros_like(self.w2)
        self.v_w3 = np.zeros_like(self.w3)
        self.v_b1 = np.zeros_like(self.b1)
        self.v_b2 = np.zeros_like(self.b2)
        self.v_b3 = np.zeros_like(self.b3)
        
    def load_weights(self, filename):
        """기존 pickle 파일 로드"""
        try:
            with gzip.open(filename, 'rb') as f:
                weights = pickle.load(f)
            
            self.w1 = np.array(weights['w1'], dtype=np.float32)
            self.b1 = np.array(weights['b1'], dtype=np.float32)
            self.w2 = np.array(weights['w2'], dtype=np.float32)
            self.b2 = np.array(weights['b2'], dtype=np.float32)
            self.w3 = np.array(weights['w3'], dtype=np.float32)
            self.b3 = np.array(weights['b3'], dtype=np.float32)
            
            # 가중치 정규화
            self.normalize_weights()
            
            print(f"Loaded weights from {filename}")
        except Exception as e:
            print(f"Error loading weights: {e}")
    
    def normalize_weights(self):
        """가중치 크기 제한"""
        max_norm = 5.0
        
        for w in [self.w1, self.w2, self.w3]:
            norm = np.linalg.norm(w)
            if norm > max_norm:
                w *= max_norm / norm
    
    def forward(self, x, training=True):
        """순전파 (안정화) - 타입 보장"""
        # numpy 배열 확인 및 변환
        if not isinstance(x, np.ndarray):
            x = np.array(x, dtype=np.float32)
        
        if len(x.shape) == 1:
            x = x.reshape(1, -1)
        
        # float32로 변환
        if x.dtype != np.float32:
            x = x.astype(np.float32)
            
        self.x = x
        
        # Layer 1 with clipping
        self.z1 = np.dot(x, self.w1) + self.b1
        self.z1 = np.clip(self.z1, -5, 5)
        self.a1 = np.maximum(0, self.z1)  # ReLU
        
        # Dropout simulation (training only)
        if training:
            dropout_mask = np.random.binomial(1, 0.9, size=self.a1.shape).astype(np.float32)
            self.a1 = self.a1 * dropout_mask / 0.9
        
        # Layer 2 with clipping
        self.z2 = np.dot(self.a1, self.w2) + self.b2
        self.z2 = np.clip(self.z2, -5, 5)
        self.a2 = np.maximum(0, self.z2)  # ReLU
        
        # Output layer
        self.z3 = np.dot(self.a2, self.w3) + self.b3
        self.z3 = np.clip(self.z3, -5, 5)
        
        # 작은 스케일 출력
        output = np.tanh(self.z3) * OUTPUT_SCALE
        
        return output.flatten()
    
    def huber_loss(self, y_true, y_pred, delta=10.0):
        """Huber Loss - outlier에 강함"""
        error = y_true - y_pred
        is_small_error = np.abs(error) <= delta
        
        squared_loss = 0.5 * error ** 2
        linear_loss = delta * np.abs(error) - 0.5 * delta ** 2
        
        return np.where(is_small_error, squared_loss, linear_loss)
    
    def backward_adam(self, x, y_true, y_pred):
        """역전파 with Adam optimizer (Huber Loss) - 타입 안전성 개선"""
        self.t += 1
        batch_size = x.shape[0] if len(x.shape) > 1 else 1
        
        # numpy 배열 확인 및 변환
        if not isinstance(y_pred, np.ndarray):
            y_pred = np.array(y_pred, dtype=np.float32)
        
        if not isinstance(y_true, np.ndarray):
            y_true = np.array(y_true, dtype=np.float32)
        
        # flatten 확인
        y_pred = y_pred.flatten()
        y_true = y_true.flatten()
        
        # 크기 확인
        if y_pred.shape != y_true.shape:
            print(f"WARNING: Shape mismatch - y_pred: {y_pred.shape}, y_true: {y_true.shape}")
            return float('inf')
        
        # 입력 검증 - 안전한 방법으로
        try:
            has_nan = False
            has_inf = False
            
            # 수동으로 체크
            for val in y_pred.flat:
                if val != val:  # NaN check
                    has_nan = True
                    break
                if abs(val) == float('inf'):
                    has_inf = True
                    break
                    
            if has_nan or has_inf:
                print("WARNING: NaN/Inf in predictions")
                return self.best_loss if hasattr(self, 'best_loss') else float('inf')
                
        except Exception as e:
            print(f"WARNING: Error checking NaN/Inf: {e}")
            return float('inf')
        
        # Huber Loss gradient
        delta = 10.0
        errors = y_pred - y_true
        
        # 안전한 방법으로 조건 확인
        is_small_error = np.abs(errors) <= delta
        d_loss = np.where(is_small_error, errors, delta * np.sign(errors))
        d_loss = d_loss / batch_size
        
        # Loss 계산
        huber_losses = self.huber_loss(y_true, y_pred, delta)
        loss = np.mean(huber_losses)
        
        # Loss 기록
        self.loss_history.append(loss)
        
        # Loss가 너무 크면 스킵
        if loss > 10000:
            print(f"WARNING: Large loss {loss:.0f}, skipping update")
            return loss
        
        # Output layer gradients
        d_tanh = 1 - np.tanh(self.z3) ** 2
        d_z3 = d_loss.reshape(-1, 1) * d_tanh * OUTPUT_SCALE
        
        # Aggressive gradient clipping
        max_grad = 0.1
        d_z3 = np.clip(d_z3, -max_grad, max_grad)
        
        d_w3 = np.dot(self.a2.T, d_z3) / batch_size
        d_b3 = np.mean(d_z3, axis=0)
        
        # Hidden layer 2 gradients
        d_a2 = np.dot(d_z3, self.w3.T)
        d_z2 = d_a2 * (self.z2 > 0)
        d_z2 = np.clip(d_z2, -max_grad, max_grad)
        
        d_w2 = np.dot(self.a1.T, d_z2) / batch_size
        d_b2 = np.mean(d_z2, axis=0)
        
        # Hidden layer 1 gradients
        d_a1 = np.dot(d_z2, self.w2.T)
        d_z1 = d_a1 * (self.z1 > 0)
        d_z1 = np.clip(d_z1, -max_grad, max_grad)
        
        d_w1 = np.dot(x.T, d_z1) / batch_size
        d_b1 = np.mean(d_z1, axis=0)
        
        # L2 regularization
        weight_decay = 0.0001
        d_w1 += weight_decay * self.w1
        d_w2 += weight_decay * self.w2
        d_w3 += weight_decay * self.w3
        
        # 적응형 학습률
        lr = self.learning_rate
        
        # 세대 기반 decay
        if hasattr(self, 'generation'):
            lr *= (self.lr_decay_rate ** self.generation)
            lr = max(lr, self.min_lr)
        
        # Loss 기반 조정
        if len(self.loss_history) >= 20:
            recent_losses = list(self.loss_history)[-20:]
            avg_loss = np.mean(recent_losses)
            
            if not hasattr(self, 'best_loss'):
                self.best_loss = avg_loss
            
            # 개선이 없으면 학습률 감소
            if not hasattr(self, 'patience_counter'):
                self.patience_counter = 0
                
            if avg_loss >= self.best_loss:
                self.patience_counter += 1
            else:
                self.patience_counter = 0
                self.best_loss = avg_loss
                
            if self.patience_counter > 10:
                lr *= 0.7
                self.patience_counter = 0
                print(f"Reducing learning rate to {lr:.2e}")
        
        # Adam update
        def adam_update(param, grad, m, v):
            # Gradient clipping by value
            grad = np.clip(grad, -1.0, 1.0)
            
            # Adam
            m = self.beta1 * m + (1 - self.beta1) * grad
            v = self.beta2 * v + (1 - self.beta2) * (grad ** 2)
            
            m_hat = m / (1 - self.beta1 ** self.t)
            v_hat = v / (1 - self.beta2 ** self.t)
            
            # Update with clipping
            update = lr * m_hat / (np.sqrt(v_hat) + self.epsilon)
            update = np.clip(update, -0.01, 0.01)
            
            new_param = param - update
            
            # Weight clipping
            new_param = np.clip(new_param, -2.0, 2.0)
            
            return new_param, m, v
        
        # 가중치 업데이트
        self.w3, self.m_w3, self.v_w3 = adam_update(self.w3, d_w3, self.m_w3, self.v_w3)
        self.w2, self.m_w2, self.v_w2 = adam_update(self.w2, d_w2, self.m_w2, self.v_w2)
        self.w1, self.m_w1, self.v_w1 = adam_update(self.w1, d_w1, self.m_w1, self.v_w1)
        
        self.b3, self.m_b3, self.v_b3 = adam_update(self.b3, d_b3, self.m_b3, self.v_b3)
        self.b2, self.m_b2, self.v_b2 = adam_update(self.b2, d_b2, self.m_b2, self.v_b2)
        self.b1, self.m_b1, self.v_b1 = adam_update(self.b1, d_b1, self.m_b1, self.v_b1)
        
        # 가중치 정규화
        self.normalize_weights()
        
        return loss

    def augment_position(self, pos):
        """포지션 데이터 증강"""
        augmented = []
        board = pos['board']
        
        # 1. 원본
        augmented.append(pos)
        
        # 2. 180도 회전
        rotated_board = [[board[7-i][7-j] for j in range(8)] for i in range(8)]
        augmented.append({
            'board': rotated_board,
            'player': pos['player'],
            'hash': self.get_board_hash_from_array(rotated_board)
        })
        
        return augmented
    
    def board_to_input(self, board):
        """보드를 입력 벡터로 변환"""
        x = np.zeros(INPUT_SIZE, dtype=np.float32)
        
        for i in range(8):
            for j in range(8):
                idx = i * 8 + j
                if board[i][j] == RED:
                    x[idx] = 1.0
                elif board[i][j] == BLUE:
                    x[idx + 64] = 1.0
                elif board[i][j] == EMPTY:
                    x[idx + 128] = 1.0
        
        return x
    
    def export_to_c_arrays(self, filename='nnue_weights_generated.h'):
        """C 헤더 파일로 내보내기"""
        with open(filename, 'w') as f:
            f.write("// Auto-generated NNUE weights from Python training v4.0\n")
            f.write("// Stable version with Huber loss\n\n")
            
            f.write("#ifndef NNUE_WEIGHTS_GENERATED_H\n")
            f.write("#define NNUE_WEIGHTS_GENERATED_H\n\n")
            
            # 양자화 스케일
            scale = 127.0
            
            # 각 레이어 출력
            for name, data in [
                ('w1', self.w1), ('b1', self.b1),
                ('w2', self.w2), ('b2', self.b2),
                ('w3', self.w3), ('b3', self.b3)
            ]:
                # int16으로 양자화
                quantized = np.clip(np.round(data * scale), -32768, 32767).astype(np.int16)
                
                if name.startswith('w'):
                    if len(data.shape) == 2:
                        rows, cols = data.shape
                        f.write(f"static const int16_t nnue_{name}[{rows}][{cols}] = {{\n")
                        for i in range(rows):
                            f.write("    {")
                            for j in range(cols):
                                f.write(f"{quantized[i,j]:6d}")
                                if j < cols - 1:
                                    f.write(", ")
                            f.write("}")
                            if i < rows - 1:
                                f.write(",")
                            f.write("\n")
                    else:
                        size = data.size
                        f.write(f"static const int16_t nnue_{name}[{size}] = {{\n    ")
                        for i in range(size):
                            f.write(f"{quantized.flat[i]:6d}")
                            if i < size - 1:
                                f.write(", ")
                                if (i + 1) % 8 == 0:
                                    f.write("\n    ")
                        f.write("\n")
                else:
                    size = data.size
                    f.write(f"static const int16_t nnue_{name}[{size}] = {{\n    ")
                    for i in range(size):
                        f.write(f"{quantized[i]:6d}")
                        if i < size - 1:
                            f.write(", ")
                            if (i + 1) % 8 == 0:
                                f.write("\n    ")
                    f.write("\n")
                
                f.write("};\n\n")
            
            f.write("#define NNUE_SCALE 127\n")
            f.write("#define NNUE_OUTPUT_SCALE 100\n\n")
            f.write("#endif // NNUE_WEIGHTS_GENERATED_H\n")
        
        print(f"Exported NNUE weights to {filename}")
    
    def copy(self):
        """딥 카피"""
        new_nnue = CompactNNUE()
        new_nnue.w1 = self.w1.copy()
        new_nnue.b1 = self.b1.copy()
        new_nnue.w2 = self.w2.copy()
        new_nnue.b2 = self.b2.copy()
        new_nnue.w3 = self.w3.copy()
        new_nnue.b3 = self.b3.copy()
        
        new_nnue.learning_rate = self.learning_rate
        new_nnue.t = self.t
        
        return new_nnue


class OctaFlipGame:
    """게임 로직"""
    
    def __init__(self):
        self.reset()
        
    def reset(self):
        self.board = [['.' for _ in range(8)] for _ in range(8)]
        self.board[0][0] = RED
        self.board[0][7] = BLUE
        self.board[7][0] = BLUE
        self.board[7][7] = RED
        self.current_player = 0
        self.move_history = []
        self.pass_count = 0
        
    def get_board_hash(self):
        """간단한 해시"""
        board_str = ''.join(''.join(row) for row in self.board)
        return hash(board_str) & 0xFFFFFFFF
    
    def copy(self):
        new_game = OctaFlipGame()
        new_game.board = [row[:] for row in self.board]
        new_game.current_player = self.current_player
        new_game.move_history = self.move_history[:]
        new_game.pass_count = self.pass_count
        return new_game
    
    def get_valid_moves(self):
        moves = []
        player_piece = RED if self.current_player == 0 else BLUE
        
        for r in range(8):
            for c in range(8):
                if self.board[r][c] == player_piece:
                    # Clone
                    for dr in [-1, 0, 1]:
                        for dc in [-1, 0, 1]:
                            if dr == 0 and dc == 0:
                                continue
                            nr, nc = r + dr, c + dc
                            if 0 <= nr < 8 and 0 <= nc < 8 and self.board[nr][nc] == EMPTY:
                                moves.append((r, c, nr, nc, 1))
                    
                    # Jump
                    for dr, dc in [(-2,0), (2,0), (0,-2), (0,2), (-2,-2), (-2,2), (2,-2), (2,2)]:
                        nr, nc = r + dr, c + dc
                        if 0 <= nr < 8 and 0 <= nc < 8 and self.board[nr][nc] == EMPTY:
                            moves.append((r, c, nr, nc, 2))
        
        return moves
    
    def make_move(self, move):
        if move is None:
            self.pass_count += 1
            self.current_player = 1 - self.current_player
            return
        
        r1, c1, r2, c2, move_type = move
        player_piece = RED if self.current_player == 0 else BLUE
        opponent_piece = BLUE if self.current_player == 0 else RED
        
        self.board[r2][c2] = player_piece
        if move_type == 2:
            self.board[r1][c1] = EMPTY
        
        # 뒤집기
        flipped = 0
        for dr in [-1, 0, 1]:
            for dc in [-1, 0, 1]:
                if dr == 0 and dc == 0:
                    continue
                nr, nc = r2 + dr, c2 + dc
                if 0 <= nr < 8 and 0 <= nc < 8 and self.board[nr][nc] == opponent_piece:
                    self.board[nr][nc] = player_piece
                    flipped += 1
        
        self.move_history.append((move, flipped))
        self.pass_count = 0
        self.current_player = 1 - self.current_player
    
    def is_game_over(self):
        if self.pass_count >= 2:
            return True
            
        red_count = sum(row.count(RED) for row in self.board)
        blue_count = sum(row.count(BLUE) for row in self.board)
        empty_count = sum(row.count(EMPTY) for row in self.board)
        
        return red_count == 0 or blue_count == 0 or empty_count == 0
    
    def get_result(self):
        red_count = sum(row.count(RED) for row in self.board)
        blue_count = sum(row.count(BLUE) for row in self.board)
        
        if red_count > blue_count:
            return 1.0, -1.0
        elif blue_count > red_count:
            return -1.0, 1.0
        else:
            return 0.0, 0.0


class OpeningBook:
    """압축된 오프닝북"""
    
    def __init__(self):
        self.book = {}
        self.position_count = {}
        
    def add_game(self, game_moves, result):
        """게임 추가"""
        game = OctaFlipGame()
        
        for i, (move, _) in enumerate(game_moves[:20]):
            if move is None:
                continue
                
            board_hash = game.get_board_hash()
            move_key = f"{move[0]},{move[1]},{move[2]},{move[3]}"
            
            # 가중치
            if result > 0 and game.current_player == 0:
                weight = 3 - i * 0.1
            elif result < 0 and game.current_player == 1:
                weight = 3 - i * 0.1
            else:
                weight = 1
            
            if board_hash not in self.book:
                self.book[board_hash] = {}
            
            if move_key not in self.book[board_hash]:
                self.book[board_hash][move_key] = 0
                
            self.book[board_hash][move_key] += weight
            
            if board_hash not in self.position_count:
                self.position_count[board_hash] = 0
            self.position_count[board_hash] += 1
            
            game.make_move(move)
    
    def get_best_moves(self, board_hash, top_n=3):
        """최선의 수들 반환"""
        if board_hash not in self.book:
            return []
        
        moves = self.book[board_hash]
        sorted_moves = sorted(moves.items(), key=lambda x: x[1], reverse=True)
        
        return sorted_moves[:top_n]
    
    def compress(self):
        """메모리 절약을 위한 압축"""
        compressed = OpeningBook()
        
        for hash_val, moves in self.book.items():
            if hash_val in self.position_count and self.position_count[hash_val] >= 5:
                best_moves = sorted(moves.items(), key=lambda x: x[1], reverse=True)[:5]
                compressed.book[hash_val] = {}
                for move, count in best_moves:
                    compressed.book[hash_val][move] = count
                compressed.position_count[hash_val] = self.position_count[hash_val]
        
        return compressed
    
    def export_to_c_header(self, filename='opening_book_data.h', max_entries=2000):
        """C 헤더 파일로 내보내기"""
        print(f"Exporting opening book to {filename}...")
        
        # 엔트리 준비 (빈도 순 정렬)
        entries = []
        for hash_val, moves in self.book.items():
            if len(moves) > 0 and hash_val in self.position_count:
                if self.position_count[hash_val] >= 3:  # 최소 3번 이상 나온 포지션
                    sorted_moves = sorted(moves.items(), 
                                        key=lambda x: x[1], 
                                        reverse=True)[:5]
                    entries.append((hash_val, sorted_moves, self.position_count[hash_val]))
        
        # 빈도순 정렬
        entries.sort(key=lambda x: x[2], reverse=True)
        entries = entries[:max_entries]
        
        with open(filename, 'w') as f:
            f.write("// Auto-generated OctaFlip opening book from Python training\n")
            f.write("// Generated by train_octoflip.py v4.0\n\n")
            
            f.write("#ifndef OPENING_BOOK_DATA_H\n")
            f.write("#define OPENING_BOOK_DATA_H\n\n")
            
            f.write("#include <stdint.h>\n\n")
            
            f.write("#define HAS_OPENING_BOOK\n\n")
            
            # 구조체 정의
            f.write("typedef struct {\n")
            f.write("    uint32_t hash;\n")
            f.write("    uint8_t moves[5][4];  // up to 5 moves\n")
            f.write("    uint8_t scores[5];    // normalized scores 0-255\n")
            f.write("    uint8_t count;        // number of valid moves\n")
            f.write("} OpeningEntry;\n\n")
            
            # 배열 크기
            f.write(f"#define OPENING_BOOK_SIZE {len(entries)}\n\n")
            
            # 배열 선언
            f.write("static const OpeningEntry opening_book[OPENING_BOOK_SIZE] = {\n")
            
            for idx, (hash_val, moves, freq) in enumerate(entries):
                f.write(f"    {{ // Entry {idx}, frequency: {freq}\n")
                f.write(f"        .hash = 0x{hash_val:08X}U,\n")
                f.write("        .moves = {")
                
                # 최대 5개 수
                for i in range(5):
                    if i < len(moves):
                        move_str = moves[i][0]
                        try:
                            coords = move_str.split(',')
                            f.write(f"{{{coords[0]},{coords[1]},{coords[2]},{coords[3]}}}")
                        except:
                            f.write("{0,0,0,0}")
                    else:
                        f.write("{0,0,0,0}")
                    if i < 4:
                        f.write(", ")
                
                f.write("},\n")
                f.write("        .scores = {")
                
                # 점수 정규화 (0-255)
                max_score = max([m[1] for m in moves]) if moves else 1
                for i in range(5):
                    if i < len(moves):
                        normalized = min(255, int(moves[i][1] * 255 / max_score))
                        f.write(f"{normalized}")
                    else:
                        f.write("0")
                    if i < 4:
                        f.write(", ")
                
                f.write("},\n")
                f.write(f"        .count = {min(5, len(moves))}\n")
                f.write("    }")
                
                if idx < len(entries) - 1:
                    f.write(",")
                f.write("\n")
            
            f.write("};\n\n")
            
            # 헬퍼 함수
            f.write("// Helper function to find opening moves\n")
            f.write("static inline const OpeningEntry* find_opening_move(uint32_t hash) {\n")
            f.write("    for (int i = 0; i < OPENING_BOOK_SIZE; i++) {\n")
            f.write("        if (opening_book[i].hash == hash) {\n")
            f.write("            return &opening_book[i];\n")
            f.write("        }\n")
            f.write("    }\n")
            f.write("    return NULL;\n")
            f.write("}\n\n")
            
            f.write("#endif // OPENING_BOOK_DATA_H\n")
        
        print(f"Exported {len(entries)} opening book entries to {filename}")


# AI 플레이어들
class SimpleAI:
    """간단한 AI"""
    
    def __init__(self, nnue):
        self.nnue = nnue
        
    def get_move(self, game):
        """빠른 평가 기반 움직임 선택"""
        moves = game.get_valid_moves()
        if not moves:
            return None
        
        best_move = moves[0]
        best_score = -float('inf')
        
        # 테스트 시에는 더 적은 움직임만 평가
        max_moves_to_eval = 15 if len(moves) > 15 else len(moves)
        
        for move in moves[:max_moves_to_eval]:
            game_copy = game.copy()
            game_copy.make_move(move)
            
            try:
                score = self.nnue.forward(self.nnue.board_to_input(game_copy.board), training=False)[0]
                
                if game.current_player == 1:
                    score = -score
                
                # 즉시 캡처 보너스 (작은 스케일)
                r1, c1, r2, c2, move_type = move
                opponent_piece = BLUE if game.current_player == 0 else RED
                captures = 0
                for dr in [-1, 0, 1]:
                    for dc in [-1, 0, 1]:
                        if dr == 0 and dc == 0:
                            continue
                        nr, nc = r2 + dr, c2 + dc
                        if 0 <= nr < 8 and 0 <= nc < 8 and game.board[nr][nc] == opponent_piece:
                            captures += 1
                
                score += captures * 10  # 작은 보너스
                
                if score > best_score:
                    best_score = score
                    best_move = move
                    
            except Exception as e:
                continue
        
        return best_move


class GreedyAI:
    """탐욕적 AI - 즉시 캡처 최대화"""
    
    def get_move(self, game):
        moves = game.get_valid_moves()
        if not moves:
            return None
        
        best_move = moves[0]
        best_captures = -1
        
        for move in moves:
            r1, c1, r2, c2, move_type = move
            opponent_piece = BLUE if game.current_player == 0 else RED
            captures = 0
            
            for dr in [-1, 0, 1]:
                for dc in [-1, 0, 1]:
                    if dr == 0 and dc == 0:
                        continue
                    nr, nc = r2 + dr, c2 + dc
                    if 0 <= nr < 8 and 0 <= nc < 8 and game.board[nr][nc] == opponent_piece:
                        captures += 1
            
            if captures > best_captures:
                best_captures = captures
                best_move = move
        
        return best_move


class RandomAI:
    """랜덤 AI"""
    
    def get_move(self, game):
        moves = game.get_valid_moves()
        return random.choice(moves) if moves else None


class PositionalAI:
    """위치 기반 AI"""
    
    POSITION_WEIGHTS = [
        [30, -10,  10,   5,   5,  10, -10,  30],
        [-10, -20,  -5,  -5,  -5,  -5, -20, -10],
        [10,  -5,   5,   3,   3,   5,  -5,  10],
        [ 5,  -5,   3,   1,   1,   3,  -5,   5],
        [ 5,  -5,   3,   1,   1,   3,  -5,   5],
        [10,  -5,   5,   3,   3,   5,  -5,  10],
        [-10, -20,  -5,  -5,  -5,  -5, -20, -10],
        [30, -10,  10,   5,   5,  10, -10,  30]
    ]
    
    def get_move(self, game):
        moves = game.get_valid_moves()
        if not moves:
            return None
        
        best_move = moves[0]
        best_score = -float('inf')
        
        for move in moves:
            r1, c1, r2, c2, move_type = move
            
            # 위치 점수
            score = self.POSITION_WEIGHTS[r2][c2]
            
            # 캡처 보너스
            opponent_piece = BLUE if game.current_player == 0 else RED
            captures = 0
            for dr in [-1, 0, 1]:
                for dc in [-1, 0, 1]:
                    if dr == 0 and dc == 0:
                        continue
                    nr, nc = r2 + dr, c2 + dc
                    if 0 <= nr < 8 and 0 <= nc < 8 and game.board[nr][nc] == opponent_piece:
                        captures += 1
            
            score += captures * 10
            
            if score > best_score:
                best_score = score
                best_move = move
        
        return best_move


def play_game_worker(args):
    """병렬 게임 실행 워커 - 디버깅 개선"""
    try:
        nnue1_weights, nnue2_weights, game_id = args
        
        # 디버깅 정보 (5세대마다만)
        # if 'positional' in str(game_id) and self.generation % 5 == 0:
        #     print(f"[DEBUG] Starting game {game_id} with positional AI")
        
        # NNUE 복원
        nnue1 = CompactNNUE()
        nnue1.w1 = np.array(nnue1_weights['w1'])
        nnue1.b1 = np.array(nnue1_weights['b1'])
        nnue1.w2 = np.array(nnue1_weights['w2'])
        nnue1.b2 = np.array(nnue1_weights['b2'])
        nnue1.w3 = np.array(nnue1_weights['w3'])
        nnue1.b3 = np.array(nnue1_weights['b3'])
        
        # 두 번째 플레이어 처리
        if isinstance(nnue2_weights, dict) and 'w1' in nnue2_weights:
            # NNUE vs NNUE
            nnue2 = CompactNNUE()
            nnue2.w1 = np.array(nnue2_weights['w1'])
            nnue2.b1 = np.array(nnue2_weights['b1'])
            nnue2.w2 = np.array(nnue2_weights['w2'])
            nnue2.b2 = np.array(nnue2_weights['b2'])
            nnue2.w3 = np.array(nnue2_weights['w3'])
            nnue2.b3 = np.array(nnue2_weights['b3'])
            ai2 = SimpleAI(nnue2)
        elif isinstance(nnue2_weights, str):
            # NNUE vs 다른 AI
            if nnue2_weights == 'random':
                ai2 = RandomAI()
            elif nnue2_weights == 'greedy':
                ai2 = GreedyAI()
            elif nnue2_weights == 'positional':
                ai2 = PositionalAI()
                # print(f"[DEBUG] Created PositionalAI for game {game_id}")
            else:
                print(f"[WARNING] Unknown AI type: {nnue2_weights}, using RandomAI")
                ai2 = RandomAI()
        else:
            ai2 = SimpleAI(nnue1)
        
        # AI 생성
        ai1 = SimpleAI(nnue1)
        
        # 게임 실행
        game = OctaFlipGame()
        positions = []
        
        move_count = 0
        while not game.is_game_over() and move_count < 60:  # 100에서 60으로 줄임
            positions.append({
                'board': [row[:] for row in game.board],
                'player': game.current_player,
                'hash': game.get_board_hash()
            })
            
            if game.current_player == 0:
                move = ai1.get_move(game)
            else:
                move = ai2.get_move(game)
            
            # if move is None and 'positional' in str(game_id) and move_count < 10:
            #     print(f"[DEBUG] Game {game_id}: Pass at move {move_count}")
            
            game.make_move(move)
            move_count += 1
        
        # if 'positional' in str(game_id):
        #     print(f"[DEBUG] Game {game_id} completed after {move_count} moves")
        
        result_red, result_blue = game.get_result()
        
        # 학습 데이터 생성 (작은 스케일)
        training_data = []
        for i, pos in enumerate(positions):
            if pos['player'] == 0:
                value = result_red
            else:
                value = result_blue
            
            # 시간 감쇠
            decay = 0.95 ** (i // 10)  # 10수마다 5% 감소
            value *= decay
            
            # 작은 스케일로 변환
            value = np.clip(value * OUTPUT_SCALE * 0.5, -OUTPUT_SCALE, OUTPUT_SCALE)
            
            training_data.append((pos, value))
        
        return training_data, game.move_history, result_red
        
    except Exception as e:
        print(f"[ERROR] Error in game worker {game_id}: {e}")
        print(f"[ERROR] Traceback:")
        traceback.print_exc()
        return [], [], 0.0


class FastTrainer:
    """빠른 학습 시스템 (안정화)"""
    
    def __init__(self, target_hours=10):
        self.target_duration = timedelta(hours=target_hours)
        self.start_time = datetime.now()
        
        # generation을 먼저 초기화
        self.generation = 0
        
        # 통계를 먼저 초기화 (mutate_nnue에서 사용하므로)
        self.stats = {
            'games_played': 0,
            'positions_trained': 0,
            'best_vs_random': 0.0,
            'best_vs_greedy': 0.0,
            'best_vs_positional': 0.0,
            'last_test_time': datetime.now(),
            'training_time_ratio': 0.0,
            'average_loss': 0.0,
            'loss_history': deque(maxlen=100),
            'generation_times': deque(maxlen=20)
        }
        
        # NNUE 풀 
        self.population = []
        self.population_size = 12  # 8에서 6으로 줄임

        # Exploration vs Exploitation 균형
        self.exploration_rate = 0.3  # 30% 탐험
        self.exploration_decay = 0.99  # 매 세대마다 1% 감소
        
        # 기존 가중치 로드 시도
        if os.path.exists('best_nnue.pkl.gz'):
            base_nnue = CompactNNUE('best_nnue.pkl.gz')
            # 변형 생성
            for i in range(self.population_size):
                nnue = base_nnue.copy()
                if i > 0:  # 첫 번째는 원본
                    self.mutate_nnue(nnue, strength=0.001)
                self.population.append(nnue)
        else:
            self.population = [CompactNNUE() for _ in range(self.population_size)]
        
        # Hall of Fame
        self.hall_of_fame = deque(maxlen=HALL_OF_FAME_SIZE)
        
        # 오프닝북
        self.opening_book = OpeningBook()
        if os.path.exists('opening_book.pkl.gz'):
            try:
                with gzip.open('opening_book.pkl.gz', 'rb') as f:
                    self.opening_book = pickle.load(f)
            except:
                pass
        
        # 학습 데이터
        self.training_buffer = deque(maxlen=MAX_BUFFER_SIZE)
        
        # 기존 진행상황 로드
        self.load_progress()
        
    def mutate_nnue(self, nnue, strength=0.001):
        """적응형 mutation"""
        # 세대가 진행될수록 mutation 감소
        adaptive_strength = strength * (0.98 ** (self.generation / 100))
        
        # 성능이 정체되면 mutation 증가
        if hasattr(self, 'stats') and 'loss_history' in self.stats and len(self.stats['loss_history']) > 50:
            recent_losses = list(self.stats['loss_history'])[-50:]
            if np.std(recent_losses) < 0.1:  # 변화가 거의 없으면
                adaptive_strength *= 3.0
                print(f"Increasing mutation due to stagnation")
        
        # Layer별 다른 mutation rate
        nnue.w1 += np.random.randn(*nnue.w1.shape).astype(np.float32) * adaptive_strength
        nnue.w2 += np.random.randn(*nnue.w2.shape).astype(np.float32) * adaptive_strength * 0.8
        nnue.w3 += np.random.randn(*nnue.w3.shape).astype(np.float32) * adaptive_strength * 0.6
        
        # Bias는 더 작게
        nnue.b1 += np.random.randn(*nnue.b1.shape).astype(np.float32) * adaptive_strength * 0.1
        nnue.b2 += np.random.randn(*nnue.b2.shape).astype(np.float32) * adaptive_strength * 0.08
        nnue.b3 += np.random.randn(*nnue.b3.shape).astype(np.float32) * adaptive_strength * 0.05
        
        nnue.normalize_weights()
        
    def load_progress(self):
        """이전 진행상황 로드"""
        if os.path.exists('training_stats.json'):
            try:
                with open('training_stats.json', 'r') as f:
                    saved_stats = json.load(f)
                    self.stats.update(saved_stats)
                    self.generation = saved_stats.get('generation', 0)
                    
                    if 'last_test_time' in saved_stats:
                        try:
                            if isinstance(saved_stats['last_test_time'], str):
                                self.stats['last_test_time'] = datetime.fromisoformat(saved_stats['last_test_time'])
                            else:
                                self.stats['last_test_time'] = datetime.now()
                        except:
                            self.stats['last_test_time'] = datetime.now()
                    
                    # deque 변환
                    for key in ['loss_history', 'generation_times']:
                        if key in saved_stats:
                            maxlen = 100 if key == 'loss_history' else 20
                            self.stats[key] = deque(saved_stats[key], maxlen=maxlen)
                            
                print(f"Resumed from generation {self.generation}")
                print(f"Previous games: {self.stats['games_played']}")
                
            except Exception as e:
                print(f"Error loading progress: {e}")
                
    def should_continue(self):
        """시간 체크"""
        elapsed = datetime.now() - self.start_time
        return elapsed < self.target_duration
        
    def validate_training_data(self, buffer_list):
        """학습 데이터 검증 및 정규화"""
        cleaned_data = []
        
        for pos, value in buffer_list:
            # Outlier 제거
            if abs(value) > OUTPUT_SCALE * 2:
                value = np.sign(value) * OUTPUT_SCALE
            
            # NaN/Inf 체크
            if np.isnan(value) or np.isinf(value):
                value = 0
                
            cleaned_data.append((pos, value))
        
        return cleaned_data
        
    def run_generation(self):
        """한 세대 실행"""
        gen_start = datetime.now()
        
        print(f"Gen {self.generation} | Games: {self.stats['games_played']}")
        
        # Loss 상태 출력 (간략히)
        if self.stats['loss_history'] and self.generation % 5 == 0:
            recent_losses = list(self.stats['loss_history'])[-10:]
            avg_loss = np.mean(recent_losses)
            print(f"Avg loss: {avg_loss:.2f}")
        
        # 1. Self-play 토너먼트
        scores = [0] * self.population_size
        games_data = []
        
        # 병렬 게임 실행
        with mp.Pool(WORKERS) as pool:
            tasks = []
            
            # Self-play games
            for i in range(self.population_size):
                for j in range(i + 1, self.population_size):
                    # 가중치 준비
                    w1 = {
                        'w1': self.population[i].w1.tolist(),
                        'b1': self.population[i].b1.tolist(),
                        'w2': self.population[i].w2.tolist(),
                        'b2': self.population[i].b2.tolist(),
                        'w3': self.population[i].w3.tolist(),
                        'b3': self.population[i].b3.tolist()
                    }
                    w2 = {
                        'w1': self.population[j].w1.tolist(),
                        'b1': self.population[j].b1.tolist(),
                        'w2': self.population[j].w2.tolist(),
                        'b2': self.population[j].b2.tolist(),
                        'w3': self.population[j].w3.tolist(),
                        'b3': self.population[j].b3.tolist()
                    }
                    
                    # 양방향 게임
                    tasks.append((i, j, (w1, w2, f"{self.generation}_{i}_{j}_1")))
                    if len(tasks) < 50:
                        tasks.append((j, i, (w2, w1, f"{self.generation}_{j}_{i}_2")))
            
            # 다양한 상대와 게임 (더 적게)
            for i in range(min(2, self.population_size)):  # 4에서 2로 줄임
                w1 = {
                    'w1': self.population[i].w1.tolist(),
                    'b1': self.population[i].b1.tolist(),
                    'w2': self.population[i].w2.tolist(),
                    'b2': self.population[i].b2.tolist(),
                    'w3': self.population[i].w3.tolist(),
                    'b3': self.population[i].b3.tolist()
                }
                
                # vs Random (1게임)
                tasks.append((i, -1, (w1, 'random', f"{self.generation}_{i}_random")))
                
                # vs Greedy (1게임)
                tasks.append((i, -2, (w1, 'greedy', f"{self.generation}_{i}_greedy")))
                
                # vs Positional (격세대)
                if self.generation % 2 == 0:
                    tasks.append((i, -3, (w1, 'positional', f"{self.generation}_{i}_positional")))
            
            print(f"Running {len(tasks)} games...", end='', flush=True)
            
            # 실행
            results = pool.map(play_game_worker, [task[2] for task in tasks])
            
            # 결과 처리
            for idx, (player1, player2, _) in enumerate(tasks):
                training_data, moves, result = results[idx]
                
                if training_data:
                    games_data.extend(training_data)
                    
                    # 오프닝북에 추가
                    if abs(result) > 0.5:
                        self.opening_book.add_game(moves, result)
                    
                    # 점수 계산
                    if player1 >= 0 and player2 >= 0:
                        if result > 0:  # Red 승리
                            scores[player1] += 3
                        elif result < 0:  # Blue 승리  
                            scores[player2] += 3
                        else:  # 무승부
                            scores[player1] += 1
                            scores[player2] += 1
                    
                    self.stats['games_played'] += 1
        
        # 학습 데이터 추가
        self.training_buffer.extend(games_data)
        
        # 10세대마다만 자세한 정보 출력
        if self.generation % 10 == 0:
            print(f"Scores: {scores}, Buffer: {len(self.training_buffer)} positions")
        
        # 상위 절반 선택하여 학습
        ranked = sorted(zip(scores, range(self.population_size)), reverse=True)
        best_indices = [idx for _, idx in ranked[:self.population_size//2]]
        
        # 학습
        train_start = datetime.now()
        self.train_networks(best_indices)
        train_time = (datetime.now() - train_start).total_seconds()
        
        # 새로운 세대 생성
        self.create_new_generation(ranked)
        
        # Hall of Fame 업데이트 (20세대마다)
        if self.generation % 20 == 0:
            best_idx = ranked[0][1]
            self.hall_of_fame.append(self.population[best_idx].copy())
        
        self.generation += 1
        
        # 통계
        gen_time = (datetime.now() - gen_start).total_seconds()
        self.stats['training_time_ratio'] = train_time / gen_time if gen_time > 0 else 0
        self.stats['generation_times'].append(gen_time)
        
        print(f" Done! Gen time: {gen_time:.1f}s")
        
    def train_networks(self, indices):
        """네트워크 학습 - 우선순위 기반 experience replay"""
        if len(self.training_buffer) < BATCH_SIZE:
            print(f"Insufficient training data: {len(self.training_buffer)} < {BATCH_SIZE}")
            return
        
        # 학습 파라미터
        epochs = 2  # 1 → 2 epochs
        mini_batch_size = min(512, BATCH_SIZE)  # 더 큰 배치 사용
        
        # 최근 데이터에 더 높은 가중치 부여
        buffer_list = list(self.training_buffer)
        
        # 데이터 검증 및 정리
        buffer_list = self.validate_training_data(buffer_list)
        
        # 가중치 계산 (최근 데이터일수록 높은 가중치)
        recency_weights = np.array([0.95 ** (len(buffer_list) - i - 1) 
                                    for i in range(len(buffer_list))])
        recency_weights /= recency_weights.sum()
        
        # 각 네트워크에 대해 학습
        for idx in indices:
            nnue = self.population[idx]
            
            # 네트워크별 학습 통계
            network_losses = []
            total_loss = 0
            batches = 0
            improved_batches = 0
            
            # 세대 정보를 NNUE에 전달 (학습률 스케줄링용)
            nnue.generation = self.generation
            
            for epoch in range(epochs):
                # 가중치 기반 샘플링
                sample_size = min(len(buffer_list), mini_batch_size * 20)
                sampled_indices = np.random.choice(
                    len(buffer_list), 
                    size=sample_size,
                    p=recency_weights,
                    replace=True
                )
                
                sampled_data = [buffer_list[i] for i in sampled_indices]
                
                # 데이터 증강 적용 (선택적)
                if self.generation % 10 == 0 and epoch == 0:  # 10세대마다 증강
                    augmented_data = []
                    for pos, value in sampled_data[:sample_size//4]:  # 25%만 증강
                        # 원본
                        augmented_data.append((pos, value))
                        
                        # 180도 회전 (값은 동일)
                        board = pos['board']
                        rotated_board = [[board[7-i][7-j] for j in range(8)] for i in range(8)]
                        rotated_pos = {
                            'board': rotated_board,
                            'player': pos['player'],
                            'hash': 0  # 단순화를 위해
                        }
                        augmented_data.append((rotated_pos, value))
                    
                    sampled_data.extend(augmented_data)
                
                # 셔플
                random.shuffle(sampled_data)
                
                # 미니배치 학습
                epoch_loss = 0
                epoch_batches = 0
                
                for i in range(0, len(sampled_data) - mini_batch_size, mini_batch_size):
                    batch = sampled_data[i:i + mini_batch_size]
                    
                    # 배치 데이터 준비
                    X = np.zeros((mini_batch_size, INPUT_SIZE), dtype=np.float32)
                    y = np.zeros(mini_batch_size, dtype=np.float32)
                    
                    # 유효한 데이터만 사용
                    valid_count = 0
                    for j, (pos, value) in enumerate(batch):
                        if valid_count >= mini_batch_size:
                            break
                        
                        # 값 검증
                        if abs(value) > OUTPUT_SCALE * 2:
                            value = np.sign(value) * OUTPUT_SCALE
                        
                        X[valid_count] = nnue.board_to_input(pos['board'])
                        y[valid_count] = value
                        valid_count += 1
                    
                    if valid_count < mini_batch_size // 2:  # 유효 데이터가 너무 적으면 스킵
                        continue
                    
                    # 실제 사용할 데이터만 추출
                    X = X[:valid_count]
                    y = y[:valid_count]
                    
                    # 학습 전 loss
                    try:
                        y_pred_before = nnue.forward(X, training=True)
                        loss_before = np.mean(nnue.huber_loss(y, y_pred_before))
                        
                        # 역전파 및 가중치 업데이트
                        y_pred = nnue.forward(X, training=True)
                        loss = nnue.backward_adam(X, y, y_pred)
                        
                        # NaN/Inf 체크
                        if np.isnan(loss) or np.isinf(loss) or loss > 10000:
                            print(f"  Network {idx}: Invalid loss {loss:.2f}, restoring weights")
                            # 이전 가중치로 복원 (구현 필요시)
                            continue
                        
                        # 개선 여부 체크
                        if loss < loss_before * 0.99:  # 1% 이상 개선
                            improved_batches += 1
                        
                        total_loss += loss
                        epoch_loss += loss
                        network_losses.append(loss)
                        batches += 1
                        epoch_batches += 1
                        
                    except Exception as e:
                        print(f"  Network {idx}: Training error in batch: {e}")
                        continue
                    
                    self.stats['positions_trained'] += valid_count
                    
                    # 주기적 출력 (디버깅용)
                    if batches % 50 == 0 and self.generation % 10 == 0:
                        recent_avg = np.mean(network_losses[-10:]) if len(network_losses) >= 10 else loss
                        print(f"    Network {idx}: {batches} batches, recent loss: {recent_avg:.4f}")
                
                # Epoch 완료 메시지
                if epoch_batches > 0:
                    epoch_avg_loss = epoch_loss / epoch_batches
                    if self.generation % 10 == 0:
                        print(f"  Network {idx} Epoch {epoch+1}: avg loss = {epoch_avg_loss:.4f}")
            
            # 네트워크 학습 완료
            if batches > 0:
                avg_loss = total_loss / batches
                self.stats['loss_history'].append(avg_loss)
                
                # 개선률 계산
                improvement_rate = improved_batches / batches if batches > 0 else 0
                
                # 상세 출력 (5세대마다)
                if self.generation % 5 == 0:
                    print(f"  Network {idx}: Loss = {avg_loss:.4f}, "
                        f"Improvement rate = {improvement_rate:.1%}, "
                        f"Batches = {batches}")
                
                # 학습이 정체된 경우 경고
                if len(network_losses) > 20:
                    recent_std = np.std(network_losses[-20:])
                    if recent_std < 0.01:
                        print(f"  Network {idx}: WARNING - Learning stagnated (std={recent_std:.4f})")
                        
                        # 학습률 조정 제안
                        if hasattr(nnue, 'learning_rate'):
                            print(f"    Current LR: {nnue.learning_rate:.2e}")
                            nnue.learning_rate *= 0.8  # 20% 감소
                            print(f"    New LR: {nnue.learning_rate:.2e}")
            else:
                print(f"  Network {idx}: No valid batches processed")
        
        # 학습 버퍼 관리 - 오래된 데이터 일부 제거
        if len(self.training_buffer) > MAX_BUFFER_SIZE * 0.9:
            # 가장 오래된 10% 제거
            remove_count = int(MAX_BUFFER_SIZE * 0.1)
            for _ in range(remove_count):
                self.training_buffer.popleft()
            
            if self.generation % 20 == 0:
                print(f"  Cleaned training buffer: removed {remove_count} old samples")
        
        # 전체 학습 통계
        if self.stats['loss_history']:
            recent_losses = list(self.stats['loss_history'])[-20:]
            avg_recent_loss = np.mean(recent_losses)
            loss_trend = np.polyfit(range(len(recent_losses)), recent_losses, 1)[0]
            
            if self.generation % 10 == 0:
                print(f"\n  Overall training stats:")
                print(f"    Average recent loss: {avg_recent_loss:.4f}")
                print(f"    Loss trend: {'↓' if loss_trend < -0.01 else '↑' if loss_trend > 0.01 else '→'} "
                    f"({loss_trend:.4f})")
                print(f"    Total positions trained: {self.stats['positions_trained']:,}")
    
    def create_new_generation(self, rankings):
        """토너먼트 선택 방식 개선"""
        new_population = []
        
        # 엘리트 유지 (상위 16%)
        elite_count = max(2, self.population_size // 6)
        for i in range(elite_count):
            _, idx = rankings[i]
            new_population.append(self.population[idx].copy())
        
        # 토너먼트 선택
        while len(new_population) < self.population_size:
            # 3-way tournament
            tournament_size = 3
            tournament = random.sample(rankings[:self.population_size//2], tournament_size)
            winner = max(tournament, key=lambda x: x[0])
            
            parent_idx = winner[1]
            child = self.population[parent_idx].copy()
            
            # 다양성을 위한 강한 mutation (확률적)
            if random.random() < self.exploration_rate:
                noise_strength = 0.01  # 10배 강한 mutation
            else:
                noise_strength = 0.001
                
            self.mutate_nnue(child, strength=noise_strength)
            new_population.append(child)
        
        # Exploration rate decay
        self.exploration_rate *= self.exploration_decay
        self.exploration_rate = max(self.exploration_rate, 0.05)  # 최소 5%
        
        self.population = new_population   

    def test_best_network(self):
        """최고 네트워크 테스트 - 빠른 버전"""
        print("\nQuick testing best network...")
        
        best_nnue = self.population[0]
        ai = SimpleAI(best_nnue)
        
        test_results = {
            'random': 0,
            'greedy': 0,
            'positional': 0
        }
        
        # vs Random (4게임)
        print(f"  Playing 4 games vs Random...", end='', flush=True)
        for i in range(4):
            game = OctaFlipGame()
            opponent = RandomAI()
            
            color = i % 2  # 0, 1, 0, 1 (선후공 교대)
            move_count = 0
            
            while not game.is_game_over() and move_count < 80:  # 50수 제한
                if game.current_player == color:
                    move = ai.get_move(game)
                else:
                    move = opponent.get_move(game)
                
                game.make_move(move)
                move_count += 1
            
            result_red, result_blue = game.get_result()
            if (color == 0 and result_red > 0) or (color == 1 and result_blue > 0):
                test_results['random'] += 1
        
        print(f" {test_results['random']}/4 wins")
        
        # vs Greedy (4게임)  
        print(f"  Playing 4 games vs Greedy...", end='', flush=True)
        for i in range(4):
            game = OctaFlipGame()
            opponent = GreedyAI()
            
            color = i % 2  # 선후공 교대
            move_count = 0
            
            while not game.is_game_over() and move_count < 50:  # 50수 제한
                if game.current_player == color:
                    move = ai.get_move(game)
                else:
                    move = opponent.get_move(game)
                
                game.make_move(move)
                move_count += 1
            
            result_red, result_blue = game.get_result()
            if (color == 0 and result_red > 0) or (color == 1 and result_blue > 0):
                test_results['greedy'] += 1
        
        print(f" {test_results['greedy']}/4 wins")
        
        # vs Positional (2게임)
        print(f"  Playing 2 games vs Positional...", end='', flush=True)
        
        for i in range(2):
            try:
                game = OctaFlipGame()
                opponent = PositionalAI()
                
                color = i % 2  # 선후공 교대
                move_count = 0
                
                while not game.is_game_over() and move_count < 50:  # 50수 제한
                    if game.current_player == color:
                        move = ai.get_move(game)
                    else:
                        move = opponent.get_move(game)
                    
                    game.make_move(move)
                    move_count += 1
                
                result_red, result_blue = game.get_result()
                if (color == 0 and result_red > 0) or (color == 1 and result_blue > 0):
                    test_results['positional'] += 1
                    
            except Exception as e:
                print(f"\n[ERROR] Error in positional test game {i}: {e}")
                # traceback.print_exc()  # 필요시만 활성화
        
        print(f" {test_results['positional']}/2 wins")
        
        # 통계 업데이트
        self.stats['best_vs_random'] = test_results['random'] * 25  # 4게임 기준 백분율
        self.stats['best_vs_greedy'] = test_results['greedy'] * 25  # 4게임 기준 백분율
        self.stats['best_vs_positional'] = test_results['positional'] * 50  # 2게임 기준 백분율
        
        print(f"\nWin rates:")
        print(f"  vs Random: {self.stats['best_vs_random']:.1f}%")
        print(f"  vs Greedy: {self.stats['best_vs_greedy']:.1f}%")
        print(f"  vs Positional: {self.stats['best_vs_positional']:.1f}%")
        
        overall = (self.stats['best_vs_random'] + self.stats['best_vs_greedy'] + 
                  self.stats['best_vs_positional']) / 3
        print(f"  Overall: {overall:.1f}%")
        
    def save_progress(self):
        """진행상황 저장"""
        # 최고 NNUE 저장
        best_nnue = self.population[0]
        weights = {
            'w1': best_nnue.w1.tolist(),
            'b1': best_nnue.b1.tolist(),
            'w2': best_nnue.w2.tolist(),
            'b2': best_nnue.b2.tolist(),
            'w3': best_nnue.w3.tolist(),
            'b3': best_nnue.b3.tolist()
        }
        
        with gzip.open('best_nnue.pkl.gz', 'wb') as f:
            pickle.dump(weights, f)
        
        # C 헤더 파일 생성
        best_nnue.export_to_c_arrays()
        
        # 오프닝북 압축 및 저장 (10세대마다만)
        if self.generation % 10 == 0:
            compressed_book = self.opening_book.compress()
            with gzip.open('opening_book.pkl.gz', 'wb') as f:
                pickle.dump(compressed_book, f)
            
            # Opening book을 C header로 export
            if len(compressed_book.book) > 0:
                compressed_book.export_to_c_header('opening_book_data.h')
                print("Exported opening book to opening_book_data.h")
            else:
                print("Opening book is empty, skipping C header export")
        
        # 통계 저장
        self.stats['generation'] = self.generation
        if isinstance(self.stats['last_test_time'], datetime):
            self.stats['last_test_time'] = self.stats['last_test_time'].isoformat()
        
        # deque를 list로 변환
        stats_to_save = self.stats.copy()
        for key in ['loss_history', 'generation_times']:
            if key in stats_to_save:
                stats_to_save[key] = list(stats_to_save[key])
                
        with open('training_stats.json', 'w') as f:
            json.dump(stats_to_save, f, indent=2)
        
        print(f"Progress saved (Gen {self.generation})")        

    def train(self):
        """메인 학습 루프"""
        print(f"Fast training for {self.target_duration.total_seconds()/3600:.1f} hours...")
        print(f"Population: {self.population_size}, Workers: {WORKERS}, Batch: {BATCH_SIZE}")
        print(f"✨ v4.0 Optimized - Faster testing & training")
        print("-" * 50)
        
        last_save_gen = self.generation
        
        try:
            while self.should_continue():
                self.run_generation()
                
                # 저장
                if self.generation - last_save_gen >= SAVE_INTERVAL_GENERATIONS:
                    self.save_progress()
                    last_save_gen = self.generation
                    
                    # 테스트 (매 40세대마다만)
                    if self.generation % 40 == 0:
                        self.test_best_network()
                        self.stats['last_test_time'] = datetime.now()
                
                # 진행상황 (더 간단히)
                if self.generation % 10 == 0:
                    elapsed = datetime.now() - self.start_time
                    progress = elapsed.total_seconds() / self.target_duration.total_seconds() * 100
                    remaining = self.target_duration - elapsed
                    print(f"\nProgress: {progress:.1f}% | Games: {self.stats['games_played']:,} | Time left: {remaining}")
                
        except KeyboardInterrupt:
            print("\nTraining interrupted by user")
        
        # 최종 저장
        self.save_progress()
        self.test_best_network()
        
        # 최종 opening book export
        final_book = self.opening_book.compress()
        if len(final_book.book) > 0:
            final_book.export_to_c_header('opening_book_data.h')
            print("\nFinal opening book exported to opening_book_data.h")
            print(f"Total positions in opening book: {len(final_book.book)}")
        
        print("\nTraining completed!")
        
        return self.population[0], final_book


def main():
    print("=== OctaFlip NNUE Training System v4.0 (Speed Optimized) ===\n")
    
    print("Training options:")
    print("1. Quick training (1 hour)")
    print("2. Standard training (10 hours)")
    print("3. Extended training (24 hours)")
    print("4. Custom duration")
    print("5. Quick test existing weights (very fast)")
    print("6. Debug test (PositionalAI only)")
    print("7. Export existing opening book to C header")
    
    choice = input("\nSelect option (1-7): ")
    
    if choice == '1':
        hours = 1
    elif choice == '2':
        hours = 10
    elif choice == '3':
        hours = 24
    elif choice == '4':
        hours = float(input("Enter hours: "))
    elif choice == '5':
        # 기존 가중치 빠른 테스트
        if os.path.exists('best_nnue.pkl.gz'):
            print("\nQuick testing existing weights...")
            
            # 단일 NNUE 로드
            nnue = CompactNNUE('best_nnue.pkl.gz')
            ai = SimpleAI(nnue)
            
            # 각 AI와 2게임씩만
            wins = {'random': 0, 'greedy': 0, 'positional': 0}
            
            # vs Random (2게임)
            print("vs Random: ", end='', flush=True)
            for i in range(2):
                game = OctaFlipGame()
                opp = RandomAI()
                color = i % 2
                moves = 0
                
                while not game.is_game_over() and moves < 40:
                    if game.current_player == color:
                        move = ai.get_move(game)
                    else:
                        move = opp.get_move(game)
                    game.make_move(move)
                    moves += 1
                    
                r, b = game.get_result()
                if (color == 0 and r > 0) or (color == 1 and b > 0):
                    wins['random'] += 1
            print(f"{wins['random']}/2 wins")
            
            # vs Greedy (2게임)
            print("vs Greedy: ", end='', flush=True)
            for i in range(2):
                game = OctaFlipGame()
                opp = GreedyAI()
                color = i % 2
                moves = 0
                
                while not game.is_game_over() and moves < 40:
                    if game.current_player == color:
                        move = ai.get_move(game)
                    else:
                        move = opp.get_move(game)
                    game.make_move(move)
                    moves += 1
                    
                r, b = game.get_result()
                if (color == 0 and r > 0) or (color == 1 and b > 0):
                    wins['greedy'] += 1
            print(f"{wins['greedy']}/2 wins")
            
            # vs Positional (1게임)
            print("vs Positional: ", end='', flush=True)
            game = OctaFlipGame()
            opp = PositionalAI()
            moves = 0
            
            while not game.is_game_over() and moves < 40:
                if game.current_player == 0:
                    move = ai.get_move(game)
                else:
                    move = opp.get_move(game)
                game.make_move(move)
                moves += 1
                
            r, b = game.get_result()
            if r > 0:
                wins['positional'] = 1
            print(f"{wins['positional']}/1 wins")
            
            print(f"\nQuick test results: Random={wins['random']*50}%, Greedy={wins['greedy']*50}%, Positional={wins['positional']*100}%")
        else:
            print("No weights found!")
        return
    elif choice == '6':
        # PositionalAI 디버그 테스트
        print("\nDebug test: Playing games with PositionalAI...")
        
        game = OctaFlipGame()
        ai1 = PositionalAI()
        ai2 = RandomAI()
        
        print("Testing PositionalAI vs RandomAI...")
        move_count = 0
        
        try:
            while not game.is_game_over() and move_count < 100:
                print(f"\nMove {move_count + 1}, Player: {game.current_player}")
                
                if game.current_player == 0:
                    move = ai1.get_move(game)
                    print(f"PositionalAI move: {move}")
                else:
                    move = ai2.get_move(game)
                    print(f"RandomAI move: {move}")
                
                if move is None:
                    print("Pass!")
                    
                game.make_move(move)
                move_count += 1
                
                # 보드 출력
                for row in game.board:
                    print(' '.join(row))
                
            print(f"\nGame finished after {move_count} moves")
            result_red, result_blue = game.get_result()
            print(f"Result: Red={result_red}, Blue={result_blue}")
            
        except Exception as e:
            print(f"Error during test: {e}")
            traceback.print_exc()
            
        return
    elif choice == '7':
        # Export existing opening book
        print("\nExporting existing opening book to C header...")
        
        if os.path.exists('opening_book.pkl.gz'):
            try:
                with gzip.open('opening_book.pkl.gz', 'rb') as f:
                    book = pickle.load(f)
                
                if isinstance(book, OpeningBook):
                    book.export_to_c_header('opening_book_data.h')
                    print(f"Successfully exported {len(book.book)} positions to opening_book_data.h")
                else:
                    print("Error: Invalid opening book format")
            except Exception as e:
                print(f"Error loading opening book: {e}")
        else:
            print("No opening book found (opening_book.pkl.gz)")
            print("Run training first to generate an opening book")
        return
    else:
        hours = 10
    
    # 학습 시작
    trainer = FastTrainer(target_hours=hours)
    best_nnue, compressed_book = trainer.train()
    
    print("\n" + "="*60)
    print("✅ Training complete!")
    print(f"📊 Final statistics:")
    print(f"   - Total games: {trainer.stats['games_played']:,}")
    print(f"   - Positions trained: {trainer.stats['positions_trained']:,}")
    print(f"   - Win vs Random: {trainer.stats['best_vs_random']:.1f}%")
    print(f"   - Win vs Greedy: {trainer.stats['best_vs_greedy']:.1f}%")
    print(f"   - Win vs Positional: {trainer.stats['best_vs_positional']:.1f}%")
    print(f"   - Generation: {trainer.generation}")
    
    if trainer.stats['loss_history']:
        avg_loss = sum(trainer.stats['loss_history']) / len(trainer.stats['loss_history'])
        print(f"   - Average loss: {avg_loss:.2f}")
    
    # Opening book 통계 추가
    if compressed_book and len(compressed_book.book) > 0:
        print(f"   - Opening book positions: {len(compressed_book.book)}")
        total_games = sum(compressed_book.position_count.values())
        print(f"   - Total position occurrences: {total_games}")
    
    print("\n💾 Files generated:")
    print("   - best_nnue.pkl.gz (Python weights)")
    print("   - nnue_weights_generated.h (C header)")
    print("   - opening_book.pkl.gz (Opening book)")
    print("   - opening_book_data.h (Opening book C header)")
    print("   - training_stats.json (Statistics)")
    
    print("\n✨ Ready for integration with client.c!")


if __name__ == "__main__":
    main()