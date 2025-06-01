#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <time.h>
#include <math.h>
#include <limits.h>
#include <sys/time.h>
#include <stdatomic.h>
#include <errno.h>
#include <stdbool.h>
#include <sched.h>
#include "cJSON.h"

// Platform compatibility
#ifdef _WIN32
    #include <windows.h>
    #include <conio.h>
    #define CLOCK_MONOTONIC 0
    void clock_gettime(int dummy, struct timespec* ts) {
        LARGE_INTEGER frequency, counter;
        QueryPerformanceFrequency(&frequency);
        QueryPerformanceCounter(&counter);
        ts->tv_sec = counter.QuadPart / frequency.QuadPart;
        ts->tv_nsec = (counter.QuadPart % frequency.QuadPart) * 1000000000 / frequency.QuadPart;
    }
    #define sysconf(x) 4
    #define _SC_NPROCESSORS_ONLN 0
#else
    #include <unistd.h>
    #include <sys/select.h>
    #include <fcntl.h>
    #include <termios.h>
#endif

// ===== GENERATED AI DATA =====
#ifdef HAS_OPENING_BOOK
#include "opening_book_data.h"
#endif

#ifdef HAS_NNUE_WEIGHTS  
#include "nnue_weights_generated.h"
#endif

// ===== GAME CONSTANTS =====
#define BOARD_SIZE 8
#define RED 'R'
#define BLUE 'B'
#define EMPTY '.'
#define BLOCKED '#'
#define RED_TURN 0
#define BLUE_TURN 1
#define CLONE 1
#define JUMP 2
#define PASS 0

// ===== AI CONSTANTS =====
#define AI_MAX_DEPTH 20
#define AI_INFINITY 1000000
#define AI_NEG_INFINITY -1000000
#define MAX_MOVES 200
#define TIME_LIMIT_S 4.5
#define BUFFER_SIZE 4096
#define CONTEMPT_FACTOR 10

// ===== ENHANCED HASH TABLE =====
#define HASH_SIZE (1 << 22)
#define HASH_MASK (HASH_SIZE - 1)
#define BUCKET_SIZE 4

// ===== MCTS CONSTANTS =====
#define MCTS_ITERATIONS 50000  // Reduced from 100000
#define MCTS_C 1.41421356237
#define MCTS_RAVE_K 1000
#define MCTS_VIRTUAL_LOSS 3
#define MCTS_EXPANSION_THRESHOLD 20  // Reduced from 40 for faster expansion
#define MCTS_NODE_POOL_SIZE 50000
#define MCTS_TIME_CHECK_INTERVAL 100
#define MCTS_MIN_ITERATIONS 1000

// ===== MULTITHREADING =====
#define MAX_THREADS 8
#define LAZY_SMP_THREADS 4  
#define MIN_SPLIT_DEPTH 4

// ===== NEURAL NETWORK PATTERNS =====
#define MAX_PATTERNS 1000
#define PATTERN_SIZE 3

// ===== OPENING BOOK =====
#define MAX_BOOK_MOVES 10
#define BOOK_DEPTH 8
#define OPENING_BOOK_BONUS 1000
#define OPENING_MAX_DEPTH 5
#define OPENING_EVAL_LIMIT 15

// Type definitions
typedef struct {
    unsigned char r1, c1, r2, c2;
    short score;
    unsigned char moveType;
} Move;

typedef struct {
    uint32_t hash;
    uint8_t moves[5][4];
    uint8_t scores[5];
    uint8_t count;
} OpeningEntry;

// Position weights for different game phases
static const double POSITION_WEIGHTS[3][8][8] = {
    // Opening (0-20 pieces): Focus on center control and mobility
    {
        {0.50, -0.30, -0.10,  0.00,  0.00, -0.10, -0.30,  0.50},
        {-0.30, -0.40, -0.20, -0.10, -0.10, -0.20, -0.40, -0.30},
        {-0.10, -0.20,  0.20,  0.15,  0.15,  0.20, -0.20, -0.10},
        {0.00, -0.10,  0.15,  0.25,  0.25,  0.15, -0.10,  0.00},
        {0.00, -0.10,  0.15,  0.25,  0.25,  0.15, -0.10,  0.00},
        {-0.10, -0.20,  0.20,  0.15,  0.15,  0.20, -0.20, -0.10},
        {-0.30, -0.40, -0.20, -0.10, -0.10, -0.20, -0.40, -0.30},
        {0.50, -0.30, -0.10,  0.00,  0.00, -0.10, -0.30,  0.50}
    },
    // Midgame (21-40 pieces): Balanced control
    {
        {0.30, -0.20,  0.10,  0.05,  0.05,  0.10, -0.20,  0.30},
        {-0.20, -0.30,  0.00,  0.00,  0.00,  0.00, -0.30, -0.20},
        {0.10,  0.00,  0.15,  0.10,  0.10,  0.15,  0.00,  0.10},
        {0.05,  0.00,  0.10,  0.05,  0.05,  0.10,  0.00,  0.05},
        {0.05,  0.00,  0.10,  0.05,  0.05,  0.10,  0.00,  0.05},
        {0.10,  0.00,  0.15,  0.10,  0.10,  0.15,  0.00,  0.10},
        {-0.20, -0.30,  0.00,  0.00,  0.00,  0.00, -0.30, -0.20},
        {0.30, -0.20,  0.10,  0.05,  0.05,  0.10, -0.20,  0.30}
    },
    // Endgame (41+ pieces): Maximize territory
    {
        {0.50,  0.40,  0.30,  0.20,  0.20,  0.30,  0.40,  0.50},
        {0.40,  0.30,  0.20,  0.10,  0.10,  0.20,  0.30,  0.40},
        {0.30,  0.20,  0.10,  0.05,  0.05,  0.10,  0.20,  0.30},
        {0.20,  0.10,  0.05,  0.00,  0.00,  0.05,  0.10,  0.20},
        {0.20,  0.10,  0.05,  0.00,  0.00,  0.05,  0.10,  0.20},
        {0.30,  0.20,  0.10,  0.05,  0.05,  0.10,  0.20,  0.30},
        {0.40,  0.30,  0.20,  0.10,  0.10,  0.20,  0.30,  0.40},
        {0.50,  0.40,  0.30,  0.20,  0.20,  0.30,  0.40,  0.50}
    }
};

// Pattern recognition structure
typedef struct {
    char pattern[PATTERN_SIZE][PATTERN_SIZE];
    double score;
    int frequency;
} Pattern;

// Transposition Table
typedef struct {
    unsigned long long hash;
    int score;
    unsigned char depth;
    unsigned char flag;
    Move bestMove;
    unsigned char age;
    unsigned char moveCount;
} TTEntry;

typedef struct {
    TTEntry entries[BUCKET_SIZE];
} TTBucket;

// MCTS Node
typedef struct MCTSNode {
    Move move;
    atomic_int visits;
    atomic_int virtualLoss;
    double score;
    double raveScore;
    atomic_int raveVisits;
    unsigned char player;
    struct MCTSNode* parent;
    struct MCTSNode** children;
    unsigned short childCount;
    unsigned short childCapacity;
    double priorScore;
    pthread_mutex_t lock;
    double heuristicScore;
    int fullyExpanded;
    double amafScore[BOARD_SIZE][BOARD_SIZE][BOARD_SIZE][BOARD_SIZE];
    int amafVisits[BOARD_SIZE][BOARD_SIZE][BOARD_SIZE][BOARD_SIZE];
} MCTSNode;

// Thread pool
typedef struct {
    pthread_t thread;
    int threadId;
    atomic_int active;
    void* (*function)(void*);
    void* argument;
} ThreadPoolWorker;

typedef struct {
    ThreadPoolWorker workers[MAX_THREADS];
    pthread_mutex_t queueMutex;
    pthread_cond_t workAvailable;
    atomic_int shutdown;
    int numThreads;
} ThreadPool;

// MCTS Worker Data
typedef struct {
    MCTSNode* root;
    char board[BOARD_SIZE][BOARD_SIZE];
    int playerId;
    atomic_int* iterationCount;
    double timeLimit;
    double startTime;
} MCTSWorkerData;

// AI Engine types
typedef enum {
    ENGINE_HYBRID_ALPHABETA = 1,
    ENGINE_PARALLEL_MCTS = 2,
    ENGINE_NEURAL_PATTERN = 3,
    ENGINE_OPENING_BOOK = 4,
    ENGINE_ENDGAME_SOLVER = 5,
    ENGINE_TOURNAMENT_BEAST = 6,
    ENGINE_ADAPTIVE_TIME = 7,
    ENGINE_LEARNING_ENGINE = 8,
    ENGINE_HUMAN_STYLE = 9,
    ENGINE_RANDOM_GOOD = 10,
    ENGINE_NNUE_BEAST = 11,
    ENGINE_OPENING_MASTER = 12,
    ENGINE_ULTIMATE_AI = 13,
    ENGINE_HYBRID_MCTS = 14,
    ENGINE_TACTICAL_GENIUS = 15,
    ENGINE_STRATEGIC_MASTER = 16,
    ENGINE_ENDGAME_GOD = 17,
    ENGINE_BLITZ_KING = 18,
    ENGINE_FORTRESS = 19,
    ENGINE_ASSASSIN = 20
} AIEngineType;

// Game state
typedef struct {
    char board[BOARD_SIZE][BOARD_SIZE];
    int currentPlayer;
    int moveCount;
    int lastMoveWasPass;
    double timeRemaining;
    int gamePhase;
} GameState;

// Global variables
static int sockfd = -1;
static char my_username[256];
static char my_color = '\0';
static GameState gameState;
static pthread_mutex_t gameMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t printMutex = PTHREAD_MUTEX_INITIALIZER;
static atomic_int gameStarted = 0;
static atomic_int myTurn = 0;
static atomic_int gameOver = 0;
static AIEngineType aiEngine = ENGINE_ULTIMATE_AI;
static int humanMode = 0;

// AI optimization globals
static struct timespec searchStart;
static atomic_int timeUp = 0;
static TTBucket* transpositionTable = NULL;
static unsigned long long zobristTable[BOARD_SIZE][BOARD_SIZE][4];
static Move killerMoves[AI_MAX_DEPTH][2];
static int historyTable[BOARD_SIZE][BOARD_SIZE][BOARD_SIZE][BOARD_SIZE];
static int butterflyTable[BOARD_SIZE][BOARD_SIZE][BOARD_SIZE][BOARD_SIZE];
static atomic_long nodeCount;
static double timeAllocated = 0.0;
static int currentAge = 0;
static ThreadPool* globalThreadPool = NULL;
static Pattern learnedPatterns[MAX_PATTERNS];
static int patternCount = 0;

// MCTS globals
static MCTSNode* nodePool = NULL;
static atomic_int nodePoolIndex = 0;
static pthread_mutex_t nodePoolMutex = PTHREAD_MUTEX_INITIALIZER;

// Late Move Reduction table
static int lmrTable[64][64];

// Recent moves tracking
static Move lastMoves[3] = {0};

// Function prototypes
void safePrint(const char* format, ...);
void sendJSON(cJSON* json);
void sendRegister(const char* username);
void sendMove(int sx, int sy, int tx, int ty);
void handleServerMessage(const char* message);
void* receiveMessages(void* arg);
void updateBoardFromJSON(cJSON* board_array);
void printBoard();
void cleanup();
void sigint_handler(int sig);

// AI function prototypes
void initializeAISystem();
void cleanupAISystem();
Move getAIMove();
Move getHumanMove();
int isValidMove(int sx, int sy, int tx, int ty);
void makeMove(char board[BOARD_SIZE][BOARD_SIZE], Move move);
void getAllValidMoves(char board[BOARD_SIZE][BOARD_SIZE], int currentPlayer, Move* moves, int* moveCount);
double evaluateBoard(char board[BOARD_SIZE][BOARD_SIZE], int forPlayer, int depth);
double evaluateEndgame(char board[BOARD_SIZE][BOARD_SIZE], int forPlayer);
int countPieces(char board[BOARD_SIZE][BOARD_SIZE], char piece);
double elapsedSeconds();
void initZobrist();
unsigned long long computeHash(char board[BOARD_SIZE][BOARD_SIZE]);
void initLMRTable();
void initThreadPool();
void destroyThreadPool();
int getGamePhase(char board[BOARD_SIZE][BOARD_SIZE]);
int getMoveType(int sx, int sy, int tx, int ty);

// Main engine functions
Move getHybridAlphaBetaMove(char board[BOARD_SIZE][BOARD_SIZE], int currentPlayer);
Move getParallelMCTSMove(char board[BOARD_SIZE][BOARD_SIZE], int currentPlayer);
Move getNeuralPatternMove(char board[BOARD_SIZE][BOARD_SIZE], int currentPlayer);
Move getOpeningBookMove(char board[BOARD_SIZE][BOARD_SIZE], int currentPlayer);
Move getEndgameSolverMove(char board[BOARD_SIZE][BOARD_SIZE], int currentPlayer);
Move getTournamentBeastMove(char board[BOARD_SIZE][BOARD_SIZE], int currentPlayer);
Move getUltimateAIMove(char board[BOARD_SIZE][BOARD_SIZE], int currentPlayer);
Move getHybridMCTSMove(char board[BOARD_SIZE][BOARD_SIZE], int currentPlayer);
Move getEndgameGodMove(char board[BOARD_SIZE][BOARD_SIZE], int currentPlayer);

// Helper functions
static uint32_t compute_board_hash(char board[BOARD_SIZE][BOARD_SIZE]);
static Move search_opening_book(char board[BOARD_SIZE][BOARD_SIZE]);
static double evaluateOpeningMove(char board[BOARD_SIZE][BOARD_SIZE], Move move, int currentPlayer);
int enhancedAlphaBeta(char board[BOARD_SIZE][BOARD_SIZE], int depth, int alpha, int beta, 
                     int currentPlayer, Move* bestMove, int pvNode, int canNull);

// MCTS functions
MCTSNode* createMCTSNode(Move move, int player, MCTSNode* parent);
void freeMCTSTree(MCTSNode* root);
double UCB1_RAVE(MCTSNode* node, int parentVisits);
MCTSNode* selectBestChild(MCTSNode* node, double c);
MCTSNode* treePolicy(MCTSNode* node, char board[BOARD_SIZE][BOARD_SIZE]);
void expand(MCTSNode* node, char board[BOARD_SIZE][BOARD_SIZE]);
double simulate(char board[BOARD_SIZE][BOARD_SIZE], int startingPlayer);
void backpropagate(MCTSNode* node, double result, Move* moveSequence, int sequenceLength);
Move getMCTSBestMove(MCTSNode* root);
void* mctsWorker(void* arg);
void mctsWorkerSingle(MCTSWorkerData* data);

// Pattern and evaluation functions
void learnPattern(char board[BOARD_SIZE][BOARD_SIZE], Move move, double outcome);
double evaluatePatterns(char board[BOARD_SIZE][BOARD_SIZE], int forPlayer);
double evaluateMobility(char board[BOARD_SIZE][BOARD_SIZE], int forPlayer);
double evaluateStability(char board[BOARD_SIZE][BOARD_SIZE], int forPlayer);
double evaluateFrontier(char board[BOARD_SIZE][BOARD_SIZE], int forPlayer);
double evaluatePotential(char board[BOARD_SIZE][BOARD_SIZE], int forPlayer);

// NNUE functions
#ifdef HAS_NNUE_WEIGHTS
static int nnue_evaluate(char board[BOARD_SIZE][BOARD_SIZE], int forPlayer);
#endif

// ===== IMPLEMENTATION =====

void safePrint(const char* format, ...) {
    pthread_mutex_lock(&printMutex);
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    fflush(stdout);
    pthread_mutex_unlock(&printMutex);
}

void initZobrist() {
    srand(time(NULL));
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            for (int k = 0; k < 4; k++) {
                zobristTable[i][j][k] = ((unsigned long long)rand() << 48) |
                                        ((unsigned long long)rand() << 32) |
                                        ((unsigned long long)rand() << 16) |
                                        ((unsigned long long)rand());
            }
        }
    }
}

unsigned long long computeHash(char board[BOARD_SIZE][BOARD_SIZE]) {
    unsigned long long hash = 0;
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            int piece;
            switch (board[i][j]) {
                case RED: piece = 0; break;
                case BLUE: piece = 1; break;
                case EMPTY: piece = 2; break;
                case BLOCKED: piece = 3; break;
                default: piece = 2;
            }
            hash ^= zobristTable[i][j][piece];
        }
    }
    return hash;
}

void initLMRTable() {
    for (int depth = 0; depth < 64; depth++) {
        for (int moves = 0; moves < 64; moves++) {
            if (depth < 3 || moves < 3) {
                lmrTable[depth][moves] = 0;
            } else {
                lmrTable[depth][moves] = (int)(0.5 + log(depth) * log(moves) / 2.5);
                if (lmrTable[depth][moves] > depth - 2) {
                    lmrTable[depth][moves] = depth - 2;
                }
            }
        }
    }
}

void initThreadPool() {
    globalThreadPool = (ThreadPool*)malloc(sizeof(ThreadPool));
    if (!globalThreadPool) return;
    
    pthread_mutex_init(&globalThreadPool->queueMutex, NULL);
    pthread_cond_init(&globalThreadPool->workAvailable, NULL);
    atomic_store(&globalThreadPool->shutdown, 0);
    
    #ifdef _WIN32
        SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);
        globalThreadPool->numThreads = sysinfo.dwNumberOfProcessors;
    #else
        globalThreadPool->numThreads = sysconf(_SC_NPROCESSORS_ONLN);
    #endif
    
    if (globalThreadPool->numThreads > MAX_THREADS) {
        globalThreadPool->numThreads = MAX_THREADS;
    }
}

void destroyThreadPool() {
    if (!globalThreadPool) return;
    
    atomic_store(&globalThreadPool->shutdown, 1);
    pthread_cond_broadcast(&globalThreadPool->workAvailable);
    
    for (int i = 0; i < globalThreadPool->numThreads; i++) {
        if (atomic_load(&globalThreadPool->workers[i].active)) {
            pthread_join(globalThreadPool->workers[i].thread, NULL);
        }
    }
    
    pthread_mutex_destroy(&globalThreadPool->queueMutex);
    pthread_cond_destroy(&globalThreadPool->workAvailable);
    free(globalThreadPool);
    globalThreadPool = NULL;
}

double elapsedSeconds() {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    
    // searchStart가 초기화되지 않았으면 0 반환
    if (searchStart.tv_sec == 0 && searchStart.tv_nsec == 0) {
        return 0.0;
    }
    
    double elapsed = (now.tv_sec - searchStart.tv_sec) + 
                    (now.tv_nsec - searchStart.tv_nsec) / 1000000000.0;
    
    // 비정상적인 값 방지
    if (elapsed < 0) {
        return 0.0;  // 음수면 0 반환
    }
    
    if (elapsed > 10.0) {
        // 10초 이상이면 문제가 있는 것이므로 안전한 값 반환
        return 5.0;  // TIME_LIMIT_S와 비슷한 값
    }
    
    return elapsed;
}

int getGamePhase(char board[BOARD_SIZE][BOARD_SIZE]) {
    int totalPieces = countPieces(board, RED) + countPieces(board, BLUE);
    if (totalPieces < 20) return 0;  // Opening
    if (totalPieces < 40) return 1;  // Midgame
    return 2;  // Endgame
}

