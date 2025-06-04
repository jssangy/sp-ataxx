#!/usr/bin/env python3
"""
OctaFlip NNUE 딥러닝 시스템 v7.0 - 안정화 버전
- Leaky ReLU로 죽은 뉴런 방지
- 개선된 초기화
- 벤치마크 AI 간소화 (4개만)
- 출력 간소화
"""

import numpy as np
import random
import time
import multiprocessing as mp
from collections import deque
import json
import pickle
import gzip
import os
import traceback
from datetime import datetime, timedelta
import warnings
warnings.filterwarnings('ignore')

# 게임 상수
BOARD_SIZE = 8
RED = 'R'
BLUE = 'B' 
EMPTY = '.'

# 학습 설정
WORKERS = min(mp.cpu_count() - 1, 8)
BATCH_SIZE = 1024
GAMES_PER_GENERATION = 200  # 줄임
TARGET_DURATION_HOURS = 30

# NNUE 구조 (client.c와 동일)
INPUT_SIZE = 192
HIDDEN1_SIZE = 128
HIDDEN2_SIZE = 64
HIDDEN3_SIZE = 32
OUTPUT_SIZE = 1

# 테스트 설정
TEST_INTERVAL_MINUTES = 30
QUICK_TEST_GAMES = 2  # 각 AI당 2게임만
SAVE_INTERVAL_GENERATIONS = 5

# 학습 데이터 설정
MAX_BUFFER_SIZE = 300000  # 줄임
HALL_OF_FAME_SIZE = 10

# 출력 스케일
OUTPUT_SCALE = 200
QUANTIZATION_SCALE = 127


