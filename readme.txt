Assignment 2: OctaFlip with Network
===================================

Compilation Commands:
====================
gcc server.c cJSON.c -o server -lpthread
gcc client.c cJSON.c -o client -lpthread -lm

# Or using Makefile:
make            # Build all
make debug      # Debug build
make clean      # Clean build files
make test       # Run quick test match
make quick-match # Run match between two AIs
make install    # Install to ~/bin
make help       # Show all commands

Execution:
==========
1. Run server: ./server
2. Run client: ./client -ip {ip} -port {port} -username {username}

Example:
========
# Terminal 1 - Start server
./server

# Terminal 2 - Player 1
./client -ip 127.0.0.1 -port 8080 -username Alice

# Terminal 3 - Player 2  
./client -ip 127.0.0.1 -port 8080 -username Bob

Additional Options:
==================
-engine {1-20}  : Select AI engine (default: 13 - Ultimate AI )
-human         : Human play mode (disable AI)

AI Engines:

DefaultEngine: Ultimate AI
./client -ip 127.0.0.1 -port 8080 -username DefaultEngine

===========
1. Hybrid Alpha-Beta      - Enhanced minimax with optimizations
   ./client -ip 127.0.0.1 -port 8080 -username AlphaBetaAI -engine 1

2. Parallel MCTS         - Monte Carlo Tree Search with threading
   ./client -ip 127.0.0.1 -port 8080 -username ParallelMCTS -engine 2

3. Neural Pattern        - Pattern recognition based
   ./client -ip 127.0.0.1 -port 8080 -username NeuralPattern -engine 3

4. Opening Book          - Pre-computed opening moves
   ./client -ip 127.0.0.1 -port 8080 -username OpeningBook -engine 4

5. Endgame Solver        - Perfect endgame play
   ./client -ip 127.0.0.1 -port 8080 -username EndgameSolver -engine 5

6. Tournament Beast      - Combination of various Strategy
   ./client -ip 127.0.0.1 -port 8080 -username TournamentBeast -engine 6

7. Adaptive Time         - Time management specialist
   ./client -ip 127.0.0.1 -port 8080 -username AdaptiveTime -engine 7

8. Learning Engine       - Self-improving engine
   ./client -ip 127.0.0.1 -port 8080 -username LearningEngine -engine 8

9. Human Style           - Human-like moves
   ./client -ip 127.0.0.1 -port 8080 -username HumanStyle -engine 9

10. Random Good          - Smart randomness
    ./client -ip 127.0.0.1 -port 8080 -username RandomGood -engine 10

11. NNUE Beast           - Pure NNUE evaluation
    ./client -ip 127.0.0.1 -port 8080 -username NNUEBeast -engine 11

12. Opening Master       - Opening book + NNUE
    ./client -ip 127.0.0.1 -port 8080 -username OpeningMaster -engine 12

13. Ultimate AI          - Best combination of all strategies (default)
    ./client -ip 127.0.0.1 -port 8080 -username UltimateAI
    ./client -ip 127.0.0.1 -port 8080 -username UltimateAI -engine 13

14. Hybrid MCTS          - MCTS with NNUE evaluation
    ./client -ip 127.0.0.1 -port 8080 -username HybridMCTS -engine 14

15. Tactical Genius      - Focus on tactical combinations
    ./client -ip 127.0.0.1 -port 8080 -username TacticalGenius -engine 15

16. Strategic Master     - Long-term strategic planning
    ./client -ip 127.0.0.1 -port 8080 -username StrategicMaster -engine 16

17. Endgame God          - Perfect endgame calculation
    ./client -ip 127.0.0.1 -port 8080 -username EndgameGod -engine 17

18. Blitz King           - Ultra-fast decision making
    ./client -ip 127.0.0.1 -port 8080 -username BlitzKing -engine 18

19. Fortress             - Defensive specialist
    ./client -ip 127.0.0.1 -port 8080 -username Fortress -engine 19

20. Assassin             - Aggressive attacker
    ./client -ip 127.0.0.1 -port 8080 -username Assassin -engine 20

Human Mode              - Manual play (disable AI)
    ./client -ip 127.0.0.1 -port 8080 -username Player1 -human

AI Implementation Details:
=========================