int countPieces(char board[BOARD_SIZE][BOARD_SIZE], char piece) {
    int count = 0;
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board[i][j] == piece) count++;
        }
    }
    return count;
}

int getMoveType(int sx, int sy, int tx, int ty) {
    int dx = abs(tx - sx);
    int dy = abs(ty - sy);
    
    if (dx == 0 && dy == 0) return -1;
    if (dx <= 1 && dy <= 1) return CLONE;
    if ((dx == 2 && dy == 0) || (dx == 0 && dy == 2) || (dx == 2 && dy == 2)) return JUMP;
    
    return -1;
}

int isValidMove(int sx, int sy, int tx, int ty) {
    if (sx < 0 || sx >= BOARD_SIZE || sy < 0 || sy >= BOARD_SIZE ||
        tx < 0 || tx >= BOARD_SIZE || ty < 0 || ty >= BOARD_SIZE) {
        return 0;
    }
    
    if (gameState.board[sx][sy] != my_color) return 0;
    if (gameState.board[tx][ty] != EMPTY) return 0;
    
    return getMoveType(sx, sy, tx, ty) > 0;
}

void makeMove(char board[BOARD_SIZE][BOARD_SIZE], Move move) {
    char playerPiece = (gameState.currentPlayer == RED_TURN) ? RED : BLUE;
    char opponentPiece = (gameState.currentPlayer == RED_TURN) ? BLUE : RED;
    
    board[move.r2][move.c2] = playerPiece;
    
    if (move.moveType == JUMP) {
        board[move.r1][move.c1] = EMPTY;
    }
    
    // Flip adjacent pieces
    for (int dr = -1; dr <= 1; dr++) {
        for (int dc = -1; dc <= 1; dc++) {
            if (dr == 0 && dc == 0) continue;
            
            int nr = move.r2 + dr;
            int nc = move.c2 + dc;
            
            if (nr >= 0 && nr < BOARD_SIZE && nc >= 0 && nc < BOARD_SIZE) {
                if (board[nr][nc] == opponentPiece) {
                    board[nr][nc] = playerPiece;
                }
            }
        }
    }
}

void getAllValidMoves(char board[BOARD_SIZE][BOARD_SIZE], int currentPlayer, Move* moves, int* moveCount) {
    char playerPiece = (currentPlayer == RED_TURN) ? RED : BLUE;
    *moveCount = 0;
    
    for (int r1 = 0; r1 < BOARD_SIZE; r1++) {
        for (int c1 = 0; c1 < BOARD_SIZE; c1++) {
            if (board[r1][c1] != playerPiece) continue;
            
            // Clone moves
            for (int dr = -1; dr <= 1; dr++) {
                for (int dc = -1; dc <= 1; dc++) {
                    if (dr == 0 && dc == 0) continue;
                    
                    int r2 = r1 + dr;
                    int c2 = c1 + dc;
                    
                    if (r2 >= 0 && r2 < BOARD_SIZE && c2 >= 0 && c2 < BOARD_SIZE) {
                        if (board[r2][c2] == EMPTY) {
                            if (*moveCount < MAX_MOVES) {
                                moves[*moveCount].r1 = r1;
                                moves[*moveCount].c1 = c1;
                                moves[*moveCount].r2 = r2;
                                moves[*moveCount].c2 = c2;
                                moves[*moveCount].moveType = CLONE;
                                moves[*moveCount].score = 0;
                                (*moveCount)++;
                            }
                        }
                    }
                }
            }
            
            // Jump moves
            int jumpDirs[8][2] = {{-2,0}, {2,0}, {0,-2}, {0,2}, {-2,-2}, {-2,2}, {2,-2}, {2,2}};
            for (int i = 0; i < 8; i++) {
                int r2 = r1 + jumpDirs[i][0];
                int c2 = c1 + jumpDirs[i][1];
                
                if (r2 >= 0 && r2 < BOARD_SIZE && c2 >= 0 && c2 < BOARD_SIZE) {
                    if (board[r2][c2] == EMPTY) {
                        if (*moveCount < MAX_MOVES) {
                            moves[*moveCount].r1 = r1;
                            moves[*moveCount].c1 = c1;
                            moves[*moveCount].r2 = r2;
                            moves[*moveCount].c2 = c2;
                            moves[*moveCount].moveType = JUMP;
                            moves[*moveCount].score = 0;
                            (*moveCount)++;
                        }
                    }
                }
            }
        }
    }
}

// Enhanced evaluation function
double evaluateBoard(char board[BOARD_SIZE][BOARD_SIZE], int forPlayer, int depth) {
    int redCount = countPieces(board, RED);
    int blueCount = countPieces(board, BLUE);
    
    if (redCount == 0) {
        return (forPlayer == BLUE_TURN) ? AI_INFINITY - depth : -AI_INFINITY + depth;
    }
    if (blueCount == 0) {
        return (forPlayer == RED_TURN) ? AI_INFINITY - depth : -AI_INFINITY + depth;
    }
    
    double score = 0.0;
    int emptyCount = countPieces(board, EMPTY);
    
    // Material score
    int pieceDiff = (forPlayer == RED_TURN) ? (redCount - blueCount) : (blueCount - redCount);
    double materialWeight = 25.0;
    if (emptyCount < 10) materialWeight = 40.0;
    else if (emptyCount < 20) materialWeight = 30.0;
    
    score += pieceDiff * materialWeight;
    
    // Position evaluation
    char playerPiece = (forPlayer == RED_TURN) ? RED : BLUE;
    char opponentPiece = (forPlayer == RED_TURN) ? BLUE : RED;
    int gamePhase = getGamePhase(board);
    
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board[i][j] == playerPiece) {
                score += POSITION_WEIGHTS[gamePhase][i][j] * 60.0;
                
                // Safety evaluation - check if piece can be captured
                int captureRisk = 0;
                for (int r = 0; r < BOARD_SIZE; r++) {
                    for (int c = 0; c < BOARD_SIZE; c++) {
                        if (board[r][c] == opponentPiece) {
                            int dr = abs(i - r);
                            int dc = abs(j - c);
                            if (dr <= 1 && dc <= 1 && dr + dc > 0) {
                                for (int dr2 = -1; dr2 <= 1; dr2++) {
                                    for (int dc2 = -1; dc2 <= 1; dc2++) {
                                        if (dr2 == 0 && dc2 == 0) continue;
                                        int nr = r + dr2;
                                        int nc = c + dc2;
                                        if (nr >= 0 && nr < BOARD_SIZE && nc >= 0 && nc < BOARD_SIZE &&
                                            board[nr][nc] == EMPTY) {
                                            if (abs(nr - i) <= 1 && abs(nc - j) <= 1) {
                                                captureRisk = 1;
                                                break;
                                            }
                                        }
                                    }
                                    if (captureRisk) break;
                                }
                            }
                            
                            if (!captureRisk && dr <= 2 && dc <= 2 && 
                                (dr == 2 || dc == 2 || (dr == 2 && dc == 2))) {
                                for (int jr = -2; jr <= 2; jr += 2) {
                                    for (int jc = -2; jc <= 2; jc += 2) {
                                        if (jr == 0 && jc == 0) continue;
                                        int nr = r + jr;
                                        int nc = c + jc;
                                        if (nr >= 0 && nr < BOARD_SIZE && nc >= 0 && nc < BOARD_SIZE &&
                                            board[nr][nc] == EMPTY) {
                                            if (abs(nr - i) <= 1 && abs(nc - j) <= 1) {
                                                captureRisk = 1;
                                                break;
                                            }
                                        }
                                    }
                                    if (captureRisk) break;
                                }
                            }
                            
                            if (captureRisk) break;
                        }
                    }
                    if (captureRisk) break;
                }
                
                if (captureRisk) {
                    score -= 150.0;
                    if ((forPlayer == RED_TURN ? redCount : blueCount) <= 2) {
                        score -= 200.0;
                    }
                }
                
                // Stability
                int friendly = 0;
                int threats = 0;
                for (int dr = -1; dr <= 1; dr++) {
                    for (int dc = -1; dc <= 1; dc++) {
                        if (dr == 0 && dc == 0) continue;
                        int nr = i + dr, nc = j + dc;
                        if (nr >= 0 && nr < BOARD_SIZE && nc >= 0 && nc < BOARD_SIZE) {
                            if (board[nr][nc] == playerPiece) {
                                friendly++;
                            } else if (board[nr][nc] == opponentPiece) {
                                threats++;
                            }
                        }
                    }
                }
                
                score += friendly * 8.0;
                score -= threats * 10.0;
            }
        }
    }
    
    // Mobility
    if (emptyCount > 15) {
        Move tempMoves[MAX_MOVES];
        int playerMoves, opponentMoves;
        getAllValidMoves(board, forPlayer, tempMoves, &playerMoves);
        getAllValidMoves(board, !forPlayer, tempMoves, &opponentMoves);
        
        if (playerMoves + opponentMoves > 0) {
            double mobilityScore = 100.0 * (playerMoves - opponentMoves) / (playerMoves + opponentMoves);
            score += mobilityScore * 3.0;
        }
        
        if (playerMoves == 0 && opponentMoves > 0) {
            score -= 50.0;
        }
    }
    
    // Capture potential
    int captureThreats = 0;
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board[i][j] == EMPTY) {
                int potentialCaptures = 0;
                for (int dr = -1; dr <= 1; dr++) {
                    for (int dc = -1; dc <= 1; dc++) {
                        if (dr == 0 && dc == 0) continue;
                        int nr = i + dr, nc = j + dc;
                        if (nr >= 0 && nr < BOARD_SIZE && nc >= 0 && nc < BOARD_SIZE) {
                            if (board[nr][nc] == opponentPiece) {
                                potentialCaptures++;
                            }
                        }
                    }
                }
                if (potentialCaptures >= 2) {
                    captureThreats++;
                }
            }
        }
    }
    score += captureThreats * 10.0;
    
    // Endgame special evaluation
    if (emptyCount < 15) {
        int corners[4][2] = {{0,0}, {0,7}, {7,0}, {7,7}};
        for (int i = 0; i < 4; i++) {
            if (board[corners[i][0]][corners[i][1]] == playerPiece) {
                score += 50.0;
            } else if (board[corners[i][0]][corners[i][1]] == opponentPiece) {
                score -= 50.0;
            }
        }
    }
    
    return score;
}

static uint32_t compute_board_hash(char board[BOARD_SIZE][BOARD_SIZE]) {
    uint32_t hash = 0;
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            hash = hash * 31 + board[i][j];
        }
    }
    return hash;
}

static Move search_opening_book(char board[BOARD_SIZE][BOARD_SIZE]) {
    Move result = {0, 0, 0, 0, 0, 0};
    
    #ifdef HAS_OPENING_BOOK
    extern const OpeningEntry opening_book[];
    extern const int opening_book_size;
    
    uint32_t hash = compute_board_hash(board);
    
    for (int i = 0; i < opening_book_size; i++) {
        if (opening_book[i].hash == hash) {
            if (opening_book[i].count > 0) {
                result.r1 = opening_book[i].moves[0][0];
                result.c1 = opening_book[i].moves[0][1];
                result.r2 = opening_book[i].moves[0][2];
                result.c2 = opening_book[i].moves[0][3];
                result.score = opening_book[i].scores[0];
                result.moveType = (abs(result.r2 - result.r1) <= 1 && 
                                  abs(result.c2 - result.c1) <= 1) ? CLONE : JUMP;
            }
            break;
        }
    }
    #else
    (void)board;
    #endif
    
    return result;
}

static double evaluateOpeningMove(char board[BOARD_SIZE][BOARD_SIZE], Move move, int currentPlayer) {
    double score = 0.0;
    char playerPiece = (currentPlayer == RED_TURN) ? RED : BLUE;
    char opponentPiece = (currentPlayer == RED_TURN) ? BLUE : RED;
    
    char tempBoard[BOARD_SIZE][BOARD_SIZE];
    memcpy(tempBoard, board, sizeof(tempBoard));
    tempBoard[move.r2][move.c2] = playerPiece;
    if (move.moveType == JUMP) {
        tempBoard[move.r1][move.c1] = EMPTY;
    }
    
    // Immediate captures
    int captures = 0;
    for (int dr = -1; dr <= 1; dr++) {
        for (int dc = -1; dc <= 1; dc++) {
            if (dr == 0 && dc == 0) continue;
            int nr = move.r2 + dr;
            int nc = move.c2 + dc;
            if (nr >= 0 && nr < BOARD_SIZE && nc >= 0 && nc < BOARD_SIZE) {
                if (board[nr][nc] == opponentPiece) {
                    tempBoard[nr][nc] = playerPiece;
                    captures++;
                }
            }
        }
    }
    score += captures * 15.0;
    
    // Position value - CORNERS ARE VALUABLE!
    double posValue = POSITION_WEIGHTS[0][move.r2][move.c2];
    score += posValue * 100.0;
    
    // Corner preservation bonus
    if ((move.r1 == 0 || move.r1 == 7) && (move.c1 == 0 || move.c1 == 7)) {
        if (move.moveType == JUMP) {
            score -= 200.0;  // Huge penalty for leaving corner
        }
    }
    
    // Safety evaluation
    int maxThreat = 0;
    Move oppMoves[MAX_MOVES];
    int oppMoveCount;
    getAllValidMoves(tempBoard, !currentPlayer, oppMoves, &oppMoveCount);
    
    for (int i = 0; i < oppMoveCount && i < 10; i++) {
        int threat = 0;
        for (int dr = -1; dr <= 1; dr++) {
            for (int dc = -1; dc <= 1; dc++) {
                if (dr == 0 && dc == 0) continue;
                int nr = oppMoves[i].r2 + dr;
                int nc = oppMoves[i].c2 + dc;
                if (nr >= 0 && nr < BOARD_SIZE && nc >= 0 && nc < BOARD_SIZE) {
                    if (tempBoard[nr][nc] == playerPiece) threat++;
                }
            }
        }
        if (threat > maxThreat) maxThreat = threat;
    }
    score -= maxThreat * 20.0;
    
    // Connectivity bonus
    int friendly = 0;
    for (int dr = -2; dr <= 2; dr++) {
        for (int dc = -2; dc <= 2; dc++) {
            if (abs(dr) + abs(dc) > 3) continue;
            int nr = move.r2 + dr;
            int nc = move.c2 + dc;
            if (nr >= 0 && nr < BOARD_SIZE && nc >= 0 && nc < BOARD_SIZE) {
                if (tempBoard[nr][nc] == playerPiece) {
                    friendly++;
                    if (abs(dr) <= 1 && abs(dc) <= 1) friendly++;
                }
            }
        }
    }
    score += friendly * 5.0;
    
    // Prefer clone in early game
    int pieceCount = countPieces(board, RED) + countPieces(board, BLUE);
    if (pieceCount < 10 && move.moveType == CLONE) {
        score += 10.0;
    }
    
    return score;
}