class DeepNNUE:
    """개선된 NNUE with Leaky ReLU"""
    
    def __init__(self, init_from_file=None):
        if init_from_file and os.path.exists(init_from_file):
            self.load_weights(init_from_file)
        else:
            # 더 큰 초기화
            self.w1 = self._init_weights((INPUT_SIZE, HIDDEN1_SIZE), 'leaky_relu')
            self.b1 = np.zeros(HIDDEN1_SIZE, dtype=np.float32) * 0.01
            
            self.w2 = self._init_weights((HIDDEN1_SIZE, HIDDEN2_SIZE), 'leaky_relu')
            self.b2 = np.zeros(HIDDEN2_SIZE, dtype=np.float32) * 0.01
            
            self.w3 = self._init_weights((HIDDEN2_SIZE, HIDDEN3_SIZE), 'leaky_relu')
            self.b3 = np.zeros(HIDDEN3_SIZE, dtype=np.float32) * 0.01
            
            # 출력층은 더 큰 초기화
            self.w4 = self._init_weights((HIDDEN3_SIZE, OUTPUT_SIZE), 'linear') * 2.0
            self.b4 = np.zeros(OUTPUT_SIZE, dtype=np.float32)
        
        # 학습 파라미터
        self.learning_rate = 0.005  # 증가
        self.beta1 = 0.9
        self.beta2 = 0.999
        self.epsilon = 1e-8
        self.t = 0
        
        # Learning rate scheduling
        self.lr_decay_rate = 0.995
        self.min_lr = 0.0005
        self.warmup_steps = 200
        
        # Leaky ReLU slope
        self.leaky_slope = 0.01
        
        # Batch normalization parameters
        self.bn_gamma1 = np.ones(HIDDEN1_SIZE, dtype=np.float32)
        self.bn_beta1 = np.zeros(HIDDEN1_SIZE, dtype=np.float32)
        self.bn_gamma2 = np.ones(HIDDEN2_SIZE, dtype=np.float32)
        self.bn_beta2 = np.zeros(HIDDEN2_SIZE, dtype=np.float32)
        self.bn_gamma3 = np.ones(HIDDEN3_SIZE, dtype=np.float32)
        self.bn_beta3 = np.zeros(HIDDEN3_SIZE, dtype=np.float32)
        
        # Running statistics for BN
        self.bn_mean1 = np.zeros(HIDDEN1_SIZE, dtype=np.float32)
        self.bn_var1 = np.ones(HIDDEN1_SIZE, dtype=np.float32)
        self.bn_mean2 = np.zeros(HIDDEN2_SIZE, dtype=np.float32)
        self.bn_var2 = np.ones(HIDDEN2_SIZE, dtype=np.float32)
        self.bn_mean3 = np.zeros(HIDDEN3_SIZE, dtype=np.float32)
        self.bn_var3 = np.ones(HIDDEN3_SIZE, dtype=np.float32)
        self.bn_momentum = 0.9

        # Adam momentum 초기화
        self._init_adam_momentum()
        
        # 학습 추적
        self.loss_history = deque(maxlen=100)
        self.best_loss = float('inf')
        self.patience_counter = 0
        self.gradient_norms = deque(maxlen=50)
        self.last_update_ratio = 0.01
        
    def _init_weights(self, shape, activation='leaky_relu'):
        """He initialization for Leaky ReLU"""
        fan_in = shape[0]
        
        if activation == 'leaky_relu':
            # He initialization for Leaky ReLU
            std = np.sqrt(2.0 / fan_in) * 1.2
        elif activation == 'linear':
            # Xavier initialization for linear
            std = np.sqrt(2.0 / (fan_in + shape[1]))
        else:
            std = 0.1
            
        weights = np.random.randn(*shape).astype(np.float32) * std
        return weights
    
    def _init_adam_momentum(self):
        """Adam optimizer states"""
        # First moment
        self.m_w1 = np.zeros_like(self.w1)
        self.m_w2 = np.zeros_like(self.w2)
        self.m_w3 = np.zeros_like(self.w3)
        self.m_w4 = np.zeros_like(self.w4)
        self.m_b1 = np.zeros_like(self.b1)
        self.m_b2 = np.zeros_like(self.b2)
        self.m_b3 = np.zeros_like(self.b3)
        self.m_b4 = np.zeros_like(self.b4)
        
        # Second moment
        self.v_w1 = np.zeros_like(self.w1)
        self.v_w2 = np.zeros_like(self.w2)
        self.v_w3 = np.zeros_like(self.w3)
        self.v_w4 = np.zeros_like(self.w4)
        self.v_b1 = np.zeros_like(self.b1)
        self.v_b2 = np.zeros_like(self.b2)
        self.v_b3 = np.zeros_like(self.b3)
        self.v_b4 = np.zeros_like(self.b4)
        
        # BN parameters
        self.m_bn_gamma1 = np.zeros_like(self.bn_gamma1)
        self.m_bn_beta1 = np.zeros_like(self.bn_beta1)
        self.v_bn_gamma1 = np.zeros_like(self.bn_gamma1)
        self.v_bn_beta1 = np.zeros_like(self.bn_beta1)
        
        self.m_bn_gamma2 = np.zeros_like(self.bn_gamma2)
        self.m_bn_beta2 = np.zeros_like(self.bn_beta2)
        self.v_bn_gamma2 = np.zeros_like(self.bn_gamma2)
        self.v_bn_beta2 = np.zeros_like(self.bn_beta2)
        
        self.m_bn_gamma3 = np.zeros_like(self.bn_gamma3)
        self.m_bn_beta3 = np.zeros_like(self.bn_beta3)
        self.v_bn_gamma3 = np.zeros_like(self.bn_gamma3)
        self.v_bn_beta3 = np.zeros_like(self.bn_beta3)
    
    def leaky_relu(self, x):
        """Leaky ReLU activation"""
        return np.where(x > 0, x, self.leaky_slope * x)
    
    def leaky_relu_derivative(self, x):
        """Leaky ReLU derivative"""
        return np.where(x > 0, 1, self.leaky_slope)
        
    def batch_norm(self, x, gamma, beta, running_mean, running_var, training=True):
        """Batch Normalization"""
        if training:
            mean = np.mean(x, axis=0)
            var = np.var(x, axis=0) + self.epsilon
            
            running_mean = self.bn_momentum * running_mean + (1 - self.bn_momentum) * mean
            running_var = self.bn_momentum * running_var + (1 - self.bn_momentum) * var
            
            x_norm = (x - mean) / np.sqrt(var + self.epsilon)
        else:
            x_norm = (x - running_mean) / np.sqrt(running_var + self.epsilon)
            
        return gamma * x_norm + beta, running_mean, running_var
    
    def forward(self, x, training=True):
        """순전파 with Leaky ReLU"""
        if not isinstance(x, np.ndarray):
            x = np.array(x, dtype=np.float32)
        
        if len(x.shape) == 1:
            x = x.reshape(1, -1)
        
        if x.dtype != np.float32:
            x = x.astype(np.float32)
            
        self.x = x
        batch_size = x.shape[0]
        
        # Layer 1 with BN
        self.z1 = np.dot(x, self.w1) + self.b1
        self.z1_bn, self.bn_mean1, self.bn_var1 = self.batch_norm(
            self.z1, self.bn_gamma1, self.bn_beta1, 
            self.bn_mean1, self.bn_var1, training
        )
        self.a1 = self.leaky_relu(self.z1_bn)
        
        # Dropout (줄임)
        if training:
            self.dropout1 = np.random.binomial(1, 0.9, size=self.a1.shape).astype(np.float32)
            self.a1 = self.a1 * self.dropout1 / 0.9
        
        # Layer 2 with BN
        self.z2 = np.dot(self.a1, self.w2) + self.b2
        self.z2_bn, self.bn_mean2, self.bn_var2 = self.batch_norm(
            self.z2, self.bn_gamma2, self.bn_beta2,
            self.bn_mean2, self.bn_var2, training
        )
        self.a2 = self.leaky_relu(self.z2_bn)
        
        # Dropout
        if training:
            self.dropout2 = np.random.binomial(1, 0.85, size=self.a2.shape).astype(np.float32)
            self.a2 = self.a2 * self.dropout2 / 0.85
        
        # Layer 3 with BN
        self.z3 = np.dot(self.a2, self.w3) + self.b3
        self.z3_bn, self.bn_mean3, self.bn_var3 = self.batch_norm(
            self.z3, self.bn_gamma3, self.bn_beta3,
            self.bn_mean3, self.bn_var3, training
        )
        self.a3 = self.leaky_relu(self.z3_bn)
        
        # Output layer
        self.z4 = np.dot(self.a3, self.w4) + self.b4
        self.z4 = np.tanh(self.z4 / 2) * 2  # Bounded output
        
        # Output
        output = self.z4 * OUTPUT_SCALE
        
        return output.flatten()
    
    def backward_adam(self, x, y_true, y_pred):
        """역전파 with Leaky ReLU"""
        self.t += 1
        batch_size = x.shape[0] if len(x.shape) > 1 else 1
        
        if not isinstance(y_pred, np.ndarray):
            y_pred = np.array(y_pred, dtype=np.float32)
        if not isinstance(y_true, np.ndarray):
            y_true = np.array(y_true, dtype=np.float32)
            
        y_pred = y_pred.flatten()
        y_true = y_true.flatten()
        
        # MSE loss gradient
        errors = (y_pred - y_true) / OUTPUT_SCALE
        errors = np.clip(errors, -1.0, 1.0)
        
        d_loss = errors / batch_size
        
        # Loss calculation
        loss = np.mean((y_pred - y_true) ** 2) / (OUTPUT_SCALE ** 2)
        
        # Skip if loss is too large
        if loss > 100 or np.isnan(loss) or np.isinf(loss):
            return self.best_loss if hasattr(self, 'best_loss') else 10.0
            
        self.loss_history.append(loss)
        
        # Output layer gradients (tanh derivative)
        d_tanh = 1 - (self.z4 ** 2) / 4  # tanh derivative
        d_z4 = d_loss.reshape(-1, 1) * d_tanh
        
        # Gradient clipping
        max_grad = 2.0  # 증가
        d_z4 = np.clip(d_z4, -max_grad, max_grad)
        
        d_w4 = np.dot(self.a3.T, d_z4) / batch_size
        d_b4 = np.mean(d_z4, axis=0)
        
        # Layer 3 gradients
        d_a3 = np.dot(d_z4, self.w4.T)
        d_z3_bn = d_a3 * self.leaky_relu_derivative(self.z3_bn)
        
        # BN backward
        d_gamma3 = np.sum(d_z3_bn * (self.z3 - self.bn_mean3) / 
                         np.sqrt(self.bn_var3 + self.epsilon), axis=0)
        d_beta3 = np.sum(d_z3_bn, axis=0)
        d_z3 = d_z3_bn * self.bn_gamma3 / np.sqrt(self.bn_var3 + self.epsilon)
        
        d_z3 = np.clip(d_z3, -max_grad, max_grad)
        d_w3 = np.dot(self.a2.T, d_z3) / batch_size
        d_b3 = np.mean(d_z3, axis=0)
        
        # Layer 2 gradients
        d_a2 = np.dot(d_z3, self.w3.T)
        
        if hasattr(self, 'dropout2'):
            d_a2 = d_a2 * self.dropout2 / 0.85
            
        d_z2_bn = d_a2 * self.leaky_relu_derivative(self.z2_bn)
            
        # BN backward
        d_gamma2 = np.sum(d_z2_bn * (self.z2 - self.bn_mean2) / 
                         np.sqrt(self.bn_var2 + self.epsilon), axis=0)
        d_beta2 = np.sum(d_z2_bn, axis=0)
        d_z2 = d_z2_bn * self.bn_gamma2 / np.sqrt(self.bn_var2 + self.epsilon)
        
        d_z2 = np.clip(d_z2, -max_grad, max_grad)
        d_w2 = np.dot(self.a1.T, d_z2) / batch_size
        d_b2 = np.mean(d_z2, axis=0)
        
        # Layer 1 gradients
        d_a1 = np.dot(d_z2, self.w2.T)
        
        if hasattr(self, 'dropout1'):
            d_a1 = d_a1 * self.dropout1 / 0.9
            
        d_z1_bn = d_a1 * self.leaky_relu_derivative(self.z1_bn)
            
        # BN backward
        d_gamma1 = np.sum(d_z1_bn * (self.z1 - self.bn_mean1) / 
                         np.sqrt(self.bn_var1 + self.epsilon), axis=0)
        d_beta1 = np.sum(d_z1_bn, axis=0)
        d_z1 = d_z1_bn * self.bn_gamma1 / np.sqrt(self.bn_var1 + self.epsilon)
        
        d_z1 = np.clip(d_z1, -max_grad, max_grad)
        d_w1 = np.dot(x.T, d_z1) / batch_size
        d_b1 = np.mean(d_z1, axis=0)
        
        # L2 regularization
        weight_decay = 0.00001
        d_w1 += weight_decay * self.w1
        d_w2 += weight_decay * self.w2
        d_w3 += weight_decay * self.w3
        d_w4 += weight_decay * self.w4
        
        # Learning rate with warmup
        if self.t < self.warmup_steps:
            lr = self.learning_rate * (self.t / self.warmup_steps)
        else:
            # Generation 기반 decay
            decay_factor = self.lr_decay_rate ** (self.generation / 100) if hasattr(self, 'generation') else 1.0
            lr = self.learning_rate * decay_factor
            lr = max(lr, self.min_lr)
        
        # Gradient norm
        grad_norm = np.sqrt(
            np.sum(d_w1**2) + np.sum(d_w2**2) + np.sum(d_w3**2) + np.sum(d_w4**2) +
            np.sum(d_b1**2) + np.sum(d_b2**2) + np.sum(d_b3**2) + np.sum(d_b4**2)
        )
        self.gradient_norms.append(grad_norm)
        
        # Adam update
        def adam_update(param, grad, m, v):
            m = self.beta1 * m + (1 - self.beta1) * grad
            v = self.beta2 * v + (1 - self.beta2) * (grad ** 2)
            
            m_hat = m / (1 - self.beta1 ** self.t)
            v_hat = v / (1 - self.beta2 ** self.t)
            
            update = lr * m_hat / (np.sqrt(v_hat) + self.epsilon)
            
            # Update clipping
            param_norm = np.linalg.norm(param)
            update_norm = np.linalg.norm(update)
            if update_norm > 0 and update_norm > 0.1 * param_norm:
                update = update * (0.1 * param_norm / update_norm)
            
            # Update ratio 추적
            if param.shape == self.w4.shape:
                self.last_update_ratio = update_norm / (param_norm + 1e-8)
            
            new_param = param - update
            
            return new_param, m, v
        
        # Update weights
        self.w4, self.m_w4, self.v_w4 = adam_update(self.w4, d_w4, self.m_w4, self.v_w4)
        self.w3, self.m_w3, self.v_w3 = adam_update(self.w3, d_w3, self.m_w3, self.v_w3)
        self.w2, self.m_w2, self.v_w2 = adam_update(self.w2, d_w2, self.m_w2, self.v_w2)
        self.w1, self.m_w1, self.v_w1 = adam_update(self.w1, d_w1, self.m_w1, self.v_w1)
        
        self.b4, self.m_b4, self.v_b4 = adam_update(self.b4, d_b4, self.m_b4, self.v_b4)
        self.b3, self.m_b3, self.v_b3 = adam_update(self.b3, d_b3, self.m_b3, self.v_b3)
        self.b2, self.m_b2, self.v_b2 = adam_update(self.b2, d_b2, self.m_b2, self.v_b2)
        self.b1, self.m_b1, self.v_b1 = adam_update(self.b1, d_b1, self.m_b1, self.v_b1)
        
        # Update BN parameters
        self.bn_gamma3, self.m_bn_gamma3, self.v_bn_gamma3 = adam_update(
            self.bn_gamma3, d_gamma3, self.m_bn_gamma3, self.v_bn_gamma3)
        self.bn_beta3, self.m_bn_beta3, self.v_bn_beta3 = adam_update(
            self.bn_beta3, d_beta3, self.m_bn_beta3, self.v_bn_beta3)
            
        self.bn_gamma2, self.m_bn_gamma2, self.v_bn_gamma2 = adam_update(
            self.bn_gamma2, d_gamma2, self.m_bn_gamma2, self.v_bn_gamma2)
        self.bn_beta2, self.m_bn_beta2, self.v_bn_beta2 = adam_update(
            self.bn_beta2, d_beta2, self.m_bn_beta2, self.v_bn_beta2)
            
        self.bn_gamma1, self.m_bn_gamma1, self.v_bn_gamma1 = adam_update(
            self.bn_gamma1, d_gamma1, self.m_bn_gamma1, self.v_bn_gamma1)
        self.bn_beta1, self.m_bn_beta1, self.v_bn_beta1 = adam_update(
            self.bn_beta1, d_beta1, self.m_bn_beta1, self.v_bn_beta1)
        
        # Weight normalization
        self.normalize_weights()
        
        return loss
    
    def normalize_weights(self):
        """가중치 정규화"""
        # Weight clipping
        for i, (w, name) in enumerate([(self.w1, 'w1'), (self.w2, 'w2'), (self.w3, 'w3')]):
            w_norm = np.linalg.norm(w)
            max_norm = 15.0
            if w_norm > max_norm:
                w *= max_norm / w_norm
            np.clip(w, -5.0, 5.0, out=w)
        
        # 출력층은 더 넓은 범위 허용
        w4_norm = np.linalg.norm(self.w4)
        if w4_norm < 0.5:
            self.w4 *= 2.0
        if w4_norm > 20.0:
            self.w4 *= 20.0 / w4_norm
        np.clip(self.w4, -10.0, 10.0, out=self.w4)
        
        # Bias clipping
        for b in [self.b1, self.b2, self.b3, self.b4]:
            np.clip(b, -2.0, 2.0, out=b)
        
        # BN parameters clipping
        for gamma in [self.bn_gamma1, self.bn_gamma2, self.bn_gamma3]:
            np.clip(gamma, 0.1, 5.0, out=gamma)  
        
        for beta in [self.bn_beta1, self.bn_beta2, self.bn_beta3]:
            np.clip(beta, -2.0, 2.0, out=beta)    

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
    
    def save_weights(self, filename):
        """가중치 저장"""
        weights = {
            'w1': self.w1, 'b1': self.b1,
            'w2': self.w2, 'b2': self.b2,
            'w3': self.w3, 'b3': self.b3,
            'w4': self.w4, 'b4': self.b4,
            'bn_gamma1': self.bn_gamma1, 'bn_beta1': self.bn_beta1,
            'bn_gamma2': self.bn_gamma2, 'bn_beta2': self.bn_beta2,
            'bn_gamma3': self.bn_gamma3, 'bn_beta3': self.bn_beta3,
            'bn_mean1': self.bn_mean1, 'bn_var1': self.bn_var1,
            'bn_mean2': self.bn_mean2, 'bn_var2': self.bn_var2,
            'bn_mean3': self.bn_mean3, 'bn_var3': self.bn_var3,
        }
        
        with gzip.open(filename, 'wb') as f:
            pickle.dump(weights, f)
    
    def load_weights(self, filename):
        """가중치 로드"""
        try:
            with gzip.open(filename, 'rb') as f:
                weights = pickle.load(f)
            
            if 'w4' in weights:
                self.w1 = np.array(weights['w1'], dtype=np.float32)
                self.b1 = np.array(weights['b1'], dtype=np.float32)
                self.w2 = np.array(weights['w2'], dtype=np.float32)
                self.b2 = np.array(weights['b2'], dtype=np.float32)
                self.w3 = np.array(weights['w3'], dtype=np.float32)
                self.b3 = np.array(weights['b3'], dtype=np.float32)
                self.w4 = np.array(weights['w4'], dtype=np.float32)
                self.b4 = np.array(weights['b4'], dtype=np.float32)
                
                # BN parameters
                if 'bn_gamma1' in weights:
                    self.bn_gamma1 = np.array(weights['bn_gamma1'], dtype=np.float32)
                    self.bn_beta1 = np.array(weights['bn_beta1'], dtype=np.float32)
                    self.bn_gamma2 = np.array(weights['bn_gamma2'], dtype=np.float32)
                    self.bn_beta2 = np.array(weights['bn_beta2'], dtype=np.float32)
                    self.bn_gamma3 = np.array(weights['bn_gamma3'], dtype=np.float32)
                    self.bn_beta3 = np.array(weights['bn_beta3'], dtype=np.float32)
                    
                    self.bn_mean1 = np.array(weights.get('bn_mean1', np.zeros(HIDDEN1_SIZE)), dtype=np.float32)
                    self.bn_var1 = np.array(weights.get('bn_var1', np.ones(HIDDEN1_SIZE)), dtype=np.float32)
                    self.bn_mean2 = np.array(weights.get('bn_mean2', np.zeros(HIDDEN2_SIZE)), dtype=np.float32)
                    self.bn_var2 = np.array(weights.get('bn_var2', np.ones(HIDDEN2_SIZE)), dtype=np.float32)
                    self.bn_mean3 = np.array(weights.get('bn_mean3', np.zeros(HIDDEN3_SIZE)), dtype=np.float32)
                    self.bn_var3 = np.array(weights.get('bn_var3', np.ones(HIDDEN3_SIZE)), dtype=np.float32)
            
            self.normalize_weights()
            print(f"Loaded weights from {filename}")
            
        except Exception as e:
            print(f"Error loading weights: {e}")
            print("Initializing with new random weights")
    
    def export_to_c_arrays(self, filename='nnue_weights_generated.h'):
        """C 헤더 파일로 내보내기 (client.c와 호환)"""
        with open(filename, 'w') as f:
            f.write("// Auto-generated Deep NNUE weights from Python training v7.0\n")
            f.write("// 4-layer structure with batch normalization\n")
            f.write("// Leaky ReLU activation\n\n")
            
            f.write("#ifndef NNUE_WEIGHTS_GENERATED_H\n")
            f.write("#define NNUE_WEIGHTS_GENERATED_H\n\n")
            
            f.write("#include <stdint.h>\n\n")
            
            # Define sizes
            f.write(f"#define NNUE_HIDDEN1_SIZE {HIDDEN1_SIZE}\n")
            f.write(f"#define NNUE_HIDDEN2_SIZE {HIDDEN2_SIZE}\n")
            f.write(f"#define NNUE_HIDDEN3_SIZE {HIDDEN3_SIZE}\n\n")
            
            # 양자화 스케일
            scale = QUANTIZATION_SCALE
            
            # 각 레이어 출력
            for name, data in [
                ('w1', self.w1), ('b1', self.b1),
                ('w2', self.w2), ('b2', self.b2),
                ('w3', self.w3), ('b3', self.b3),
                ('w4', self.w4), ('b4', self.b4),
                ('bn_gamma1', self.bn_gamma1), ('bn_beta1', self.bn_beta1),
                ('bn_gamma2', self.bn_gamma2), ('bn_beta2', self.bn_beta2),
                ('bn_gamma3', self.bn_gamma3), ('bn_beta3', self.bn_beta3),
            ]:
                # int16으로 양자화
                quantized = np.clip(np.round(data * scale), -32768, 32767).astype(np.int16)
                
                if name.startswith('w') and len(data.shape) == 2:
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
                        f.write(f"{quantized[i]:6d}")
                        if i < size - 1:
                            f.write(", ")
                            if (i + 1) % 8 == 0:
                                f.write("\n    ")
                    f.write("\n")
                
                f.write("};\n\n")
            
            # BN statistics (float로 저장)
            for name, data in [
                ('bn_mean1', self.bn_mean1), ('bn_var1', self.bn_var1),
                ('bn_mean2', self.bn_mean2), ('bn_var2', self.bn_var2),
                ('bn_mean3', self.bn_mean3), ('bn_var3', self.bn_var3),
            ]:
                size = data.size
                f.write(f"static const float nnue_{name}[{size}] = {{\n    ")
                for i in range(size):
                    f.write(f"{data[i]:.6f}f")
                    if i < size - 1:
                        f.write(", ")
                        if (i + 1) % 4 == 0:
                            f.write("\n    ")
                f.write("\n};\n\n")
            
            f.write(f"#define NNUE_SCALE {QUANTIZATION_SCALE}\n")
            f.write(f"#define NNUE_OUTPUT_SCALE {OUTPUT_SCALE}\n")
            f.write("#define HAS_NNUE_WEIGHTS\n\n")
            f.write("#endif // NNUE_WEIGHTS_GENERATED_H\n")
        
        print(f"Exported Deep NNUE weights to {filename}")
    
    def copy(self):
        """딥 카피"""
        new_nnue = DeepNNUE()
        
        # Weights
        new_nnue.w1 = self.w1.copy()
        new_nnue.b1 = self.b1.copy()
        new_nnue.w2 = self.w2.copy()
        new_nnue.b2 = self.b2.copy()
        new_nnue.w3 = self.w3.copy()
        new_nnue.b3 = self.b3.copy()
        new_nnue.w4 = self.w4.copy()
        new_nnue.b4 = self.b4.copy()
        
        # BN parameters
        new_nnue.bn_gamma1 = self.bn_gamma1.copy()
        new_nnue.bn_beta1 = self.bn_beta1.copy()
        new_nnue.bn_gamma2 = self.bn_gamma2.copy()
        new_nnue.bn_beta2 = self.bn_beta2.copy()
        new_nnue.bn_gamma3 = self.bn_gamma3.copy()
        new_nnue.bn_beta3 = self.bn_beta3.copy()
        
        # BN statistics
        new_nnue.bn_mean1 = self.bn_mean1.copy()
        new_nnue.bn_var1 = self.bn_var1.copy()
        new_nnue.bn_mean2 = self.bn_mean2.copy()
        new_nnue.bn_var2 = self.bn_var2.copy()
        new_nnue.bn_mean3 = self.bn_mean3.copy()
        new_nnue.bn_var3 = self.bn_var3.copy()
        
        # Training parameters
        new_nnue.learning_rate = self.learning_rate
        new_nnue.t = self.t

        if hasattr(self, 'm_w1'):
            new_nnue.m_w1 = self.m_w1.copy()
            new_nnue.v_w1 = self.v_w1.copy()
        
        return new_nnue