## 1. Hybrid Alpha-Beta (ENGINE_HYBRID_ALPHABETA)
- **Algorithm**: Enhanced minimax with alpha-beta pruning
- **Features**:
  - Iterative deepening with aspiration windows
  - Transposition table (22-bit hash, 4-way bucket)
  - Killer move heuristic (2 killers per depth)
  - History heuristic with butterfly table
  - Late Move Reduction (LMR)
  - Null move pruning (R=2-3)
  - Principal Variation Search (PVS)
  - Futility pruning in low depths
- **Depth**: 4-10 plies based on time and position complexity
- **Time Management**: 80-85% of allocated time

## 2. Parallel MCTS (ENGINE_PARALLEL_MCTS)
- **Algorithm**: Monte Carlo Tree Search with Lazy SMP
- **Features**:
  - UCT with exploration constant C=√2
  - RAVE (Rapid Action Value Estimation)
  - Virtual loss for parallel efficiency
  - Progressive bias with heuristic evaluation
  - Node pool allocation (100,000 nodes)
  - AMAF (All Moves As First) statistics
- **Threads**: 4 worker threads
- **Simulations**: Target 100,000+ iterations

## 3. Neural Pattern (ENGINE_NEURAL_PATTERN)
- **Algorithm**: Pattern matching with learned weights
- **Features**:
  - 3x3 pattern recognition
  - Dynamic pattern learning during play
  - Position-based evaluation
  - Capture detection bonus
  - Mobility consideration
- **Pattern Database**: Up to 1000 patterns

## 4. Opening Book (ENGINE_OPENING_BOOK)
- **Algorithm**: Pre-computed opening sequences
- **Features**:
  - Hash-based position lookup
  - Move scoring based on win statistics
  - Center control emphasis
  - Capture prioritization
  - Clone vs Jump preference in early game
- **Depth**: First 20 moves

## 5. Endgame Solver (ENGINE_ENDGAME_SOLVER)
- **Algorithm**: Deep search with perfect play
- **Features**:
  - Variable depth based on empty squares
  - Repetition avoidance
  - Emergency time handling
  - Perfect play with ≤8 empty squares
- **Depth**: 4-8 plies (up to 12 with few pieces)

## 6. Tournament Beast (ENGINE_TOURNAMENT_BEAST)
- **Algorithm**: Opening specialist with NNUE verification
- **Features**:
  - Opening book integration
  - NNUE evaluation for move verification
  - Deep 3-ply lookahead for critical positions
  - Combined traditional and neural evaluation
  - Top-5 candidate move analysis
- **Transition**: Switches to alpha-beta after move 15

## 7-12. [Other engines with specialized strategies]

## 13. Ultimate AI (ENGINE_ULTIMATE_AI) - DEFAULT
- **Algorithm**: Phase-based engine selection
- **Opening (0-19 pieces)**:
  - Tournament Beast with enhanced NNUE guidance
  - Double-check critical moves with deeper NNUE
  - Worst-case analysis for very early game
- **Midgame (28-50 pieces)**:
  - Extended Hybrid MCTS as primary engine
  - Time allocation based on position complexity
  - NNUE verification for late midgame
  - Tournament Beast fallback on MCTS failure
- **Endgame (51+ pieces)**:
  - Pure Endgame God for ≤3 empty squares
  - MCTS for close material balance
  - Comparison between MCTS and Endgame God choices
  - Final NNUE sanity check for critical moves
- **Safety Features**:
  - Timeout protection
  - Move validation
  - Engine fallback mechanisms

## 14. Hybrid MCTS (ENGINE_HYBRID_MCTS)
- **Algorithm**: MCTS with neural network guidance
- **Features**:
  - NNUE evaluation in tree policy
  - Reduced simulation count
  - Smart node expansion
  - Conservative time allocation (70-80%)
  - Single-threaded warmup phase
- **Minimum Requirements**: 5+ valid moves, 0.5s+ time

## 17. Endgame God (ENGINE_ENDGAME_GOD)
- **Algorithm**: Perfect endgame solver
- **Features**:
  - Handles single empty square perfectly
  - Iterative deepening with time control
  - Aspiration windows for efficiency
  - Depth based on empty squares (4-12 plies)
  - Enhanced time allocation (+10-20%)