// Alpha-Beta implementation
int enhancedAlphaBeta(char board[BOARD_SIZE][BOARD_SIZE], int depth, int alpha, int beta, 
                     int currentPlayer, Move* bestMove, int pvNode, int canNull) {
    atomic_fetch_add(&nodeCount, 1);
    
    if ((atomic_load(&nodeCount) & 127) == 0) {
        double elapsed = elapsedSeconds();
        if (elapsed > timeAllocated * 0.85) {
            atomic_store(&timeUp, 1);
            return evaluateBoard(board, currentPlayer, depth);
        }
        
        if (depth > 4 && elapsed > timeAllocated * 0.6) {
            double timePerNode = elapsed / atomic_load(&nodeCount);
            double estimatedNodes = atomic_load(&nodeCount) * 1.5;
            if (elapsed + (timePerNode * estimatedNodes) > timeAllocated * 0.85) {
                atomic_store(&timeUp, 1);
                return evaluateBoard(board, currentPlayer, depth);
            }
        }
    }
    
    if (atomic_load(&timeUp)) {
        return evaluateBoard(board, currentPlayer, depth);
    }
    
    if (depth == 0 || countPieces(board, EMPTY) == 0 ||
        countPieces(board, RED) == 0 || countPieces(board, BLUE) == 0) {
        return evaluateBoard(board, currentPlayer, depth);
    }
    
    unsigned long long hash = computeHash(board);
    int ttIndex = (hash & HASH_MASK) % HASH_SIZE;
    TTBucket* bucket = &transpositionTable[ttIndex];
    TTEntry* ttEntry = NULL;
    
    for (int i = 0; i < BUCKET_SIZE; i++) {
        if (bucket->entries[i].hash == hash) {
            ttEntry = &bucket->entries[i];
            break;
        }
    }
    
    if (ttEntry && ttEntry->depth >= depth && !pvNode) {
        if (ttEntry->flag == 0) {
            if (bestMove) *bestMove = ttEntry->bestMove;
            return ttEntry->score;
        } else if (ttEntry->flag == 1) {
            alpha = (alpha > ttEntry->score) ? alpha : ttEntry->score;
        } else if (ttEntry->flag == 2) {
            beta = (beta < ttEntry->score) ? beta : ttEntry->score;
        }
        
        if (alpha >= beta) {
            if (bestMove) *bestMove = ttEntry->bestMove;
            return ttEntry->score;
        }
    }
    
    if (canNull && depth >= 3 && !pvNode && 
        countPieces(board, (currentPlayer == RED_TURN) ? RED : BLUE) > 3) {
        
        int R = (depth >= 6) ? 3 : 2;
        int nullScore = -enhancedAlphaBeta(board, depth - R - 1, -beta, -beta + 1, 
                                          !currentPlayer, NULL, 0, 0);
        
        if (nullScore >= beta) {
            return beta;
        }
    }
    
    if (pvNode && depth >= 6 && (!ttEntry || ttEntry->depth < depth - 2)) {
        enhancedAlphaBeta(board, depth - 2, alpha, beta, currentPlayer, bestMove, 1, 1);
    }
    
    Move moves[MAX_MOVES];
    int moveCount;
    getAllValidMoves(board, currentPlayer, moves, &moveCount);
    
    if (moveCount == 0) {
        return -enhancedAlphaBeta(board, depth - 1, -beta, -alpha, !currentPlayer, NULL, pvNode, 1);
    }
    
    // Move ordering
    for (int i = 0; i < moveCount; i++) {
        moves[i].score = 0;
        
        if (ttEntry && moves[i].r1 == ttEntry->bestMove.r1 && 
            moves[i].c1 == ttEntry->bestMove.c1 &&
            moves[i].r2 == ttEntry->bestMove.r2 && 
            moves[i].c2 == ttEntry->bestMove.c2) {
            moves[i].score += 20000;
        }
        
        if (depth < AI_MAX_DEPTH) {
            for (int k = 0; k < 2; k++) {
                if (moves[i].r1 == killerMoves[depth][k].r1 &&
                    moves[i].c1 == killerMoves[depth][k].c1 &&
                    moves[i].r2 == killerMoves[depth][k].r2 &&
                    moves[i].c2 == killerMoves[depth][k].c2) {
                    moves[i].score += 10000;
                    break;
                }
            }
        }
        
        moves[i].score += historyTable[moves[i].r1][moves[i].c1][moves[i].r2][moves[i].c2];
        moves[i].score += butterflyTable[moves[i].r1][moves[i].c1][moves[i].r2][moves[i].c2] / 10;
        
        char opponentPiece = (currentPlayer == RED_TURN) ? BLUE : RED;
        int captures = 0;
        for (int dr = -1; dr <= 1; dr++) {
            for (int dc = -1; dc <= 1; dc++) {
                if (dr == 0 && dc == 0) continue;
                int nr = moves[i].r2 + dr;
                int nc = moves[i].c2 + dc;
                if (nr >= 0 && nr < BOARD_SIZE && nc >= 0 && nc < BOARD_SIZE &&
                    board[nr][nc] == opponentPiece) {
                    captures++;
                }
            }
        }
        moves[i].score += captures * 1000;
        
        moves[i].score += (int)(POSITION_WEIGHTS[getGamePhase(board)][moves[i].r2][moves[i].c2] * 100);
    }
    
    // Sort moves
    for (int i = 0; i < moveCount - 1; i++) {
        for (int j = i + 1; j < moveCount; j++) {
            if (moves[j].score > moves[i].score) {
                Move temp = moves[i];
                moves[i] = moves[j];
                moves[j] = temp;
            }
        }
    }
    
    Move localBestMove = moves[0];
    int bestScore = AI_NEG_INFINITY;
    int flag = 2;
    int searchedMoves = 0;
    int quietMoves = 0;
    
    int maxMoves = moveCount;
    if (elapsedSeconds() > timeAllocated * 0.7) {
        maxMoves = (moveCount < 10) ? moveCount : 10;
    }
    
    for (int i = 0; i < maxMoves && !atomic_load(&timeUp); i++) {
        char newBoard[BOARD_SIZE][BOARD_SIZE];
        memcpy(newBoard, board, sizeof(newBoard));
        makeMove(newBoard, moves[i]);
        
        butterflyTable[moves[i].r1][moves[i].c1][moves[i].r2][moves[i].c2]++;
        
        int score;
        
        int reduction = 0;
        if (depth >= 4 && i >= 4 && !pvNode && moves[i].score < 1000) {
            quietMoves++;
            reduction = lmrTable[depth < 64 ? depth : 63][quietMoves < 64 ? quietMoves : 63];
            
            if (moves[i].score >= 1000) reduction = 0;
        }
        
        if (i == 0) {
            score = -enhancedAlphaBeta(newBoard, depth - 1, -beta, -alpha, 
                                      !currentPlayer, NULL, pvNode, 1);
        } else {
            score = -enhancedAlphaBeta(newBoard, depth - 1 - reduction, -alpha - 1, -alpha, 
                                      !currentPlayer, NULL, 0, 1);
            
            if (score > alpha && (reduction > 0 || score < beta)) {
                score = -enhancedAlphaBeta(newBoard, depth - 1, -beta, -alpha, 
                                          !currentPlayer, NULL, pvNode, 1);
            }
        }
        
        searchedMoves++;
        
        if (score > bestScore) {
            bestScore = score;
            localBestMove = moves[i];
            
            if (score > alpha) {
                alpha = score;
                flag = 0;
                
                if (score >= beta) {
                    if (depth < AI_MAX_DEPTH) {
                        if (moves[i].r1 != killerMoves[depth][0].r1 ||
                            moves[i].c1 != killerMoves[depth][0].c1 ||
                            moves[i].r2 != killerMoves[depth][0].r2 ||
                            moves[i].c2 != killerMoves[depth][0].c2) {
                            killerMoves[depth][1] = killerMoves[depth][0];
                            killerMoves[depth][0] = moves[i];
                        }
                    }
                    
                    if (moves[i].score < 1000) {
                        historyTable[moves[i].r1][moves[i].c1][moves[i].r2][moves[i].c2] += depth * depth;
                        
                        if (historyTable[moves[i].r1][moves[i].c1][moves[i].r2][moves[i].c2] > 10000) {
                            for (int a = 0; a < BOARD_SIZE; a++) {
                                for (int b = 0; b < BOARD_SIZE; b++) {
                                    for (int c = 0; c < BOARD_SIZE; c++) {
                                        for (int d = 0; d < BOARD_SIZE; d++) {
                                            historyTable[a][b][c][d] /= 2;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    
                    flag = 1;
                    break;
                }
            }
        }
        
        if (!pvNode && depth <= 3 && bestScore > -AI_INFINITY/2) {
            int futilityMargin = 200 * depth;
            if (bestScore - futilityMargin >= beta) {
                break;
            }
        }
    }
    
    if (!atomic_load(&timeUp)) {
        int replaceIdx = 0;
        int minDepth = 100;
        
        for (int i = 0; i < BUCKET_SIZE; i++) {
            if (bucket->entries[i].hash == 0 || bucket->entries[i].age != currentAge) {
                replaceIdx = i;
                break;
            }
            if (bucket->entries[i].depth < minDepth) {
                minDepth = bucket->entries[i].depth;
                replaceIdx = i;
            }
        }
        
        bucket->entries[replaceIdx].hash = hash;
        bucket->entries[replaceIdx].score = bestScore;
        bucket->entries[replaceIdx].depth = depth;
        bucket->entries[replaceIdx].flag = flag;
        bucket->entries[replaceIdx].bestMove = localBestMove;
        bucket->entries[replaceIdx].age = currentAge;
        bucket->entries[replaceIdx].moveCount = searchedMoves;
    }
    
    if (bestMove) *bestMove = localBestMove;
    return bestScore;
}

// Tournament Beast - strong opening play with NNUE enhancement
Move getTournamentBeastMove(char board[BOARD_SIZE][BOARD_SIZE], int currentPlayer) {
    int gamePhase = getGamePhase(board);
    int moveNumber = 60 - countPieces(board, EMPTY);
    int emptyCount = countPieces(board, EMPTY);
    
    int myPieces = countPieces(board, (currentPlayer == RED_TURN) ? RED : BLUE);
    int oppPieces = countPieces(board, (currentPlayer == RED_TURN) ? BLUE : RED);
    int materialDiff = myPieces - oppPieces;
    
    safePrint("Tournament Beast + NNUE: phase=%d, move=%d, material_diff=%d, empty=%d\n", 
             gamePhase, moveNumber, materialDiff, emptyCount);
    
    Move moves[MAX_MOVES];
    int moveCount;
    getAllValidMoves(board, currentPlayer, moves, &moveCount);
    
    if (moveCount == 0) {
        return (Move){0, 0, 0, 0, 0, 0};
    }
    
    if (moveCount == 1) {
        safePrint("Only one move available\n");
        return moves[0];
    }
    
    // Early game - try opening book first
    if (moveNumber < 15) {
        #ifdef HAS_OPENING_BOOK
        Move bookMove = search_opening_book(board);
        if (bookMove.r1 != 0 || bookMove.c1 != 0 || 
            bookMove.r2 != 0 || bookMove.c2 != 0) {
            safePrint("Using opening book move\n");
            
            #ifdef HAS_NNUE_WEIGHTS
            // Verify book move with NNUE
            char tempBoard[BOARD_SIZE][BOARD_SIZE];
            memcpy(tempBoard, board, sizeof(tempBoard));
            makeMove(tempBoard, bookMove);
            int nnueScore = nnue_evaluate(tempBoard, currentPlayer);
            
            if (nnueScore < -200) {
                safePrint("NNUE rejects book move (score=%d), finding alternative\n", nnueScore);
            } else {
                safePrint("NNUE approves book move (score=%d)\n", nnueScore);
                return bookMove;
            }
            #else
            return bookMove;
            #endif
        }
        #endif
        
        // Enhanced opening strategy with NNUE guidance
        Move bestMove = moves[0];
        double bestScore = -AI_INFINITY;
        
        #ifdef HAS_NNUE_WEIGHTS
        // Use NNUE to pre-filter and score moves
        safePrint("Using NNUE for opening move evaluation\n");
        
        typedef struct {
            Move move;
            double score;
            int nnueScore;
        } ScoredMove;
        
        ScoredMove scoredMoves[MAX_MOVES];
        int validCount = (moveCount < 30) ? moveCount : 30;
        
        // First pass: NNUE evaluation
        for (int i = 0; i < validCount; i++) {
            char tempBoard[BOARD_SIZE][BOARD_SIZE];
            memcpy(tempBoard, board, sizeof(tempBoard));
            makeMove(tempBoard, moves[i]);
            
            scoredMoves[i].move = moves[i];
            scoredMoves[i].nnueScore = nnue_evaluate(tempBoard, currentPlayer);
            scoredMoves[i].score = evaluateOpeningMove(board, moves[i], currentPlayer);
            
            // Combine NNUE and traditional evaluation
            scoredMoves[i].score += scoredMoves[i].nnueScore * 0.3;
        }
        
        // Sort by combined score
        for (int i = 0; i < validCount - 1; i++) {
            for (int j = i + 1; j < validCount; j++) {
                if (scoredMoves[j].score > scoredMoves[i].score) {
                    ScoredMove temp = scoredMoves[i];
                    scoredMoves[i] = scoredMoves[j];
                    scoredMoves[j] = temp;
                }
            }
        }
        
        // Deep evaluation of top 5 moves
        safePrint("Top 5 opening candidates:\n");
        int topN = (validCount < 5) ? validCount : 5;
        
        for (int i = 0; i < topN; i++) {
            char tempBoard[BOARD_SIZE][BOARD_SIZE];
            memcpy(tempBoard, board, sizeof(tempBoard));
            makeMove(tempBoard, scoredMoves[i].move);
            
            // 3-ply lookahead with NNUE
            Move dummy;
            int deepScore = -enhancedAlphaBeta(tempBoard, 3, -AI_INFINITY, AI_INFINITY,
                                              !currentPlayer, &dummy, 0, 1);
            
            double finalScore = scoredMoves[i].score + deepScore * 0.01;
            
            safePrint("  %d. (%d,%d)->(%d,%d): base=%.1f, nnue=%d, deep=%d, final=%.1f\n",
                     i+1, scoredMoves[i].move.r1+1, scoredMoves[i].move.c1+1,
                     scoredMoves[i].move.r2+1, scoredMoves[i].move.c2+1,
                     scoredMoves[i].score, scoredMoves[i].nnueScore, deepScore, finalScore);
            
            if (finalScore > bestScore) {
                bestScore = finalScore;
                bestMove = scoredMoves[i].move;
            }
        }
        
        #else
        // Fallback to traditional evaluation
        for (int i = 0; i < moveCount && i < 20; i++) {
            double score = evaluateOpeningMove(board, moves[i], currentPlayer);
            
            if (score > bestScore) {
                bestScore = score;
                bestMove = moves[i];
            }
        }
        #endif
        
        safePrint("Selected opening move with score %.1f\n", bestScore);
        return bestMove;
    }
    
    // Use alpha-beta for mid and endgame
    return getHybridAlphaBetaMove(board, currentPlayer);
}

// Hybrid MCTS - for complex midgame positions
Move getHybridMCTSMove(char board[BOARD_SIZE][BOARD_SIZE], int currentPlayer) {
    // searchStart를 함수 시작 시 명확히 초기화
    clock_gettime(CLOCK_MONOTONIC, &searchStart);
    atomic_store(&timeUp, 0);
    atomic_store(&nodeCount, 0);
    
    if (!nodePool) {
        nodePool = (MCTSNode*)calloc(MCTS_NODE_POOL_SIZE, sizeof(MCTSNode));
        if (!nodePool) {
            safePrint("MCTS: Failed to allocate node pool, using fallback\n");
            return getHybridAlphaBetaMove(board, currentPlayer);
        }
    }
    
    pthread_mutex_lock(&nodePoolMutex);
    nodePoolIndex = 0;
    pthread_mutex_unlock(&nodePoolMutex);
    
    Move moves[MAX_MOVES];
    int moveCount;
    getAllValidMoves(board, currentPlayer, moves, &moveCount);
    
    if (moveCount == 0) {
        return (Move){0, 0, 0, 0, 0, 0};
    }
    
    if (moveCount < 5) {
        safePrint("Too few moves for MCTS (%d), using alpha-beta\n", moveCount);
        return getHybridAlphaBetaMove(board, currentPlayer);
    }
    
    // Emergency bailout for very limited time
    if (timeAllocated < 0.5) {
        safePrint("Insufficient time for MCTS (%.2fs), using quick evaluation\n", timeAllocated);
        
        int bestIdx = 0;
        double bestScore = -AI_INFINITY;
        char opponentPiece = (currentPlayer == RED_TURN) ? BLUE : RED;
        
        for (int i = 0; i < moveCount && i < 10; i++) {
            double score = POSITION_WEIGHTS[getGamePhase(board)][moves[i].r2][moves[i].c2] * 20.0;
            
            for (int dr = -1; dr <= 1; dr++) {
                for (int dc = -1; dc <= 1; dc++) {
                    if (dr == 0 && dc == 0) continue;
                    int nr = moves[i].r2 + dr;
                    int nc = moves[i].c2 + dc;
                    if (nr >= 0 && nr < BOARD_SIZE && nc >= 0 && nc < BOARD_SIZE &&
                        board[nr][nc] == opponentPiece) {
                        score += 30.0;
                    }
                }
            }
            
            if (score > bestScore) {
                bestScore = score;
                bestIdx = i;
            }
        }
        
        return moves[bestIdx];
    }
    
    int savedCurrentPlayer = gameState.currentPlayer;
    gameState.currentPlayer = currentPlayer;
    
    MCTSNode* root = createMCTSNode((Move){0, 0, 0, 0, 0, 0}, !currentPlayer, NULL);
    if (!root) {
        gameState.currentPlayer = savedCurrentPlayer;
        safePrint("Failed to create root node\n");
        return moves[0];
    }
    
    // ===== 안전한 스레드 관리 =====
    
    // 보수적인 시간 할당
    double allocatedTime = timeAllocated * 0.6;  // 기존대로 60% 사용
    
    // 스레드 수 결정 - 시간이 충분할 때만 많은 스레드 사용
    int actualThreads = 2;  // 기본값 1
    
    if (timeAllocated >= 3.0 && moveCount < 30) {
        actualThreads = LAZY_SMP_THREADS;  // 시간이 충분하고 복잡도가 낮을 때만
    } else if (timeAllocated >= 2.0 && moveCount < 40) {
        actualThreads = (LAZY_SMP_THREADS > 2) ? 2 : LAZY_SMP_THREADS;
    } else {
        actualThreads = 2;  // 그 외에는 단일 스레드
    }
    
    // 복잡한 포지션에서는 시간 더 줄이기 (기존 로직 유지)
    if (moveCount > 35) {
        allocatedTime *= 0.7;
        actualThreads = 2;  // 복잡하면 무조건 단일 스레드
        safePrint("Very complex position, using single thread\n");
    } else if (moveCount > 30) {
        allocatedTime *= 0.8;
        if (actualThreads > 2) actualThreads = 2;
        safePrint("Complex position, limiting threads\n");
    }

        // 극도로 시간이 부족한 경우에만 1개로 줄임 (선택사항)
    if (timeAllocated < 0.8) {
        actualThreads = 1;  // 0.8초 미만일 때만 단일 스레드
        safePrint("CRITICAL: Extremely limited time, forced single thread\n");
    }
        
    int emptyCount = countPieces(board, EMPTY);
    
    if (emptyCount < 15) {
        allocatedTime = timeAllocated * 0.8;
        safePrint("Endgame MCTS: increasing time allocation to %.2fs\n", allocatedTime);
    }
    
    safePrint("Hybrid MCTS: %d threads, %.2fs allocated, %d moves available\n", 
             actualThreads, allocatedTime, moveCount);
    
    // allocatedTime 검증
    if (allocatedTime < 0.1 || allocatedTime > 10.0) {
        safePrint("ERROR: Invalid allocated time %.2fs, using fallback\n", allocatedTime);
        gameState.currentPlayer = savedCurrentPlayer;
        freeMCTSTree(root);
        return getHybridAlphaBetaMove(board, currentPlayer);
    }
    
    MCTSWorkerData workerData[MAX_THREADS];
    atomic_int iterationCount;
    atomic_init(&iterationCount, 0);
    
    // 시작 시간 기록 (각 워커가 참조할 수 있도록)
    struct timespec mctsStartTime;
    clock_gettime(CLOCK_MONOTONIC, &mctsStartTime);
    
    workerData[0].root = root;
    memcpy(workerData[0].board, board, BOARD_SIZE * BOARD_SIZE * sizeof(char));
    workerData[0].playerId = currentPlayer;
    workerData[0].iterationCount = &iterationCount;
    workerData[0].timeLimit = allocatedTime;
    
    // Initial single-threaded warmup
    int warmupIterations = 0;
    safePrint("MCTS warmup phase...\n");
    
    // 시간 체크를 자주 하여 오버런 방지
    while (warmupIterations < 200) {
        // 매 10회마다 시간 체크
        if (warmupIterations % 10 == 0) {
            struct timespec now;
            clock_gettime(CLOCK_MONOTONIC, &now);
            double elapsed = (now.tv_sec - mctsStartTime.tv_sec) + 
                           (now.tv_nsec - mctsStartTime.tv_nsec) / 1000000000.0;
            
            if (elapsed > allocatedTime * 0.15) {
                safePrint("Warmup time limit reached\n");
                break;
            }
        }
        
        mctsWorkerSingle(&workerData[0]);
        warmupIterations++;
    }
    
    safePrint("Warmup complete: %d iterations\n", warmupIterations);
    
    // 병렬 처리 - actualThreads가 1보다 클 때만
    if (actualThreads >= 2) {
        // 현재 경과 시간 체크
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        double currentElapsed = (now.tv_sec - mctsStartTime.tv_sec) + 
                               (now.tv_nsec - mctsStartTime.tv_nsec) / 1000000000.0;
        
        if (currentElapsed < allocatedTime * 0.5) {
            safePrint("Starting parallel MCTS phase...\n");
            pthread_t threads[MAX_THREADS];
            int threadCreated[MAX_THREADS] = {0};
            
            for (int i = 0; i < actualThreads; i++) {
                workerData[i].root = root;
                memcpy(workerData[i].board, board, BOARD_SIZE * BOARD_SIZE * sizeof(char));
                workerData[i].playerId = currentPlayer;
                workerData[i].iterationCount = &iterationCount;
                workerData[i].timeLimit = allocatedTime;
                
                if (pthread_create(&threads[i], NULL, mctsWorker, &workerData[i]) == 0) {
                    threadCreated[i] = 1;
                } else {
                    safePrint("Failed to create thread %d\n", i);
                }
            }
            
            safePrint("Waiting for MCTS threads...\n");
            
            
            for (int i = 0; i < actualThreads; i++) {
                if (threadCreated[i]) {
                    pthread_join(threads[i], NULL);
                }
            }
            
            safePrint("All threads joined\n");
        } else {
            safePrint("Skipping parallel phase due to time limit\n");
        }
    } else {
        // 단일 스레드 모드 - 남은 시간 동안 계속
        safePrint("Continuing with single thread...\n");
        
        while (atomic_load(&iterationCount) < MCTS_ITERATIONS) {
            // 자주 시간 체크
            if (atomic_load(&iterationCount) % 50 == 0) {
                struct timespec now;
                clock_gettime(CLOCK_MONOTONIC, &now);
                double elapsed = (now.tv_sec - mctsStartTime.tv_sec) + 
                               (now.tv_nsec - mctsStartTime.tv_nsec) / 1000000000.0;
                
                if (elapsed >= allocatedTime * 0.9) {
                    safePrint("Time limit approaching, stopping\n");
                    break;
                }
            }
            
            mctsWorkerSingle(&workerData[0]);
        }
    }
    
    gameState.currentPlayer = savedCurrentPlayer;
    
    int totalIterations = atomic_load(&iterationCount);
    
    // 최종 시간 측정 (안전하게)
    struct timespec finalTime;
    clock_gettime(CLOCK_MONOTONIC, &finalTime);
    double totalElapsed = (finalTime.tv_sec - mctsStartTime.tv_sec) + 
                         (finalTime.tv_nsec - mctsStartTime.tv_nsec) / 1000000000.0;
    
    if (totalIterations > 0) {
        safePrint("MCTS completed: %d iterations in %.2fs (%.0f iter/sec)\n",
                 totalIterations, totalElapsed,
                 totalIterations / (totalElapsed + 0.001));
    } else {
        safePrint("MCTS failed: no iterations completed\n");
    }
    
    // More lenient iteration requirements
    if (totalIterations < 200) {
        safePrint("Insufficient MCTS iterations (%d), using alpha-beta fallback\n", 
                 totalIterations);
        freeMCTSTree(root);
        
        if (totalElapsed < allocatedTime * 0.9) {
            return getHybridAlphaBetaMove(board, currentPlayer);
        } else {
            safePrint("Emergency: returning first valid move\n");
            return moves[0];
        }
    }
    
    Move bestMove = getMCTSBestMove(root);
    
    if (bestMove.r1 == 0 && bestMove.c1 == 0 && 
        bestMove.r2 == 0 && bestMove.c2 == 0 && moveCount > 0) {
        safePrint("MCTS returned invalid pass, selecting first valid move\n");
        bestMove = moves[0];
    }
    
    freeMCTSTree(root);
    return bestMove;
}

// Endgame God - perfect endgame play
Move getEndgameGodMove(char board[BOARD_SIZE][BOARD_SIZE], int currentPlayer) {
    Move bestMove = {0, 0, 0, 0, 0, 0};
    
    clock_gettime(CLOCK_MONOTONIC, &searchStart);
    atomic_store(&timeUp, 0);
    atomic_store(&nodeCount, 0);
    
    int emptyCount = countPieces(board, EMPTY);
    
    if (emptyCount == 1) {
        int emptyR = -1, emptyC = -1;
        for (int i = 0; i < BOARD_SIZE; i++) {
            for (int j = 0; j < BOARD_SIZE; j++) {
                if (board[i][j] == EMPTY) {
                    emptyR = i;
                    emptyC = j;
                    break;
                }
            }
            if (emptyR != -1) break;
        }
        
        Move moves[MAX_MOVES];
        int moveCount;
        getAllValidMoves(board, currentPlayer, moves, &moveCount);
        
        for (int i = 0; i < moveCount; i++) {
            if (moves[i].r2 == emptyR && moves[i].c2 == emptyC) {
                safePrint("Endgame: Moving to the only empty square (%d,%d)\n", 
                         emptyR+1, emptyC+1);
                return moves[i];
            }
        }
        
        safePrint("Endgame: Cannot move to empty square, passing\n");
        return (Move){0, 0, 0, 0, 0, 0};
    }
    
    int maxDepth;
    
    if (timeAllocated < 1.5) {
        maxDepth = 5;
        safePrint("Endgame: Very limited time, depth=%d\n", maxDepth);
    } else if (timeAllocated < 2.5) {
        maxDepth = (emptyCount <= 4) ? 8 : 6;
        safePrint("Endgame: Limited time, depth=%d\n", maxDepth);
    } else if (emptyCount <= 4) {
        maxDepth = 12;
    } else if (emptyCount <= 6) {
        maxDepth = 10;
    } else if (emptyCount <= 8) {
        maxDepth = 9;
    } else {
        maxDepth = 8;
    }
    
    safePrint("Endgame God: %d empty squares, searching to depth %d, time=%.2fs\n", 
             emptyCount, maxDepth, timeAllocated);
    
    Move moves[MAX_MOVES];
    int moveCount;
    getAllValidMoves(board, currentPlayer, moves, &moveCount);
    
    if (moveCount == 0) {
        return bestMove;
    }
    
    if (moveCount == 1) {
        safePrint("Only one move available, returning immediately\n");
        return moves[0];
    }
    
    // Use iterative deepening
    for (int depth = 1; depth <= maxDepth && !atomic_load(&timeUp); depth++) {
        double depthStartTime = elapsedSeconds();
        
        Move iterationBest = bestMove;
        int alpha = AI_NEG_INFINITY;
        int beta = AI_INFINITY;
        
        if (depth > 3) {
            alpha = -100;
            beta = 100;
        }
        
        int score = enhancedAlphaBeta(board, depth, alpha, beta, currentPlayer, 
                                     &iterationBest, 1, 1);
        
        if ((score <= alpha || score >= beta) && !atomic_load(&timeUp)) {
            score = enhancedAlphaBeta(board, depth, AI_NEG_INFINITY, AI_INFINITY, 
                                     currentPlayer, &iterationBest, 1, 1);
        }
        
        double depthTime = elapsedSeconds() - depthStartTime;
        
        if (atomic_load(&timeUp)) {
            safePrint("  Depth %d incomplete (timeout)\n", depth);
            break;
        }
        
        bestMove = iterationBest;
        
        long nodes = atomic_load(&nodeCount);
        double totalTime = elapsedSeconds();
        double nps = nodes / (totalTime + 0.001);
        
        safePrint("  Depth %d: score=%.2f, nodes=%ld, time=%.2fs (depth:%.2fs), nps=%.0f\n",
                 depth, score / 100.0, nodes, totalTime, depthTime, nps);
        
        if (score > AI_INFINITY/2 || score < -AI_INFINITY/2) {
            safePrint("  Found perfect winning/losing line!\n");
            break;
        }
        
        if (totalTime > timeAllocated * 0.75) {
            safePrint("  Approaching time limit, stopping search\n");
            break;
        }
        
        if (depth >= 3 && depthTime * 2.5 > timeAllocated - totalTime) {
            safePrint("  Next depth would likely exceed time limit\n");
            break;
        }
    }
    
    if (atomic_load(&timeUp)) {
        safePrint("Endgame search timeout, using best found so far\n");
    }
    
    if (bestMove.r1 == 0 && bestMove.c1 == 0 && 
        bestMove.r2 == 0 && bestMove.c2 == 0 && moveCount > 0) {
        safePrint("Endgame solver failed, using first valid move\n");
        bestMove = moves[0];
    }
    
    return bestMove;
}

// NEW Ultimate AI - Win-focused strategy
// Philosophy: Prioritize winning over maximum evaluation score
// - Opening: Tournament Beast + NNUE for strong foundation
// - Midgame: MCTS with Tournament Beast fallback for complex positions  
// - Endgame: MCTS-based for win probability, Endgame God for perfect play
Move getUltimateAIMove(char board[BOARD_SIZE][BOARD_SIZE], int currentPlayer) {
    int totalPieces = countPieces(board, RED) + countPieces(board, BLUE);
    int emptyCount = countPieces(board, EMPTY);
    int moveNumber = totalPieces - 4;  // Starting from 4 pieces
    
    safePrint("Ultimate AI : move=%d, pieces=%d, empty=%d, time=%.2fs\n", 
             moveNumber, totalPieces, emptyCount, timeAllocated);
    safePrint("Strategy: Win-focused approach prioritizing victory over maximum score\n");
    
    // Emergency time handling
    if (timeAllocated < 0.8) {
        safePrint("EMERGENCY: Very limited time, using Tournament Beast for quick decision\n");
        return getTournamentBeastMove(board, currentPlayer);
    }
    
    // Phase-based engine selection
    
    // Extended opening phase (0-19 pieces) - Tournament Beast with enhanced NNUE
    if (totalPieces < 20) {
        safePrint("Opening phase: Using Tournament Beast + NNUE\n");
        Move tournamentMove = getTournamentBeastMove(board, currentPlayer);
        
        #ifdef HAS_NNUE_WEIGHTS
        // Double-check critical moves with deeper NNUE analysis
        if (totalPieces <= 12) {  // Very early game
            char tempBoard[BOARD_SIZE][BOARD_SIZE];
            memcpy(tempBoard, board, sizeof(tempBoard));
            makeMove(tempBoard, tournamentMove);
            
            int immediateScore = nnue_evaluate(tempBoard, currentPlayer);
            safePrint("NNUE immediate evaluation: %d\n", immediateScore);
            
            // Look one move ahead for opponent's best response
            Move oppMoves[MAX_MOVES];
            int oppCount;
            getAllValidMoves(tempBoard, !currentPlayer, oppMoves, &oppCount);
            
            int worstCase = AI_INFINITY;
            for (int i = 0; i < oppCount && i < 10; i++) {
                char tempBoard2[BOARD_SIZE][BOARD_SIZE];
                memcpy(tempBoard2, tempBoard, sizeof(tempBoard2));
                makeMove(tempBoard2, oppMoves[i]);
                
                int oppScore = -nnue_evaluate(tempBoard2, currentPlayer);
                if (oppScore < worstCase) {
                    worstCase = oppScore;
                }
            }
            
            safePrint("NNUE worst case after opponent's best: %d\n", worstCase);
            
            // If the position becomes very bad, reconsider
            if (worstCase < -300) {
                safePrint("NNUE predicts danger, using careful alpha-beta instead\n");
                return getHybridAlphaBetaMove(board, currentPlayer);
            }
        }
        #endif
        
        return tournamentMove;
    }
    
    // Midgame phase (28-50 pieces) - Hybrid MCTS with Tournament Beast fallback
    else if (totalPieces <= 50) {
        safePrint("Midgame phase: Using Hybrid MCTS with Tournament Beast fallback\n");
        
        // Adjust time allocation based on position complexity
        Move moves[MAX_MOVES];
        int moveCount;
        getAllValidMoves(board, currentPlayer, moves, &moveCount);
        
        double savedTime = timeAllocated;
        
        // More conservative time allocation for MCTS
        if (moveCount > 35) {
            timeAllocated *= 0.9;  // Reduce time for very complex positions
            safePrint("Very complex position (%d moves), reducing time\n", moveCount);
        } else if (moveCount > 25) {
            timeAllocated *= 0.95;  // Slightly reduce for complex positions
            safePrint("Complex position (%d moves), slightly reducing time\n", moveCount);
        } else if (moveCount < 10) {
            timeAllocated *= 0.8;  // Save time for endgame
            safePrint("Simple position (%d moves), saving time\n", moveCount);
        }
        
        // Set a hard timeout for MCTS
        double mctsMaxTime = (timeAllocated < 2.0) ? timeAllocated * 0.6 : timeAllocated * 0.7;
        safePrint("MCTS max time: %.2fs\n", mctsMaxTime);
        
        // Save original time for restoration
        double originalTimeAllocated = timeAllocated;
        timeAllocated = mctsMaxTime;
        
        // Save start time for timeout detection
        double mctsStartTime = elapsedSeconds();
        
        Move mctsMove = getHybridMCTSMove(board, currentPlayer);
        
        double mctsEndTime = elapsedSeconds();
        double mctsElapsed = mctsEndTime - mctsStartTime;
        
        // Restore original time
        timeAllocated = originalTimeAllocated;
        
        // Check if MCTS failed or took too long
        bool mctsFailed = (mctsMove.r1 == 0 && mctsMove.c1 == 0 && 
                          mctsMove.r2 == 0 && mctsMove.c2 == 0 && moveCount > 0);
        bool mctsTimeout = (mctsElapsed > mctsMaxTime * 1.1);  // 10% tolerance
        
        if (mctsFailed || mctsTimeout) {
            if (mctsFailed) {
                safePrint("MCTS failed, using Tournament Beast + NNUE fallback\n");
            } else {
                safePrint("MCTS timeout (%.2fs > %.2fs), using Tournament Beast + NNUE fallback\n", 
                         mctsElapsed, mctsMaxTime);
            }
            
            // Restore saved time for Tournament Beast
            timeAllocated = savedTime;
            
            // Use the same opening strategy: Tournament Beast with NNUE enhancement
            safePrint("Falling back to opening strategy for midgame move\n");
            return getTournamentBeastMove(board, currentPlayer);
        }
        
        safePrint("MCTS completed successfully in %.2fs\n", mctsElapsed);
        
        #ifdef HAS_NNUE_WEIGHTS
        // Quick NNUE verification for critical midgame positions
        if (totalPieces >= 40) {  // Late midgame
            char tempBoard[BOARD_SIZE][BOARD_SIZE];
            memcpy(tempBoard, board, sizeof(tempBoard));
            makeMove(tempBoard, mctsMove);
            
            int nnueScore = nnue_evaluate(tempBoard, currentPlayer);
            if (nnueScore < -200) {
                safePrint("NNUE warns about MCTS move (score=%d), using Tournament Beast\n", nnueScore);
                timeAllocated = savedTime;
                return getTournamentBeastMove(board, currentPlayer);
            }
        }
        #endif
        
        return mctsMove;
    }
    
    // Endgame phase (51+ pieces) - MCTS-based approach with Endgame God backup
    else {
        safePrint("Endgame phase: Using MCTS-based approach for winning focus\n");
        
        int myPieces = countPieces(board, (currentPlayer == RED_TURN) ? RED : BLUE);
        int oppPieces = countPieces(board, (currentPlayer == RED_TURN) ? BLUE : RED);
        int materialDiff = myPieces - oppPieces;
        
        Move bestMove;
        
        // Special case: very few empty squares - use Endgame God for perfect play
        if (emptyCount <= 3) {
            safePrint("Very few empty squares (%d), using Endgame God for perfect play\n", emptyCount);
            
            double savedTime = timeAllocated;
            timeAllocated *= 1.3;  // More time for perfect endgame
            
            bestMove = getEndgameGodMove(board, currentPlayer);
            
            timeAllocated = savedTime;
            
            // Verify it's not a pass when moves are available
            Move moves[MAX_MOVES];
            int moveCount;
            getAllValidMoves(board, currentPlayer, moves, &moveCount);
            
            if (bestMove.r1 == 0 && bestMove.c1 == 0 && 
                bestMove.r2 == 0 && bestMove.c2 == 0 && moveCount > 0) {
                safePrint("Endgame God returned pass, using first valid move\n");
                bestMove = moves[0];
            }
            
            return bestMove;
        }
        
        // Default: Use MCTS for win-focused play
        safePrint("Using MCTS for win-focused endgame (empty=%d, diff=%d)\n", emptyCount, materialDiff);
        
        // Allocate appropriate time for MCTS in endgame
        double savedTime = timeAllocated;
        double mctsTime = timeAllocated * 0.8;  // 80% for MCTS
        
        if (emptyCount <= 10) {
            mctsTime = timeAllocated * 0.9;  // More time when close to end
            safePrint("Near-endgame: allocating %.2fs to MCTS\n", mctsTime);
        }
        
        // Save original time
        double originalTime = timeAllocated;
        timeAllocated = mctsTime;
        
        // Try MCTS first
        double mctsStartTime = elapsedSeconds();
        Move mctsMove = getHybridMCTSMove(board, currentPlayer);
        double mctsElapsed = elapsedSeconds() - mctsStartTime;
        
        // Restore time
        timeAllocated = originalTime;
        
        // Check if MCTS succeeded
        Move moves[MAX_MOVES];
        int moveCount;
        getAllValidMoves(board, currentPlayer, moves, &moveCount);
        
        bool mctsFailed = (mctsMove.r1 == 0 && mctsMove.c1 == 0 && 
                          mctsMove.r2 == 0 && mctsMove.c2 == 0 && moveCount > 0);
        bool mctsTimeout = (mctsElapsed > mctsTime * 1.1);
        
        if (mctsFailed || mctsTimeout) {
            if (mctsFailed) {
                safePrint("MCTS failed in endgame, falling back to Endgame God\n");
            } else {
                safePrint("MCTS timeout in endgame (%.2fs), falling back to Endgame God\n", mctsElapsed);
            }
            
            // Fallback to Endgame God
            timeAllocated = savedTime - mctsElapsed;  // Use remaining time
            if (timeAllocated < 0.5) timeAllocated = 0.5;
            
            bestMove = getEndgameGodMove(board, currentPlayer);
            
            // Final safety check
            if (bestMove.r1 == 0 && bestMove.c1 == 0 && 
                bestMove.r2 == 0 && bestMove.c2 == 0 && moveCount > 0) {
                safePrint("Both engines failed, using first valid move\n");
                bestMove = moves[0];
            }
        } else {
            bestMove = mctsMove;
            safePrint("MCTS succeeded with move (%d,%d)->(%d,%d)\n",
                     bestMove.r1+1, bestMove.c1+1, bestMove.r2+1, bestMove.c2+1);
            
            // Optional: Quick verification only if we have extra time
            if (mctsElapsed < mctsTime * 0.7 && emptyCount <= 12) {
                safePrint("Time available for verification, checking with Endgame God\n");
                
                // Quick Endgame God check
                double verifyTime = timeAllocated;
                timeAllocated = (savedTime - mctsElapsed) * 0.5;  // Use half remaining time
                
                Move endgameMove = getEndgameGodMove(board, currentPlayer);
                
                timeAllocated = verifyTime;
                
                // Only switch if Endgame God finds significantly better move
                if (!(endgameMove.r1 == 0 && endgameMove.c1 == 0 && 
                      endgameMove.r2 == 0 && endgameMove.c2 == 0)) {
                    
                    // Quick evaluation of both moves
                    char tempBoard[BOARD_SIZE][BOARD_SIZE];
                    memcpy(tempBoard, board, sizeof(tempBoard));
                    makeMove(tempBoard, mctsMove);
                    int mctsResult = countPieces(tempBoard, (currentPlayer == RED_TURN) ? RED : BLUE) -
                                    countPieces(tempBoard, (currentPlayer == RED_TURN) ? BLUE : RED);
                    
                    memcpy(tempBoard, board, sizeof(tempBoard));
                    makeMove(tempBoard, endgameMove);
                    int endgameResult = countPieces(tempBoard, (currentPlayer == RED_TURN) ? RED : BLUE) -
                                        countPieces(tempBoard, (currentPlayer == RED_TURN) ? BLUE : RED);
                    
                    // Only switch if Endgame God's move is clearly better
                    if (endgameResult > mctsResult + 2) {
                        safePrint("Endgame God found better move (diff=%d vs %d)\n", 
                                 endgameResult, mctsResult);
                        bestMove = endgameMove;
                    }
                }
            }
        }
        
        #ifdef HAS_NNUE_WEIGHTS
        // Final NNUE sanity check
        if (emptyCount <= 10) {
            char tempBoard[BOARD_SIZE][BOARD_SIZE];
            memcpy(tempBoard, board, sizeof(tempBoard));
            makeMove(tempBoard, bestMove);
            
            int nnueScore = nnue_evaluate(tempBoard, currentPlayer);
            int materialAfter = countPieces(tempBoard, (currentPlayer == RED_TURN) ? RED : BLUE) -
                               countPieces(tempBoard, (currentPlayer == RED_TURN) ? BLUE : RED);
            
            safePrint("Final move check: NNUE=%d, material_diff=%d\n", nnueScore, materialAfter);
            
            // Warning if NNUE strongly disagrees
            if ((materialAfter > 0 && nnueScore < -400) || 
                (materialAfter < 0 && nnueScore > 400)) {
                safePrint("WARNING: NNUE strongly disagrees with chosen move!\n");
            }
        }
        #endif
        
        return bestMove;
    }
}

// MCTS Node functions
MCTSNode* createMCTSNode(Move move, int player, MCTSNode* parent) {
    MCTSNode* node = NULL;
    
    pthread_mutex_lock(&nodePoolMutex);
    if (nodePoolIndex < MCTS_NODE_POOL_SIZE) {
        node = &nodePool[nodePoolIndex++];
    }
    pthread_mutex_unlock(&nodePoolMutex);
    
    if (!node) {
        node = (MCTSNode*)malloc(sizeof(MCTSNode));
        if (!node) return NULL;
    }
    
    node->move = move;
    atomic_init(&node->visits, 0);
    atomic_init(&node->virtualLoss, 0);
    node->score = 0.0;
    node->raveScore = 0.0;
    atomic_init(&node->raveVisits, 0);
    node->player = player;
    node->parent = parent;
    node->children = NULL;
    node->childCount = 0;
    node->childCapacity = 0;
    node->priorScore = 0.0;
    pthread_mutex_init(&node->lock, NULL);
    node->heuristicScore = 0.0;
    node->fullyExpanded = 0;
    
    memset(node->amafScore, 0, sizeof(node->amafScore));
    memset(node->amafVisits, 0, sizeof(node->amafVisits));
    
    return node;
}

void freeMCTSTree(MCTSNode* root) {
    if (!root) return;
    
    for (int i = 0; i < root->childCount; i++) {
        freeMCTSTree(root->children[i]);
    }
    
    if (root->children) free(root->children);
    pthread_mutex_destroy(&root->lock);
    
    if ((char*)root < (char*)nodePool || 
        (char*)root >= (char*)nodePool + sizeof(MCTSNode) * MCTS_NODE_POOL_SIZE) {
        free(root);
    }
}

MCTSNode* selectBestChild(MCTSNode* node, double c) {
    MCTSNode* best = NULL;
    double bestValue = -AI_INFINITY;
    
    int parentVisits = atomic_load(&node->visits);
    
    // Check if we're in endgame
    int emptyCount = 64;  // Approximate, will be refined if needed
    
    for (int i = 0; i < node->childCount; i++) {
        MCTSNode* child = node->children[i];
        
        int vLoss = atomic_load(&child->virtualLoss);
        if (vLoss > 0) continue;
        
        double exploitation = 0.0;
        double exploration = 0.0;
        
        if (atomic_load(&child->visits) == 0) {
            bestValue = AI_INFINITY + child->priorScore;
        } else {
            int visits = atomic_load(&child->visits);
            exploitation = child->score / visits;
            
            // Adjust exploration for endgame (less exploration, more exploitation)
            double cValue = c;
            if (emptyCount < 15) {
                cValue *= 0.7;  // Reduce exploration in endgame
            }
            
            exploration = cValue * sqrt(log(parentVisits) / visits);
            
            if (atomic_load(&child->raveVisits) > 0) {
                int raveVisits = atomic_load(&child->raveVisits);
                double beta = sqrt(MCTS_RAVE_K / (3.0 * visits + MCTS_RAVE_K));
                
                // Less RAVE influence in endgame
                if (emptyCount < 15) {
                    beta *= 0.5;
                }
                
                double raveScore = child->raveScore / raveVisits;
                exploitation = beta * raveScore + (1.0 - beta) * exploitation;
            }
            
            if (visits < 50) {
                exploitation += child->heuristicScore / (visits + 1);
            }
        }
        
        double value = exploitation + exploration;
        
        if (value > bestValue) {
            bestValue = value;
            best = child;
        }
    }
    
    if (best) {
        atomic_fetch_add(&best->virtualLoss, MCTS_VIRTUAL_LOSS);
    }
    
    return best;
}

void expand(MCTSNode* node, char board[BOARD_SIZE][BOARD_SIZE]) {
    pthread_mutex_lock(&node->lock);
    
    if (node->fullyExpanded) {
        pthread_mutex_unlock(&node->lock);
        return;
    }
    
    Move moves[MAX_MOVES];
    int moveCount;
    int nextPlayer = !node->player;
    
    getAllValidMoves(board, nextPlayer, moves, &moveCount);
    
    int maxChildren = (int)sqrt(atomic_load(&node->visits) * 2) + 5;  // Reduced from 10
    
    // More aggressive expansion in endgame
    int emptyCount = 0;
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board[i][j] == EMPTY) emptyCount++;
        }
    }
    
    if (emptyCount < 15) {
        maxChildren = (int)sqrt(atomic_load(&node->visits) * 3) + 8;  // More children in endgame
    }
    
    if (maxChildren > 20) maxChildren = 20;  // Hard limit
    if (maxChildren > moveCount) maxChildren = moveCount;
    
    for (int i = 0; i < moveCount; i++) {
        char tempBoard[BOARD_SIZE][BOARD_SIZE];
        memcpy(tempBoard, board, sizeof(tempBoard));
        makeMove(tempBoard, moves[i]);
        
        double prior = 0.0;
        
        // Position value
        prior += POSITION_WEIGHTS[getGamePhase(board)][moves[i].r2][moves[i].c2] * 10.0;
        
        // Capture bonus
        char opponentPiece = (nextPlayer == RED_TURN) ? BLUE : RED;
        int captures = 0;
        for (int dr = -1; dr <= 1; dr++) {
            for (int dc = -1; dc <= 1; dc++) {
                if (dr == 0 && dc == 0) continue;
                int nr = moves[i].r2 + dr;
                int nc = moves[i].c2 + dc;
                if (nr >= 0 && nr < BOARD_SIZE && nc >= 0 && nc < BOARD_SIZE &&
                    board[nr][nc] == opponentPiece) {
                    captures++;
                }
            }
        }
        
        // In endgame, captures are even more valuable
        int emptyCount = 0;
        for (int r = 0; r < BOARD_SIZE; r++) {
            for (int c = 0; c < BOARD_SIZE; c++) {
                if (board[r][c] == EMPTY) emptyCount++;
            }
        }
        
        if (emptyCount < 15) {
            prior += captures * 10.0;  // Double capture value in endgame
        } else {
            prior += captures * 5.0;
        }
        
        if (moves[i].moveType == CLONE) {
            prior += 2.0;
        }
        
        // Endgame bonus for controlling key squares
        if (emptyCount < 15) {
            // Corners and edges are valuable in endgame
            if ((moves[i].r2 == 0 || moves[i].r2 == 7) && 
                (moves[i].c2 == 0 || moves[i].c2 == 7)) {
                prior += 8.0;  // Corner bonus
            } else if (moves[i].r2 == 0 || moves[i].r2 == 7 || 
                       moves[i].c2 == 0 || moves[i].c2 == 7) {
                prior += 4.0;  // Edge bonus
            }
        }
        
        moves[i].score = (short)prior;
    }
    
    for (int i = 0; i < moveCount - 1; i++) {
        for (int j = i + 1; j < moveCount; j++) {
            if (moves[j].score > moves[i].score) {
                Move temp = moves[i];
                moves[i] = moves[j];
                moves[j] = temp;
            }
        }
    }
    
    if (node->childCapacity < maxChildren) {
        node->childCapacity = maxChildren + 10;
        node->children = (MCTSNode**)realloc(node->children, 
                                            node->childCapacity * sizeof(MCTSNode*));
    }
    
    for (int i = node->childCount; i < maxChildren && i < moveCount; i++) {
        MCTSNode* child = createMCTSNode(moves[i], nextPlayer, node);
        if (child) {
            child->priorScore = moves[i].score / 100.0;
            node->children[node->childCount++] = child;
        }
    }
    
    if (node->childCount >= moveCount) {
        node->fullyExpanded = 1;
    }
    
    pthread_mutex_unlock(&node->lock);
}

double simulate(char board[BOARD_SIZE][BOARD_SIZE], int startingPlayer) {
    char simBoard[BOARD_SIZE][BOARD_SIZE];
    memcpy(simBoard, board, sizeof(simBoard));
    
    int currentPlayer = startingPlayer;
    int moveCount = 0;
    int passCount = 0;
    int maxSimMoves = 15;  // Reduced from 20
    
    // Check if we're in endgame
    int emptyCount = countPieces(simBoard, EMPTY);
    if (emptyCount < 10) {
        maxSimMoves = emptyCount + 2;  // Play almost to the end
    }
    
    while (moveCount < maxSimMoves && passCount < 2) {
        Move moves[MAX_MOVES];
        int numMoves;
        
        getAllValidMoves(simBoard, currentPlayer, moves, &numMoves);
        
        if (numMoves == 0) {
            passCount++;
            currentPlayer = !currentPlayer;
            continue;
        }
        
        passCount = 0;
        
        Move selectedMove;
        
        // In endgame, be more careful
        if (emptyCount < 15 && rand() % 100 < 90) {
            int bestIdx = 0;
            int bestCaptures = -1;
            
            // Quick evaluation focusing on captures
            int evalCount = (numMoves < 8) ? numMoves : 8;
            
            for (int i = 0; i < evalCount; i++) {
                // Count immediate captures
                int captures = 0;
                char opponentPiece = (currentPlayer == RED_TURN) ? BLUE : RED;
                
                for (int dr = -1; dr <= 1; dr++) {
                    for (int dc = -1; dc <= 1; dc++) {
                        if (dr == 0 && dc == 0) continue;
                        int nr = moves[i].r2 + dr;
                        int nc = moves[i].c2 + dc;
                        if (nr >= 0 && nr < BOARD_SIZE && nc >= 0 && nc < BOARD_SIZE &&
                            simBoard[nr][nc] == opponentPiece) {
                            captures++;
                        }
                    }
                }
                
                // Prefer moves with more captures
                if (captures > bestCaptures) {
                    bestCaptures = captures;
                    bestIdx = i;
                } else if (captures == bestCaptures) {
                    // Tie-break by position
                    double posScore = POSITION_WEIGHTS[2][moves[i].r2][moves[i].c2];
                    double bestPosScore = POSITION_WEIGHTS[2][moves[bestIdx].r2][moves[bestIdx].c2];
                    if (posScore > bestPosScore) {
                        bestIdx = i;
                    }
                }
            }
            
            selectedMove = moves[bestIdx];
        } else if (rand() % 100 < 70) {  // Reduced from 80
            int bestIdx = 0;
            double bestScore = -1000.0;
            
            // Only evaluate top 5 moves for speed
            int evalCount = (numMoves < 5) ? numMoves : 5;
            
            for (int i = 0; i < evalCount; i++) {
                double score = 0.0;
                
                score += POSITION_WEIGHTS[getGamePhase(simBoard)][moves[i].r2][moves[i].c2] * 10.0;
                
                char opponentPiece = (currentPlayer == RED_TURN) ? BLUE : RED;
                for (int dr = -1; dr <= 1; dr++) {
                    for (int dc = -1; dc <= 1; dc++) {
                        if (dr == 0 && dc == 0) continue;
                        int nr = moves[i].r2 + dr;
                        int nc = moves[i].c2 + dc;
                        if (nr >= 0 && nr < BOARD_SIZE && nc >= 0 && nc < BOARD_SIZE &&
                            simBoard[nr][nc] == opponentPiece) {
                            score += 5.0;
                        }
                    }
                }
                
                if (score > bestScore) {
                    bestScore = score;
                    bestIdx = i;
                }
            }
            
            selectedMove = moves[bestIdx];
        } else {
            selectedMove = moves[rand() % numMoves];
        }
        
        makeMove(simBoard, selectedMove);
        currentPlayer = !currentPlayer;
        moveCount++;
        emptyCount = countPieces(simBoard, EMPTY);
    }
    
    // Win-focused evaluation
    int myPieces = countPieces(simBoard, (startingPlayer == RED_TURN) ? RED : BLUE);
    int oppPieces = countPieces(simBoard, (startingPlayer == RED_TURN) ? BLUE : RED);
    
    // Check for clear win/loss
    if (myPieces == 0) return 0.0;  // Lost
    if (oppPieces == 0) return 1.0;  // Won
    
    // If game not finished, evaluate by piece difference
    double winProb;
    int diff = myPieces - oppPieces;
    
    if (emptyCount == 0) {
        // Game finished - binary result
        winProb = (diff > 0) ? 1.0 : (diff < 0) ? 0.0 : 0.5;
    } else {
        // Game ongoing - smooth probability based on advantage
        // Stronger sigmoid for clearer winning chances
        winProb = 1.0 / (1.0 + exp(-diff * 0.3));
    }
    
    return winProb;
}

void backpropagate(MCTSNode* node, double result, Move* moveSequence, int sequenceLength) {
    MCTSNode* current = node;
    
    while (current != NULL) {
        atomic_fetch_add(&current->visits, 1);
        
        double nodeResult = (current->player == RED_TURN) ? result : 1.0 - result;
        
        pthread_mutex_lock(&current->lock);
        current->score += nodeResult;
        pthread_mutex_unlock(&current->lock);
        
        if (current->parent != NULL) {
            for (int i = 0; i < sequenceLength; i++) {
                if (current->move.r1 == moveSequence[i].r1 &&
                    current->move.c1 == moveSequence[i].c1 &&
                    current->move.r2 == moveSequence[i].r2 &&
                    current->move.c2 == moveSequence[i].c2) {
                    atomic_fetch_add(&current->raveVisits, 1);
                    pthread_mutex_lock(&current->lock);
                    current->raveScore += nodeResult;
                    pthread_mutex_unlock(&current->lock);
                    break;
                }
            }
        }
        
        if (current->parent != NULL) {
            pthread_mutex_lock(&current->parent->lock);
            for (int i = 0; i < sequenceLength; i++) {
                Move* m = &moveSequence[i];
                current->parent->amafScore[m->r1][m->c1][m->r2][m->c2] += nodeResult;
                current->parent->amafVisits[m->r1][m->c1][m->r2][m->c2]++;
            }
            pthread_mutex_unlock(&current->parent->lock);
        }
        
        atomic_fetch_sub(&current->virtualLoss, MCTS_VIRTUAL_LOSS);
        
        current = current->parent;
    }
}

Move getMCTSBestMove(MCTSNode* root) {
    Move bestMove = {0, 0, 0, 0, 0, 0};
    int mostVisits = 0;
    double bestWinRate = 0.0;
    
    // In endgame, also consider win rate more heavily
    int considerWinRate = 0;
    if (root->childCount > 0) {
        // Check approximate game phase
        int totalVisits = atomic_load(&root->visits);
        if (totalVisits > 1000) {
            considerWinRate = 1;
        }
    }
    
    for (int i = 0; i < root->childCount; i++) {
        MCTSNode* child = root->children[i];
        int visits = atomic_load(&child->visits);
        
        if (visits > mostVisits) {
            mostVisits = visits;
            bestMove = child->move;
            bestWinRate = visits > 0 ? child->score / visits : 0.0;
        } else if (considerWinRate && visits > mostVisits * 0.8) {
            // In endgame, also consider moves with slightly fewer visits but better win rate
            double winRate = visits > 0 ? child->score / visits : 0.0;
            if (winRate > bestWinRate + 0.05) {  // 5% better win rate
                mostVisits = visits;
                bestMove = child->move;
                bestWinRate = winRate;
            }
        }
    }
    
    if (root->childCount > 0) {
        // Only print top moves if we have reasonable number of visits
        if (mostVisits > 50) {
            safePrint("MCTS top moves:\n");
            
            MCTSNode** sorted = (MCTSNode**)malloc(root->childCount * sizeof(MCTSNode*));
            if (sorted) {
                memcpy(sorted, root->children, root->childCount * sizeof(MCTSNode*));
                
                for (int i = 0; i < root->childCount - 1; i++) {
                    for (int j = i + 1; j < root->childCount; j++) {
                        if (atomic_load(&sorted[j]->visits) > atomic_load(&sorted[i]->visits)) {
                            MCTSNode* temp = sorted[i];
                            sorted[i] = sorted[j];
                            sorted[j] = temp;
                        }
                    }
                }
                
                int printCount = (root->childCount < 3) ? root->childCount : 3;  // Print only top 3
                for (int i = 0; i < printCount; i++) {
                    int visits = atomic_load(&sorted[i]->visits);
                    double winRate = visits > 0 ? sorted[i]->score / visits : 0.0;
                    
                    safePrint("  (%d,%d)->(%d,%d): visits=%d, win=%.1f%%\n",
                             sorted[i]->move.r1 + 1, sorted[i]->move.c1 + 1,
                             sorted[i]->move.r2 + 1, sorted[i]->move.c2 + 1,
                             visits, winRate * 100.0);
                }
                
                free(sorted);
            }
        }
        
        safePrint("Selected: (%d,%d)->(%d,%d) with %d visits, %.1f%% win rate\n",
                 bestMove.r1 + 1, bestMove.c1 + 1,
                 bestMove.r2 + 1, bestMove.c2 + 1,
                 mostVisits, bestWinRate * 100.0);
    }
    
    return bestMove;
}

void* mctsWorker(void* arg) {
    MCTSWorkerData* data = (MCTSWorkerData*)arg;
    int localIterations = 0;
    
    while (1) {
        // Check time every iteration for first 10, then every 10 iterations
        if (localIterations < 10 || localIterations % 10 == 0) {
            double elapsed = elapsedSeconds() - data->startTime;
            if (elapsed >= data->timeLimit * 0.95) {
                // Only print for main thread
                if (data == &((MCTSWorkerData*)arg)[0]) {
                    safePrint("Worker timeout at %.2fs (limit: %.2fs)\n", elapsed, data->timeLimit);
                }
                break;
            }
            if (atomic_load(data->iterationCount) >= MCTS_ITERATIONS) {
                break;
            }
        }
        
        MCTSNode* current = data->root;
        char board[BOARD_SIZE][BOARD_SIZE];
        memcpy(board, data->board, sizeof(board));
        
        Move moveSequence[100];
        int sequenceLength = 0;
        
        if (!current) break;
        
        // Tree traversal with timeout check
        int traversalSteps = 0;
        while (current->childCount > 0 && atomic_load(&current->visits) > MCTS_EXPANSION_THRESHOLD) {
            current = selectBestChild(current, MCTS_C);
            if (!current) break;
            
            makeMove(board, current->move);
            moveSequence[sequenceLength++] = current->move;
            
            // Check for infinite loops in tree traversal
            traversalSteps++;
            if (traversalSteps > 50) {  // Maximum tree depth
                // Only print warning once
                if (localIterations == 0) {
                    safePrint("Warning: Deep tree traversal, breaking\n");
                }
                break;
            }
        }
        
        if (!current) continue;
        
        if (atomic_load(&current->visits) > 0 && !current->fullyExpanded) {
            expand(current, board);
            
            if (current->childCount > 0) {
                current = current->children[0];
                makeMove(board, current->move);
                moveSequence[sequenceLength++] = current->move;
            }
        }
        
        double result = simulate(board, data->playerId);
        
        backpropagate(current, result, moveSequence, sequenceLength);
        
        atomic_fetch_add(data->iterationCount, 1);
        localIterations++;
        
        // Hard timeout check
        double currentElapsed = elapsedSeconds() - data->startTime;
        if (currentElapsed > data->timeLimit) {
            if (localIterations > 0 && data == &((MCTSWorkerData*)arg)[0]) {
                safePrint("Worker hard timeout at %.2fs\n", currentElapsed);
            }
            break;
        }
        
        // Yield to other threads periodically
        if (localIterations % 100 == 0) {
            sched_yield();
        }
    }
    
    // Only print for main thread
    if (localIterations > 0 && data == &((MCTSWorkerData*)arg)[0]) {
        safePrint("Main worker completed %d iterations\n", localIterations);
    }
    return NULL;
}

void mctsWorkerSingle(MCTSWorkerData* data) {
    // Check time before starting
    double elapsed = elapsedSeconds() - data->startTime;
    if (elapsed >= data->timeLimit * 0.9) {
        return;
    }
    
    MCTSNode* current = data->root;
    char board[BOARD_SIZE][BOARD_SIZE];
    memcpy(board, data->board, sizeof(board));
    
    Move moveSequence[100];
    int sequenceLength = 0;
    
    if (!current) return;
    
    // Tree traversal with depth limit
    int traversalDepth = 0;
    while (current->childCount > 0 && atomic_load(&current->visits) > MCTS_EXPANSION_THRESHOLD) {
        current = selectBestChild(current, MCTS_C);
        if (!current) break;
        
        makeMove(board, current->move);
        moveSequence[sequenceLength++] = current->move;
        
        traversalDepth++;
        if (traversalDepth > 30) {  // Prevent infinite loops
            break;
        }
    }
    
    if (!current) return;
    
    if (atomic_load(&current->visits) > 0 && !current->fullyExpanded) {
        expand(current, board);
        
        if (current->childCount > 0) {
            current = current->children[0];
            makeMove(board, current->move);
            moveSequence[sequenceLength++] = current->move;
        }
    }
    
    double result = simulate(board, data->playerId);
    
    backpropagate(current, result, moveSequence, sequenceLength);
    
    atomic_fetch_add(data->iterationCount, 1);
}

// Additional evaluation functions
double evaluateMobility(char board[BOARD_SIZE][BOARD_SIZE], int forPlayer) {
    Move tempMoves[MAX_MOVES];
    int playerMoves, opponentMoves;
    
    getAllValidMoves(board, forPlayer, tempMoves, &playerMoves);
    getAllValidMoves(board, !forPlayer, tempMoves, &opponentMoves);
    
    if (playerMoves + opponentMoves == 0) return 0.0;
    
    double actualMobility = 100.0 * (playerMoves - opponentMoves) / (playerMoves + opponentMoves);
    
    int playerPotential = 0, opponentPotential = 0;
    char playerPiece = (forPlayer == RED_TURN) ? RED : BLUE;
    char opponentPiece = (forPlayer == RED_TURN) ? BLUE : RED;
    
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board[i][j] == EMPTY) {
                int playerNeighbors = 0, opponentNeighbors = 0;
                
                for (int di = -1; di <= 1; di++) {
                    for (int dj = -1; dj <= 1; dj++) {
                        if (di == 0 && dj == 0) continue;
                        int ni = i + di, nj = j + dj;
                        if (ni >= 0 && ni < BOARD_SIZE && nj >= 0 && nj < BOARD_SIZE) {
                            if (board[ni][nj] == playerPiece) playerNeighbors++;
                            else if (board[ni][nj] == opponentPiece) opponentNeighbors++;
                        }
                    }
                }
                
                if (playerNeighbors > opponentNeighbors) playerPotential++;
                else if (opponentNeighbors > playerNeighbors) opponentPotential++;
            }
        }
    }
    
    double potentialMobility = 0.0;
    if (playerPotential + opponentPotential > 0) {
        potentialMobility = 50.0 * (playerPotential - opponentPotential) / 
                            (playerPotential + opponentPotential);
    }
    
    return actualMobility + potentialMobility * 0.5;
}