# 게임 클래스
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


# AI 플레이어들
class SimpleAI:
    """NNUE 기반 AI"""
    
    def __init__(self, nnue):
        self.nnue = nnue
        
    def get_move(self, game):
        """빠른 평가 기반 움직임 선택"""
        moves = game.get_valid_moves()
        if not moves:
            return None
        
        best_move = moves[0]
        best_score = -float('inf')
        
        max_moves_to_eval = min(20, len(moves))
        
        for move in moves[:max_moves_to_eval]:
            game_copy = game.copy()
            game_copy.make_move(move)
            
            try:
                score = self.nnue.forward(self.nnue.board_to_input(game_copy.board), training=False)[0]
                
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
                
                score += captures * 20
                
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


class DefensiveAI:
    """수비적 AI - 캡처 당하지 않기 우선"""
    
    def get_move(self, game):
        moves = game.get_valid_moves()
        if not moves:
            return None
        
        best_move = moves[0]
        best_score = -float('inf')
        
        for move in moves:
            r1, c1, r2, c2, move_type = move
            
            game_copy = game.copy()
            game_copy.make_move(move)
            
            opponent_moves = game_copy.get_valid_moves()
            vulnerability = 0
            
            my_piece = RED if game.current_player == 0 else BLUE
            
            for opp_move in opponent_moves:
                _, _, or2, oc2, _ = opp_move
                
                for dr in [-1, 0, 1]:
                    for dc in [-1, 0, 1]:
                        if dr == 0 and dc == 0:
                            continue
                        nr, nc = or2 + dr, oc2 + dc
                        if 0 <= nr < 8 and 0 <= nc < 8 and game_copy.board[nr][nc] == my_piece:
                            vulnerability += 1
            
            score = -vulnerability * 10
            
            opponent_piece = BLUE if game.current_player == 0 else RED
            captures = 0
            for dr in [-1, 0, 1]:
                for dc in [-1, 0, 1]:
                    if dr == 0 and dc == 0:
                        continue
                    nr, nc = r2 + dr, c2 + dc
                    if 0 <= nr < 8 and 0 <= nc < 8 and game.board[nr][nc] == opponent_piece:
                        captures += 1
            
            score += captures * 3
            
            if score > best_score:
                best_score = score
                best_move = move
        
        return best_move