- **Specialization**: Optimal for <15 empty squares

NNUE Integration:
================
- **Network Structure**: 192-64-32-1 (compact design)
- **Input Encoding**: 3 planes (Red, Blue, Empty)
- **Weights**: Quantized to int16 for efficiency
- **Scale Factor**: 127 for weight quantization
- **Output Range**: [-10000, 10000] centipawns
- **Integration Points**:
  - Direct evaluation in NNUE Beast
  - Move verification in Tournament Beast
  - Hybrid evaluation in Ultimate AI
  - Tree policy guidance in Hybrid MCTS

Execution Examples:
==================
# Default AI (Ultimate AI )
./client -ip 127.0.0.1 -port 8080 -username DefaultAI

# Train NNUE weights
python3 train_octoflip.py

Human Mode Controls:
===================
- Move format: sx sy tx ty (1-indexed, e.g., "1 1 2 2")
- Pass turn: 0 0 0 0
- Quit game: type "quit" or "exit"

Move Generation:
===============
The client implements automatic move generation through the getAIMove() 
function that analyzes the current board state and returns optimal moves 
within the 5-second timeout. Time allocation varies by game phase:
- Opening: 80% of time limit
- Midgame: 85% of time limit  
- Endgame: 70-80% of time limit
- Emergency buffer: 0.3 seconds reserved

Network Protocol:
================
- TCP sockets with JSON payloads
- Messages delimited with '\n'
- Supports register, move, game_start, your_turn, game_over messages
- Partial message handling with buffering

Implementation Features:
=======================
- Automatic move generation with 20 AI algorithms
- Multi-threaded support (up to 8 threads)
- Transposition tables (4MB default)
- Time management system
- Human vs AI support
- Server-side game logging
- Pass move handling
- Memory-safe implementation
- NNUE neural network evaluation
- Opening book database
- Move repetition detection
- Contempt factor support

Advanced Features:
=================
- **Memory Management**: 
  - Node pool allocation for MCTS
  - Transposition table with age-based replacement
  - History table compression
- **Search Enhancements**:
  - Killer moves (2 per ply)
  - History heuristic with overflow handling
  - Butterfly table for move ordering
  - Late move reduction (depth/move based)
- **NNUE Features**:
  - Incremental updates (future enhancement)
  - King-relative encoding (future enhancement)
  - Efficiently updatable evaluation
- **Time Control**:
  - Iterative deepening with early termination
  - Soft/hard time limits
  - Branching factor estimation
  - Emergency move generation

Files Included:
==============
- server.c                  : Game server implementation
- client.c                  : Client with 20 AI engines
- cJSON.c/h                : JSON parsing library
- nnue_weights_generated.h  : Pre-trained NNUE weights (auto-generated)
- opening_book_data.h      : Opening book database (auto-generated)
- train_octoflip.py        : NNUE training system v4.0
- training_stats.json      : NNUE training statistics
- read_pickle_weights.py   : Legacy NNUE training system v2.2
- Makefile                : Build automation (macOS/Linux compatible)
- readme.txt              : This file

NNUE Training System:
====================
The train_octoflip.py implements a complete NNUE training pipeline:

## Features:
- **Architecture**: 192-64-32-1 compact NNUE
- **Training Algorithm**: Self-play with evolutionary approach
- **Population**: 6 networks competing per generation
- **Loss Function**: Huber loss for outlier robustness
- **Optimizer**: Adam with aggressive gradient clipping
- **Regularization**: L2 weight decay + dropout simulation

## Training Process:
1. **Self-play tournament**: Networks compete against each other
2. **Diverse opponents**: Games vs Random, Greedy, Positional AIs
3. **Data collection**: Positions from games with temporal difference values
4. **Network training**: Best performers train on collected data
5. **Evolution**: Top networks survive, others mutate from parents
6. **Validation**: Regular testing against baseline AIs

## Usage:
```bash
# Quick training (1 hour)
python3 train_octoflip.py
# Select option 1

# Standard training (10 hours) - Recommended
python3 train_octoflip.py
# Select option 2

# Extended training (24 hours) - Best quality
python3 train_octoflip.py
# Select option 3

# Test existing weights
python3 train_octoflip.py
# Select option 5