double evaluateStability(char board[BOARD_SIZE][BOARD_SIZE], int forPlayer) {
    double score = 0.0;
    char playerPiece = (forPlayer == RED_TURN) ? RED : BLUE;
    char opponentPiece = (forPlayer == RED_TURN) ? BLUE : RED;
    
    int corners[4][2] = {{0,0}, {0,7}, {7,0}, {7,7}};
    for (int i = 0; i < 4; i++) {
        if (board[corners[i][0]][corners[i][1]] == playerPiece) {
            score += 25.0;
        } else if (board[corners[i][0]][corners[i][1]] == opponentPiece) {
            score -= 25.0;
        }
    }
    
    int xSquares[4][2] = {{1,1}, {1,6}, {6,1}, {6,6}};
    for (int i = 0; i < 4; i++) {
        int cornerR = (xSquares[i][0] == 1) ? 0 : 7;
        int cornerC = (xSquares[i][1] == 1) ? 0 : 7;
        
        if (board[cornerR][cornerC] == EMPTY) {
            if (board[xSquares[i][0]][xSquares[i][1]] == playerPiece) {
                score -= 15.0;
            } else if (board[xSquares[i][0]][xSquares[i][1]] == opponentPiece) {
                score += 15.0;
            }
        }
    }
    
    for (int i = 0; i < BOARD_SIZE; i++) {
        if (board[0][i] == playerPiece) score += 5.0;
        else if (board[0][i] == opponentPiece) score -= 5.0;
        
        if (board[7][i] == playerPiece) score += 5.0;
        else if (board[7][i] == opponentPiece) score -= 5.0;
        
        if (i > 0 && i < 7) {
            if (board[i][0] == playerPiece) score += 5.0;
            else if (board[i][0] == opponentPiece) score -= 5.0;
            
            if (board[i][7] == playerPiece) score += 5.0;
            else if (board[i][7] == opponentPiece) score -= 5.0;
        }
    }
    
    return score;
}