class MinimaxDepth1AI:
    """1수 미니맥스 AI"""
    
    def evaluate_position(self, game, for_player):
        """간단한 평가함수"""
        my_piece = RED if for_player == 0 else BLUE
        opp_piece = BLUE if for_player == 0 else RED
        
        my_count = sum(row.count(my_piece) for row in game.board)
        opp_count = sum(row.count(opp_piece) for row in game.board)
        
        # 기본 점수
        score = (my_count - opp_count) * 100
        
        # 위치 점수
        for r in range(8):
            for c in range(8):
                if game.board[r][c] == my_piece:
                    # 코너 보너스
                    if (r in [0, 7]) and (c in [0, 7]):
                        score += 30
                    # 엣지 페널티
                    elif r in [0, 7] or c in [0, 7]:
                        score -= 10
                    # 중앙 보너스
                    elif 2 <= r <= 5 and 2 <= c <= 5:
                        score += 5
        
        return score
    
    def get_move(self, game):
        moves = game.get_valid_moves()
        if not moves:
            return None
        
        best_move = moves[0]
        best_score = -float('inf')
        
        for move in moves:
            # 내 수 시뮬레이션
            game_copy = game.copy()
            game_copy.make_move(move)
            
            # 상대 최선의 수 찾기
            opp_moves = game_copy.get_valid_moves()
            worst_case = float('inf')
            
            if not opp_moves:
                # 상대가 패스해야 함
                worst_case = self.evaluate_position(game_copy, game.current_player)
            else:
                for opp_move in opp_moves[:20]:  # 상위 20개만
                    game_copy2 = game_copy.copy()
                    game_copy2.make_move(opp_move)
                    eval_score = self.evaluate_position(game_copy2, game.current_player)
                    worst_case = min(worst_case, eval_score)
            
            if worst_case > best_score:
                best_score = worst_case
                best_move = move
        
        return best_move


