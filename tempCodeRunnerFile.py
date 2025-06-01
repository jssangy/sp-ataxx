    def forward(self, x, training=True):
        """순전파 (안정화) - 출력 타입 보장"""
        if len(x.shape) == 1:
            x = x.reshape(1, -1)
        
        # 입력을 float32로 확실히 변환
        x = x.astype(np.float32)
        self.x = x
        
        # Layer 1 with clipping
        self.z1 = np.dot(x, self.w1) + self.b1
        self.z1 = np.clip(self.z1, -5, 5)
        self.a1 = np.maximum(0, self.z1)  # ReLU
        
        # Dropout simulation (training only)
        if training:
            dropout_mask = np.random.binomial(1, 0.9, size=self.a1.shape)
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
        
        # float32 배열로 반환 보장
        return output.flatten().astype(np.float32)