double evaluateFrontier(char board[BOARD_SIZE][BOARD_SIZE], int forPlayer) {
    int playerFrontier = 0, opponentFrontier = 0;
    char playerPiece = (forPlayer == RED_TURN) ? RED : BLUE;
    char opponentPiece = (forPlayer == RED_TURN) ? BLUE : RED;
    
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board[i][j] == playerPiece || board[i][j] == opponentPiece) {
                int hasEmptyNeighbor = 0;
                
                for (int di = -1; di <= 1; di++) {
                    for (int dj = -1; dj <= 1; dj++) {
                        if (di == 0 && dj == 0) continue;
                        int ni = i + di, nj = j + dj;
                        if (ni >= 0 && ni < BOARD_SIZE && nj >= 0 && nj < BOARD_SIZE) {
                            if (board[ni][nj] == EMPTY) {
                                hasEmptyNeighbor = 1;
                                break;
                            }
                        }
                    }
                    if (hasEmptyNeighbor) break;
                }
                
                if (hasEmptyNeighbor) {
                    if (board[i][j] == playerPiece) playerFrontier++;
                    else opponentFrontier++;
                }
            }
        }
    }
    
    if (playerFrontier + opponentFrontier > 0) {
        return -10.0 * (playerFrontier - opponentFrontier) / (playerFrontier + opponentFrontier);
    }
    
    return 0.0;
}

