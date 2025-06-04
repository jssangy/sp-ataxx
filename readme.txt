# Assignment 3: OctaFlip with LED Matrix Display
**Team Shannon**

## Overview
This project implements an OctaFlip game client with LED matrix display support and advanced AI engines. The system includes a game server, AI client with 6 different engines, 64x64 RGB LED matrix visualization, and a neural network training system.

## Compilation Instructions

### Quick Compilation
```bash
# Using Makefile (Recommended)
make              # Build all components (server, client, board)
make server       # Build only the server
make client       # Build only the client with LED support
make board        # Build standalone board display for testing

# Manual Compilation
gcc -O3 -pthread server.c cJSON.c -o server -lm
gcc -O3 -pthread client.c board.c cJSON.c -o client -lm
gcc -O3 -pthread -DSTANDALONE_BUILD board.c -o board -lm

# With LED Matrix Library (Raspberry Pi) - Auto-detected by Makefile
gcc -O3 -pthread -DSTANDALONE_BUILD -DHAS_LED_MATRIX board.c -o board -lm -lrgbmatrix -I/usr/local/include -L/usr/local/lib
```

### Check LED Matrix Installation
```bash
make check-led    # Check if LED matrix library is installed
```

## Execution Instructions

### 1. Connect to Official Server (Assignment Requirement)
```bash
# Connect to the TA's server at DGIST
./client -ip 10.8.128.233 -port 8080 -username TeamShannon

# Select AI engine (1-6)
./client -ip 10.8.128.233 -port 8080 -username TeamShannon -engine 6
```

### 2. Local Testing
```bash
# Terminal 1: Start local server
./server

# Terminal 2: Run AI client
./client -ip 127.0.0.1 -port 8080 -username TeamShannon

```

### 3. LED Display Testing (For Grading)
```bash
# Standalone board display - reads 8 lines from stdin
sudo ./board < test_input.txt

# Or manually:
sudo ./board
R......B
........
........
........
........
........
........
B......R

# Test with sample board
make test-board

# Test LED patterns
make test-led
```

## AI Engines

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

## LED Matrix Display

### Features
- 64x64 RGB LED matrix support
- Real-time board state visualization
- Team name animation on startup
- Victory/defeat animations
- Grid-based layout with 8x8 cells

### Display Mapping
- Each OctaFlip cell → 8x8 LED pixels
- 1-pixel grid lines between cells
- 6x6 colored area for pieces
- Colors:
  - Red pieces: RGB(255, 30, 30)
  - Blue pieces: RGB(30, 120, 255)
  - Grid lines: RGB(80, 80, 80)
  - Empty cells: Black

### Testing LED Display
```bash
# Test with sample board
make test-board

# Manual input mode
sudo ./board -manual

# Network mode (if enabled)
./board -network

# Check LED installation
make check-led
```

## Advanced Features

### NNUE Neural Network
- **Architecture**: 192-128-64-32-1 (4-layer deep network with batch normalization)
- **Input**: 3 planes × 64 squares = 192 features
- **Weights**: Quantized to int16 for efficiency
- **Training**: 30+ hours of self-play with Leaky ReLU activation
- **Integration**: Used in engines 2, 4, and 6
- **Output**: Scaled evaluation in range [-200, 200]

### Game Phase Detection
- **Opening**: < 20 total pieces (focus on center control)
- **Midgame**: 20-50 pieces (balanced play)
- **Early Endgame**: 50-58 pieces (consolidation)
- **Late Endgame**: 58-64 pieces (corner control critical)

### Time Management
- **Base**: 3.0 seconds per move
- **Opening**: 70% of time limit
- **Midgame**: 80% of time limit
- **Early Endgame**: 85% of time limit
- **Late Endgame**: 90% of time limit
- **Safety margin**: Always keeps 0.1s buffer

## Tournament System

### Running Tournaments
```bash
# Full double round-robin
python3 tournament.py
# Select option 1

# Quick match between two engines
python3 tournament.py
# Select option 2, then choose engines
```

### Features
- Automated game execution
- Win/loss/draw statistics
- Performance comparison
- Log file generation

## NNUE Training

### Training New Weights
```bash
# Standard 30-hour training
python3 train_octoflip.py
# Select option 3

# Quick 1-hour test
python3 train_octoflip.py
# Select option 1

# Test existing weights
python3 train_octoflip.py
# Select option 4
```

### Training Features
- Deep NNUE with Leaky ReLU activation
- Population-based training (12 networks)
- Self-play against multiple AI types
- Adaptive exploration rate
- Automatic C header generation (nnue_weights_generated.h)

## Technical Implementation

### Core Algorithms

#### 1. Move Generation
- Validates all clone (1-step) and jump (2-step) moves
- Phase-specific move ordering
- Capture detection and prioritization
- Handles pass moves when no valid moves exist

#### 2. Board Evaluation
- Material count (phase-dependent: 80-150 points per piece)
- Position weights (3 different tables for game phases)
- NNUE evaluation blend (when available)
- Mobility bonus (2 points per available move)

#### 3. Time Management
- 3-second hard limit per move
- Phase-based allocation (70-90% of limit)
- Iterative deepening with time checks
- Emergency move at 85% time usage

#### 4. Memory Optimization
- Hash table: 256K entries (RPi optimized)
- Transposition table with 3-flag system
- Killer moves: 2 per depth level
- History table for move ordering

### Network Protocol
- TCP sockets with JSON messages
- Newline-delimited message framing
- Supports all Assignment 2 message types
- Robust partial message handling

## File Structure
```
hw4_전용준_202311162_Shannon/
├── client.c                    # AI client with 6 engines
├── board.c                     # LED matrix display
├── board.h                     # Display interface
├── server.c                    # Game server (reference)
├── cJSON.c/h                   # JSON parsing library
├── nnue_weights_generated.h    # Pre-trained NNUE weights
├── train_octoflip.py          # NNUE training system v7.0
├── tournament.py              # Tournament runner
├── Makefile                   # Build automation
└── README.txt                 # This file
```

## Troubleshooting

### Common Issues
1. **LED Matrix not working**: Ensure you have rgbmatrix library installed
2. **Connection timeout**: Check if you're on DGIST WiFi
3. **Compilation errors**: Install pthread and math libraries
4. **Python errors**: Requires Python 3.6+ with numpy

### Debug Mode
```bash
# Build with debug symbols
make debug

# Enable debug output
./client -username Debug -engine 1
```

## Performance Notes
- Optimized for Raspberry Pi 4
- Uses -O3 optimization
- Multi-threaded search (4 threads max)
- Memory usage under 50MB
- CPU usage varies by engine (20-80%)

## Credits
**Team Shannon**
- Advanced AI implementation
- LED visualization system  
- NNUE neural network
- Complete system integration

---
*Assignment 3 - DGIST System Programming course*

g++ -o board board.c -DSTANDALONE_BUILD -DHAS_LED_MATRIX -I./rpi-rgb-led-matrix/include -L./rpi-rgb-led-matrix/lib -lrgbmatrix -lrt -lm -lpthread