def play_game_worker(args):
    """병렬 게임 실행 워커"""
    try:
        nnue1_weights, nnue2_weights, game_id = args
        
        # NNUE 복원
        nnue1 = DeepNNUE()
        nnue1.w1 = np.array(nnue1_weights['w1'])
        nnue1.b1 = np.array(nnue1_weights['b1'])
        nnue1.w2 = np.array(nnue1_weights['w2'])
        nnue1.b2 = np.array(nnue1_weights['b2'])
        nnue1.w3 = np.array(nnue1_weights['w3'])
        nnue1.b3 = np.array(nnue1_weights['b3'])
        nnue1.w4 = np.array(nnue1_weights['w4'])
        nnue1.b4 = np.array(nnue1_weights['b4'])
        
        # BN parameters
        if 'bn_gamma1' in nnue1_weights:
            nnue1.bn_gamma1 = np.array(nnue1_weights['bn_gamma1'])
            nnue1.bn_beta1 = np.array(nnue1_weights['bn_beta1'])
            nnue1.bn_gamma2 = np.array(nnue1_weights['bn_gamma2'])
            nnue1.bn_beta2 = np.array(nnue1_weights['bn_beta2'])
            nnue1.bn_gamma3 = np.array(nnue1_weights['bn_gamma3'])
            nnue1.bn_beta3 = np.array(nnue1_weights['bn_beta3'])
            
            nnue1.bn_mean1 = np.array(nnue1_weights.get('bn_mean1', np.zeros(HIDDEN1_SIZE)))
            nnue1.bn_var1 = np.array(nnue1_weights.get('bn_var1', np.ones(HIDDEN1_SIZE)))
            nnue1.bn_mean2 = np.array(nnue1_weights.get('bn_mean2', np.zeros(HIDDEN2_SIZE)))
            nnue1.bn_var2 = np.array(nnue1_weights.get('bn_var2', np.ones(HIDDEN2_SIZE)))
            nnue1.bn_mean3 = np.array(nnue1_weights.get('bn_mean3', np.zeros(HIDDEN3_SIZE)))
            nnue1.bn_var3 = np.array(nnue1_weights.get('bn_var3', np.ones(HIDDEN3_SIZE)))
        
        # 두 번째 플레이어 처리
        if isinstance(nnue2_weights, dict) and 'w1' in nnue2_weights:
            # NNUE vs NNUE
            nnue2 = DeepNNUE()
            nnue2.w1 = np.array(nnue2_weights['w1'])
            nnue2.b1 = np.array(nnue2_weights['b1'])
            nnue2.w2 = np.array(nnue2_weights['w2'])
            nnue2.b2 = np.array(nnue2_weights['b2'])
            nnue2.w3 = np.array(nnue2_weights['w3'])
            nnue2.b3 = np.array(nnue2_weights['b3'])
            nnue2.w4 = np.array(nnue2_weights['w4'])
            nnue2.b4 = np.array(nnue2_weights['b4'])
            
            # BN parameters
            if 'bn_gamma1' in nnue2_weights:
                nnue2.bn_gamma1 = np.array(nnue2_weights['bn_gamma1'])
                nnue2.bn_beta1 = np.array(nnue2_weights['bn_beta1'])
                nnue2.bn_gamma2 = np.array(nnue2_weights['bn_gamma2'])
                nnue2.bn_beta2 = np.array(nnue2_weights['bn_beta2'])
                nnue2.bn_gamma3 = np.array(nnue2_weights['bn_gamma3'])
                nnue2.bn_beta3 = np.array(nnue2_weights['bn_beta3'])
                
                nnue2.bn_mean1 = np.array(nnue2_weights.get('bn_mean1', np.zeros(HIDDEN1_SIZE)))
                nnue2.bn_var1 = np.array(nnue2_weights.get('bn_var1', np.ones(HIDDEN1_SIZE)))
                nnue2.bn_mean2 = np.array(nnue2_weights.get('bn_mean2', np.zeros(HIDDEN2_SIZE)))
                nnue2.bn_var2 = np.array(nnue2_weights.get('bn_var2', np.ones(HIDDEN2_SIZE)))
                nnue2.bn_mean3 = np.array(nnue2_weights.get('bn_mean3', np.zeros(HIDDEN3_SIZE)))
                nnue2.bn_var3 = np.array(nnue2_weights.get('bn_var3', np.ones(HIDDEN3_SIZE)))
                
            ai2 = SimpleAI(nnue2)
        elif isinstance(nnue2_weights, str):
            # NNUE vs 다른 AI
            if nnue2_weights == 'random':
                ai2 = RandomAI()
            elif nnue2_weights == 'greedy':
                ai2 = GreedyAI()
            elif nnue2_weights == 'minimax1':
                ai2 = MinimaxDepth1AI()
            else:
                ai2 = RandomAI()
        else:
            ai2 = SimpleAI(nnue1)
        
        # AI 생성
        ai1 = SimpleAI(nnue1)
        
        # 게임 실행
        game = OctaFlipGame()
        positions = []
        
        move_count = 0
        while not game.is_game_over() and move_count < 80:
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
            decay = 0.95 ** (i // 10)
            value *= decay
            
            # 스케일 조정
            value = np.clip(value * OUTPUT_SCALE * 0.8, -OUTPUT_SCALE, OUTPUT_SCALE)
            
            training_data.append((pos, value))
        
        return training_data, game.move_history, result_red
        
    except Exception as e:
        print(f"[ERROR] Error in game worker {game_id}: {e}")
        traceback.print_exc()
        return [], [], 0.0


class StableTrainer:
    """개선된 학습 시스템"""
    
    def __init__(self, target_hours=30):
        self.target_duration = timedelta(hours=target_hours)
        self.start_time = datetime.now()
        
        self.generation = 0
        
        # 통계 초기화
        self.stats = {
            'games_played': 0,
            'positions_trained': 0,
            'best_vs_random': -1.0,
            'best_vs_greedy': -1.0,
            'best_vs_minimax1': -1.0,
            'last_test_time': datetime.now(),
            'average_loss': 0.0,
            'loss_history': deque(maxlen=100),
            'generation_times': deque(maxlen=20),
        }
        
        # NNUE 풀
        self.population = []
        self.population_size = 12  # 줄임
        
        self.games_per_matchup = 2
        
        # Diversity control
        self.exploration_rate = 0.3  
        self.exploration_min = 0.05
        self.exploration_max = 0.5
        self.mutation_strength = 0.02
        
        # 기존 가중치 로드
        if os.path.exists('best_nnue_deep.pkl.gz'):
            base_nnue = DeepNNUE('best_nnue_deep.pkl.gz')
            for i in range(self.population_size):
                nnue = base_nnue.copy()
                if i > 0:
                    self.mutate_nnue(nnue, strength=self.mutation_strength * (1 + i * 0.1))
                self.population.append(nnue)
        else:
            self.population = [DeepNNUE() for _ in range(self.population_size)]
        
        # Hall of Fame
        self.hall_of_fame = deque(maxlen=HALL_OF_FAME_SIZE)
        
        # 학습 데이터
        self.training_buffer = deque(maxlen=MAX_BUFFER_SIZE)
        
        # 진행상황 로드
        self.load_progress()
        
        # AI 승률 추적
        self.ai_win_tracking = {}
        self.recent_improvements = deque(maxlen=10)  # 최근 개선 추적
    
    def mutate_nnue(self, nnue, strength=0.2):
        """적응형 mutation"""
        # Layer-wise mutation
        nnue.w1 += np.random.randn(*nnue.w1.shape).astype(np.float32) * strength
        nnue.w2 += np.random.randn(*nnue.w2.shape).astype(np.float32) * strength * 0.9
        nnue.w3 += np.random.randn(*nnue.w3.shape).astype(np.float32) * strength * 0.8
        nnue.w4 += np.random.randn(*nnue.w4.shape).astype(np.float32) * strength * 0.7
        
        # Bias mutati
        nnue.b1 += np.random.randn(*nnue.b1.shape).astype(np.float32) * strength * 3
        nnue.b2 += np.random.randn(*nnue.b2.shape).astype(np.float32) * strength * 3
        nnue.b3 += np.random.randn(*nnue.b3.shape).astype(np.float32) * strength * 3
        nnue.b4 += np.random.randn(*nnue.b4.shape).astype(np.float32) * strength * 3
        
        # BN parameters (작게)
        nnue.bn_gamma1 += np.random.randn(*nnue.bn_gamma1.shape).astype(np.float32) * strength * 0.3
        nnue.bn_gamma2 += np.random.randn(*nnue.bn_gamma2.shape).astype(np.float32) * strength * 0.3
        nnue.bn_gamma3 += np.random.randn(*nnue.bn_gamma3.shape).astype(np.float32) * strength * 0.3
        
        nnue.normalize_weights()
    
    def load_progress(self):
        """진행상황 로드"""
        if os.path.exists('training_stats_deep.json'):
            try:
                with open('training_stats_deep.json', 'r') as f:
                    saved_stats = json.load(f)
                    
                    self.generation = saved_stats.get('generation', 0)
                    
                    for key, value in saved_stats.items():
                        if key not in ['loss_history', 'generation_times', 'last_test_time']:
                            self.stats[key] = value
                    
                    for key in ['loss_history', 'generation_times']:
                        if key in saved_stats:
                            maxlen = 100 if 'loss' in key else 20
                            self.stats[key] = deque(saved_stats[key], maxlen=maxlen)
                    
                    if 'last_test_time' in saved_stats:
                        try:
                            if isinstance(saved_stats['last_test_time'], str):
                                self.stats['last_test_time'] = datetime.fromisoformat(saved_stats['last_test_time'])
                            else:
                                self.stats['last_test_time'] = datetime.now()
                        except:
                            self.stats['last_test_time'] = datetime.now()
                    else:
                        self.stats['last_test_time'] = datetime.now()
                            
                print(f"Resumed from generation {self.generation}")
                print(f"Previous games: {self.stats['games_played']}")
                
            except Exception as e:
                print(f"Error loading progress: {e}")
    
    def should_continue(self):
        """시간 체크"""
        elapsed = datetime.now() - self.start_time
        return elapsed < self.target_duration
    
    def update_exploration_rate(self):
        """성능 기반 exploration rate 조정"""
        # 실제 평균 승률 계산
        winrates = []
        for ai_type in ['random', 'greedy', 'minimax1']:
            key = f'best_vs_{ai_type}'
            if key in self.stats and self.stats[key] >= 0:
                winrates.append(self.stats[key])
        
        if len(winrates) >= 2:
            current_avg = np.mean(winrates)
            
            # 개선 추적
            if len(self.recent_improvements) > 0:
                last_avg = self.recent_improvements[-1]
                improvement = current_avg - last_avg
            else:
                improvement = 0
            
            self.recent_improvements.append(current_avg)
            
            # 적응형 조정
            if improvement > 1.0:  # 개선됨
                self.exploration_rate *= 0.95
            elif improvement < -0.5:  # 악화됨
                self.exploration_rate *= 1.1
            else:  # 정체
                if current_avg < 55:
                    self.exploration_rate *= 1.05
                else:
                    self.exploration_rate *= 0.98
            
            # 범위 제한
            self.exploration_rate = np.clip(self.exploration_rate, self.exploration_min, self.exploration_max)
            
            # Mutation strength도 조정
            if current_avg < 50:
                self.mutation_strength = min(0.05, self.mutation_strength * 1.1)
            elif current_avg > 80:
                self.mutation_strength = max(0.005, self.mutation_strength * 0.9)

    def run_generation(self):
        """한 세대 실행"""
        gen_start = datetime.now()
        
        print(f"\n{'='*60}")
        print(f"Generation {self.generation} | Games: {self.stats['games_played']:,}")
        print(f"Exploration: {self.exploration_rate:.3f}, Mutation: {self.mutation_strength:.4f}")
        
        # Loss 상태
        if self.stats['loss_history']:
            recent_losses = list(self.stats['loss_history'])[-20:]
            avg_loss = np.mean(recent_losses)
            print(f"Avg loss: {avg_loss:.4f}")
        
        # AI 승률 추적 초기화
        self.ai_win_tracking = {
            'vs_random': {'wins': 0, 'total': 0},
            'vs_greedy': {'wins': 0, 'total': 0},
            'vs_minimax1': {'wins': 0, 'total': 0},
        }
        
        # 1. Self-play 토너먼트
        scores = [0] * self.population_size
        games_data = []
        
        # 병렬 게임 실행
        with mp.Pool(WORKERS) as pool:
            tasks = []
            
            # Round-robin tournament
            for i in range(self.population_size):
                for j in range(i + 1, self.population_size):
                    w1 = self._serialize_weights(self.population[i])
                    w2 = self._serialize_weights(self.population[j])
                    
                    for game_num in range(self.games_per_matchup):
                        tasks.append((i, j, (w1, w2, f"{self.generation}_{i}_{j}_game{game_num}_1")))
                        if game_num == 0:  # 한 게임만 reverse
                            tasks.append((j, i, (w2, w1, f"{self.generation}_{j}_{i}_game{game_num}_2")))        
            
            # 벤치마크 AI와 게임 (4종류만)
            for i in range(self.population_size):
                w1 = self._serialize_weights(self.population[i])
                
                for ai_type in ['random', 'greedy', 'minimax1']:
                    ai_id = {
                        'random': -1, 'greedy': -2, 
                        'minimax1': -3
                    }[ai_type]
                    
                    # 각 AI와 2게임씩
                    tasks.append((i, ai_id, (w1, ai_type, f"{self.generation}_{i}_{ai_type}_1")))
                    tasks.append((i, ai_id, (w1, ai_type, f"{self.generation}_{i}_{ai_type}_2")))
            
            print(f"Running {len(tasks)} games...", end='', flush=True)
            
            # 실행
            results = pool.map(play_game_worker, [task[2] for task in tasks])
            
            # 결과 처리
            for idx, (player1, player2, _) in enumerate(tasks):
                training_data, moves, result = results[idx]
                
                if training_data:
                    games_data.extend(training_data)
                    
                    # 점수 업데이트
                    if player1 >= 0 and player2 >= 0:
                        if result > 0:
                            scores[player1] += 3
                        elif result < 0:
                            scores[player2] += 3
                        else:
                            scores[player1] += 1
                            scores[player2] += 1
                    
                    # AI 승률 추적
                    if player1 >= 0 and player2 < 0:
                        ai_type_map = {
                            -1: 'vs_random', -2: 'vs_greedy',
                            -3: 'vs_minimax1'
                        }
                        
                        if player2 in ai_type_map:
                            ai_key = ai_type_map[player2]
                            self.ai_win_tracking[ai_key]['total'] += 1
                            if result > 0:
                                self.ai_win_tracking[ai_key]['wins'] += 1
                    
                    self.stats['games_played'] += 1
        
        print(f" Done! {len(games_data)} positions")
        
        # 승률 출력
        print("Win rates:")
        for ai, stats in self.ai_win_tracking.items():
            if stats['total'] > 0:
                wr = stats['wins'] / stats['total'] * 100
                print(f"  {ai}: {wr:.1f}% ({stats['wins']}/{stats['total']})")
        
        # 데이터 추가
        self.training_buffer.extend(games_data)
        
        # 상위 선택
        ranked = sorted(zip(scores, range(self.population_size)), reverse=True)
        best_indices = [idx for _, idx in ranked[:self.population_size//2]]
        
        # 학습
        train_start = datetime.now()
        self.train_networks(best_indices)
        train_time = (datetime.now() - train_start).total_seconds()
        
        # 새 세대 생성
        self.create_new_generation(ranked)
        
        # Exploration rate 업데이트
        self.update_exploration_rate()
        
        # Hall of Fame
        if self.generation % 10 == 0:
            best_idx = ranked[0][1]
            self.hall_of_fame.append(self.population[best_idx].copy())
        
        self.generation += 1
        
        # 통계
        gen_time = (datetime.now() - gen_start).total_seconds()
        self.stats['generation_times'].append(gen_time)
        
        print(f"Gen time: {gen_time:.1f}s (train: {train_time:.1f}s)")
    
    def _serialize_weights(self, nnue):
        """가중치 직렬화"""
        return {
            'w1': nnue.w1.tolist(), 'b1': nnue.b1.tolist(),
            'w2': nnue.w2.tolist(), 'b2': nnue.b2.tolist(),
            'w3': nnue.w3.tolist(), 'b3': nnue.b3.tolist(),
            'w4': nnue.w4.tolist(), 'b4': nnue.b4.tolist(),
            'bn_gamma1': nnue.bn_gamma1.tolist(), 'bn_beta1': nnue.bn_beta1.tolist(),
            'bn_gamma2': nnue.bn_gamma2.tolist(), 'bn_beta2': nnue.bn_beta2.tolist(),
            'bn_gamma3': nnue.bn_gamma3.tolist(), 'bn_beta3': nnue.bn_beta3.tolist(),
            'bn_mean1': nnue.bn_mean1.tolist(), 'bn_var1': nnue.bn_var1.tolist(),
            'bn_mean2': nnue.bn_mean2.tolist(), 'bn_var2': nnue.bn_var2.tolist(),
            'bn_mean3': nnue.bn_mean3.tolist(), 'bn_var3': nnue.bn_var3.tolist(),
        }
    
    def train_networks(self, indices):
        """네트워크 학습"""
        if len(self.training_buffer) < BATCH_SIZE * 2:
            return
        
        # 학습 파라미터
        epochs = 2
        mini_batch_size = min(BATCH_SIZE, 512)
        
        # 데이터 준비
        buffer_list = list(self.training_buffer)
        
        # 가중치 계산
        recency_weights = np.array([0.99 ** (len(buffer_list) - i - 1) 
                                    for i in range(len(buffer_list))])
        recency_weights /= recency_weights.sum()
        
        # 각 네트워크 학습
        for idx in indices[:3]:  # 상위 3개만 학습
            nnue = self.population[idx]
            nnue.generation = self.generation
            
            for epoch in range(epochs):
                # Training data sampling
                sample_size = min(len(buffer_list), mini_batch_size * 20)
                sampled_indices = np.random.choice(
                    len(buffer_list), 
                    size=sample_size,
                    p=recency_weights,
                    replace=True
                )
                
                sampled_data = [buffer_list[i] for i in sampled_indices]
                random.shuffle(sampled_data)
                
                # Training
                epoch_losses = []
                
                for i in range(0, len(sampled_data) - mini_batch_size, mini_batch_size):
                    batch = sampled_data[i:i + mini_batch_size]
                    
                    # 배치 준비
                    X = np.zeros((mini_batch_size, INPUT_SIZE), dtype=np.float32)
                    y = np.zeros(mini_batch_size, dtype=np.float32)
                    
                    for j, (pos, value) in enumerate(batch):
                        X[j] = nnue.board_to_input(pos['board'])
                        y[j] = np.clip(value, -OUTPUT_SCALE, OUTPUT_SCALE)
                    
                    # Forward & backward
                    y_pred = nnue.forward(X, training=True)
                    loss = nnue.backward_adam(X, y, y_pred)
                    
                    if not np.isnan(loss) and not np.isinf(loss) and loss < 100:
                        epoch_losses.append(loss)
                    
                    self.stats['positions_trained'] += mini_batch_size
                
                # Update loss history
                if epoch_losses:
                    avg_loss = np.mean(epoch_losses)
                    self.stats['loss_history'].append(avg_loss)
        
        # Buffer management
        if len(self.training_buffer) > MAX_BUFFER_SIZE * 0.9:
            remove_count = int(MAX_BUFFER_SIZE * 0.1)
            for _ in range(remove_count):
                self.training_buffer.popleft()
    
    def create_new_generation(self, rankings):
        """새 세대 생성"""
        new_population = []
        
        # 엘리트
        elite_count = max(2, self.population_size // 4)
        for i in range(elite_count):
            _, idx = rankings[i]
            new_population.append(self.population[idx].copy())
        
        # Tournament selection
        while len(new_population) < self.population_size:
            tournament = random.sample(rankings[:self.population_size//2], 3)
            winner = max(tournament, key=lambda x: x[0])
            
            parent_idx = winner[1]
            child = self.population[parent_idx].copy()
            
            # Adaptive mutation
            if random.random() < self.exploration_rate:
                strength = self.mutation_strength * 3
            else:
                strength = self.mutation_strength
                
            self.mutate_nnue(child, strength=strength)
            new_population.append(child)
        
        self.population = new_population
    
    def test_best_network(self):
        """네트워크 테스트 (간소화)"""
        print("\nTesting best network...")
        
        best_nnue = self.population[0]
        ai = SimpleAI(best_nnue)
        
        test_results = {}
        ai_list = ['random', 'greedy', 'minimax1']
        
        for ai_type in ai_list:
            test_results[ai_type] = 0
            
            ai_class_map = {
                'random': RandomAI,
                'greedy': GreedyAI,
                'minimax1': MinimaxDepth1AI
            }
            
            print(f"  vs {ai_type}: ", end='', flush=True)
            
            # 각 AI와 2게임만 (선공/후공)
            for i in range(QUICK_TEST_GAMES):
                game = OctaFlipGame()
                opponent = ai_class_map[ai_type]()
                
                color = i % 2
                move_count = 0
                
                while not game.is_game_over() and move_count < 60:
                    if game.current_player == color:
                        move = ai.get_move(game)
                    else:
                        move = opponent.get_move(game)
                    
                    game.make_move(move)
                    move_count += 1
                
                result_red, result_blue = game.get_result()
                if (color == 0 and result_red > 0) or (color == 1 and result_blue > 0):
                    test_results[ai_type] += 1
            
            print(f"{test_results[ai_type]}/{QUICK_TEST_GAMES} wins")
        
        # 통계 업데이트
        for ai_type in ai_list:
            key = f'best_vs_{ai_type}'
            self.stats[key] = test_results[ai_type] * 100 / QUICK_TEST_GAMES
        
        # 전체 평균
        all_wins = sum(test_results.values())
        all_games = len(ai_list) * QUICK_TEST_GAMES
        overall = all_wins * 100 / all_games
        
        print(f"Overall win rate: {overall:.1f}%")
    
    def save_progress(self):
        """진행상황 저장"""
        # 최고 NNUE 저장
        best_nnue = self.population[0]
        best_nnue.save_weights('best_nnue_deep.pkl.gz')
        
        # C 헤더 생성
        best_nnue.export_to_c_arrays()
        
        # 통계 저장
        self.stats['generation'] = self.generation
        if isinstance(self.stats['last_test_time'], datetime):
            self.stats['last_test_time'] = self.stats['last_test_time'].isoformat()
        
        def convert_to_json_serializable(obj):
            if isinstance(obj, np.integer):
                return int(obj)
            elif isinstance(obj, np.floating):
                return float(obj)
            elif isinstance(obj, np.ndarray):
                return obj.tolist()
            elif isinstance(obj, list):
                return [convert_to_json_serializable(item) for item in obj]
            elif isinstance(obj, dict):
                return {key: convert_to_json_serializable(value) for key, value in obj.items()}
            elif isinstance(obj, (deque, tuple)):
                return list(convert_to_json_serializable(item) for item in obj)
            else:
                return obj
        
        stats_to_save = convert_to_json_serializable(self.stats)
        
        with open('training_stats_deep.json', 'w') as f:
            json.dump(stats_to_save, f, indent=2)
        
        print(f"Progress saved (Gen {self.generation})")
    
    def train(self):
        """메인 학습 루프"""
        print(f"Training for {self.target_duration.total_seconds()/3600:.1f} hours...")
        print(f"Architecture: {INPUT_SIZE}-{HIDDEN1_SIZE}-{HIDDEN2_SIZE}-{HIDDEN3_SIZE}-{OUTPUT_SIZE}")
        print(f"Using Leaky ReLU activation")
        print(f"Benchmark AIs: Random, Greedy, Minimax1")
        print("-" * 60)
        
        last_save_gen = self.generation
        
        try:
            while self.should_continue():
                self.run_generation()
                
                # 저장
                if self.generation - last_save_gen >= SAVE_INTERVAL_GENERATIONS:
                    self.save_progress()
                    last_save_gen = self.generation
                    
                    # 테스트
                    if self.generation % 10 == 0:
                        self.test_best_network()
                        self.stats['last_test_time'] = datetime.now()
                
                # 진행상황
                if self.generation % 5 == 0:
                    elapsed = datetime.now() - self.start_time
                    progress = elapsed.total_seconds() / self.target_duration.total_seconds() * 100
                    remaining = self.target_duration - elapsed
                    
                    print(f"\nProgress: {progress:.1f}% | "
                          f"Games: {self.stats['games_played']:,} | "
                          f"Time left: {remaining}")
                
        except KeyboardInterrupt:
            print("\nTraining interrupted by user")
        
        # 최종 저장
        self.save_progress()
        self.test_best_network()
        
        # 최종 보고서
        print("\n" + "="*60)
        print("✅ Training complete!")
        print(f"📊 Final statistics:")
        print(f"   - Generations: {self.generation}")
        print(f"   - Total games: {self.stats['games_played']:,}")
        print(f"   - Positions trained: {self.stats['positions_trained']:,}")
        
        # 각 AI별 최종 승률
        print(f"\n📈 Final win rates:")
        print(f"   - vs Random: {self.stats.get('best_vs_random', -1):.1f}%")
        print(f"   - vs Greedy: {self.stats.get('best_vs_greedy', -1):.1f}%")
        print(f"   - vs Minimax1: {self.stats.get('best_vs_minimax1', -1):.1f}%")
        
        # 평균 계산
        all_winrates = []
        for ai_type in ['random', 'greedy', 'minimax1']:
            key = f'best_vs_{ai_type}'
            if key in self.stats and self.stats[key] >= 0:
                all_winrates.append(self.stats[key])
        
        if all_winrates:
            overall_avg = np.mean(all_winrates)
            print(f"\n   Overall average: {overall_avg:.1f}%")
        
        if self.stats['loss_history']:
            recent_loss = np.mean(list(self.stats['loss_history'])[-50:])
            print(f"   - Recent loss: {recent_loss:.6f}")
        
        print("\n💾 Files generated:")
        print("   - best_nnue_deep.pkl.gz (Python weights)")
        print("   - nnue_weights_generated.h (C header)")
        print("   - training_stats_deep.json (Statistics)")
        
        return self.population[0]


def main():
    print("=== OctaFlip NNUE Training System v7.0 ===\n")
    
    print("Training options:")
    print("1. Quick test (1 hour)")
    print("2. Short training (5 hours)")
    print("3. Standard training (30 hours)")
    print("4. Test existing weights")
    
    choice = input("\nSelect option (1-4): ")
    
    if choice == '1':
        hours = 1
    elif choice == '2':
        hours = 5
    elif choice == '3':
        hours = 30
    elif choice == '4':
        # 기존 가중치 테스트
        if os.path.exists('best_nnue_deep.pkl.gz'):
            print("\nTesting existing weights...")
            
            nnue = DeepNNUE('best_nnue_deep.pkl.gz')
            ai = SimpleAI(nnue)
            
            wins = {}
            ai_list = ['random', 'greedy', 'minimax1']
            
            for opponent_type in ai_list:
                wins[opponent_type] = 0
                
                ai_class_map = {
                    'random': RandomAI,
                    'greedy': GreedyAI,
                    'minimax1': MinimaxDepth1AI
                }
                
                print(f"\nvs {opponent_type}: ", end='', flush=True)
                
                test_games = 10
                
                for i in range(test_games):
                    game = OctaFlipGame()
                    opp = ai_class_map[opponent_type]()
                    color = i % 2
                    moves = 0
                    
                    while not game.is_game_over() and moves < 100:
                        if game.current_player == color:
                            move = ai.get_move(game)
                        else:
                            move = opp.get_move(game)
                        game.make_move(move)
                        moves += 1
                    
                    r, b = game.get_result()
                    if (color == 0 and r > 0) or (color == 1 and b > 0):
                        wins[opponent_type] += 1
                
                print(f"{wins[opponent_type]}/{test_games} wins ({wins[opponent_type]*10:.0f}%)")
            
            overall = sum(wins.values()) * 100 / (len(ai_list) * test_games)
            print(f"\nOverall win rate: {overall:.1f}%")
            
            # 네트워크 상태 체크
            print(f"\n=== Network Analysis ===")
            test_game = OctaFlipGame()
            test_input = nnue.board_to_input(test_game.board)
            test_output = nnue.forward(test_input.reshape(1, -1), training=False)[0]
            
            print(f"Initial position evaluation: {test_output:.1f}")
            print(f"Weight norms:")
            print(f"  W1: {np.linalg.norm(nnue.w1):.2f}")
            print(f"  W2: {np.linalg.norm(nnue.w2):.2f}")
            print(f"  W3: {np.linalg.norm(nnue.w3):.2f}")
            print(f"  W4: {np.linalg.norm(nnue.w4):.2f}")
        else:
            print("No weights found!")
        return
    
    else:
        hours = 30
    
    # 학습 시작
    trainer = StableTrainer(target_hours=hours)
    best_nnue = trainer.train()
    
    print("\n✨ NNUE training completed successfully!")
    print("Use the generated files in your C client for enhanced gameplay.")


if __name__ == "__main__":
    main()