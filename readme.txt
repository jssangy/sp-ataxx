1. 컴파일 방법
cd rpi-led-rgb-matrix
make
cd ..
g++ -o board board.c -DSTANDALONE_BUILD -DHAS_LED_MATRIX -I./rpi-rgb-led-matrix/include -L./rpi-rgb-led-matrix/lib -lrgbmatrix -lrt -lm -lpthread
make

2. 실행 방법 (board)
sudo ./board

3. 실행 방법 (client)
sudo ./client -ip 10.8.128.233 -port 8080 -username TeamShannon


번외 - 여러 AI engine

### 1. Eunsong Engine (ENGINE_EUNSONG)
```bash
./client -ip 10.8.128.233 -port 8080 -username Eunsong_Engine -engine 1
./client -ip 127.0.0.1 -port 8080 -username Eunsong_Engine -engine 1

```
- **Algorithm**: Parallel Negamax with Alpha-Beta pruning
- **Features**: 
  - Multi-threaded search (4 threads)
  - History heuristic
  - Killer moves
  - Dynamic depth (4-6 plies)
- **Strengths**: Fast and reliable baseline

### 2. MCTS with Neural Network (ENGINE_MCTS_NN)
```bash
./client -ip 10.8.128.233 -port 8080 -username MCTS_NN -engine 2
./client -ip 127.0.0.1 -port 8080 -username MCTS_NN -engine 2
```
- **Algorithm**: Monte Carlo Tree Search with NNUE evaluation
- **Features**:
  - Deep neural network evaluation (4-layer, 192-128-64-32-1)
  - UCB1 selection with exploration constant 1.41
  - Parallel simulation (4 threads)
  - Limited expansion for memory efficiency
- **Strengths**: Strong positional understanding

### 3. MCTS Classic (ENGINE_MCTS_CLASSIC)
```bash
./client -ip 10.8.128.233 -port 8080 -username MCTS_Classic -engine 3
./client -ip 127.0.0.1 -port 8080 -username MCTS_Classic -engine 3

```
- **Algorithm**: Pure Monte Carlo Tree Search
- **Features**:
  - Capture-biased playouts
  - Fast simulation (30 moves max)
  - Progressive widening
  - Robust move selection
- **Strengths**: Good in tactical positions

### 4. Minimax with Neural Network (ENGINE_MINIMAX_NN)  
```bash
./client -ip 10.8.128.233 -port 8080 -username Minimax_NN -engine 4
./client -ip 127.0.0.1 -port 8080 -username Minimax_NN -engine 4

```
- **Algorithm**: Alpha-Beta with NNUE evaluation
- **Features**:
  - Blended evaluation (NNUE + classical)
  - Phase-dependent weights
  - Transposition table (256K entries)
  - Iterative deepening to depth 7
- **Strengths**: Excellent endgame play

### 5. Minimax Classic (ENGINE_MINIMAX_CLASSIC) 
```bash
./client -ip 10.8.128.233 -port 8080 -username Minimax_Classic -engine 5
./client -ip 127.0.0.1 -port 8080 -username Minimax_Classic -engine 5
```
- **Algorithm**: Pure Alpha-Beta search
- **Features**:
  - Phase-specific position weights
  - Killer moves and history heuristic
  - Move ordering with capture bonus
  - Adaptive time management
- **Strengths**: Reliable and predictable

### 6. Tournament Beast (ENGINE_TOURNAMENT_BEAST) [Default]
```bash
./client -ip 10.8.128.233 -port 8080 -username Tournament_Beast -engine 6
./client -ip 127.0.0.1 -port 8080 -username Tournament_Beast -engine 6
```
- **Algorithm**: Phase-adaptive hybrid engine
- **Features**:
  - Opening (< 38 pieces): Minimax blend (80% NN, 20% Classic)
  - Midgame (38-43 pieces): MCTS blend (80% NN, 20% Classic)
  - Early Endgame (43-50 pieces): MCTS blend (60% NN, 40% Classic)
  - Late Endgame (58-64 pieces): Minimax blend (30% NN, 70% Classic)
- **Strengths**: Best overall performance, adapts to game phase
