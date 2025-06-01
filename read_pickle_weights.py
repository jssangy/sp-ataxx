#!/usr/bin/env python3
"""
OctaFlip NNUE 고효율 학습 시스템 v2.2 - 안정화 강화 버전
- Loss 폭발 방지
- 그래디언트 클리핑 강화
- 학습률 자동 조정
- 가중치 복구 메커니즘
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
from datetime import datetime, timedelta
import warnings
warnings.filterwarnings('ignore')

# 게임 상수
BOARD_SIZE = 8
RED = 'R'
BLUE = 'B' 
EMPTY = '.'

# 학습 설정 (M3 최적화)
WORKERS = min(mp.cpu_count() - 2, 8)
BATCH_SIZE = 2048  # 4096 -> 2048 (더 안정적)
GAMES_PER_GENERATION = 100
TARGET_DURATION_HOURS = 10

# NNUE 구조
INPUT_SIZE = 192
HIDDEN1_SIZE = 64
HIDDEN2_SIZE = 32
OUTPUT_SIZE = 1

# 테스트 설정
TEST_INTERVAL_MINUTES = 120
QUICK_TEST_GAMES = 10
SAVE_INTERVAL_GENERATIONS = 5

# 학습 데이터 설정
MAX_BUFFER_SIZE = 100000  # 200000 -> 100000
HALL_OF_FAME_SIZE = 10


class CompactNNUE:
    """메모리 효율적인 NNUE (안정화 강화)"""
    
    def __init__(self, init_from_file=None):
        if init_from_file and os.path.exists(init_from_file):
            self.load_weights(init_from_file)
        else:
            # 더 작은 초기화 (He initialization with smaller scale)
            scale = 0.1  # 0.5 -> 0.1
            self.w1 = np.random.randn(INPUT_SIZE, HIDDEN1_SIZE).astype(np.float32) * np.sqrt(2.0 / INPUT_SIZE) * scale
            self.b1 = np.zeros(HIDDEN1_SIZE, dtype=np.float32)
            
            self.w2 = np.random.randn(HIDDEN1_SIZE, HIDDEN2_SIZE).astype(np.float32) * np.sqrt(2.0 / HIDDEN1_SIZE) * scale
            self.b2 = np.zeros(HIDDEN2_SIZE, dtype=np.float32)
            
            self.w3 = np.random.randn(HIDDEN2_SIZE, OUTPUT_SIZE).astype(np.float32) * np.sqrt(2.0 / HIDDEN2_SIZE) * scale
            self.b3 = np.zeros(OUTPUT_SIZE, dtype=np.float32)
        
        # Adam optimizer 파라미터 (더 안전하게)
        self.learning_rate = 0.00001  # 매우 낮은 학습률
        self.beta1 = 0.9
        self.beta2 = 0.999
        self.epsilon = 1e-8
        self.t = 0
        
        # Momentum (초기화)
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
        
        # 백업용 가중치
        self.backup_weights = None
        self.backup_loss = float('inf')
        
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
        """가중치 정규화 (폭발 방지)"""
        # L2 norm으로 제한
        for w in [self.w1, self.w2, self.w3]:
            norm = np.linalg.norm(w)
            if norm > 10.0:
                w *= 10.0 / norm
                
    def save_backup(self, loss):
        """현재 가중치 백업"""
        if loss < self.backup_loss:
            self.backup_weights = {
                'w1': self.w1.copy(),
                'b1': self.b1.copy(),
                'w2': self.w2.copy(),
                'b2': self.b2.copy(),
                'w3': self.w3.copy(),
                'b3': self.b3.copy()
            }
            self.backup_loss = loss
            
    def restore_backup(self):
        """백업된 가중치 복원"""
        if self.backup_weights:
            self.w1 = self.backup_weights['w1'].copy()
            self.b1 = self.backup_weights['b1'].copy()
            self.w2 = self.backup_weights['w2'].copy()
            self.b2 = self.backup_weights['b2'].copy()
            self.w3 = self.backup_weights['w3'].copy()
            self.b3 = self.backup_weights['b3'].copy()
            print("Restored backup weights")
            
    def forward(self, x):
        """순전파 (안정화)"""
        if len(x.shape) == 1:
            x = x.reshape(1, -1)
            
        self.x = x
        
        # Layer 1
        self.z1 = np.dot(x, self.w1) + self.b1
        self.z1 = np.clip(self.z1, -10, 10)  # Pre-activation 클리핑
        self.a1 = np.maximum(0, self.z1)  # ReLU
        
        # Layer 2
        self.z2 = np.dot(self.a1, self.w2) + self.b2
        self.z2 = np.clip(self.z2, -10, 10)
        self.a2 = np.maximum(0, self.z2)  # ReLU
        
        # Output layer
        self.z3 = np.dot(self.a2, self.w3) + self.b3
        self.z3 = np.clip(self.z3, -100, 100)  # 출력 제한
        
        # NNUE 스케일 출력
        output = np.tanh(self.z3 / 400) * 10000
        
        return output.flatten()
    
    def backward_adam(self, x, y_true, y_pred, learning_rate_scale=1.0):
        """Adam optimizer로 역전파 (안정화 강화)"""
        self.t += 1
        batch_size = x.shape[0] if len(x.shape) > 1 else 1
        
        # NaN/Inf 체크
        if np.any(np.isnan(y_pred)) or np.any(np.isinf(y_pred)):
            print("WARNING: NaN/Inf in predictions!")
            return 1000000.0
        
        # 타겟 값 안정화
        y_true = np.clip(y_true, -10000, 10000)
        y_pred = np.clip(y_pred, -10000, 10000)
        
        # Loss 계산 (안정화)
        diff = y_pred - y_true
        mse = np.mean(diff ** 2)
        
        # Loss 스케일
        loss_scale = 1.0 / 100000.0
        
        # 손실 그래디언트
        d_loss = 2 * diff / batch_size * loss_scale
        
        # Output layer gradients
        d_tanh = 1 - np.tanh(self.z3 / 400) ** 2
        d_z3 = d_loss.reshape(-1, 1) * d_tanh * 10000 / 400
        d_z3 = np.clip(d_z3, -1.0, 1.0)  # 그래디언트 클리핑
        
        d_w3 = np.dot(self.a2.T, d_z3)
        d_b3 = np.sum(d_z3, axis=0)
        
        # Hidden layer 2 gradients
        d_a2 = np.dot(d_z3, self.w3.T)
        d_z2 = d_a2 * (self.z2 > 0)
        d_z2 = np.clip(d_z2, -1.0, 1.0)
        
        d_w2 = np.dot(self.a1.T, d_z2)
        d_b2 = np.sum(d_z2, axis=0)
        
        # Hidden layer 1 gradients
        d_a1 = np.dot(d_z2, self.w2.T)
        d_z1 = d_a1 * (self.z1 > 0)
        d_z1 = np.clip(d_z1, -1.0, 1.0)
        
        d_w1 = np.dot(x.T, d_z1)
        d_b1 = np.sum(d_z1, axis=0)
        
        # 강화된 Gradient clipping
        max_grad_norm = 0.1
        all_grads = [d_w1, d_w2, d_w3, d_b1, d_b2, d_b3]
        
        for i, grad in enumerate(all_grads):
            grad_norm = np.linalg.norm(grad)
            if grad_norm > max_grad_norm:
                all_grads[i] = grad * max_grad_norm / grad_norm
        
        d_w1, d_w2, d_w3, d_b1, d_b2, d_b3 = all_grads
        
        # 학습률 조정 (매우 보수적)
        lr = self.learning_rate * learning_rate_scale * 0.01
        
        # Loss가 크면 학습률 더 낮추기
        if mse > 1000000:
            lr *= 0.1
        
        # Adam update
        def adam_update(param, grad, m, v):
            # NaN 체크
            if np.any(np.isnan(grad)):
                print("WARNING: NaN gradient detected!")
                return param, m, v
                
            m = self.beta1 * m + (1 - self.beta1) * grad
            v = self.beta2 * v + (1 - self.beta2) * (grad ** 2)
            
            m_hat = m / (1 - self.beta1 ** self.t)
            v_hat = v / (1 - self.beta2 ** self.t)
            
            # 업데이트 크기 제한
            update = lr * m_hat / (np.sqrt(v_hat) + self.epsilon)
            update = np.clip(update, -0.01, 0.01)
            
            param -= update
            return param, m, v
        
        # 가중치 업데이트
        self.w3, self.m_w3, self.v_w3 = adam_update(self.w3, d_w3, self.m_w3, self.v_w3)
        self.w2, self.m_w2, self.v_w2 = adam_update(self.w2, d_w2, self.m_w2, self.v_w2)
        self.w1, self.m_w1, self.v_w1 = adam_update(self.w1, d_w1, self.m_w1, self.v_w1)
        
        self.b3, self.m_b3, self.v_b3 = adam_update(self.b3, d_b3, self.m_b3, self.v_b3)
        self.b2, self.m_b2, self.v_b2 = adam_update(self.b2, d_b2, self.m_b2, self.v_b2)
        self.b1, self.m_b1, self.v_b1 = adam_update(self.b1, d_b1, self.m_b1, self.v_b1)
        
        # 가중치 정규화
        self.normalize_weights()
        
        # 안정화된 loss 반환
        normalized_loss = mse / 1000000.0
        return min(normalized_loss, 10000.0)  # 상한선 설정
    
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
            f.write("// Auto-generated NNUE weights from Python training\n")
            f.write("// Compatible with OctaFlip client.c\n\n")
            
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
                        # 1D array
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
                    # Bias
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
            
            f.write("#define NNUE_SCALE 127\n\n")
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
        
        # Momentum도 복사
        new_nnue.m_w1 = self.m_w1.copy()
        new_nnue.m_w2 = self.m_w2.copy()
        new_nnue.m_w3 = self.m_w3.copy()
        new_nnue.m_b1 = self.m_b1.copy()
        new_nnue.m_b2 = self.m_b2.copy()
        new_nnue.m_b3 = self.m_b3.copy()
        
        new_nnue.v_w1 = self.v_w1.copy()
        new_nnue.v_w2 = self.v_w2.copy()
        new_nnue.v_w3 = self.v_w3.copy()
        new_nnue.v_b1 = self.v_b1.copy()
        new_nnue.v_b2 = self.v_b2.copy()
        new_nnue.v_b3 = self.v_b3.copy()
        
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
        
        for move in moves[:min(len(moves), 20)]:
            game_copy = game.copy()
            game_copy.make_move(move)
            
            try:
                score = self.nnue.forward(self.nnue.board_to_input(game_copy.board))[0]
                
                # NaN 체크
                if np.isnan(score):
                    score = 0
                    
                if game.current_player == 1:
                    score = -score
                
                # 즉시 캡처 보너스
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
                
                score += captures * 1000  # 캡처 보너스
                
                if score > best_score:
                    best_score = score
                    best_move = move
                    
            except Exception as e:
                print(f"Error in move evaluation: {e}")
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
    """병렬 게임 실행 워커"""
    try:
        nnue1_weights, nnue2_weights, game_id = args
        
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
            else:
                ai2 = RandomAI()
        else:
            ai2 = SimpleAI(nnue1)  # 기본값
        
        # AI 생성
        ai1 = SimpleAI(nnue1)
        
        # 게임 실행
        game = OctaFlipGame()
        positions = []
        
        move_count = 0
        while not game.is_game_over() and move_count < 100:
            positions.append({
                'board': [row[:] for row in game.board],
                'player': game.current_player,
                'hash': game.get_board_hash()
            })
            
            if game.current_player == 0:
                move = ai1.get_move(game)
            else:
                move = ai2.get_move(game)
            
            game.make_move(move)
            move_count += 1
        
        result_red, result_blue = game.get_result()
        
        # 학습 데이터 생성
        training_data = []
        for i, pos in enumerate(positions):
            if pos['player'] == 0:
                value = result_red
            else:
                value = result_blue
            
            # 시간 감쇠
            value *= (1.0 - i / (len(positions) * 2))
            
            # NNUE 스케일로 변환 (안정화)
            value = np.clip(value * 5000, -10000, 10000)
            
            training_data.append((pos, value))
        
        return training_data, game.move_history, result_red
        
    except Exception as e:
        print(f"Error in game worker: {e}")
        return [], [], 0.0


class FastTrainer:
    """빠른 학습 시스템 (안정화)"""
    
    def __init__(self, target_hours=10):
        self.target_duration = timedelta(hours=target_hours)
        self.start_time = datetime.now()
        
        # generation을 먼저 초기화
        self.generation = 0
        
        # NNUE 풀 
        self.population = []
        self.population_size = 8
        
        # 기존 가중치 로드 시도
        if os.path.exists('best_nnue.pkl.gz'):
            base_nnue = CompactNNUE('best_nnue.pkl.gz')
            # 변형 생성
            for i in range(self.population_size):
                nnue = base_nnue.copy()
                if i > 0:  # 첫 번째는 원본
                    self.mutate_nnue(nnue, strength=0.01)  # 더 작은 변형
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
        
        # 통계
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
            'loss_explosion_count': 0,
            'weight_resets': 0
        }
        
        # 기존 진행상황 로드
        self.load_progress()
        
    def mutate_nnue(self, nnue, strength=0.01):
        """NNUE 변형 (더 작은 노이즈)"""
        noise_scale = strength * (0.95 ** (self.generation / 100))
        
        # 가중치에만 노이즈 추가 (bias는 더 작게)
        nnue.w1 += np.random.randn(*nnue.w1.shape).astype(np.float32) * noise_scale
        nnue.w2 += np.random.randn(*nnue.w2.shape).astype(np.float32) * noise_scale
        nnue.w3 += np.random.randn(*nnue.w3.shape).astype(np.float32) * noise_scale
        
        nnue.b1 += np.random.randn(*nnue.b1.shape).astype(np.float32) * noise_scale * 0.1
        nnue.b2 += np.random.randn(*nnue.b2.shape).astype(np.float32) * noise_scale * 0.1
        nnue.b3 += np.random.randn(*nnue.b3.shape).astype(np.float32) * noise_scale * 0.1
        
        # 정규화
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
                    
                    # loss_history를 deque로 변환
                    if 'loss_history' in saved_stats:
                        self.stats['loss_history'] = deque(saved_stats['loss_history'], maxlen=100)
                            
                print(f"Resumed from generation {self.generation}")
                print(f"Previous games: {self.stats['games_played']}")
                
                # Loss 폭발 경고
                if self.stats.get('loss_explosion_count', 0) > 0:
                    print(f"WARNING: Previous loss explosions: {self.stats['loss_explosion_count']}")
                    
            except Exception as e:
                print(f"Error loading progress: {e}")
                
    def should_continue(self):
        """시간 체크"""
        elapsed = datetime.now() - self.start_time
        return elapsed < self.target_duration
        
    def check_loss_stability(self):
        """Loss 안정성 체크 및 복구"""
        if len(self.stats['loss_history']) < 5:
            return True
            
        recent_losses = list(self.stats['loss_history'])[-5:]
        avg_loss = sum(recent_losses) / len(recent_losses)
        
        # Loss 폭발 감지
        if avg_loss > 100000:
            self.stats['loss_explosion_count'] += 1
            print(f"\n⚠️  WARNING: Loss explosion detected! Average loss: {avg_loss:.0f}")
            
            # 모든 네트워크의 백업 복원
            for nnue in self.population:
                if nnue.backup_weights and nnue.backup_loss < avg_loss:
                    nnue.restore_backup()
            
            # 학습률 급격히 감소
            for nnue in self.population:
                nnue.learning_rate *= 0.1
                
            print(f"Applied emergency measures: restored backups, reduced learning rate")
            return False
            
        # Loss가 계속 증가하는지 체크
        if len(recent_losses) >= 5:
            increasing_count = sum(1 for i in range(1, 5) if recent_losses[i] > recent_losses[i-1] * 1.5)
            if increasing_count >= 3:
                print(f"\n⚠️  WARNING: Loss is continuously increasing!")
                # 학습률 감소
                for nnue in self.population:
                    nnue.learning_rate *= 0.5
                return False
                
        return True
        
    def run_generation(self):
        """한 세대 실행 (다양한 상대)"""
        gen_start = datetime.now()
        
        print(f"\nGeneration {self.generation} | Total: {self.stats['games_played']} games")
        
        # Loss 안정성 체크
        if not self.check_loss_stability():
            # 버퍼 일부 제거 (오래된 데이터)
            if len(self.training_buffer) > BATCH_SIZE * 10:
                for _ in range(BATCH_SIZE * 5):
                    self.training_buffer.popleft()
            print("Cleared old training data")
        
        # 1. Self-play 토너먼트
        scores = [0] * self.population_size
        games_data = []
        
        # 병렬 게임 실행
        with mp.Pool(WORKERS) as pool:
            tasks = []
            
            # Self-play games (60%)
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
                    if len(tasks) < 50:  # 너무 많지 않게
                        tasks.append((j, i, (w2, w1, f"{self.generation}_{j}_{i}_2")))
            
            # 2. 다양한 상대와 게임 (40%)
            # 상위 4개 NNUE vs 다른 AI
            for i in range(min(4, self.population_size)):
                w1 = {
                    'w1': self.population[i].w1.tolist(),
                    'b1': self.population[i].b1.tolist(),
                    'w2': self.population[i].w2.tolist(),
                    'b2': self.population[i].b2.tolist(),
                    'w3': self.population[i].w3.tolist(),
                    'b3': self.population[i].b3.tolist()
                }
                
                # vs Random
                for _ in range(2):
                    tasks.append((i, -1, (w1, 'random', f"{self.generation}_{i}_random")))
                
                # vs Greedy  
                for _ in range(2):
                    tasks.append((i, -2, (w1, 'greedy', f"{self.generation}_{i}_greedy")))
                
                # vs Positional
                tasks.append((i, -3, (w1, 'positional', f"{self.generation}_{i}_positional")))
            
            # 실행
            results = pool.map(play_game_worker, [task[2] for task in tasks])
            
            # 결과 처리
            for idx, (player1, player2, _) in enumerate(tasks):
                training_data, moves, result = results[idx]
                
                if training_data:  # 에러 체크
                    games_data.extend(training_data)
                    
                    # 오프닝북에 추가
                    if abs(result) > 0.5:
                        self.opening_book.add_game(moves, result)
                    
                    # 점수 계산 (self-play만)
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
        
        # Hall of Fame 업데이트 (10세대마다)
        if self.generation % 10 == 0:
            best_idx = ranked[0][1]
            self.hall_of_fame.append(self.population[best_idx].copy())
        
        self.generation += 1
        
        # 통계
        gen_time = (datetime.now() - gen_start).total_seconds()
        self.stats['training_time_ratio'] = train_time / gen_time if gen_time > 0 else 0
        
        print(f"Generation time: {gen_time:.1f}s (training: {train_time:.1f}s, {self.stats['training_time_ratio']:.1%})")
        
        # Loss 통계
        if self.stats['loss_history']:
            avg_loss = sum(self.stats['loss_history']) / len(self.stats['loss_history'])
            print(f"Average loss: {avg_loss:.2f}")
            
            # Loss 경고
            if avg_loss > 10000:
                print(f"⚠️  High average loss detected!")
        
    def train_networks(self, indices):
        """네트워크 학습 (안정화)"""
        if len(self.training_buffer) < BATCH_SIZE:
            return
        
        # 학습률 스케줄링 (더 보수적으로)
        base_lr = 0.00001
        lr_scale = 0.95 ** (self.generation / 100)
        
        # Loss가 높으면 학습률 더 낮추기
        if self.stats['loss_history'] and len(self.stats['loss_history']) > 0:
            recent_avg = sum(list(self.stats['loss_history'])[-10:]) / min(10, len(self.stats['loss_history']))
            if recent_avg > 10000:
                lr_scale *= 0.1
                print(f"Reducing learning rate due to high loss (avg: {recent_avg:.0f})")
        
        epochs = 1  # 항상 1 epoch
        
        for epoch in range(epochs):
            buffer_list = list(self.training_buffer)
            random.shuffle(buffer_list)
            
            for idx in indices:
                nnue = self.population[idx]
                
                total_loss = 0
                batches = 0
                
                # 배치 크기 조정
                batch_size = min(BATCH_SIZE, 512) if self.generation < 10 else BATCH_SIZE
                
                for i in range(0, len(buffer_list) - batch_size, batch_size):
                    batch = buffer_list[i:i + batch_size]
                    
                    # 배치 데이터 준비
                    X = np.zeros((batch_size, INPUT_SIZE), dtype=np.float32)
                    y = np.zeros(batch_size, dtype=np.float32)
                    
                    for j, (pos, value) in enumerate(batch):
                        X[j] = nnue.board_to_input(pos['board'])
                        y[j] = value
                    
                    # 타겟 값 정규화 (outlier 제거)
                    y = np.clip(y, -10000, 10000)
                    
                    # 학습
                    try:
                        y_pred = nnue.forward(X)
                        loss = nnue.backward_adam(X, y, y_pred, lr_scale)
                        
                        # NaN/Inf 체크
                        if np.isnan(loss) or np.isinf(loss) or loss > 100000:
                            print(f"  Network {idx}: Invalid loss {loss}, skipping batch")
                            continue
                            
                        total_loss += loss
                        batches += 1
                        
                        # 백업 저장 (좋은 loss일 때)
                        if loss < 1000:
                            nnue.save_backup(loss)
                            
                    except Exception as e:
                        print(f"  Network {idx}: Training error: {e}")
                        continue
                    
                    self.stats['positions_trained'] += batch_size
                
                if batches > 0:
                    avg_batch_loss = total_loss / batches
                    self.stats['loss_history'].append(avg_batch_loss)
                    print(f"  Network {idx}: Loss = {avg_batch_loss:.4f}")
                else:
                    print(f"  Network {idx}: No valid batches")
    
    def create_new_generation(self, rankings):
        """새 세대 생성"""
        new_population = []
        
        # 상위 25% 유지
        elite_count = max(2, self.population_size // 4)
        for i in range(elite_count):
            _, idx = rankings[i]
            new_population.append(self.population[idx].copy())
        
        # 나머지는 변형
        while len(new_population) < self.population_size:
            # 상위 50%에서 부모 선택
            parent_idx = random.choice([idx for _, idx in rankings[:self.population_size//2]])
            child = self.population[parent_idx].copy()
            
            # 변형 (점진적으로 감소)
            noise_strength = 0.01 * (1.0 + random.random() * 0.5)
            self.mutate_nnue(child, strength=noise_strength)
            
            new_population.append(child)
        
        self.population = new_population
    
    def test_best_network(self):
        """최고 네트워크 테스트"""
        print("\nTesting best network...")
        
        best_nnue = self.population[0]
        ai = SimpleAI(best_nnue)
        
        test_results = {
            'random': 0,
            'greedy': 0,
            'positional': 0
        }
        
        # vs Random
        print(f"  Playing {QUICK_TEST_GAMES} games vs Random...", end='', flush=True)
        for i in range(QUICK_TEST_GAMES):
            game = OctaFlipGame()
            opponent = RandomAI()
            
            color = i % 2  # 색 번갈아가며
            
            while not game.is_game_over():
                if game.current_player == color:
                    move = ai.get_move(game)
                else:
                    move = opponent.get_move(game)
                
                game.make_move(move)
            
            result_red, result_blue = game.get_result()
            if (color == 0 and result_red > 0) or (color == 1 and result_blue > 0):
                test_results['random'] += 1
        
        print(f" {test_results['random']}/{QUICK_TEST_GAMES} wins")
        
        # vs Greedy
        print(f"  Playing {QUICK_TEST_GAMES} games vs Greedy...", end='', flush=True)
        for i in range(QUICK_TEST_GAMES):
            game = OctaFlipGame()
            opponent = GreedyAI()
            
            color = i % 2
            
            while not game.is_game_over():
                if game.current_player == color:
                    move = ai.get_move(game)
                else:
                    move = opponent.get_move(game)
                
                game.make_move(move)
            
            result_red, result_blue = game.get_result()
            if (color == 0 and result_red > 0) or (color == 1 and result_blue > 0):
                test_results['greedy'] += 1
        
        print(f" {test_results['greedy']}/{QUICK_TEST_GAMES} wins")
        
        # vs Positional
        print(f"  Playing {QUICK_TEST_GAMES//2} games vs Positional...", end='', flush=True)
        for i in range(QUICK_TEST_GAMES//2):
            game = OctaFlipGame()
            opponent = PositionalAI()
            
            color = i % 2
            
            while not game.is_game_over():
                if game.current_player == color:
                    move = ai.get_move(game)
                else:
                    move = opponent.get_move(game)
                
                game.make_move(move)
            
            result_red, result_blue = game.get_result()
            if (color == 0 and result_red > 0) or (color == 1 and result_blue > 0):
                test_results['positional'] += 1
        
        print(f" {test_results['positional']}/{QUICK_TEST_GAMES//2} wins")
        
        # 통계 업데이트
        self.stats['best_vs_random'] = test_results['random'] * 100 / QUICK_TEST_GAMES
        self.stats['best_vs_greedy'] = test_results['greedy'] * 100 / QUICK_TEST_GAMES
        self.stats['best_vs_positional'] = test_results['positional'] * 100 / (QUICK_TEST_GAMES//2)
        
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
        
        # 오프닝북 압축 및 저장
        compressed_book = self.opening_book.compress()
        with gzip.open('opening_book.pkl.gz', 'wb') as f:
            pickle.dump(compressed_book, f)
        
        # 통계 저장
        self.stats['generation'] = self.generation
        if isinstance(self.stats['last_test_time'], datetime):
            self.stats['last_test_time'] = self.stats['last_test_time'].isoformat()
        
        # deque를 list로 변환
        stats_to_save = self.stats.copy()
        if 'loss_history' in stats_to_save:
            stats_to_save['loss_history'] = list(stats_to_save['loss_history'])
            
        with open('training_stats.json', 'w') as f:
            json.dump(stats_to_save, f, indent=2)
        
        print(f"Progress saved (Gen {self.generation})")
        
    def train(self):
        """메인 학습 루프"""
        print(f"Fast training for {self.target_duration.total_seconds()/3600:.1f} hours...")
        print(f"Population size: {self.population_size}")
        print(f"Workers: {WORKERS}")
        print(f"⚠️  Stabilized version with loss explosion prevention")
        
        last_save_gen = self.generation
        
        try:
            while self.should_continue():
                self.run_generation()
                
                # 저장
                if self.generation - last_save_gen >= SAVE_INTERVAL_GENERATIONS:
                    self.save_progress()
                    last_save_gen = self.generation
                    
                    # 테스트
                    last_test = datetime.fromisoformat(self.stats['last_test_time']) if isinstance(self.stats['last_test_time'], str) else self.stats['last_test_time']
                    if (datetime.now() - last_test).total_seconds() > TEST_INTERVAL_MINUTES * 60:
                        self.test_best_network()
                        self.stats['last_test_time'] = datetime.now()
                
                # 진행상황
                elapsed = datetime.now() - self.start_time
                progress = elapsed.total_seconds() / self.target_duration.total_seconds() * 100
                
                if self.generation % 5 == 0:
                    remaining = self.target_duration - elapsed
                    print(f"\nProgress: {progress:.1f}% | Games: {self.stats['games_played']:,} | "
                          f"Positions: {self.stats['positions_trained']:,}")
                    print(f"Time remaining: {remaining}")
                    
                    # Loss 상태 출력
                    if self.stats['loss_history']:
                        recent_losses = list(self.stats['loss_history'])[-10:]
                        avg_recent = sum(recent_losses) / len(recent_losses)
                        print(f"Recent avg loss: {avg_recent:.2f}")
                
        except KeyboardInterrupt:
            print("\nTraining interrupted by user")
        
        # 최종 저장
        self.save_progress()
        self.test_best_network()
        print("\nTraining completed!")
        
        return self.population[0], self.opening_book.compress()


def main():
    print("=== OctaFlip NNUE Training System v2.2 (Stabilized) ===\n")
    
    print("Training options:")
    print("1. Quick training (1 hour)")
    print("2. Standard training (10 hours)")
    print("3. Extended training (24 hours)")
    print("4. Custom duration")
    print("5. Fix loss explosion (reset and retrain)")
    
    choice = input("\nSelect option (1-5): ")
    
    if choice == '1':
        hours = 1
    elif choice == '2':
        hours = 10
    elif choice == '3':
        hours = 24
    elif choice == '4':
        hours = float(input("Enter hours: "))
    elif choice == '5':
        # Loss 폭발 수정 모드
        print("\nFixing loss explosion...")
        
        # 통계 파일 백업
        if os.path.exists('training_stats.json'):
            os.rename('training_stats.json', 'training_stats_backup.json')
            
        # 새로운 NNUE로 시작
        print("Creating fresh NNUE weights...")
        nnue = CompactNNUE()
        weights = {
            'w1': nnue.w1.tolist(),
            'b1': nnue.b1.tolist(),
            'w2': nnue.w2.tolist(),
            'b2': nnue.b2.tolist(),
            'w3': nnue.w3.tolist(),
            'b3': nnue.b3.tolist()
        }
        
        with gzip.open('best_nnue.pkl.gz', 'wb') as f:
            pickle.dump(weights, f)
            
        print("Reset complete. Starting fresh training...")
        hours = 10
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
    print(f"   - Loss explosions: {trainer.stats.get('loss_explosion_count', 0)}")
    
    if trainer.stats['loss_history']:
        avg_loss = sum(trainer.stats['loss_history']) / len(trainer.stats['loss_history'])
        print(f"   - Average loss: {avg_loss:.2f}")
    
    print("\n💾 Files generated:")
    print("   - best_nnue.pkl.gz (Python weights)")
    print("   - nnue_weights_generated.h (C header)")
    print("   - opening_book.pkl.gz (Opening book)")
    print("   - training_stats.json (Statistics)")
    
    print("\n✨ Ready for integration with client.c!")


if __name__ == "__main__":
    main()