double evaluatePotential(char board[BOARD_SIZE][BOARD_SIZE], int forPlayer) {
    double score = 0.0;
    char playerPiece = (forPlayer == RED_TURN) ? RED : BLUE;
    char opponentPiece = (forPlayer == RED_TURN) ? BLUE : RED;
    
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board[i][j] == EMPTY) {
                int playerFlips = 0, opponentFlips = 0;
                
                for (int di = -1; di <= 1; di++) {
                    for (int dj = -1; dj <= 1; dj++) {
                        if (di == 0 && dj == 0) continue;
                        int ni = i + di, nj = j + dj;
                        if (ni >= 0 && ni < BOARD_SIZE && nj >= 0 && nj < BOARD_SIZE) {
                            if (board[ni][nj] == opponentPiece) playerFlips++;
                            else if (board[ni][nj] == playerPiece) opponentFlips++;
                        }
                    }
                }
                
                double posValue = POSITION_WEIGHTS[getGamePhase(board)][i][j];
                score += (playerFlips - opponentFlips) * posValue;
            }
        }
    }
    
    return score;
}

// Pattern functions
void learnPattern(char board[BOARD_SIZE][BOARD_SIZE], Move move, double outcome) {
    if (patternCount >= MAX_PATTERNS) return;
    
    int r = move.r2;
    int c = move.c2;
    
    if (r >= 1 && r <= 6 && c >= 1 && c <= 6) {
        Pattern* pattern = &learnedPatterns[patternCount];
        
        for (int i = 0; i < PATTERN_SIZE; i++) {
            for (int j = 0; j < PATTERN_SIZE; j++) {
                pattern->pattern[i][j] = board[r-1+i][c-1+j];
            }
        }
        
        pattern->score = outcome * 10.0;
        pattern->frequency = 1;
        patternCount++;
    }
}

double evaluatePatterns(char board[BOARD_SIZE][BOARD_SIZE], int forPlayer) {
    double score = 0.0;
    
    for (int p = 0; p < patternCount; p++) {
        Pattern* pattern = &learnedPatterns[p];
        
        for (int i = 0; i <= BOARD_SIZE - PATTERN_SIZE; i++) {
            for (int j = 0; j <= BOARD_SIZE - PATTERN_SIZE; j++) {
                int matches = 1;
                
                for (int pi = 0; pi < PATTERN_SIZE && matches; pi++) {
                    for (int pj = 0; pj < PATTERN_SIZE && matches; pj++) {
                        if (pattern->pattern[pi][pj] != '?' &&
                            pattern->pattern[pi][pj] != board[i + pi][j + pj]) {
                            matches = 0;
                        }
                    }
                }
                
                if (matches) {
                    score += pattern->score * (pattern->frequency / 100.0);
                }
            }
        }
    }
    
    if (board[3][3] == board[3][4] && board[3][3] == board[4][3] && 
        board[3][3] == board[4][4] && board[3][3] != EMPTY) {
        if (board[3][3] == ((forPlayer == RED_TURN) ? RED : BLUE)) {
            score += 20.0;
        } else {
            score -= 20.0;
        }
    }
    
    return score;
}

#ifdef HAS_NNUE_WEIGHTS
typedef struct {
    int16_t hidden1[NNUE_HIDDEN1_SIZE];
    int16_t hidden2[NNUE_HIDDEN2_SIZE];
} NNUEAccumulator;

static int nnue_evaluate(char board[BOARD_SIZE][BOARD_SIZE], int forPlayer) {
    int16_t input[192] = {0};
    
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            int idx = i * 8 + j;
            if (board[i][j] == RED) {
                input[idx] = NNUE_SCALE;
            } else if (board[i][j] == BLUE) {
                input[idx + 64] = NNUE_SCALE;
            } else if (board[i][j] == EMPTY) {
                input[idx + 128] = NNUE_SCALE;
            }
        }
    }
    
    int32_t acc1[NNUE_HIDDEN1_SIZE];
    for (int i = 0; i < NNUE_HIDDEN1_SIZE; i++) {
        acc1[i] = nnue_b1[i] * NNUE_SCALE;
        for (int j = 0; j < 192; j++) {
            acc1[i] += (int32_t)input[j] * nnue_w1[j][i];
        }
        acc1[i] = acc1[i] > 0 ? acc1[i] : 0;
        acc1[i] /= NNUE_SCALE;
    }
    
    int32_t acc2[NNUE_HIDDEN2_SIZE];
    for (int i = 0; i < NNUE_HIDDEN2_SIZE; i++) {
        acc2[i] = nnue_b2[i] * NNUE_SCALE;
        for (int j = 0; j < NNUE_HIDDEN1_SIZE; j++) {
            acc2[i] += acc1[j] * nnue_w2[j][i];
        }
        acc2[i] = acc2[i] > 0 ? acc2[i] : 0;
        acc2[i] /= NNUE_SCALE;
    }
    
    int32_t output = nnue_b3[0] * NNUE_SCALE;
    for (int i = 0; i < NNUE_HIDDEN2_SIZE; i++) {
        output += acc2[i] * nnue_w3[i];
    }
    
    output /= NNUE_SCALE;
    if (output > NNUE_OUTPUT_SCALE) output = NNUE_OUTPUT_SCALE;
    if (output < -NNUE_OUTPUT_SCALE) output = -NNUE_OUTPUT_SCALE;
    
    return (forPlayer == RED_TURN) ? output : -output;
}
#endif

// Hybrid Alpha-Beta
Move getHybridAlphaBetaMove(char board[BOARD_SIZE][BOARD_SIZE], int currentPlayer) {
    Move bestMove = {0, 0, 0, 0, 0, 0};
    Move lastGoodMove = {0, 0, 0, 0, 0, 0};
    
    clock_gettime(CLOCK_MONOTONIC, &searchStart);
    atomic_store(&timeUp, 0);
    atomic_store(&nodeCount, 0);
    currentAge++;
    
    memset(killerMoves, 0, sizeof(killerMoves));
    
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            for (int k = 0; k < BOARD_SIZE; k++) {
                for (int l = 0; l < BOARD_SIZE; l++) {
                    historyTable[i][j][k][l] /= 2;
                    butterflyTable[i][j][k][l] /= 2;
                }
            }
        }
    }
    
    int gamePhase = getGamePhase(board);
    int emptyCount = countPieces(board, EMPTY);
    int maxDepth;
    
    if (timeAllocated < 0.8) {
        maxDepth = 4;
        safePrint("EMERGENCY: Very limited time (%.2fs), max_depth=4\n", timeAllocated);
    } else if (timeAllocated < 1.5) {
        maxDepth = 5;
        safePrint("Very limited time (%.2fs), max_depth=5\n", timeAllocated);
    } else if (timeAllocated < 2.0) {
        maxDepth = 6;
        safePrint("Limited time (%.2fs), max_depth=6\n", timeAllocated);
    } else if (timeAllocated < 3.0) {
        maxDepth = (gamePhase == 0) ? 7 : (gamePhase == 1) ? 8 : 9;
    } else {
        if (emptyCount < 8) {
            maxDepth = 10;
        } else if (gamePhase == 0) {
            maxDepth = 7;
        } else if (gamePhase == 1) {
            maxDepth = 8;
        } else {
            maxDepth = 9;
        }
    }
    
    safePrint("Hybrid Alpha-Beta: phase=%d, empty=%d, max_depth=%d, time=%.2fs\n", 
             gamePhase, emptyCount, maxDepth, timeAllocated);
    
    Move moves[MAX_MOVES];
    int moveCount;
    getAllValidMoves(board, currentPlayer, moves, &moveCount);
    
    if (moveCount == 0) {
        return bestMove;
    }
    
    bestMove = moves[0];
    lastGoodMove = moves[0];
    
    if (moveCount == 1) {
        safePrint("Only one move available, returning immediately\n");
        return bestMove;
    }
    
    int previousScore = 0;
    int consecutiveFailures = 0;
    double lastIterationTime = 0.0;
    int completedDepth = 0;
    
    for (int depth = 1; depth <= maxDepth && !atomic_load(&timeUp); depth++) {
        double depthStartTime = elapsedSeconds();
        
        double timeUsed = depthStartTime;
        double timeRemaining = timeAllocated - timeUsed;
        
        if (depth > 2 && lastIterationTime > 0) {
            double estimatedTime = lastIterationTime * 2.2;
            
            if (timeUsed + estimatedTime > timeAllocated * 0.85) {
                safePrint("  Depth %d: Insufficient time (used %.2fs, est next %.2fs)\n", 
                         depth, timeUsed, estimatedTime);
                break;
            }
        }
        
        if (timeUsed > timeAllocated * 0.75) {
            safePrint("  Time limit approaching (%.1f%%), stopping at depth %d\n", 
                     (timeUsed / timeAllocated) * 100, depth - 1);
            break;
        }
        
        Move iterationBest = bestMove;
        
        int alpha = AI_NEG_INFINITY;
        int beta = AI_INFINITY;
        
        if (depth > 3 && abs(previousScore) < AI_INFINITY / 2) {
            int window = 50;
            alpha = previousScore - window;
            beta = previousScore + window;
        }
        
        int score = enhancedAlphaBeta(board, depth, alpha, beta, currentPlayer, 
                                     &iterationBest, 1, 1);
        
        if ((score <= alpha || score >= beta) && !atomic_load(&timeUp)) {
            score = enhancedAlphaBeta(board, depth, AI_NEG_INFINITY, AI_INFINITY, 
                                     currentPlayer, &iterationBest, 1, 1);
        }
        
        double depthTime = elapsedSeconds() - depthStartTime;
        lastIterationTime = depthTime;
        
        if (atomic_load(&timeUp)) {
            safePrint("  Depth %d incomplete (timeout), using depth %d result\n", 
                     depth, completedDepth);
            bestMove = lastGoodMove;
            break;
        }
        
        bestMove = iterationBest;
        lastGoodMove = iterationBest;
        previousScore = score;
        completedDepth = depth;
        
        long nodes = atomic_load(&nodeCount);
        double totalTime = elapsedSeconds();
        double nps = nodes / (totalTime + 0.001);
        
        safePrint("  Depth %d: score=%.2f, nodes=%ld, time=%.2fs (depth:%.2fs), nps=%.0f\n",
                 depth, score / 100.0, nodes, totalTime, depthTime, nps);
        
        if (score > AI_INFINITY/2 || score < -AI_INFINITY/2) {
            safePrint("  Found winning/losing line!\n");
            break;
        }
        
        if (totalTime > timeAllocated * 0.80) {
            safePrint("  Approaching time limit (%.1f%%), stopping search\n",
                     (totalTime / timeAllocated) * 100);
            break;
        }
        
        if (depth >= 3 && depthTime > timeRemaining * 0.3) {
            safePrint("  Next depth would likely exceed time limit\n");
            break;
        }
        
        if (depth > 5 && abs(score - previousScore) < 10) {
            consecutiveFailures++;
            if (consecutiveFailures >= 2) {
                safePrint("  Score stable, ending search early\n");
                break;
            }
        } else {
            consecutiveFailures = 0;
        }
        
        if (emptyCount < 8 && depth >= 6) {
            safePrint("  Sufficient depth for endgame position\n");
            break;
        }
    }
    
    double finalTime = elapsedSeconds();
    if (finalTime > timeAllocated * 0.90) {
        safePrint("WARNING: Close to timeout! Used %.2fs of %.2fs (%.1f%%)\n", 
                 finalTime, timeAllocated, (finalTime / timeAllocated) * 100);
    }
    
    long totalNodes = atomic_load(&nodeCount);
    safePrint("Search complete: %ld nodes in %.2fs (%.0f nps), depth %d\n", 
             totalNodes, finalTime, totalNodes / (finalTime + 0.001), completedDepth);
    
    if (bestMove.r1 == 0 && bestMove.c1 == 0 && 
        bestMove.r2 == 0 && bestMove.c2 == 0 && moveCount > 0) {
        safePrint("ERROR: Alpha-beta returned invalid pass, using first valid move\n");
        bestMove = moves[0];
    }
    
    return bestMove;
}

// Neural Pattern move
Move getNeuralPatternMove(char board[BOARD_SIZE][BOARD_SIZE], int currentPlayer) {
    Move moves[MAX_MOVES];
    int moveCount;
    getAllValidMoves(board, currentPlayer, moves, &moveCount);
    
    if (moveCount == 0) {
        return (Move){0, 0, 0, 0, 0, 0};
    }
    
    for (int i = 0; i < moveCount; i++) {
        moves[i].score = 0;
        
        char tempBoard[BOARD_SIZE][BOARD_SIZE];
        memcpy(tempBoard, board, sizeof(tempBoard));
        gameState.currentPlayer = currentPlayer;
        makeMove(tempBoard, moves[i]);
        gameState.currentPlayer = (my_color == RED) ? RED_TURN : BLUE_TURN;
        
        moves[i].score = (int)(evaluatePatterns(tempBoard, currentPlayer) * 10);
        
        moves[i].score += (int)(POSITION_WEIGHTS[getGamePhase(board)][moves[i].r2][moves[i].c2] * 100);
        
        char opponentPiece = (currentPlayer == RED_TURN) ? BLUE : RED;
        for (int dr = -1; dr <= 1; dr++) {
            for (int dc = -1; dc <= 1; dc++) {
                if (dr == 0 && dc == 0) continue;
                int nr = moves[i].r2 + dr;
                int nc = moves[i].c2 + dc;
                if (nr >= 0 && nr < BOARD_SIZE && nc >= 0 && nc < BOARD_SIZE &&
                    board[nr][nc] == opponentPiece) {
                    moves[i].score += 50;
                }
            }
        }
        
        Move futureMoves[MAX_MOVES];
        int futureMoveCount;
        getAllValidMoves(tempBoard, currentPlayer, futureMoves, &futureMoveCount);
        moves[i].score += futureMoveCount * 2;
    }
    
    int bestIdx = 0;
    for (int i = 1; i < moveCount; i++) {
        if (moves[i].score > moves[bestIdx].score) {
            bestIdx = i;
        }
    }
    
    return moves[bestIdx];
}

// Opening Book move
Move getOpeningBookMove(char board[BOARD_SIZE][BOARD_SIZE], int currentPlayer) {
    int pieceCount = countPieces(board, RED) + countPieces(board, BLUE);
    
    if (pieceCount > 20) {
        return (Move){0, 0, 0, 0, 0, 0};
    }
    
    safePrint("Enhanced opening book: %d pieces\n", pieceCount);
    
    if (pieceCount == 4) {
        if (board[0][0] == RED && board[7][7] == RED && currentPlayer == RED_TURN) {
            if (board[1][1] == EMPTY) return (Move){0, 0, 1, 1, 0, CLONE};
            if (board[6][6] == EMPTY) return (Move){7, 7, 6, 6, 0, CLONE};
        }
    }
    
    Move moves[MAX_MOVES];
    int moveCount;
    getAllValidMoves(board, currentPlayer, moves, &moveCount);
    
    if (moveCount == 0) return (Move){0, 0, 0, 0, 0, 0};
    
    Move bestMove = moves[0];
    double bestScore = -AI_INFINITY;
    
    for (int i = 0; i < moveCount && i < 15; i++) {
        double score = 0.0;
        
        double centerDist = sqrt(pow(moves[i].r2 - 3.5, 2) + pow(moves[i].c2 - 3.5, 2));
        score -= centerDist * 20.0;
        
        if ((moves[i].r2 == 0 || moves[i].r2 == 7) && 
            (moves[i].c2 == 0 || moves[i].c2 == 7)) {
            score -= 100.0;
        }
        
        char opponentPiece = (currentPlayer == RED_TURN) ? BLUE : RED;
        for (int dr = -1; dr <= 1; dr++) {
            for (int dc = -1; dc <= 1; dc++) {
                if (dr == 0 && dc == 0) continue;
                int nr = moves[i].r2 + dr;
                int nc = moves[i].c2 + dc;
                if (nr >= 0 && nr < BOARD_SIZE && nc >= 0 && nc < BOARD_SIZE &&
                    board[nr][nc] == opponentPiece) {
                    score += 50.0;
                }
            }
        }
        
        if (score > bestScore) {
            bestScore = score;
            bestMove = moves[i];
        }
    }
    
    return bestMove;
}

// Endgame Solver
Move getEndgameSolverMove(char board[BOARD_SIZE][BOARD_SIZE], int currentPlayer) {
    Move bestMove = {0, 0, 0, 0, 0, 0};
    
    clock_gettime(CLOCK_MONOTONIC, &searchStart);
    atomic_store(&timeUp, 0);
    atomic_store(&nodeCount, 0);
    
    int emptyCount = countPieces(board, EMPTY);
    
    if (emptyCount == 1) {
        int emptyR = -1, emptyC = -1;
        for (int i = 0; i < BOARD_SIZE; i++) {
            for (int j = 0; j < BOARD_SIZE; j++) {
                if (board[i][j] == EMPTY) {
                    emptyR = i;
                    emptyC = j;
                    break;
                }
            }
            if (emptyR != -1) break;
        }
        
        Move moves[MAX_MOVES];
        int moveCount;
        getAllValidMoves(board, currentPlayer, moves, &moveCount);
        
        for (int i = 0; i < moveCount; i++) {
            if (moves[i].r2 == emptyR && moves[i].c2 == emptyC) {
                safePrint("Endgame: Moving to the only empty square (%d,%d)\n", 
                         emptyR+1, emptyC+1);
                return moves[i];
            }
        }
        
        safePrint("Endgame: Cannot move to empty square, passing\n");
        return (Move){0, 0, 0, 0, 0, 0};
    }
    
    int maxDepth;
    
    if (timeAllocated < 1.5) {
        maxDepth = 4;
        safePrint("Endgame: Very limited time, depth=%d\n", maxDepth);
    } else if (timeAllocated < 2.5) {
        maxDepth = (emptyCount <= 4) ? 6 : 5;
        safePrint("Endgame: Limited time, depth=%d\n", maxDepth);
    } else if (emptyCount <= 4) {
        maxDepth = 8;
    } else if (emptyCount <= 6) {
        maxDepth = 7;
    } else if (emptyCount <= 8) {
        maxDepth = 6;
    } else {
        maxDepth = 5;
    }
    
    safePrint("Endgame solver: %d empty squares, searching to depth %d, time=%.2fs\n", 
             emptyCount, maxDepth, timeAllocated);
    
    Move moves[MAX_MOVES];
    int moveCount;
    getAllValidMoves(board, currentPlayer, moves, &moveCount);
    
    if (moveCount == 0) {
        return bestMove;
    }
    
    if (moveCount == 1) {
        safePrint("Only one move available, returning immediately\n");
        return moves[0];
    }
    
    if (emptyCount <= 3 && moveCount > 1) {
        for (int i = 0; i < moveCount; i++) {
            int isRepetitive = 0;
            
            for (int j = 0; j < 3; j++) {
                Move* histMove = &lastMoves[j];
                if (moves[i].r1 == histMove->r2 && moves[i].c1 == histMove->c2 &&
                    moves[i].r2 == histMove->r1 && moves[i].c2 == histMove->c1) {
                    isRepetitive = 1;
                    break;
                }
            }
            
            if (!isRepetitive) {
                Move temp = moves[0];
                moves[0] = moves[i];
                moves[i] = temp;
                break;
            }
        }
    }
    
    if (timeAllocated < 1.0) {
        safePrint("Emergency endgame evaluation\n");
        
        double bestScore = -AI_INFINITY;
        for (int i = 0; i < moveCount && i < 10; i++) {
            char tempBoard[BOARD_SIZE][BOARD_SIZE];
            memcpy(tempBoard, board, sizeof(tempBoard));
            makeMove(tempBoard, moves[i]);
            
            double score = evaluateBoard(tempBoard, currentPlayer, 0);
            if (score > bestScore) {
                bestScore = score;
                bestMove = moves[i];
            }
        }
        
        return bestMove;
    }
    
    enhancedAlphaBeta(board, maxDepth, AI_NEG_INFINITY, AI_INFINITY, 
                     currentPlayer, &bestMove, 1, 1);
    
    if (atomic_load(&timeUp)) {
        safePrint("Endgame search timeout, using best found so far\n");
    }
    
    if (bestMove.r1 == 0 && bestMove.c1 == 0 && 
        bestMove.r2 == 0 && bestMove.c2 == 0 && moveCount > 0) {
        safePrint("Endgame solver failed, using first valid move\n");
        bestMove = moves[0];
    }
    
    return bestMove;
}

// Get AI move - main entry point
Move getAIMove() {
    pthread_mutex_lock(&gameMutex);
    
    Move bestMove = {0, 0, 0, 0, 0, 0};
    char board[BOARD_SIZE][BOARD_SIZE];
    memcpy(board, gameState.board, sizeof(board));
    int currentPlayer = (my_color == RED) ? RED_TURN : BLUE_TURN;
    
    pthread_mutex_unlock(&gameMutex);
    
    Move moves[MAX_MOVES];
    int moveCount;
    getAllValidMoves(board, currentPlayer, moves, &moveCount);
    
    if (moveCount == 0) {
        return bestMove;
    }
    
    int emptyCount = countPieces(board, EMPTY);
    int moveNumber = 60 - emptyCount;
    
    if (moveNumber < 10) {
        timeAllocated = TIME_LIMIT_S * 0.8;
    } else if (moveNumber < 30) {
        timeAllocated = TIME_LIMIT_S * 0.85;
    } else if (emptyCount > 10) {
        timeAllocated = TIME_LIMIT_S * 0.85;
    } else {
        if (emptyCount <= 6) {
            timeAllocated = TIME_LIMIT_S * 0.7;
        } else {
            timeAllocated = TIME_LIMIT_S * 0.8;
        }
    }

    timeAllocated -= 0.3;
    
    if (timeAllocated < 0.5) {
        timeAllocated = 0.5;
    }
    
    if (timeAllocated < 1.0) {
        safePrint("WARNING: Very limited time available: %.2fs\n", timeAllocated);
    }
    
    safePrint("\n=== AI THINKING ===\n");
    safePrint("Engine: %d, Time: %.2fs, Moves: %d, Empty: %d\n", 
             aiEngine, timeAllocated, moveCount, emptyCount);
    
    switch (aiEngine) {
        case ENGINE_HYBRID_ALPHABETA:
            bestMove = getHybridAlphaBetaMove(board, currentPlayer);
            break;
            
        case ENGINE_PARALLEL_MCTS:
            bestMove = getParallelMCTSMove(board, currentPlayer);
            break;
            
        case ENGINE_NEURAL_PATTERN:
            bestMove = getNeuralPatternMove(board, currentPlayer);
            break;
            
        case ENGINE_OPENING_BOOK:
            bestMove = getOpeningBookMove(board, currentPlayer);
            break;
            
        case ENGINE_ENDGAME_SOLVER:
            bestMove = getEndgameSolverMove(board, currentPlayer);
            break;
            
        case ENGINE_TOURNAMENT_BEAST:
            bestMove = getTournamentBeastMove(board, currentPlayer);
            break;
            
        case ENGINE_ULTIMATE_AI:
            bestMove = getUltimateAIMove(board, currentPlayer);
            break;
            
        case ENGINE_HYBRID_MCTS:
            bestMove = getHybridMCTSMove(board, currentPlayer);
            break;
            
        case ENGINE_ENDGAME_GOD:
            bestMove = getEndgameGodMove(board, currentPlayer);
            break;
            
        default:
            bestMove = getTournamentBeastMove(board, currentPlayer);
            break;
    }
    
    static Move recentMoves[5] = {0};
    static int recentMoveIndex = 0;
    
    int isRepetitive = 0;
    for (int i = 0; i < 5; i++) {
        Move* recent = &recentMoves[i];
        if (bestMove.r1 == recent->r2 && bestMove.c1 == recent->c2 &&
            bestMove.r2 == recent->r1 && bestMove.c2 == recent->c1) {
            isRepetitive = 1;
            safePrint("Detected exact reverse of recent move %d\n", i);
            break;
        }
    }
    
    if (isRepetitive && moveCount > 1) {
        safePrint("Avoiding repetitive move, finding alternative...\n");
        
        Move alternativeMove = bestMove;
        double bestAltScore = -AI_INFINITY;
        
        for (int i = 0; i < moveCount && i < 20; i++) {
            int rep = 0;
            
            for (int j = 0; j < 5; j++) {
                Move* recent = &recentMoves[j];
                if (moves[i].r1 == recent->r2 && moves[i].c1 == recent->c2 &&
                    moves[i].r2 == recent->r1 && moves[i].c2 == recent->c1) {
                    rep = 1;
                    break;
                }
            }
            
            if (!rep) {
                char tempBoard[BOARD_SIZE][BOARD_SIZE];
                memcpy(tempBoard, board, sizeof(tempBoard));
                makeMove(tempBoard, moves[i]);
                
                double score = evaluateBoard(tempBoard, currentPlayer, 0);
                
                if (score > bestAltScore) {
                    bestAltScore = score;
                    alternativeMove = moves[i];
                }
            }
        }
        
        if (bestAltScore > -500.0) {
            bestMove = alternativeMove;
            safePrint("Selected alternative move with score %.1f\n", bestAltScore);
        }
    }
    
    if (bestMove.r1 == 0 && bestMove.c1 == 0 && 
        bestMove.r2 == 0 && bestMove.c2 == 0 && moveCount > 0) {
        safePrint("WARNING: AI returned invalid pass, using first valid move\n");
        bestMove = moves[0];
    }
    
    recentMoves[recentMoveIndex % 5] = bestMove;
    recentMoveIndex++;
    
    double totalTime = elapsedSeconds();
    if (totalTime > timeAllocated) {
        safePrint("WARNING: Exceeded allocated time! Used %.2fs of %.2fs\n", 
                 totalTime, timeAllocated);
    }
    
    bestMove.r1++;
    bestMove.c1++;
    bestMove.r2++;
    bestMove.c2++;
    
    return bestMove;
}

// Get human move
Move getHumanMove() {
    Move move = {0, 0, 0, 0, 0, 0};
    
    safePrint("\n=== YOUR TURN ===\n");
    safePrint("Enter move (sx sy tx ty) or '0 0 0 0' to pass: ");
    fflush(stdout);
    
    char input[256];
    if (fgets(input, sizeof(input), stdin) == NULL) {
        safePrint("Input error! Using pass.\n");
        return move;
    }
    
    input[strcspn(input, "\n")] = 0;
    
    if (strcmp(input, "quit") == 0 || strcmp(input, "exit") == 0) {
        safePrint("Exiting...\n");
        atomic_store(&gameOver, 1);
        return move;
    }
    
    int sx, sy, tx, ty;
    int parsed = sscanf(input, "%d %d %d %d", &sx, &sy, &tx, &ty);
    
    if (parsed != 4) {
        safePrint("Invalid format! Expected: 'sx sy tx ty' (e.g., '1 1 2 2')\n");
        safePrint("Got: '%s' (parsed %d values)\n", input, parsed);
        return getHumanMove();
    }
    
    move.r1 = sx;
    move.c1 = sy;
    move.r2 = tx;
    move.c2 = ty;
    
    if (!(sx == 0 && sy == 0 && tx == 0 && ty == 0)) {
        if (!isValidMove(sx-1, sy-1, tx-1, ty-1)) {
            safePrint("Invalid move! ");
            
            if (sx < 1 || sx > 8 || sy < 1 || sy > 8 || tx < 1 || tx > 8 || ty < 1 || ty > 8) {
                safePrint("Coordinates must be between 1 and 8.\n");
            } else if (gameState.board[sx-1][sy-1] != my_color) {
                safePrint("You can only move your own pieces (%c).\n", my_color);
            } else if (gameState.board[tx-1][ty-1] != EMPTY) {
                safePrint("Destination must be empty.\n");
            } else {
                safePrint("Move type not valid (must be adjacent or 2 squares away).\n");
            }
            
            return getHumanMove();
        }
        
        move.moveType = getMoveType(sx-1, sy-1, tx-1, ty-1);
    }
    
    return move;
}

// Network functions
void sendJSON(cJSON* json) {
    if (!json || sockfd < 0) return;
    
    char* json_str = cJSON_PrintUnformatted(json);
    if (json_str) {
        send(sockfd, json_str, strlen(json_str), 0);
        send(sockfd, "\n", 1, 0);
        safePrint("[CLIENT->SERVER] %s\n", json_str);
        free(json_str);
    }
}

void sendRegister(const char* username) {
    cJSON* message = cJSON_CreateObject();
    cJSON_AddStringToObject(message, "type", "register");
    cJSON_AddStringToObject(message, "username", username);
    sendJSON(message);
    cJSON_Delete(message);
}

void sendMove(int sx, int sy, int tx, int ty) {
    if (atomic_load(&gameOver) || !atomic_load(&myTurn)) {
        safePrint("Cannot send move: game over or not my turn\n");
        return;
    }
    
    cJSON* message = cJSON_CreateObject();
    cJSON_AddStringToObject(message, "type", "move");
    cJSON_AddStringToObject(message, "username", my_username);
    cJSON_AddNumberToObject(message, "sx", sx);
    cJSON_AddNumberToObject(message, "sy", sy);
    cJSON_AddNumberToObject(message, "tx", tx);
    cJSON_AddNumberToObject(message, "ty", ty);
    sendJSON(message);
    cJSON_Delete(message);
}

void updateBoardFromJSON(cJSON* board_array) {
    if (!board_array || !cJSON_IsArray(board_array)) return;
    
    pthread_mutex_lock(&gameMutex);
    
    int i = 0;
    cJSON* row = NULL;
    cJSON_ArrayForEach(row, board_array) {
        if (cJSON_IsString(row) && i < BOARD_SIZE) {
            const char* row_str = row->valuestring;
            for (int j = 0; j < BOARD_SIZE && row_str[j]; j++) {
                gameState.board[i][j] = row_str[j];
            }
            i++;
        }
    }
    
    gameState.gamePhase = getGamePhase(gameState.board);
    
    pthread_mutex_unlock(&gameMutex);
}

void printBoard() {
    pthread_mutex_lock(&gameMutex);
    
    safePrint("\n=== Current Board ===\n");
    safePrint("  1 2 3 4 5 6 7 8\n");
    
    for (int i = 0; i < BOARD_SIZE; i++) {
        safePrint("%d ", i + 1);
        for (int j = 0; j < BOARD_SIZE; j++) {
            safePrint("%c ", gameState.board[i][j]);
        }
        safePrint("\n");
    }
    
    int red_count = countPieces(gameState.board, RED);
    int blue_count = countPieces(gameState.board, BLUE);
    int empty_count = countPieces(gameState.board, EMPTY);
    
    safePrint("\nRed: %d, Blue: %d, Empty: %d\n", red_count, blue_count, empty_count);
    safePrint("You are: %s (%c)\n", my_color == RED ? "Red" : "Blue", my_color);
    safePrint("====================\n");
    
    pthread_mutex_unlock(&gameMutex);
}

// Message handlers
void handleRegisterResponse(cJSON* message) {
    cJSON* type = cJSON_GetObjectItem(message, "type");
    if (!type || !cJSON_IsString(type)) return;
    
    if (strcmp(type->valuestring, "register_ack") == 0) {
        safePrint("✓ Registration successful! Waiting for another player...\n");
    } else if (strcmp(type->valuestring, "register_nack") == 0) {
        cJSON* reason = cJSON_GetObjectItem(message, "reason");
        if (reason && cJSON_IsString(reason)) {
            safePrint("✗ Registration failed: %s\n", reason->valuestring);
        }
        cleanup();
        exit(1);
    }
}

void handleGameStart(cJSON* message) {
    pthread_mutex_lock(&gameMutex);
    
    safePrint("\n🎮 GAME STARTED! 🎮\n");
    
    cJSON* players = cJSON_GetObjectItem(message, "players");
    if (players && cJSON_IsArray(players)) {
        cJSON* player1 = cJSON_GetArrayItem(players, 0);
        cJSON* player2 = cJSON_GetArrayItem(players, 1);
        
        if (player1 && player2 && cJSON_IsString(player1) && cJSON_IsString(player2)) {
            safePrint("Players: %s (Red) vs %s (Blue)\n", 
                     player1->valuestring, player2->valuestring);
            
            if (strcmp(my_username, player1->valuestring) == 0) {
                my_color = RED;
                gameState.currentPlayer = RED_TURN;
                safePrint("You are playing as: Red (R)\n");
            } else {
                my_color = BLUE;
                gameState.currentPlayer = BLUE_TURN;
                safePrint("You are playing as: Blue (B)\n");
            }
        }
    }
    
    cJSON* first_player = cJSON_GetObjectItem(message, "first_player");
    if (first_player && cJSON_IsString(first_player)) {
        safePrint("First player: %s\n", first_player->valuestring);
    }
    
    atomic_store(&gameStarted, 1);
    gameState.moveCount = 0;
    gameState.lastMoveWasPass = 0;
    
    pthread_mutex_unlock(&gameMutex);
}

void handleYourTurn(cJSON* message) {
    if (atomic_load(&gameOver) || !atomic_load(&gameStarted)) {
        return;
    }
    
    safePrint("\n=== YOUR TURN ===\n");
    
    cJSON* board = cJSON_GetObjectItem(message, "board");
    if (board) {
        updateBoardFromJSON(board);
        printBoard();
    }
    
    cJSON* timeout = cJSON_GetObjectItem(message, "timeout");
    if (timeout && cJSON_IsNumber(timeout)) {
        safePrint("Timeout: %.0f seconds\n", timeout->valuedouble);
        gameState.timeRemaining = timeout->valuedouble;
    }
    
    atomic_store(&myTurn, 1);
    
    Move bestMove;
    if (humanMode) {
        bestMove = getHumanMove();
    } else {
        bestMove = getAIMove();
    }
    
    if (!atomic_load(&myTurn) || atomic_load(&gameOver)) {
        safePrint("Turn changed or game ended, not sending move.\n");
        return;
    }
    
    if (bestMove.r1 == 0 && bestMove.c1 == 0 && 
        bestMove.r2 == 0 && bestMove.c2 == 0) {
        safePrint(">>> Sending PASS\n");
    } else {
        safePrint(">>> Sending move: (%d,%d) -> (%d,%d)\n", 
                 bestMove.r1, bestMove.c1, bestMove.r2, bestMove.c2);
    }
    
    sendMove(bestMove.r1, bestMove.c1, bestMove.r2, bestMove.c2);
    
    atomic_store(&myTurn, 0);
}

void handleGameOver(cJSON* message) {
    safePrint("\n🏁 GAME OVER! 🏁\n");
    safePrint("================\n");
    
    cJSON* scores = cJSON_GetObjectItem(message, "scores");
    if (scores && cJSON_IsObject(scores)) {
        safePrint("Final Scores:\n");
        
        cJSON* score = NULL;
        int my_score = 0, opponent_score = 0;
        
        cJSON_ArrayForEach(score, scores) {
            if (cJSON_IsNumber(score)) {
                int score_value = (int)score->valuedouble;
                safePrint("  %s: %d points\n", score->string, score_value);
                
                if (strcmp(score->string, my_username) == 0) {
                    my_score = score_value;
                } else {
                    opponent_score = score_value;
                }
            }
        }
        
        safePrint("\n");
        if (my_score > opponent_score) {
            safePrint("🎉 YOU WIN! 🎉\n");
        } else if (opponent_score > my_score) {
            safePrint("😔 You lost.\n");
        } else {
            safePrint("🤝 It's a DRAW!\n");
        }
    }
    
    atomic_store(&gameStarted, 0);
    atomic_store(&gameOver, 1);
    atomic_store(&myTurn, 0);
}

void handleMoveResponse(cJSON* message) {
    cJSON* type = cJSON_GetObjectItem(message, "type");
    if (!type || !cJSON_IsString(type)) return;
    
    if (strcmp(type->valuestring, "move_ok") == 0) {
        safePrint("✅ Move accepted!\n");
    } else if (strcmp(type->valuestring, "invalid_move") == 0) {
        safePrint("❌ Invalid move!\n");
        cJSON* reason = cJSON_GetObjectItem(message, "reason");
        if (reason && cJSON_IsString(reason)) {
            safePrint("Reason: %s\n", reason->valuestring);
            if (strcmp(reason->valuestring, "not your turn") == 0) {
                atomic_store(&myTurn, 0);
            }
        }
    }
    
    cJSON* board = cJSON_GetObjectItem(message, "board");
    if (board) {
        updateBoardFromJSON(board);
        printBoard();
    }
    
    cJSON* next_player = cJSON_GetObjectItem(message, "next_player");
    if (next_player && cJSON_IsString(next_player)) {
        if (strcmp(next_player->valuestring, my_username) == 0) {
            if (strcmp(type->valuestring, "move_ok") == 0) {
                atomic_store(&myTurn, 1);
                safePrint("Still your turn!\n");
            }
        } else {
            atomic_store(&myTurn, 0);
            safePrint("Waiting for %s...\n", next_player->valuestring);
        }
    }
}

void handlePass(cJSON* message) {
    safePrint("\n⏭️  A player passed their turn.\n");
    
    cJSON* next_player = cJSON_GetObjectItem(message, "next_player");
    if (next_player && cJSON_IsString(next_player)) {
        if (strcmp(next_player->valuestring, my_username) == 0) {
            atomic_store(&myTurn, 1);
            safePrint("It's now your turn!\n");
        } else {
            atomic_store(&myTurn, 0);
            safePrint("Waiting for %s...\n", next_player->valuestring);
        }
    }
    
    atomic_store(&timeUp, 1);
}

void handleServerMessage(const char* json_str) {
    cJSON* message = cJSON_Parse(json_str);
    if (!message) {
        safePrint("Failed to parse: %s\n", json_str);
        return;
    }
    
    safePrint("[SERVER->CLIENT] %s\n", json_str);
    
    cJSON* type = cJSON_GetObjectItem(message, "type");
    if (!type || !cJSON_IsString(type)) {
        cJSON_Delete(message);
        return;
    }
    
    const char* type_str = type->valuestring;
    
    if (strcmp(type_str, "register_ack") == 0 || 
        strcmp(type_str, "register_nack") == 0) {
        handleRegisterResponse(message);
    } else if (strcmp(type_str, "game_start") == 0) {
        handleGameStart(message);
    } else if (strcmp(type_str, "your_turn") == 0) {
        handleYourTurn(message);
    } else if (strcmp(type_str, "game_over") == 0) {
        handleGameOver(message);
    } else if (strcmp(type_str, "move_ok") == 0 || 
               strcmp(type_str, "invalid_move") == 0) {
        handleMoveResponse(message);
    } else if (strcmp(type_str, "pass") == 0) {
        handlePass(message);
    }
    
    cJSON_Delete(message);
}

void* receiveMessages(void* arg) {
    (void)arg; 
    char buffer[BUFFER_SIZE];
    char recv_buffer[BUFFER_SIZE * 2];
    int recv_len = 0;
    
    while (!atomic_load(&gameOver)) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(sockfd, buffer, BUFFER_SIZE - 1, 0);
        
        if (bytes_received <= 0) {
            if (bytes_received == 0) {
                safePrint("\n💔 Server disconnected.\n");
            } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
                safePrint("\n💔 Connection error: %s\n", strerror(errno));
            }
            atomic_store(&gameOver, 1);
            break;
        }
        
        if (recv_len + bytes_received < BUFFER_SIZE * 2) {
            memcpy(recv_buffer + recv_len, buffer, bytes_received);
            recv_len += bytes_received;
            recv_buffer[recv_len] = '\0';
            
            char* start = recv_buffer;
            char* newline = NULL;
            
            while ((newline = strchr(start, '\n')) != NULL) {
                *newline = '\0';
                handleServerMessage(start);
                start = newline + 1;
            }
            
            int remaining = recv_len - (start - recv_buffer);
            if (remaining > 0) {
                memmove(recv_buffer, start, remaining);
            }
            recv_len = remaining;
        }
    }
    
    return NULL;
}

// Initialize AI system
void initializeAISystem() {
    safePrint("Initializing AI System...\n");
    
    srand(time(NULL));
    
    initZobrist();
    
    initLMRTable();
    
    size_t ttSize = HASH_SIZE * sizeof(TTBucket);
    transpositionTable = (TTBucket*)calloc(1, ttSize);
    if (!transpositionTable) {
        safePrint("Failed to allocate transposition table (%zu bytes)\n", ttSize);
        exit(1);
    }
    
    initThreadPool();
    
    nodePool = (MCTSNode*)malloc(MCTS_NODE_POOL_SIZE * sizeof(MCTSNode));
    if (!nodePool) {
        safePrint("Failed to allocate MCTS node pool\n");
        exit(1);
    }
    
    safePrint("AI System initialized successfully!\n");
    safePrint("- Transposition table: %zu MB\n", ttSize / (1024 * 1024));
    safePrint("- MCTS node pool: %d nodes\n", MCTS_NODE_POOL_SIZE);
    safePrint("- Thread pool: %d threads\n", globalThreadPool ? globalThreadPool->numThreads : 0);
}

// Cleanup AI system
void cleanupAISystem() {
    if (transpositionTable) {
        free(transpositionTable);
        transpositionTable = NULL;
    }
    
    if (nodePool) {
        free(nodePool);
        nodePool = NULL;
    }
    
    destroyThreadPool();
}

// Cleanup
void cleanup() {
    if (sockfd >= 0) {
        close(sockfd);
        sockfd = -1;
    }
    cleanupAISystem();
}

// Signal handler
void sigint_handler(int sig) {
    (void)sig; 
    safePrint("\n👋 Exiting...\n");
    atomic_store(&gameOver, 1);
    cleanup();
    exit(0);
}

// Parallel MCTS Move
Move getParallelMCTSMove(char board[BOARD_SIZE][BOARD_SIZE], int currentPlayer) {
    clock_gettime(CLOCK_MONOTONIC, &searchStart);
    atomic_store(&timeUp, 0);
    atomic_store(&nodeCount, 0);
    
    if (!nodePool) {
        nodePool = (MCTSNode*)calloc(MCTS_NODE_POOL_SIZE, sizeof(MCTSNode));
        if (!nodePool) {
            safePrint("MCTS: Failed to allocate node pool, using fallback\n");
            return getHybridAlphaBetaMove(board, currentPlayer);
        }
    }
    
    pthread_mutex_lock(&nodePoolMutex);
    nodePoolIndex = 0;
    pthread_mutex_unlock(&nodePoolMutex);
    
    Move moves[MAX_MOVES];
    int moveCount;
    getAllValidMoves(board, currentPlayer, moves, &moveCount);
    
    if (moveCount == 0) {
        return (Move){0, 0, 0, 0, 0, 0};
    }
    
    if (moveCount < 5) {
        safePrint("Too few moves for MCTS (%d), using alpha-beta\n", moveCount);
        return getHybridAlphaBetaMove(board, currentPlayer);
    }
    
    if (timeAllocated < 0.5) {
        safePrint("Insufficient time for MCTS (%.2fs), using quick evaluation\n", timeAllocated);
        
        int bestIdx = 0;
        double bestScore = -AI_INFINITY;
        char opponentPiece = (currentPlayer == RED_TURN) ? BLUE : RED;
        
        for (int i = 0; i < moveCount && i < 10; i++) {
            double score = POSITION_WEIGHTS[getGamePhase(board)][moves[i].r2][moves[i].c2] * 20.0;
            
            for (int dr = -1; dr <= 1; dr++) {
                for (int dc = -1; dc <= 1; dc++) {
                    if (dr == 0 && dc == 0) continue;
                    int nr = moves[i].r2 + dr;
                    int nc = moves[i].c2 + dc;
                    if (nr >= 0 && nr < BOARD_SIZE && nc >= 0 && nc < BOARD_SIZE &&
                        board[nr][nc] == opponentPiece) {
                        score += 30.0;
                    }
                }
            }
            
            if (score > bestScore) {
                bestScore = score;
                bestIdx = i;
            }
        }
        
        return moves[bestIdx];
    }
    
    int savedCurrentPlayer = gameState.currentPlayer;
    gameState.currentPlayer = currentPlayer;
    
    MCTSNode* root = createMCTSNode((Move){0, 0, 0, 0, 0, 0}, !currentPlayer, NULL);
    if (!root) {
        gameState.currentPlayer = savedCurrentPlayer;
        safePrint("Failed to create root node\n");
        return moves[0];
    }
    
    double allocatedTime = timeAllocated * 0.8;
    
    safePrint("Parallel MCTS: %d threads, %.2fs allocated, %d moves available\n", 
             LAZY_SMP_THREADS, allocatedTime, moveCount);
    
    MCTSWorkerData workerData[LAZY_SMP_THREADS];
    atomic_int iterationCount;
    atomic_init(&iterationCount, 0);
    
    workerData[0].root = root;
    memcpy(workerData[0].board, board, BOARD_SIZE * BOARD_SIZE * sizeof(char));
    workerData[0].playerId = currentPlayer;
    workerData[0].iterationCount = &iterationCount;
    workerData[0].timeLimit = allocatedTime;
    
    while (elapsedSeconds() < allocatedTime * 0.2 && 
           atomic_load(&iterationCount) < 1000) {
        mctsWorkerSingle(&workerData[0]);
    }
    
    if (elapsedSeconds() < allocatedTime * 0.5) {
        pthread_t threads[LAZY_SMP_THREADS];
        int threadCreated[LAZY_SMP_THREADS] = {0};
        
        for (int i = 0; i < LAZY_SMP_THREADS; i++) {
            workerData[i].root = root;
            memcpy(workerData[i].board, board, BOARD_SIZE * BOARD_SIZE * sizeof(char));
            workerData[i].playerId = currentPlayer;
            workerData[i].iterationCount = &iterationCount;
            workerData[i].timeLimit = allocatedTime;
            
            if (pthread_create(&threads[i], NULL, mctsWorker, &workerData[i]) == 0) {
                threadCreated[i] = 1;
            } else {
                safePrint("Failed to create thread %d\n", i);
            }
        }
        
        for (int i = 0; i < LAZY_SMP_THREADS; i++) {
            if (threadCreated[i]) {
                pthread_join(threads[i], NULL);
            }
        }
    }
    
    gameState.currentPlayer = savedCurrentPlayer;
    
    int totalIterations = atomic_load(&iterationCount);
    double elapsedTime = elapsedSeconds();
    
    safePrint("MCTS completed: %d iterations in %.2fs (%.0f iter/sec)\n",
             totalIterations, elapsedTime,
             totalIterations / (elapsedTime + 0.001));
    
    if (totalIterations < 500) {
        safePrint("Insufficient MCTS iterations (%d), using alpha-beta fallback\n", 
                 totalIterations);
        freeMCTSTree(root);
        
        if (elapsedTime < allocatedTime * 0.5) {
            return getHybridAlphaBetaMove(board, currentPlayer);
        } else {
            return moves[0];
        }
    }
    
    Move bestMove = getMCTSBestMove(root);
    
    if (bestMove.r1 == 0 && bestMove.c1 == 0 && 
        bestMove.r2 == 0 && bestMove.c2 == 0 && moveCount > 0) {
        safePrint("MCTS returned invalid pass, selecting first valid move\n");
        bestMove = moves[0];
    }
    
    freeMCTSTree(root);
    return bestMove;
}

int main(int argc, char *argv[]) {
    struct addrinfo hints, *res;
    int status;
    char server_addr[256] = "127.0.0.1";
    char server_port[32] = "8080";
    
    memset(my_username, 0, sizeof(my_username));
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-ip") == 0 && i + 1 < argc) {
            strcpy(server_addr, argv[++i]);
        } else if (strcmp(argv[i], "-port") == 0 && i + 1 < argc) {
            strcpy(server_port, argv[++i]);
        } else if (strcmp(argv[i], "-username") == 0 && i + 1 < argc) {
            strcpy(my_username, argv[++i]);
        } else if (strcmp(argv[i], "-engine") == 0 && i + 1 < argc) {
            int engine = atoi(argv[++i]);
            if (engine >= 1 && engine <= 20) {
                aiEngine = (AIEngineType)engine;
            }
        } else if (strcmp(argv[i], "-human") == 0) {
            humanMode = 1;
        }
    }
    
    if (strlen(my_username) == 0) {
        sprintf(my_username, "Player_%d", (int)time(NULL) % 10000);
    }
    
    printf("\n=== OctaFlip Client (Ultimate AI ) ===\n");
    printf("Username: %s\n", my_username);
    printf("Server: %s:%s\n", server_addr, server_port);
    
    if (humanMode) {
        printf("Mode: Human Player\n");
        printf("\nControls:\n");
        printf("- Enter moves as: sx sy tx ty (e.g., '1 1 2 2')\n");
        printf("- Pass turn: 0 0 0 0\n");
        printf("- Quit: type 'quit' or 'exit'\n");
    } else {
        const char* engineNames[] = {
            "Hybrid Alpha-Beta",
            "Parallel MCTS", 
            "Neural Pattern",
            "Opening Book",
            "Endgame Solver",
            "Tournament Beast",
            "Adaptive Time",
            "Learning Engine",
            "Human Style",
            "Random Good",
            "NNUE Beast",
            "Opening Master",
            "Ultimate AI",      // New improved version with better NNUE
            "Hybrid MCTS",
            "Tactical Genius",
            "Strategic Master",
            "Endgame God",
            "Blitz King",
            "Fortress",
            "Assassin"
        };
        
        printf("AI Engine: %s\n", engineNames[(int)aiEngine - 1]);
        
        printf("\nUltimate AI Features:\n");
        printf("- Opening (0-27): Tournament Beast + Enhanced NNUE guidance\n");
        printf("- Midgame (28-50): Extended Hybrid MCTS with Tournament Beast fallback\n");
        printf("- Endgame (51+): MCTS-based win focus with Endgame God for perfect endgames\n");
        printf("- Improved stability: Better timeout handling throughout all phases\n");
        
        printf("\nAvailable engines:\n");
        for (int i = 0; i < 20; i++) {
            printf("  %d = %s\n", i + 1, engineNames[i]);
        }
        printf("\nUsage: %s -engine <1-20> [-human]\n", argv[0]);
    }
    
    if (!humanMode) {
        initializeAISystem();
    }
    
    memset(&gameState, 0, sizeof(gameState));
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            gameState.board[i][j] = EMPTY;
        }
    }
    gameState.board[0][0] = RED;
    gameState.board[0][7] = BLUE;
    gameState.board[7][0] = BLUE;
    gameState.board[7][7] = RED;
    
    signal(SIGINT, sigint_handler);
    
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    
    status = getaddrinfo(server_addr, server_port, &hints, &res);
    if (status != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        cleanup();
        exit(1);
    }
    
    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd == -1) {
        perror("socket error");
        cleanup();
        exit(1);
    }
    
    printf("\nConnecting to %s:%s...\n", server_addr, server_port);
    status = connect(sockfd, res->ai_addr, res->ai_addrlen);
    if (status == -1) {
        perror("connect error");
        cleanup();
        exit(1);
    }
    
    printf("✅ Connected to server!\n\n");
    
    sendRegister(my_username);
    
    pthread_t receiver_thread;
    if (pthread_create(&receiver_thread, NULL, receiveMessages, NULL) != 0) {
        perror("pthread_create error");
        cleanup();
        exit(1);
    }
    
    pthread_join(receiver_thread, NULL);
    
    printf("\nGame session ended. Goodbye!\n");
    freeaddrinfo(res);
    cleanup();
    
    return 0;
}