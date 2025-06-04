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
#include <arpa/inet.h>
#include "cJSON.h"
#include "board.h"

// Platform compatibility
#ifdef _WIN32
    #include <windows.h>
    #define CLOCK_MONOTONIC 0
    void clock_gettime(int dummy, struct timespec* ts) {
        LARGE_INTEGER frequency, counter;
        QueryPerformanceFrequency(&frequency);
        QueryPerformanceCounter(&counter);
        ts->tv_sec = counter.QuadPart / frequency.QuadPart;
        ts->tv_nsec = (counter.QuadPart % frequency.QuadPart) * 1000000000 / frequency.QuadPart;
    }
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
#define MAX_DEPTH 20
#define INF_SCORE 1000000
#define NEG_INF_SCORE -1000000
#define MAX_MOVES 200
#define TIME_LIMIT 3.0
#define SAFETY_MARGIN 0.1
#define BUFFER_SIZE 4096

// ===== HASH TABLE (Optimized for RPi) =====
#define HASH_SIZE (1 << 18)  // 256K entries for RPi
#define HASH_MASK (HASH_SIZE - 1)

// ===== MCTS CONSTANTS (Simplified and Optimized) =====
#define MCTS_C 1.414                  // UCB constant (sqrt(2))
#define MCTS_THREADS 4                 // Thread count for parallel simulations
#define MCTS_TIME_CHECK_INTERVAL 100   // Check time every N iterations
#define MCTS_MAX_CHILDREN 50           // Maximum children per node
#define MCTS_SIMULATION_DEPTH 30       // Max moves in simulation
#define MCTS_MIN_VISITS 10             // Minimum visits for robust selection

// ===== MINIMAX CONSTANTS =====
#define MINIMAX_THREADS 4
#define ASPIRATION_WINDOW 50

// ===== PHASE CONSTANTS =====
#define PHASE_OPENING_END 38
#define PHASE_MIDGAME_END 43
#define PHASE_ENDGAME_START 50

// ===== DEFAULT SERVER =====
#define DEFAULT_SERVER_IP "10.8.128.233"
#define DEFAULT_SERVER_PORT "8080"

// ===== POSITION WEIGHTS (Carefully redesigned for OctaFlip) =====

// Opening: 중앙 제어 + 초기 위치 활용
static const double POSITION_WEIGHTS_OPENING[8][8] = {
    { 20, -10,  10,   5,   5,  10, -10,  20},
    {-10, -20,   5,   5,   5,   5, -20, -10},
    { 10,   5,  15,  20,  20,  15,   5,  10},
    {  5,   5,  20,  25,  25,  20,   5,   5},
    {  5,   5,  20,  25,  25,  20,   5,   5},
    { 10,   5,  15,  20,  20,  15,   5,  10},
    {-10, -20,   5,   5,   5,   5, -20, -10},
    { 20, -10,  10,   5,   5,  10, -10,  20}
};

// Midgame: 안정성 vs 기동성 균형
static const double POSITION_WEIGHTS_MIDGAME[8][8] = {
    { 50, -20,  20,  10,  10,  20, -20,  50},
    {-20, -40,   0,   0,   0,   0, -40, -20},
    { 20,   0,  10,  10,  10,  10,   0,  20},
    { 10,   0,  10,  15,  15,  10,   0,  10},
    { 10,   0,  10,  15,  15,  10,   0,  10},
    { 20,   0,  10,  10,  10,  10,   0,  20},
    {-20, -40,   0,   0,   0,   0, -40, -20},
    { 50, -20,  20,  10,  10,  20, -20,  50}
};

// Endgame: 코너와 엣지가 결정적
static const double POSITION_WEIGHTS_ENDGAME[8][8] = {
    {100, -40,  40,  30,  30,  40, -40, 100},
    {-40, -80, -10, -10, -10, -10, -80, -40},
    { 40, -10,  10,   0,   0,  10, -10,  40},
    { 30, -10,   0,   0,   0,   0, -10,  30},
    { 30, -10,   0,   0,   0,   0, -10,  30},
    { 40, -10,  10,   0,   0,  10, -10,  40},
    {-40, -80, -10, -10, -10, -10, -80, -40},
    {100, -40,  40,  30,  30,  40, -40, 100}
};

// Move structure
typedef struct {
    int r1, c1, r2, c2;
    int score;
    int moveType;
} Move;

// Transposition Table Entry
typedef struct {
    unsigned long long hash;
    int score;
    int depth;
    int flag;  // 0=exact, 1=lower, 2=upper
    Move bestMove;
} TTEntry;

// MCTS Node (Simplified)
typedef struct MCTSNode {
    Move move;
    int visits;
    double totalScore;
    double winRate;
    int player;
    struct MCTSNode* parent;
    struct MCTSNode** children;
    int childCount;
    int childCapacity;
    int fullyExpanded;
} MCTSNode;

// Thread data structures
typedef struct {
    char board[BOARD_SIZE][BOARD_SIZE];
    Move move;
    char player;
    int score;
    int depth;
    int alpha;
    int beta;
    int threadId;
    bool useNN;
    bool useHybrid;
} ThreadData;

typedef struct {
    char board[BOARD_SIZE][BOARD_SIZE];
    int currentPlayer;
    int iterations;
    double totalScore;
    bool useNN;
} MCTSSimulationData;

// AI Engine types
typedef enum {
    ENGINE_EUNSONG = 1,
    ENGINE_MCTS_NN = 2,
    ENGINE_MCTS_CLASSIC = 3,
    ENGINE_MINIMAX_NN = 4,
    ENGINE_MINIMAX_CLASSIC = 5,
    ENGINE_TOURNAMENT_BEAST = 6
} AIEngineType;

// Game phase
typedef enum {
    PHASE_OPENING,
    PHASE_MIDGAME,
    PHASE_ENDGAME_EARLY,
    PHASE_ENDGAME_LATE
} GamePhase;

// Global variables
static int sockfd = -1;
static char my_username[256];
static char my_color = '\0';
static char board[BOARD_SIZE][BOARD_SIZE];
static int currentPlayer = RED_TURN;
static atomic_int gameStarted = 0;
static atomic_int myTurn = 0;
static atomic_int gameOver = 0;
static AIEngineType aiEngine = ENGINE_TOURNAMENT_BEAST;
static int boardServerEnabled = 1;
static int led_initialized = 0;
static int totalMoveCount = 0;
static GamePhase currentPhase = PHASE_OPENING;

// AI optimization globals
static struct timespec searchStart;
static atomic_int timeUp = 0;
static TTEntry* transpositionTable = NULL;
static unsigned long long zobristTable[BOARD_SIZE][BOARD_SIZE][4];
static Move killerMoves[MAX_DEPTH][2];
static int historyTable[BOARD_SIZE][BOARD_SIZE][BOARD_SIZE][BOARD_SIZE];
static atomic_long nodeCount;
static double timeAllocated = TIME_LIMIT;

// Position history for repetition detection
static unsigned long long positionHistory[10];
static int positionHistoryCount = 0;
static Move moveHistory[10];
static int moveHistoryCount = 0;

// Eunsong specific globals
static time_t eunsongStartTime;
static int moveDir[8][2] = {
    {-1, -1}, {-1, 0}, {-1, 1},
    { 0, -1},          { 0, 1},
    { 1, -1}, { 1, 0}, { 1, 1}
};

// Function prototypes
void safePrint(const char* format, ...);
void initializeAISystem();
void cleanupAISystem();
Move generate_move();
int isValidMove(int sx, int sy, int tx, int ty);
void makeMove(char board[BOARD_SIZE][BOARD_SIZE], Move move);
void getAllValidMoves(char board[BOARD_SIZE][BOARD_SIZE], int currentPlayer, Move* moves, int* moveCount);
double evaluateBoard(char board[BOARD_SIZE][BOARD_SIZE], int forPlayer);
double evaluateBoardPhased(char board[BOARD_SIZE][BOARD_SIZE], int forPlayer, GamePhase phase);
double evaluateHybrid(char board[BOARD_SIZE][BOARD_SIZE], int forPlayer, GamePhase phase);
double elapsedSeconds();
void initZobrist();
unsigned long long computeHash(char board[BOARD_SIZE][BOARD_SIZE]);
int countPieces(char board[BOARD_SIZE][BOARD_SIZE], char piece);
void copyBoard(char dst[BOARD_SIZE][BOARD_SIZE], char src[BOARD_SIZE][BOARD_SIZE]);
GamePhase getGamePhase(char board[BOARD_SIZE][BOARD_SIZE]);
bool isRepetition(unsigned long long hash, Move move);
void addToHistory(unsigned long long hash, Move move);
void filterBadMoves(Move* moves, int* moveCount, char board[BOARD_SIZE][BOARD_SIZE], int currentPlayer);
Move checkInstantWin(char board[BOARD_SIZE][BOARD_SIZE], int currentPlayer);
int countCaptures(char board[BOARD_SIZE][BOARD_SIZE], Move move, int currentPlayer);
bool isDangerousMove(char board[BOARD_SIZE][BOARD_SIZE], Move move, int currentPlayer);

// Engine functions
Move getEunsongMove(char board[BOARD_SIZE][BOARD_SIZE], int currentPlayer);
Move getMCTSMoveNN(char board[BOARD_SIZE][BOARD_SIZE], int currentPlayer);
Move getMCTSMoveClassic(char board[BOARD_SIZE][BOARD_SIZE], int currentPlayer);
Move getMinimaxMoveNN(char board[BOARD_SIZE][BOARD_SIZE], int currentPlayer);
Move getMinimaxMoveClassic(char board[BOARD_SIZE][BOARD_SIZE], int currentPlayer);
Move getTournamentBeastMove(char board[BOARD_SIZE][BOARD_SIZE], int currentPlayer);

// MCTS functions (Simplified and Optimized)
MCTSNode* mcts_createNode(Move move, int player, MCTSNode* parent);
void mcts_freeTree(MCTSNode* root);
MCTSNode* mcts_selectChild(MCTSNode* node);
void mcts_expand(MCTSNode* node, char board[BOARD_SIZE][BOARD_SIZE]);
double mcts_simulate(char board[BOARD_SIZE][BOARD_SIZE], int player, bool useNN);
void mcts_backpropagate(MCTSNode* node, double score);
Move mcts_getBestMove(MCTSNode* root);
Move mcts_search(char board[BOARD_SIZE][BOARD_SIZE], int currentPlayer, bool useNN);

// Improved Minimax functions (Eunsong style)
int negamaxPhased(char board[BOARD_SIZE][BOARD_SIZE], int depth, int alpha, int beta, 
                  int currentPlayer, Move* bestMove, GamePhase phase, bool useHybrid);
void orderMovesPhased(Move* moves, int moveCount, char board[BOARD_SIZE][BOARD_SIZE], 
                      int currentPlayer, int depth, GamePhase phase);
void* minimaxWorkerPhased(void* arg);

// Network functions
void sendJSON(cJSON* json);
void sendRegister(const char* username);
void sendMove(int sx, int sy, int tx, int ty);
void handleServerMessage(const char* message);
void* receiveMessages(void* arg);
void updateBoardFromJSON(cJSON* board_array);
void printBoard();
void cleanup();
void sigint_handler(int sig);

// NNUE functions
#ifdef HAS_NNUE_WEIGHTS
static int nnue_evaluate(char board[BOARD_SIZE][BOARD_SIZE], int forPlayer);
#endif

// LED functions
void initLEDDisplay();
void updateLEDDisplay();
void cleanupLEDDisplay();

// ===== UTILITY FUNCTIONS =====

void safePrint(const char* format, ...) {
    static pthread_mutex_t printMutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_lock(&printMutex);
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    fflush(stdout);
    pthread_mutex_unlock(&printMutex);
}

double elapsedSeconds() {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec - searchStart.tv_sec) + 
           (now.tv_nsec - searchStart.tv_nsec) / 1000000000.0;
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
    
    if (board[sx][sy] != my_color) return 0;
    if (board[tx][ty] != EMPTY) return 0;
    
    return getMoveType(sx, sy, tx, ty) > 0;
}

void copyBoard(char dst[BOARD_SIZE][BOARD_SIZE], char src[BOARD_SIZE][BOARD_SIZE]) {
    memcpy(dst, src, BOARD_SIZE * BOARD_SIZE);
}

void makeMove(char board[BOARD_SIZE][BOARD_SIZE], Move move) {
    char playerPiece = board[move.r1][move.c1];
    char opponentPiece = (playerPiece == RED) ? BLUE : RED;
    
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
            
            // Clone moves (1칸 이동)
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
            
            // Jump moves (2칸 이동)
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

int countCaptures(char board[BOARD_SIZE][BOARD_SIZE], Move move, int currentPlayer) {
    char opponentPiece = (currentPlayer == RED_TURN) ? BLUE : RED;
    int captures = 0;
    
    for (int dr = -1; dr <= 1; dr++) {
        for (int dc = -1; dc <= 1; dc++) {
            if (dr == 0 && dc == 0) continue;
            int nr = move.r2 + dr;
            int nc = move.c2 + dc;
            if (nr >= 0 && nr < BOARD_SIZE && nc >= 0 && nc < BOARD_SIZE &&
                board[nr][nc] == opponentPiece) {
                captures++;
            }
        }
    }
    
    return captures;
}

bool isDangerousMove(char board[BOARD_SIZE][BOARD_SIZE], Move move, int currentPlayer) {
    char playerPiece = (currentPlayer == RED_TURN) ? RED : BLUE;
    
    // X-square 체크
    if ((move.r2 == 1 || move.r2 == 6) && (move.c2 == 1 || move.c2 == 6)) {
        int cornerR = (move.r2 == 1) ? 0 : 7;
        int cornerC = (move.c2 == 1) ? 0 : 7;
        if (board[cornerR][cornerC] != playerPiece) {
            return true;
        }
    }
    
    // 코너를 떠나는 수
    if ((move.r1 == 0 || move.r1 == 7) && (move.c1 == 0 || move.c1 == 7)) {
        return true;
    }
    
    return false;
}

Move checkInstantWin(char board[BOARD_SIZE][BOARD_SIZE], int currentPlayer) {
    Move moves[MAX_MOVES];
    int moveCount;
    getAllValidMoves(board, currentPlayer, moves, &moveCount);
    
    if (moveCount == 0) {
        return (Move){0, 0, 0, 0, 0, 0};
    }
    
    char playerPiece = (currentPlayer == RED_TURN) ? RED : BLUE;
    char opponentPiece = (currentPlayer == RED_TURN) ? BLUE : RED;
    
    // 빈 칸이 1개일 때 최선의 수 찾기
    int emptyCount = countPieces(board, EMPTY);
    if (emptyCount == 1) {
        Move bestMove = moves[0];
        int bestScore = -1000;
        
        for (int i = 0; i < moveCount; i++) {
            char tempBoard[BOARD_SIZE][BOARD_SIZE];
            copyBoard(tempBoard, board);
            makeMove(tempBoard, moves[i]);
            
            int myPieces = countPieces(tempBoard, playerPiece);
            int oppPieces = countPieces(tempBoard, opponentPiece);
            int score = myPieces - oppPieces;
            
            if (score > bestScore) {
                bestScore = score;
                bestMove = moves[i];
            }
        }
        
        bestMove.r1++;
        bestMove.c1++;
        bestMove.r2++;
        bestMove.c2++;
        return bestMove;
    }
    
    // 즉시 승리 체크
    for (int i = 0; i < moveCount; i++) {
        char tempBoard[BOARD_SIZE][BOARD_SIZE];
        copyBoard(tempBoard, board);
        makeMove(tempBoard, moves[i]);
        
        int oppPieces = countPieces(tempBoard, opponentPiece);
        if (oppPieces == 0) {
            moves[i].r1++;
            moves[i].c1++;
            moves[i].r2++;
            moves[i].c2++;
            return moves[i];
        }
    }
    
    return (Move){0, 0, 0, 0, 0, 0};
}

void filterBadMoves(Move* moves, int* moveCount, char board[BOARD_SIZE][BOARD_SIZE], int currentPlayer) {
    char playerPiece = (currentPlayer == RED_TURN) ? RED : BLUE;
    char opponentPiece = (currentPlayer == RED_TURN) ? BLUE : RED;
    GamePhase phase = getGamePhase(board);
    
    int validMoves[MAX_MOVES];
    int validCount = 0;
    
    for (int i = 0; i < *moveCount; i++) {
        Move* m = &moves[i];
        bool badMove = false;
        
        // 1. 절대 금지: 코너를 떠나는 수
        if ((m->r1 == 0 || m->r1 == 7) && (m->c1 == 0 || m->c1 == 7)) {
            badMove = true;
            continue;
        }
        
        // 2. X-square 체크 (소유하지 않은 코너 인접)
        if ((m->r2 == 1 || m->r2 == 6) && (m->c2 == 1 || m->c2 == 6)) {
            int cornerR = (m->r2 == 1) ? 0 : 7;
            int cornerC = (m->c2 == 1) ? 0 : 7;
            if (board[cornerR][cornerC] != playerPiece) {
                // 상대가 다음 턴에 코너를 차지할 수 있는지 확인
                bool opponentCanTakeCorner = false;
                for (int dr = -2; dr <= 2; dr++) {
                    for (int dc = -2; dc <= 2; dc++) {
                        if (dr == 0 && dc == 0) continue;
                        int or1 = cornerR + dr;
                        int oc1 = cornerC + dc;
                        if (or1 >= 0 && or1 < 8 && oc1 >= 0 && oc1 < 8 &&
                            board[or1][oc1] == opponentPiece) {
                            int moveType = getMoveType(or1, oc1, cornerR, cornerC);
                            if (moveType > 0) {
                                opponentCanTakeCorner = true;
                                break;
                            }
                        }
                    }
                    if (opponentCanTakeCorner) break;
                }
                if (opponentCanTakeCorner) {
                    badMove = true;
                    continue;
                }
            }
        }
        
        // 3. 초반 점프 제한 (더 엄격하게)
        if (phase == PHASE_OPENING && m->moveType == JUMP) {
            // 점프로 얻는 캡처가 2개 이상이어야 허용
            int captures = 0;
            for (int dr = -1; dr <= 1; dr++) {
                for (int dc = -1; dc <= 1; dc++) {
                    if (dr == 0 && dc == 0) continue;
                    int nr = m->r2 + dr;
                    int nc = m->c2 + dc;
                    if (nr >= 0 && nr < 8 && nc >= 0 && nc < 8 &&
                        board[nr][nc] == opponentPiece) {
                        captures++;
                    }
                }
            }
            if (captures < 2) {
                badMove = true;
                continue;
            }
        }
        
        // 4. 위험한 엣지 확인 (상대가 코너를 소유한 경우)
        if ((m->r2 == 0 || m->r2 == 7) || (m->c2 == 0 || m->c2 == 7)) {
            // 엣지 위치인데 인접 코너를 상대가 소유하고 있는지 확인
            bool dangerousEdge = false;
            
            // 상단 엣지
            if (m->r2 == 0 && m->c2 > 0 && m->c2 < 7) {
                if ((board[0][0] == opponentPiece && m->c2 <= 2) ||
                    (board[0][7] == opponentPiece && m->c2 >= 5)) {
                    dangerousEdge = true;
                }
            }
            // 하단 엣지
            else if (m->r2 == 7 && m->c2 > 0 && m->c2 < 7) {
                if ((board[7][0] == opponentPiece && m->c2 <= 2) ||
                    (board[7][7] == opponentPiece && m->c2 >= 5)) {
                    dangerousEdge = true;
                }
            }
            // 좌측 엣지
            else if (m->c2 == 0 && m->r2 > 0 && m->r2 < 7) {
                if ((board[0][0] == opponentPiece && m->r2 <= 2) ||
                    (board[7][0] == opponentPiece && m->r2 >= 5)) {
                    dangerousEdge = true;
                }
            }
            // 우측 엣지
            else if (m->c2 == 7 && m->r2 > 0 && m->r2 < 7) {
                if ((board[0][7] == opponentPiece && m->r2 <= 2) ||
                    (board[7][7] == opponentPiece && m->r2 >= 5)) {
                    dangerousEdge = true;
                }
            }
            
            if (dangerousEdge) {
                badMove = true;
                continue;
            }
        }
        
        if (!badMove) {
            validMoves[validCount++] = i;
        }
    }
    
    // 유효한 수들만 앞으로 이동
    if (validCount > 0) {
        Move tempMoves[MAX_MOVES];
        for (int i = 0; i < validCount; i++) {
            tempMoves[i] = moves[validMoves[i]];
        }
        for (int i = 0; i < validCount; i++) {
            moves[i] = tempMoves[i];
        }
        *moveCount = validCount;
    }
    // 모든 수가 나쁘면 원래대로 유지 (패스보다는 나음)
}

GamePhase getGamePhase(char board[BOARD_SIZE][BOARD_SIZE]) {
    int totalPieces = countPieces(board, RED) + countPieces(board, BLUE);
    
    if (totalPieces < PHASE_OPENING_END) {
        return PHASE_OPENING;
    } else if (totalPieces < PHASE_MIDGAME_END) {
        return PHASE_MIDGAME;
    } else if (totalPieces < PHASE_ENDGAME_START) {
        return PHASE_ENDGAME_EARLY;
    } else {
        return PHASE_ENDGAME_LATE;
    }
}

bool isRepetition(unsigned long long hash, Move move) {
    // Check position repetition
    int count = 0;
    for (int i = 0; i < positionHistoryCount; i++) {
        if (positionHistory[i] == hash) {
            count++;
            if (count >= 2) return true;
        }
    }
    
    // Check move repetition
    if (moveHistoryCount >= 4) {
        bool sameMove = true;
        for (int i = moveHistoryCount - 4; i < moveHistoryCount - 2; i += 2) {
            if (moveHistory[i].r1 != move.r1 || moveHistory[i].c1 != move.c1 ||
                moveHistory[i].r2 != move.r2 || moveHistory[i].c2 != move.c2) {
                sameMove = false;
                break;
            }
        }
        if (sameMove) return true;
    }
    
    return false;
}

void addToHistory(unsigned long long hash, Move move) {
    // Add to position history
    positionHistory[positionHistoryCount % 10] = hash;
    positionHistoryCount++;
    
    // Add to move history
    moveHistory[moveHistoryCount % 10] = move;
    moveHistoryCount++;
}

double evaluateBoard(char board[BOARD_SIZE][BOARD_SIZE], int forPlayer) {
    GamePhase phase = getGamePhase(board);
    return evaluateBoardPhased(board, forPlayer, phase);
}

double evaluateBoardPhased(char board[BOARD_SIZE][BOARD_SIZE], int forPlayer, GamePhase phase) {
    int redCount = countPieces(board, RED);
    int blueCount = countPieces(board, BLUE);
    
    // 게임 종료 체크
    if (redCount == 0) {
        return (forPlayer == BLUE_TURN) ? INF_SCORE : NEG_INF_SCORE;
    }
    if (blueCount == 0) {
        return (forPlayer == RED_TURN) ? INF_SCORE : NEG_INF_SCORE;
    }
    
    double score = 0.0;
    int pieceDiff = (forPlayer == RED_TURN) ? (redCount - blueCount) : (blueCount - redCount);
    
    // 기본 재료 점수 (phase별 가중치)
    double pieceMultiplier;
    switch (phase) {
        case PHASE_OPENING:
            pieceMultiplier = 80.0;
            break;
        case PHASE_MIDGAME:
            pieceMultiplier = 100.0;
            break;
        case PHASE_ENDGAME_EARLY:
            pieceMultiplier = 120.0;
            break;
        case PHASE_ENDGAME_LATE:
            pieceMultiplier = 200.0;  // 종반엔 각 말이 매우 중요
            break;
    }
    score += pieceDiff * pieceMultiplier;
    
    // 위치 가중치 선택
    const double (*posWeights)[8];
    switch (phase) {
        case PHASE_OPENING:
            posWeights = POSITION_WEIGHTS_OPENING;
            break;
        case PHASE_MIDGAME:
            posWeights = POSITION_WEIGHTS_MIDGAME;
            break;
        case PHASE_ENDGAME_EARLY:
        case PHASE_ENDGAME_LATE:
            posWeights = POSITION_WEIGHTS_ENDGAME;
            break;
    }
    
    char playerPiece = (forPlayer == RED_TURN) ? RED : BLUE;
    char opponentPiece = (forPlayer == RED_TURN) ? BLUE : RED;
    
    // 위치 평가
    double myPositionScore = 0.0;
    double oppPositionScore = 0.0;
    
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board[i][j] == playerPiece) {
                myPositionScore += posWeights[i][j];
            } else if (board[i][j] == opponentPiece) {
                oppPositionScore += posWeights[i][j];
            }
        }
    }
    
    score += (myPositionScore - oppPositionScore);
    
    // 추가 패턴 평가
    int myCorners = 0, oppCorners = 0;
    int myXSquares = 0, oppXSquares = 0;
    
    // 코너 체크
    if (board[0][0] == playerPiece) myCorners++;
    if (board[0][7] == playerPiece) myCorners++;
    if (board[7][0] == playerPiece) myCorners++;
    if (board[7][7] == playerPiece) myCorners++;
    
    if (board[0][0] == opponentPiece) oppCorners++;
    if (board[0][7] == opponentPiece) oppCorners++;
    if (board[7][0] == opponentPiece) oppCorners++;
    if (board[7][7] == opponentPiece) oppCorners++;
    
    // X-square 체크 (코너 소유 여부에 따라 다르게 평가)
    if (board[1][1] == playerPiece && board[0][0] != playerPiece) myXSquares++;
    if (board[1][6] == playerPiece && board[0][7] != playerPiece) myXSquares++;
    if (board[6][1] == playerPiece && board[7][0] != playerPiece) myXSquares++;
    if (board[6][6] == playerPiece && board[7][7] != playerPiece) myXSquares++;
    
    if (board[1][1] == opponentPiece && board[0][0] != opponentPiece) oppXSquares++;
    if (board[1][6] == opponentPiece && board[0][7] != opponentPiece) oppXSquares++;
    if (board[6][1] == opponentPiece && board[7][0] != opponentPiece) oppXSquares++;
    if (board[6][6] == opponentPiece && board[7][7] != opponentPiece) oppXSquares++;
    
    // 코너와 X-square 보너스/페널티
    score += (myCorners - oppCorners) * 300;
    score -= (myXSquares - oppXSquares) * 150;
    
    // 기동성 (가능한 수의 개수)
    Move tempMoves[MAX_MOVES];
    int myMobility, oppMobility;
    getAllValidMoves(board, forPlayer, tempMoves, &myMobility);
    getAllValidMoves(board, 1 - forPlayer, tempMoves, &oppMobility);
    
    score += (myMobility - oppMobility) * 5;
    
    // 연결성 보너스
    int connectivity = 0;
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board[i][j] == playerPiece) {
                for (int di = -1; di <= 1; di++) {
                    for (int dj = -1; dj <= 1; dj++) {
                        if (di == 0 && dj == 0) continue;
                        int ni = i + di, nj = j + dj;
                        if (ni >= 0 && ni < BOARD_SIZE && nj >= 0 && nj < BOARD_SIZE &&
                            board[ni][nj] == playerPiece) {
                            connectivity++;
                        }
                    }
                }
            }
        }
    }
    score += connectivity * 3;
    
    // 엣지 제어 보너스
    int edgeCount = 0;
    for (int i = 0; i < BOARD_SIZE; i++) {
        if (board[0][i] == playerPiece) edgeCount++;
        if (board[7][i] == playerPiece) edgeCount++;
        if (board[i][0] == playerPiece && i != 0 && i != 7) edgeCount++;
        if (board[i][7] == playerPiece && i != 0 && i != 7) edgeCount++;
    }
    score += edgeCount * 10;
    
    return score;
}

// Hybrid evaluation function for Tournament Beast
double evaluateHybrid(char board[BOARD_SIZE][BOARD_SIZE], int forPlayer, GamePhase phase) {
    double classicEval = evaluateBoardPhased(board, forPlayer, phase);
    
    #ifdef HAS_NNUE_WEIGHTS
    double nnueEval = nnue_evaluate(board, forPlayer);
    
    // Phase-specific blend weights
    double nnWeight;
    switch (phase) {
        case PHASE_OPENING:
            nnWeight = 0.8;  // 80% NNUE, 20% Classic
            break;
        case PHASE_MIDGAME:
            nnWeight = 0.8;  // 80% NNUE, 20% Classic
            break;
        case PHASE_ENDGAME_EARLY:
            nnWeight = 0.6;  // 50% NNUE, 50% Classic
            break;
        case PHASE_ENDGAME_LATE:
            nnWeight = 0.3;  // 30% NNUE, 70% Classic (endgame heuristics more reliable)
            break;
    }
    
    return classicEval * (1.0 - nnWeight) + nnueEval * nnWeight;
    #else
    return classicEval;
    #endif
}

// ===== NNUE EVALUATION =====
#ifdef HAS_NNUE_WEIGHTS
static int nnue_evaluate(char board[BOARD_SIZE][BOARD_SIZE], int forPlayer) {
    int16_t input[INPUT_SIZE] = {0};
    
    // Encode board
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
    
    // Layer 1
    int32_t acc1[NNUE_HIDDEN1_SIZE];
    for (int i = 0; i < NNUE_HIDDEN1_SIZE; i++) {
        acc1[i] = nnue_b1[i] * NNUE_SCALE;
        for (int j = 0; j < INPUT_SIZE; j++) {
            acc1[i] += (int32_t)input[j] * nnue_w1[j][i];
        }
        acc1[i] = (acc1[i] > 0) ? acc1[i] : (acc1[i] / 100);  // Leaky ReLU
        acc1[i] /= NNUE_SCALE;
    }
    
    // Layer 2
    int32_t acc2[NNUE_HIDDEN2_SIZE];
    for (int i = 0; i < NNUE_HIDDEN2_SIZE; i++) {
        acc2[i] = nnue_b2[i] * NNUE_SCALE;
        for (int j = 0; j < NNUE_HIDDEN1_SIZE; j++) {
            acc2[i] += acc1[j] * nnue_w2[j][i];
        }
        acc2[i] = (acc2[i] > 0) ? acc2[i] : (acc2[i] / 100);  // Leaky ReLU
        acc2[i] /= NNUE_SCALE;
    }
    
    // Layer 3
    int32_t acc3[NNUE_HIDDEN3_SIZE];
    for (int i = 0; i < NNUE_HIDDEN3_SIZE; i++) {
        acc3[i] = nnue_b3[i] * NNUE_SCALE;
        for (int j = 0; j < NNUE_HIDDEN2_SIZE; j++) {
            acc3[i] += acc2[j] * nnue_w3[j][i];
        }
        acc3[i] = (acc3[i] > 0) ? acc3[i] : (acc3[i] / 100);  // Leaky ReLU
        acc3[i] /= NNUE_SCALE;
    }
    
    // Output layer
    int32_t output = nnue_b4[0] * NNUE_SCALE;
    for (int i = 0; i < NNUE_HIDDEN3_SIZE; i++) {
        output += acc3[i] * nnue_w4[i];
    }
    
    output /= NNUE_SCALE;
    
    // Apply tanh-like bounding
    if (output > NNUE_OUTPUT_SCALE) output = NNUE_OUTPUT_SCALE;
    if (output < -NNUE_OUTPUT_SCALE) output = -NNUE_OUTPUT_SCALE;
    
    return (forPlayer == RED_TURN) ? output : -output;
}
#endif

// ===== MCTS IMPLEMENTATION (Simplified and Optimized) =====

MCTSNode* mcts_createNode(Move move, int player, MCTSNode* parent) {
    MCTSNode* node = (MCTSNode*)calloc(1, sizeof(MCTSNode));
    if (!node) return NULL;
    
    node->move = move;
    node->visits = 0;
    node->totalScore = 0.0;
    node->winRate = 0.0;
    node->player = player;
    node->parent = parent;
    node->children = NULL;
    node->childCount = 0;
    node->childCapacity = 0;
    node->fullyExpanded = 0;
    
    return node;
}

void mcts_freeTree(MCTSNode* root) {
    if (!root) return;
    
    for (int i = 0; i < root->childCount; i++) {
        mcts_freeTree(root->children[i]);
    }
    
    if (root->children) free(root->children);
    free(root);
}

MCTSNode* mcts_selectChild(MCTSNode* node) {
    if (!node || node->childCount == 0) return NULL;
    
    MCTSNode* bestChild = NULL;
    double bestValue = -1e9;
    
    double parentVisits = (double)node->visits;
    double logParent = log(parentVisits);
    
    for (int i = 0; i < node->childCount; i++) {
        MCTSNode* child = node->children[i];
        if (!child) continue;
        
        double value;
        if (child->visits == 0) {
            // Unvisited nodes get priority
            value = 1e6 + (rand() % 1000);
        } else {
            // UCB1 formula
            double exploitation = child->winRate;
            double exploration = MCTS_C * sqrt(logParent / child->visits);
            value = exploitation + exploration;
        }
        
        if (value > bestValue) {
            bestValue = value;
            bestChild = child;
        }
    }
    
    return bestChild;
}

void mcts_expand(MCTSNode* node, char board[BOARD_SIZE][BOARD_SIZE]) {
    if (node->fullyExpanded || node->childCount > 0) return;
    
    Move moves[MAX_MOVES];
    int moveCount;
    
    // Get moves for the next player
    int nextPlayer = 1 - node->player;
    getAllValidMoves(board, nextPlayer, moves, &moveCount);
    
    if (moveCount == 0) {
        node->fullyExpanded = 1;
        return;
    }
    
    // Limit children count for memory efficiency
    int maxChildren = (moveCount < MCTS_MAX_CHILDREN) ? moveCount : MCTS_MAX_CHILDREN;
    
    // Prioritize moves by immediate value
    for (int i = 0; i < moveCount; i++) {
        moves[i].score = countCaptures(board, moves[i], nextPlayer) * 100;
        
        // Bonus for corners
        if ((moves[i].r2 == 0 || moves[i].r2 == 7) && 
            (moves[i].c2 == 0 || moves[i].c2 == 7)) {
            moves[i].score += 200;
        }
        
        // Penalty for dangerous moves
        if (isDangerousMove(board, moves[i], nextPlayer)) {
            moves[i].score -= 300;
        }
    }
    
    // Sort moves by score
    for (int i = 0; i < moveCount - 1; i++) {
        for (int j = i + 1; j < moveCount; j++) {
            if (moves[j].score > moves[i].score) {
                Move temp = moves[i];
                moves[i] = moves[j];
                moves[j] = temp;
            }
        }
    }
    
    // Allocate children array
    node->children = (MCTSNode**)calloc(maxChildren, sizeof(MCTSNode*));
    if (!node->children) return;
    
    node->childCapacity = maxChildren;
    
    // Create child nodes for top moves
    for (int i = 0; i < maxChildren; i++) {
        node->children[i] = mcts_createNode(moves[i], nextPlayer, node);
        if (node->children[i]) {
            node->childCount++;
        }
    }
    
    node->fullyExpanded = (node->childCount == moveCount);
}

double mcts_simulate(char board[BOARD_SIZE][BOARD_SIZE], int startingPlayer, bool useNN) {
    char simBoard[BOARD_SIZE][BOARD_SIZE];
    copyBoard(simBoard, board);
    
    int currentPlayer = startingPlayer;
    int moveCount = 0;
    
    // Run simulation
    while (moveCount < MCTS_SIMULATION_DEPTH) {
        Move moves[MAX_MOVES];
        int numMoves;
        getAllValidMoves(simBoard, currentPlayer, moves, &numMoves);
        
        if (numMoves == 0) break;
        
        Move selectedMove;
        
        if (useNN && moveCount < 5) {
            // Use NNUE for first few moves
            #ifdef HAS_NNUE_WEIGHTS
            Move bestMove = moves[0];
            int bestScore = NEG_INF_SCORE;
            
            // Evaluate top moves
            int evalCount = (numMoves < 10) ? numMoves : 10;
            for (int i = 0; i < evalCount; i++) {
                char tempBoard[BOARD_SIZE][BOARD_SIZE];
                copyBoard(tempBoard, simBoard);
                makeMove(tempBoard, moves[i]);
                
                int score = nnue_evaluate(tempBoard, currentPlayer);
                if (score > bestScore) {
                    bestScore = score;
                    bestMove = moves[i];
                }
            }
            selectedMove = bestMove;
            #else
            selectedMove = moves[rand() % numMoves];
            #endif
        } else {
            // Use heuristics for rest of simulation
            Move bestMove = moves[0];
            int bestCaptures = -1;
            
            // Quick evaluation of top moves
            int evalCount = (numMoves < 5) ? numMoves : 5;
            for (int i = 0; i < evalCount; i++) {
                int captures = countCaptures(simBoard, moves[i], currentPlayer);
                if (captures > bestCaptures) {
                    bestCaptures = captures;
                    bestMove = moves[i];
                }
            }
            
            // Add some randomness
            if (bestCaptures == 0 && rand() % 3 == 0) {
                selectedMove = moves[rand() % numMoves];
            } else {
                selectedMove = bestMove;
            }
        }
        
        makeMove(simBoard, selectedMove);
        currentPlayer = 1 - currentPlayer;
        moveCount++;
    }
    
    // Evaluate final position
    char playerPiece = (startingPlayer == RED_TURN) ? RED : BLUE;
    char opponentPiece = (startingPlayer == RED_TURN) ? BLUE : RED;
    
    int myPieces = countPieces(simBoard, playerPiece);
    int oppPieces = countPieces(simBoard, opponentPiece);
    
    if (myPieces == 0) return 0.0;
    if (oppPieces == 0) return 1.0;
    
    // Return win rate based on piece count
    int totalPieces = myPieces + oppPieces;
    return (double)myPieces / totalPieces;
}

void mcts_backpropagate(MCTSNode* node, double score) {
    while (node != NULL) {
        node->visits++;
        node->totalScore += score;
        node->winRate = node->totalScore / node->visits;
        
        // Invert score for opponent
        score = 1.0 - score;
        node = node->parent;
    }
}

Move mcts_getBestMove(MCTSNode* root) {
    if (!root || root->childCount == 0) {
        return (Move){0, 0, 0, 0, 0, 0};
    }
    
    // Find most visited child (robust selection)
    MCTSNode* bestChild = NULL;
    int mostVisits = 0;
    
    for (int i = 0; i < root->childCount; i++) {
        MCTSNode* child = root->children[i];
        if (child && child->visits > mostVisits) {
            mostVisits = child->visits;
            bestChild = child;
        }
    }
    
    // Debug info
    safePrint("MCTS Best move: visits=%d, winrate=%.3f\n", 
              mostVisits, bestChild ? bestChild->winRate : 0.0);
    
    // Show top 3 moves
    for (int i = 0; i < root->childCount && i < 3; i++) {
        MCTSNode* child = root->children[i];
        if (child && child->visits > MCTS_MIN_VISITS) {
            safePrint("  Move %d: visits=%d, winrate=%.3f\n", 
                      i+1, child->visits, child->winRate);
        }
    }
    
    return bestChild ? bestChild->move : (Move){0, 0, 0, 0, 0, 0};
}

// Thread worker for parallel simulations
typedef struct {
    char board[BOARD_SIZE][BOARD_SIZE];
    int player;
    bool useNN;
    int simulations;
    double totalScore;
} MCTSWorkerData;

void* mcts_simulationWorker(void* arg) {
    MCTSWorkerData* data = (MCTSWorkerData*)arg;
    
    data->totalScore = 0.0;
    for (int i = 0; i < data->simulations; i++) {
        data->totalScore += mcts_simulate(data->board, data->player, data->useNN);
    }
    
    return NULL;
}

Move mcts_search(char board[BOARD_SIZE][BOARD_SIZE], int currentPlayer, bool useNN) {
    clock_gettime(CLOCK_MONOTONIC, &searchStart);
    
    // Create root node
    MCTSNode* root = mcts_createNode((Move){0, 0, 0, 0, 0, 0}, 1 - currentPlayer, NULL);
    if (!root) {
        Move moves[MAX_MOVES];
        int moveCount;
        getAllValidMoves(board, currentPlayer, moves, &moveCount);
        return (moveCount > 0) ? moves[0] : (Move){0, 0, 0, 0, 0, 0};
    }
    
    int iterations = 0;
    GamePhase phase = getGamePhase(board);
    
    // Time allocation based on phase
    double allocatedTime = timeAllocated;
    switch (phase) {
        case PHASE_OPENING:
            allocatedTime *= 0.8;
            break;
        case PHASE_MIDGAME:
            allocatedTime *= 0.85;
            break;
        case PHASE_ENDGAME_EARLY:
            allocatedTime *= 0.9;
            break;
        case PHASE_ENDGAME_LATE:
            allocatedTime *= 0.95;
            break;
    }
    
    // Main MCTS loop
    while (elapsedSeconds() < allocatedTime) {
        MCTSNode* current = root;
        char simBoard[BOARD_SIZE][BOARD_SIZE];
        copyBoard(simBoard, board);
        
        // 1. Selection - traverse tree using UCB1
        while (current->childCount > 0 && current->fullyExpanded) {
            current = mcts_selectChild(current);
            if (!current) break;
            makeMove(simBoard, current->move);
        }
        
        if (!current) {
            iterations++;
            continue;
        }
        
        // 2. Expansion - add new child if not fully expanded
        if (current->visits > 0 && !current->fullyExpanded) {
            mcts_expand(current, simBoard);
            
            if (current->childCount > 0) {
                // Select first unvisited child
                for (int i = 0; i < current->childCount; i++) {
                    if (current->children[i] && current->children[i]->visits == 0) {
                        current = current->children[i];
                        makeMove(simBoard, current->move);
                        break;
                    }
                }
            }
        }
        
        // 3. Simulation - play out random game
        double result;
        
        // Use parallel simulations for leaf nodes with few visits
        if (current->visits < 10 && MCTS_THREADS > 1) {
            pthread_t threads[MCTS_THREADS];
            MCTSWorkerData workerData[MCTS_THREADS];
            
            int simsPerThread = 4;
            
            for (int i = 0; i < MCTS_THREADS; i++) {
                copyBoard(workerData[i].board, simBoard);
                workerData[i].player = currentPlayer;
                workerData[i].useNN = useNN;
                workerData[i].simulations = simsPerThread;
                pthread_create(&threads[i], NULL, mcts_simulationWorker, &workerData[i]);
            }
            
            double totalResult = 0.0;
            for (int i = 0; i < MCTS_THREADS; i++) {
                pthread_join(threads[i], NULL);
                totalResult += workerData[i].totalScore;
            }
            
            result = totalResult / (MCTS_THREADS * simsPerThread);
        } else {
            // Single simulation for well-visited nodes
            result = mcts_simulate(simBoard, currentPlayer, useNN);
        }
        
        // 4. Backpropagation - update all nodes in path
        mcts_backpropagate(current, result);
        
        iterations++;
        
        // Time check
        if (iterations % MCTS_TIME_CHECK_INTERVAL == 0) {
            if (elapsedSeconds() > allocatedTime) {
                break;
            }
        }
    }
    
    const char* engineType = useNN ? "MCTS-NN" : "MCTS-Classic";
    safePrint("%s: %d iterations in %.2fs\n", engineType, iterations, elapsedSeconds());
    
    // Get best move
    Move bestMove = mcts_getBestMove(root);
    
    // Clean up
    mcts_freeTree(root);
    
    // Validate move
    if (bestMove.r1 == bestMove.r2 && bestMove.c1 == bestMove.c2) {
        safePrint("Warning: Invalid move from MCTS, using fallback\n");
        Move moves[MAX_MOVES];
        int moveCount;
        getAllValidMoves(board, currentPlayer, moves, &moveCount);
        if (moveCount > 0) {
            bestMove = moves[0];
        }
    }
    
    // Check for repetition
    unsigned long long currentHash = computeHash(board);
    if (isRepetition(currentHash, bestMove)) {
        safePrint("Avoiding repetition in MCTS\n");
        
        // Try to find alternative from root's children
        for (int i = 0; i < root->childCount; i++) {
            MCTSNode* child = root->children[i];
            if (child && child->visits > MCTS_MIN_VISITS) {
                Move altMove = child->move;
                if (altMove.r1 != bestMove.r1 || altMove.c1 != bestMove.c1 ||
                    altMove.r2 != bestMove.r2 || altMove.c2 != bestMove.c2) {
                    bestMove = altMove;
                    break;
                }
            }
        }
    }
    
    addToHistory(currentHash, bestMove);
    
    // Convert to 1-indexed
    bestMove.r1++;
    bestMove.c1++;
    bestMove.r2++;
    bestMove.c2++;
    
    return bestMove;
}

// ===== EUNSONG ENGINE =====

int eunsongValidMove(char board[BOARD_SIZE][BOARD_SIZE], int r1, int c1, int r2, int c2, char player) {
    if (r1 < 0 || r1 >= 8 || c1 < 0 || c1 >= 8 || 
        r2 < 0 || r2 >= 8 || c2 < 0 || c2 >= 8) return 0;
    if (board[r1][c1] != player || board[r2][c2] != '.') return 0;
    
    int dr = abs(r2 - r1), dc = abs(c2 - c1);
    if ((dr == 1 || dr == 0) && (dc == 1 || dc == 0) && !(dr == 0 && dc == 0)) return 1;
    if ((dr == 2 || dr == 0) && (dc == 2 || dc == 0) && !(dr == 0 && dc == 0)) return 2;
    return 0;
}

void eunsongApplyMove(char board[BOARD_SIZE][BOARD_SIZE], int sx, int sy, int tx, int ty, char player) {
    int isJump = abs(tx - sx) > 1 || abs(ty - sy) > 1;
    if (isJump) board[sx][sy] = '.';
    board[tx][ty] = player;
    
    char opp = (player == 'R') ? 'B' : 'R';
    for (int i = 0; i < 8; i++) {
        int nr = tx + moveDir[i][0];
        int nc = ty + moveDir[i][1];
        if (nr >= 0 && nr < 8 && nc >= 0 && nc < 8 && board[nr][nc] == opp) {
            board[nr][nc] = player;
        }
    }
}

int eunsongEvaluate(char board[BOARD_SIZE][BOARD_SIZE], char player) {
    char opp = (player == 'R') ? 'B' : 'R';
    int p = 0, o = 0;
    
    GamePhase phase = getGamePhase(board);
    const double (*posWeights)[8];
    
    switch (phase) {
        case PHASE_OPENING:
            posWeights = POSITION_WEIGHTS_OPENING;
            break;
        case PHASE_MIDGAME:
            posWeights = POSITION_WEIGHTS_MIDGAME;
            break;
        default:
            posWeights = POSITION_WEIGHTS_ENDGAME;
            break;
    }
    
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            if (board[i][j] == player) {
                p++;
                p += posWeights[i][j] * 5;
            } else if (board[i][j] == opp) {
                o++;
                o += posWeights[i][j] * 5;
            }
        }
    }
    
    return (p - o) * 100;
}

int eunsongGenerateMoves(char board[BOARD_SIZE][BOARD_SIZE], char player, Move *moves) {
    int count = 0;
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            if (board[i][j] != player) continue;
            
            for (int d = 0; d < 8; d++) {
                for (int dist = 1; dist <= 2; dist++) {
                    int ni = i + moveDir[d][0] * dist;
                    int nj = j + moveDir[d][1] * dist;
                    if (ni >= 0 && ni < 8 && nj >= 0 && nj < 8 && board[ni][nj] == '.') {
                        moves[count].r1 = i;
                        moves[count].c1 = j;
                        moves[count].r2 = ni;
                        moves[count].c2 = nj;
                        moves[count].moveType = dist;
                        moves[count].score = 0;
                        count++;
                    }
                }
            }
        }
    }
    return count;
}

int eunsongNegamax(char board[BOARD_SIZE][BOARD_SIZE], char player, int depth, int alpha, int beta) {
    atomic_fetch_add(&nodeCount, 1);
    
    if (time(NULL) - eunsongStartTime >= (int)(TIME_LIMIT * 0.9) || depth == 0) {
        return eunsongEvaluate(board, player);
    }
    
    Move moves[MAX_MOVES];
    int n = eunsongGenerateMoves(board, player, moves);
    if (n == 0) return eunsongEvaluate(board, player);
    
    // Move ordering
    for (int i = 0; i < n; i++) {
        char opp = (player == 'R') ? 'B' : 'R';
        int captures = 0;
        for (int d = 0; d < 8; d++) {
            int nr = moves[i].r2 + moveDir[d][0];
            int nc = moves[i].c2 + moveDir[d][1];
            if (nr >= 0 && nr < 8 && nc >= 0 && nc < 8 && board[nr][nc] == opp) {
                captures++;
            }
        }
        
        GamePhase phase = getGamePhase(board);
        const double (*posWeights)[8];
        switch (phase) {
            case PHASE_OPENING:
                posWeights = POSITION_WEIGHTS_OPENING;
                break;
            case PHASE_MIDGAME:
                posWeights = POSITION_WEIGHTS_MIDGAME;
                break;
            default:
                posWeights = POSITION_WEIGHTS_ENDGAME;
                break;
        }
        
        moves[i].score = captures * 100 + posWeights[moves[i].r2][moves[i].c2] * 10;
        moves[i].score += historyTable[moves[i].r1][moves[i].c1][moves[i].r2][moves[i].c2];
    }
    
    // Sort moves
    for (int i = 0; i < n - 1; i++) {
        for (int j = i + 1; j < n; j++) {
            if (moves[j].score > moves[i].score) {
                Move temp = moves[i];
                moves[i] = moves[j];
                moves[j] = temp;
            }
        }
    }
    
    int best = NEG_INF_SCORE;
    for (int i = 0; i < n; i++) {
        char sim[8][8];
        copyBoard(sim, board);
        eunsongApplyMove(sim, moves[i].r1, moves[i].c1, moves[i].r2, moves[i].c2, player);
        
        int score = -eunsongNegamax(sim, (player == 'R') ? 'B' : 'R', depth - 1, -beta, -alpha);
        
        if (score > best) {
            best = score;
            
            if (best >= beta) {
                historyTable[moves[i].r1][moves[i].c1][moves[i].r2][moves[i].c2] += depth * depth;
                if (depth < MAX_DEPTH) {
                    killerMoves[depth][1] = killerMoves[depth][0];
                    killerMoves[depth][0] = moves[i];
                }
            }
        }
        
        if (best > alpha) alpha = best;
        if (alpha >= beta) break;
    }
    
    return best;
}

void* eunsongThreadFunc(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    char sim[8][8];
    copyBoard(sim, data->board);
    eunsongApplyMove(sim, data->move.r1, data->move.c1, data->move.r2, data->move.c2, data->player);
    data->score = -eunsongNegamax(sim, (data->player == 'R') ? 'B' : 'R', data->depth - 1, NEG_INF_SCORE, INF_SCORE);
    return NULL;
}

Move getEunsongMove(char board[BOARD_SIZE][BOARD_SIZE], int currentPlayer) {
    Move moves[MAX_MOVES];
    char player = (currentPlayer == RED_TURN) ? 'R' : 'B';
    int n = eunsongGenerateMoves(board, player, moves);
    
    if (n == 0) {
        return (Move){0, 0, 0, 0, 0, 0};
    }
    
    eunsongStartTime = time(NULL);
    atomic_store(&nodeCount, 0);
    
    int maxDepth = 4;
    if (countPieces(board, EMPTY) < 10) maxDepth = 6;
    
    Move bestMove = moves[0];
    int bestScore = NEG_INF_SCORE;
    
    for (int depth = 1; depth <= maxDepth && time(NULL) - eunsongStartTime < (int)(TIME_LIMIT * 0.9); depth++) {
        pthread_t threads[4];
        ThreadData data[4];
        
        for (int i = 0; i < n; i += 4) {
            int batch = (i + 4 > n) ? (n - i) : 4;
            
            for (int j = 0; j < batch; j++) {
                copyBoard(data[j].board, board);
                data[j].move = moves[i + j];
                data[j].player = player;
                data[j].score = NEG_INF_SCORE;
                data[j].depth = depth;
                pthread_create(&threads[j], NULL, eunsongThreadFunc, &data[j]);
            }
            
            for (int j = 0; j < batch; j++) {
                pthread_join(threads[j], NULL);
                if (data[j].score > bestScore) {
                    bestScore = data[j].score;
                    bestMove = moves[i + j];
                }
            }
            
            if (time(NULL) - eunsongStartTime >= (int)(TIME_LIMIT * 0.9)) break;
        }
        
        safePrint("Eunsong depth %d: score=%d, nodes=%ld\n", depth, bestScore, atomic_load(&nodeCount));
    }
    
    bestMove.r1++;
    bestMove.c1++;
    bestMove.r2++;
    bestMove.c2++;
    
    return bestMove;
}

// ===== MODIFIED NEGAMAX FOR HYBRID EVALUATION =====

int negamaxPhased(char board[BOARD_SIZE][BOARD_SIZE], int depth, int alpha, int beta, 
                  int currentPlayer, Move* bestMove, GamePhase phase, bool useHybrid) {
    atomic_fetch_add(&nodeCount, 1);
    
    // Time check
    if ((atomic_load(&nodeCount) & 127) == 0) {
        if (elapsedSeconds() > timeAllocated * 0.85) {
            atomic_store(&timeUp, 1);
            return useHybrid ? evaluateHybrid(board, currentPlayer, phase) : 
                              evaluateBoardPhased(board, currentPlayer, phase);
        }
    }
    
    if (atomic_load(&timeUp)) {
        return useHybrid ? evaluateHybrid(board, currentPlayer, phase) : 
                          evaluateBoardPhased(board, currentPlayer, phase);
    }
    
    // Terminal node or depth limit
    if (depth == 0) {
        return useHybrid ? evaluateHybrid(board, currentPlayer, phase) : 
                          evaluateBoardPhased(board, currentPlayer, phase);
    }
    
    // Transposition table lookup
    unsigned long long hash = computeHash(board);
    int ttIndex = (hash & HASH_MASK);
    TTEntry* ttEntry = &transpositionTable[ttIndex];
    
    if (ttEntry->hash == hash && ttEntry->depth >= depth) {
        if (ttEntry->flag == 0) {  // Exact
            if (bestMove) *bestMove = ttEntry->bestMove;
            return ttEntry->score;
        } else if (ttEntry->flag == 1) {  // Lower bound
            alpha = (alpha > ttEntry->score) ? alpha : ttEntry->score;
        } else if (ttEntry->flag == 2) {  // Upper bound
            beta = (beta < ttEntry->score) ? beta : ttEntry->score;
        }
        
        if (alpha >= beta) {
            if (bestMove) *bestMove = ttEntry->bestMove;
            return ttEntry->score;
        }
    }
    
    // Generate moves
    Move moves[MAX_MOVES];
    int moveCount;
    getAllValidMoves(board, currentPlayer, moves, &moveCount);
    
    // No moves - pass turn
    if (moveCount == 0) {
        return -negamaxPhased(board, depth - 1, -beta, -alpha, 1 - currentPlayer, NULL, phase, useHybrid);
    }
    
    // Move ordering
    orderMovesPhased(moves, moveCount, board, currentPlayer, depth, phase);
    
    Move localBestMove = moves[0];
    int bestScore = NEG_INF_SCORE;
    int flag = 2;  // Upper bound
    
    // Search moves
    int maxMovesToEval = moveCount;
    if (elapsedSeconds() > timeAllocated * 0.5 && moveCount > 10) {
        maxMovesToEval = 10;
    }
    
    for (int i = 0; i < maxMovesToEval && !atomic_load(&timeUp); i++) {
        char newBoard[BOARD_SIZE][BOARD_SIZE];
        copyBoard(newBoard, board);
        makeMove(newBoard, moves[i]);
        
        // Negamax recursion
        int score = -negamaxPhased(newBoard, depth - 1, -beta, -alpha, 
                                  1 - currentPlayer, NULL, phase, useHybrid);
        
        if (score > bestScore) {
            bestScore = score;
            localBestMove = moves[i];
            
            if (score > alpha) {
                alpha = score;
                flag = 0;  // Exact
                
                if (alpha >= beta) {
                    // Update history and killer moves
                    if (depth < MAX_DEPTH) {
                        killerMoves[depth][1] = killerMoves[depth][0];
                        killerMoves[depth][0] = moves[i];
                    }
                    historyTable[moves[i].r1][moves[i].c1][moves[i].r2][moves[i].c2] += depth * depth;
                    
                    flag = 1;  // Lower bound
                    break;
                }
            }
        }
    }
    
    // Store in TT
    if (!atomic_load(&timeUp)) {
        ttEntry->hash = hash;
        ttEntry->score = bestScore;
        ttEntry->depth = depth;
        ttEntry->flag = flag;
        ttEntry->bestMove = localBestMove;
    }
    
    if (bestMove) *bestMove = localBestMove;
    return bestScore;
}

// Thread worker for parallel search
void* minimaxWorkerPhased(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    
    char tempBoard[BOARD_SIZE][BOARD_SIZE];
    copyBoard(tempBoard, data->board);
    makeMove(tempBoard, data->move);
    
    GamePhase phase = getGamePhase(tempBoard);
    int player = (data->player == 'R') ? RED_TURN : BLUE_TURN;
    
    data->score = -negamaxPhased(tempBoard, data->depth - 1, 
                                NEG_INF_SCORE, INF_SCORE, 
                                1 - player, NULL, phase, data->useHybrid);
    
    return NULL;
}

Move getMinimaxMoveClassic(char board[BOARD_SIZE][BOARD_SIZE], int currentPlayer) {
    // 즉시 승리 체크
    Move instantWin = checkInstantWin(board, currentPlayer);
    if (instantWin.r1 != 0 || instantWin.c1 != 0) {
        return instantWin;
    }
    
    Move bestMove = {0, 0, 0, 0, 0, 0};
    
    clock_gettime(CLOCK_MONOTONIC, &searchStart);
    atomic_store(&timeUp, 0);
    atomic_store(&nodeCount, 0);
    
    memset(killerMoves, 0, sizeof(killerMoves));
    
    GamePhase phase = getGamePhase(board);
    
    // Dynamic depth based on phase and piece count
    int maxDepth = 5;
    int emptyCount = countPieces(board, EMPTY);
    
    if (phase == PHASE_OPENING) {
        maxDepth = 4;
    } else if (phase == PHASE_MIDGAME) {
        maxDepth = 5;
    } else if (phase == PHASE_ENDGAME_EARLY) {
        maxDepth = 6;
    } else {  // PHASE_ENDGAME_LATE
        maxDepth = 7;
        if (emptyCount < 10) maxDepth = 8;
    }
    
    int bestScore = NEG_INF_SCORE;
    
    // Generate all moves
    Move moves[MAX_MOVES];
    int moveCount;
    getAllValidMoves(board, currentPlayer, moves, &moveCount);
    
    if (moveCount == 0) {
        return (Move){0, 0, 0, 0, 0, 0};
    }
    
    // Filter bad moves first
    filterBadMoves(moves, &moveCount, board, currentPlayer);
    
    // Iterative deepening
    for (int depth = 1; depth <= maxDepth && !atomic_load(&timeUp); depth++) {
        // Use previous best move for move ordering
        if (depth > 1 && bestMove.r1 != 0) {
            for (int i = 0; i < moveCount; i++) {
                if (moves[i].r1 == bestMove.r1 && moves[i].c1 == bestMove.c1 &&
                    moves[i].r2 == bestMove.r2 && moves[i].c2 == bestMove.c2) {
                    Move temp = moves[0];
                    moves[0] = moves[i];
                    moves[i] = temp;
                    break;
                }
            }
        }
        
        orderMovesPhased(moves, moveCount, board, currentPlayer, depth, phase);
        
        // Parallel search for first few moves
        int parallelMoves = (moveCount < MINIMAX_THREADS) ? moveCount : MINIMAX_THREADS;
        
        if (depth >= 3 && parallelMoves > 1 && moveCount > 4) {
            pthread_t threads[MINIMAX_THREADS];
            ThreadData threadData[MINIMAX_THREADS];
            
            char player = (currentPlayer == RED_TURN) ? 'R' : 'B';
            
            for (int i = 0; i < parallelMoves; i++) {
                copyBoard(threadData[i].board, board);
                threadData[i].move = moves[i];
                threadData[i].player = player;
                threadData[i].score = NEG_INF_SCORE;
                threadData[i].depth = depth;
                threadData[i].threadId = i;
                threadData[i].useNN = false;
                threadData[i].useHybrid = false;  // Classic evaluation
                
                pthread_create(&threads[i], NULL, minimaxWorkerPhased, &threadData[i]);
            }
            
            for (int i = 0; i < parallelMoves; i++) {
                pthread_join(threads[i], NULL);
                
                if (threadData[i].score > bestScore) {
                    bestScore = threadData[i].score;
                    bestMove = moves[i];
                }
            }
            
            // Search remaining moves sequentially
            for (int i = parallelMoves; i < moveCount && !atomic_load(&timeUp); i++) {
                char tempBoard[BOARD_SIZE][BOARD_SIZE];
                copyBoard(tempBoard, board);
                makeMove(tempBoard, moves[i]);
                
                int score = -negamaxPhased(tempBoard, depth - 1, NEG_INF_SCORE, INF_SCORE,
                                          1 - currentPlayer, NULL, phase, false);
                
                if (score > bestScore) {
                    bestScore = score;
                    bestMove = moves[i];
                }
                
                if (elapsedSeconds() > timeAllocated * 0.6) break;
            }
            
            safePrint("Minimax Classic depth %d: score=%d, nodes=%ld (MT)\n", 
                      depth, bestScore, atomic_load(&nodeCount));
        } else {
            // Sequential search for shallow depths
            for (int i = 0; i < moveCount && !atomic_load(&timeUp); i++) {
                char tempBoard[BOARD_SIZE][BOARD_SIZE];
                copyBoard(tempBoard, board);
                makeMove(tempBoard, moves[i]);
                
                int score = -negamaxPhased(tempBoard, depth - 1, NEG_INF_SCORE, INF_SCORE,
                                          1 - currentPlayer, NULL, phase, false);
                
                if (score > bestScore) {
                    bestScore = score;
                    bestMove = moves[i];
                }
            }
            
            safePrint("Minimax Classic depth %d: score=%d, nodes=%ld\n", 
                      depth, bestScore, atomic_load(&nodeCount));
        }
        
        // Early exit if winning/losing
        if (bestScore > INF_SCORE/2 || bestScore < -INF_SCORE/2) {
            break;
        }
        
        // Time management
        if (elapsedSeconds() > timeAllocated * 0.3 * depth / maxDepth) {
            break;
        }
    }
    
    // Fallback if no good move found
    if (bestMove.r1 == 0 && bestMove.c1 == 0 && 
        bestMove.r2 == 0 && bestMove.c2 == 0) {
        bestMove = moves[0];
    }
    
    // Check for repetition
    unsigned long long currentHash = computeHash(board);
    if (isRepetition(currentHash, bestMove)) {
        safePrint("Avoiding repetition in Minimax\n");
        
        for (int i = 0; i < moveCount; i++) {
            if (moves[i].r1 != bestMove.r1 || moves[i].c1 != bestMove.c1 ||
                moves[i].r2 != bestMove.r2 || moves[i].c2 != bestMove.c2) {
                bestMove = moves[i];
                break;
            }
        }
    }
    
    addToHistory(currentHash, bestMove);
    
    bestMove.r1++;
    bestMove.c1++;
    bestMove.r2++;
    bestMove.c2++;
    
    return bestMove;
}

Move getMinimaxMoveNN(char board[BOARD_SIZE][BOARD_SIZE], int currentPlayer) {
    // 즉시 승리 체크
    Move instantWin = checkInstantWin(board, currentPlayer);
    if (instantWin.r1 != 0 || instantWin.c1 != 0) {
        return instantWin;
    }
    
    Move bestMove = {0, 0, 0, 0, 0, 0};
    
    clock_gettime(CLOCK_MONOTONIC, &searchStart);
    atomic_store(&timeUp, 0);
    atomic_store(&nodeCount, 0);
    
    memset(killerMoves, 0, sizeof(killerMoves));
    
    GamePhase phase = getGamePhase(board);
    
    // Dynamic depth based on phase and piece count
    int maxDepth = 5;
    int emptyCount = countPieces(board, EMPTY);
    
    if (phase == PHASE_OPENING) {
        maxDepth = 4;
    } else if (phase == PHASE_MIDGAME) {
        maxDepth = 5;
    } else if (phase == PHASE_ENDGAME_EARLY) {
        maxDepth = 6;
    } else {  // PHASE_ENDGAME_LATE
        maxDepth = 7;
        if (emptyCount < 10) maxDepth = 8;
    }
    
    int bestScore = NEG_INF_SCORE;
    
    // Generate all moves
    Move moves[MAX_MOVES];
    int moveCount;
    getAllValidMoves(board, currentPlayer, moves, &moveCount);
    
    if (moveCount == 0) {
        return (Move){0, 0, 0, 0, 0, 0};
    }
    
    // Filter bad moves first
    filterBadMoves(moves, &moveCount, board, currentPlayer);
    
    // Iterative deepening with HYBRID evaluation
    for (int depth = 1; depth <= maxDepth && !atomic_load(&timeUp); depth++) {
        // Use previous best move for move ordering
        if (depth > 1 && bestMove.r1 != 0) {
            for (int i = 0; i < moveCount; i++) {
                if (moves[i].r1 == bestMove.r1 && moves[i].c1 == bestMove.c1 &&
                    moves[i].r2 == bestMove.r2 && moves[i].c2 == bestMove.c2) {
                    Move temp = moves[0];
                    moves[0] = moves[i];
                    moves[i] = temp;
                    break;
                }
            }
        }
        
        orderMovesPhased(moves, moveCount, board, currentPlayer, depth, phase);
        
        // Parallel search for first few moves
        int parallelMoves = (moveCount < MINIMAX_THREADS) ? moveCount : MINIMAX_THREADS;
        
        if (depth >= 3 && parallelMoves > 1 && moveCount > 4) {
            pthread_t threads[MINIMAX_THREADS];
            ThreadData threadData[MINIMAX_THREADS];
            
            char player = (currentPlayer == RED_TURN) ? 'R' : 'B';
            
            for (int i = 0; i < parallelMoves; i++) {
                copyBoard(threadData[i].board, board);
                threadData[i].move = moves[i];
                threadData[i].player = player;
                threadData[i].score = NEG_INF_SCORE;
                threadData[i].depth = depth;
                threadData[i].threadId = i;
                threadData[i].useNN = true;
                threadData[i].useHybrid = true;  // Use hybrid evaluation
                
                pthread_create(&threads[i], NULL, minimaxWorkerPhased, &threadData[i]);
            }
            
            for (int i = 0; i < parallelMoves; i++) {
                pthread_join(threads[i], NULL);
                
                if (threadData[i].score > bestScore) {
                    bestScore = threadData[i].score;
                    bestMove = moves[i];
                }
            }
            
            // Search remaining moves sequentially
            for (int i = parallelMoves; i < moveCount && !atomic_load(&timeUp); i++) {
                char tempBoard[BOARD_SIZE][BOARD_SIZE];
                copyBoard(tempBoard, board);
                makeMove(tempBoard, moves[i]);
                
                int score = -negamaxPhased(tempBoard, depth - 1, NEG_INF_SCORE, INF_SCORE,
                                          1 - currentPlayer, NULL, phase, true);
                
                if (score > bestScore) {
                    bestScore = score;
                    bestMove = moves[i];
                }
                
                if (elapsedSeconds() > timeAllocated * 0.6) break;
            }
            
            safePrint("Minimax-Hybrid depth %d: score=%d, nodes=%ld (MT)\n", 
                      depth, bestScore, atomic_load(&nodeCount));
        } else {
            // Sequential search for shallow depths
            for (int i = 0; i < moveCount && !atomic_load(&timeUp); i++) {
                char tempBoard[BOARD_SIZE][BOARD_SIZE];
                copyBoard(tempBoard, board);
                makeMove(tempBoard, moves[i]);
                
                int score = -negamaxPhased(tempBoard, depth - 1, NEG_INF_SCORE, INF_SCORE,
                                          1 - currentPlayer, NULL, phase, true);
                
                if (score > bestScore) {
                    bestScore = score;
                    bestMove = moves[i];
                }
            }
            
            safePrint("Minimax-Hybrid depth %d: score=%d, nodes=%ld\n", 
                      depth, bestScore, atomic_load(&nodeCount));
        }
        
        // Early exit if winning/losing
        if (bestScore > INF_SCORE/2 || bestScore < -INF_SCORE/2) {
            break;
        }
        
        // Time management
        if (elapsedSeconds() > timeAllocated * 0.3 * depth / maxDepth) {
            break;
        }
    }
    
    // Fallback if no good move found
    if (bestMove.r1 == 0 && bestMove.c1 == 0 && 
        bestMove.r2 == 0 && bestMove.c2 == 0) {
        bestMove = moves[0];
    }
    
    // Check for repetition
    unsigned long long currentHash = computeHash(board);
    if (isRepetition(currentHash, bestMove)) {
        safePrint("Avoiding repetition in Minimax-Hybrid\n");
        
        for (int i = 0; i < moveCount; i++) {
            if (moves[i].r1 != bestMove.r1 || moves[i].c1 != bestMove.c1 ||
                moves[i].r2 != bestMove.r2 || moves[i].c2 != bestMove.c2) {
                bestMove = moves[i];
                break;
            }
        }
    }
    
    addToHistory(currentHash, bestMove);
    
    bestMove.r1++;
    bestMove.c1++;
    bestMove.r2++;
    bestMove.c2++;
    
    return bestMove;
}

// ===== ENGINE IMPLEMENTATIONS =====

Move getMCTSMoveNN(char board[BOARD_SIZE][BOARD_SIZE], int currentPlayer) {
    // 즉시 승리 체크
    Move instantWin = checkInstantWin(board, currentPlayer);
    if (instantWin.r1 != 0 || instantWin.c1 != 0) {
        return instantWin;
    }
    
    return mcts_search(board, currentPlayer, true);
}

Move getMCTSMoveClassic(char board[BOARD_SIZE][BOARD_SIZE], int currentPlayer) {
    // 즉시 승리 체크
    Move instantWin = checkInstantWin(board, currentPlayer);
    if (instantWin.r1 != 0 || instantWin.c1 != 0) {
        return instantWin;
    }
    
    return mcts_search(board, currentPlayer, false);
}

Move getTournamentBeastMove(char board[BOARD_SIZE][BOARD_SIZE], int currentPlayer) {
    GamePhase phase = getGamePhase(board);
    int totalPieces = countPieces(board, RED) + countPieces(board, BLUE);
    
    safePrint("Tournament Beast: pieces=%d, phase=", totalPieces);
    
    clock_gettime(CLOCK_MONOTONIC, &searchStart);
    atomic_store(&timeUp, 0);
    atomic_store(&nodeCount, 0);
    
    // Phase-based strategy selection with hybrid evaluation
    if (phase == PHASE_OPENING) {
        safePrint("Opening (Minimax hybrid)\n");
        return getMinimaxMoveNN(board, currentPlayer);
    } else if (phase == PHASE_MIDGAME) {
        safePrint("Midgame (MCTS hybrid)\n");
        return getMCTSMoveNN(board, currentPlayer);
    } else if (phase == PHASE_ENDGAME_EARLY) {
        safePrint("Early Endgame (MCTS hybrid)\n");
        return getMCTSMoveNN(board, currentPlayer);
    } else {
        safePrint("Late Endgame (Minimax hybrd)\n");
        return getMinimaxMoveNN(board, currentPlayer);
    }
}

// ===== MAIN AI INTERFACE =====

Move generate_move() {
    Move moves[MAX_MOVES];
    int moveCount;
    
    currentPlayer = (my_color == RED) ? RED_TURN : BLUE_TURN;
    
    getAllValidMoves(board, currentPlayer, moves, &moveCount);
    
    if (moveCount == 0) {
        return (Move){0, 0, 0, 0, 0, 0};
    }
    
    // 즉시 승리 체크 (공통)
    Move instantWin = checkInstantWin(board, currentPlayer);
    if (instantWin.r1 != 0 || instantWin.c1 != 0) {
        totalMoveCount++;
        return instantWin;
    }
    
    // 나쁜 수 필터링
    filterBadMoves(moves, &moveCount, board, currentPlayer);
    
    if (moveCount == 0) {
        getAllValidMoves(board, currentPlayer, moves, &moveCount);
    }
    
    currentPhase = getGamePhase(board);
    
    // Time allocation based on phase
    switch (currentPhase) {
        case PHASE_OPENING:
            timeAllocated = TIME_LIMIT * 0.7;
            break;
        case PHASE_MIDGAME:
            timeAllocated = TIME_LIMIT * 0.8;
            break;
        case PHASE_ENDGAME_EARLY:
            timeAllocated = TIME_LIMIT * 0.85;
            break;
        case PHASE_ENDGAME_LATE:
            timeAllocated = TIME_LIMIT * 0.9;
            break;
    }
    
    Move bestMove;
    
    // 엔진별 수 선택
    switch (aiEngine) {
        case ENGINE_EUNSONG:
            bestMove = getEunsongMove(board, currentPlayer);
            break;
        case ENGINE_MCTS_NN:
            bestMove = getMCTSMoveNN(board, currentPlayer);
            break;
        case ENGINE_MCTS_CLASSIC:
            bestMove = getMCTSMoveClassic(board, currentPlayer);
            break;
        case ENGINE_MINIMAX_NN:
            bestMove = getMinimaxMoveNN(board, currentPlayer);
            break;
        case ENGINE_MINIMAX_CLASSIC:
            bestMove = getMinimaxMoveClassic(board, currentPlayer);
            break;
        case ENGINE_TOURNAMENT_BEAST:
        default:
            bestMove = getTournamentBeastMove(board, currentPlayer);
            break;
    }
    
    totalMoveCount++;
    
    return bestMove;
}

void orderMovesPhased(Move* moves, int moveCount, char board[BOARD_SIZE][BOARD_SIZE], 
                      int currentPlayer, int depth, GamePhase phase) {
    char playerPiece = (currentPlayer == RED_TURN) ? RED : BLUE;
    char opponentPiece = (currentPlayer == RED_TURN) ? BLUE : RED;
    
    // 현재 돌 수 차이 계산
    int currentMyPieces = countPieces(board, playerPiece);
    int currentOppPieces = countPieces(board, opponentPiece);
    int currentDiff = currentMyPieces - currentOppPieces;
    
    // Get position weights for current phase
    const double (*posWeights)[8];
    switch (phase) {
        case PHASE_OPENING:
            posWeights = POSITION_WEIGHTS_OPENING;
            break;
        case PHASE_MIDGAME:
            posWeights = POSITION_WEIGHTS_MIDGAME;
            break;
        default:
            posWeights = POSITION_WEIGHTS_ENDGAME;
            break;
    }
    
    for (int i = 0; i < moveCount; i++) {
        moves[i].score = 0;
        
        // 1. Killer move bonus
        if (depth < MAX_DEPTH) {
            if ((moves[i].r1 == killerMoves[depth][0].r1 && 
                 moves[i].c1 == killerMoves[depth][0].c1 &&
                 moves[i].r2 == killerMoves[depth][0].r2 && 
                 moves[i].c2 == killerMoves[depth][0].c2) ||
                (moves[i].r1 == killerMoves[depth][1].r1 && 
                 moves[i].c1 == killerMoves[depth][1].c1 &&
                 moves[i].r2 == killerMoves[depth][1].r2 && 
                 moves[i].c2 == killerMoves[depth][1].c2)) {
                moves[i].score += 900;
            }
        }
        
        // 2. History heuristic
        moves[i].score += historyTable[moves[i].r1][moves[i].c1][moves[i].r2][moves[i].c2];
        
        // 3. 수를 적용한 후의 보드 상태 계산
        char tempBoard[BOARD_SIZE][BOARD_SIZE];
        copyBoard(tempBoard, board);
        makeMove(tempBoard, moves[i]);
        
        int afterMyPieces = countPieces(tempBoard, playerPiece);
        int afterOppPieces = countPieces(tempBoard, opponentPiece);
        int afterDiff = afterMyPieces - afterOppPieces;
        int emptyAfter = countPieces(tempBoard, EMPTY);
        
        // === 승리 조건 체크 (최우선순위) ===
        // 1. 상대 돌을 모두 없앨 수 있는 수
        if (afterOppPieces == 0) {
            moves[i].score += 100000;  // 즉시 승리 - 매우 높은 점수
        }
        // 2. 마지막 빈칸을 채우면서 이기는 경우
        else if (emptyAfter == 0 && afterDiff > 0) {
            moves[i].score += 50000;  // 종반 승리 - 높은 점수
        }
        // 3. 현재 빈칸이 1개일 때 마지막 수
        else if (countPieces(board, EMPTY) == 1 && emptyAfter == 0) {
            if (afterDiff > 0) {
                moves[i].score += 50000;  // 마지막 수로 승리
            } else {
                moves[i].score -= 50000;  // 마지막 수로 패배
            }
        }
        
        // 돌 수 차이 증가량
        int diffIncrease = afterDiff - currentDiff;
        moves[i].score += diffIncrease * 150;
        
        // 4. Position value difference
        double posDiff = posWeights[moves[i].r2][moves[i].c2] - 
                        posWeights[moves[i].r1][moves[i].c1];
        moves[i].score += (int)(posDiff * 10);
        
        // 5. Phase-specific bonuses
        if (phase == PHASE_OPENING) {
            // Prefer clones in opening
            if (moves[i].moveType == CLONE) {
                moves[i].score += 50;
            }
            // Jump에 약간의 페널티
            if (moves[i].moveType == JUMP) {
                moves[i].score -= 30;
            }
        } else if (phase == PHASE_ENDGAME_LATE) {
            // Corner moves are crucial
            if ((moves[i].r2 == 0 || moves[i].r2 == 7) && 
                (moves[i].c2 == 0 || moves[i].c2 == 7)) {
                moves[i].score += 1000;
            }
            // Never leave corners
            if ((moves[i].r1 == 0 || moves[i].r1 == 7) && 
                (moves[i].c1 == 0 || moves[i].c1 == 7)) {
                moves[i].score -= 2000;
            }
        }
        
        // 6. Avoid bad squares
        // X-squares are dangerous
        if (((moves[i].r2 == 1 || moves[i].r2 == 6) && 
             (moves[i].c2 == 1 || moves[i].c2 == 6))) {
            int cornerR = (moves[i].r2 == 1) ? 0 : 7;
            int cornerC = (moves[i].c2 == 1) ? 0 : 7;
            if (board[cornerR][cornerC] != playerPiece) {
                moves[i].score -= 500;  // Big penalty
            }
        }
    }
    
    // Sort moves by score (insertion sort for small arrays)
    for (int i = 1; i < moveCount; i++) {
        Move key = moves[i];
        int j = i - 1;
        while (j >= 0 && moves[j].score < key.score) {
            moves[j + 1] = moves[j];
            j--;
        }
        moves[j + 1] = key;
    }
}

// ===== LED DISPLAY FUNCTIONS =====

void initLEDDisplay() {
    if (!led_initialized && boardServerEnabled) {
        if (init_led_display() == 0) {
            led_initialized = 1;
            show_team_name_on_led();
            safePrint("✓ LED display initialized!\n");
        } else {
            safePrint("Warning: LED display initialization failed\n");
        }
    }
}

void updateLEDDisplay() {
    if (led_initialized && boardServerEnabled) {
        render_board_to_led(board);
    }
}

void cleanupLEDDisplay() {
    if (led_initialized) {
        cleanup_led_display();
        led_initialized = 0;
    }
}

// ===== NETWORK FUNCTIONS =====

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
    
    int i = 0;
    cJSON* row = NULL;
    cJSON_ArrayForEach(row, board_array) {
        if (cJSON_IsString(row) && i < BOARD_SIZE) {
            const char* row_str = row->valuestring;
            for (int j = 0; j < BOARD_SIZE && row_str[j]; j++) {
                board[i][j] = row_str[j];
            }
            i++;
        }
    }
    
    updateLEDDisplay();
}

void printBoard() {
    safePrint("\n=== Current Board ===\n");
    safePrint("  1 2 3 4 5 6 7 8\n");
    
    for (int i = 0; i < BOARD_SIZE; i++) {
        safePrint("%d ", i + 1);
        for (int j = 0; j < BOARD_SIZE; j++) {
            safePrint("%c ", board[i][j]);
        }
        safePrint("\n");
    }
    
    int red_count = countPieces(board, RED);
    int blue_count = countPieces(board, BLUE);
    int empty_count = countPieces(board, EMPTY);
    
    safePrint("\nRed: %d, Blue: %d, Empty: %d\n", red_count, blue_count, empty_count);
    safePrint("You are: %s (%c)\n", my_color == RED ? "Red" : "Blue", my_color);
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
    
    if (strcmp(type_str, "register_ack") == 0) {
        safePrint("✓ Registration successful!\n");
    }
    else if (strcmp(type_str, "register_nack") == 0) {
        safePrint("✗ Registration failed!\n");
        cleanup();
        exit(1);
    }
    else if (strcmp(type_str, "game_start") == 0) {
        safePrint("\n🎮 GAME STARTED! 🎮\n");
        
        cJSON* players = cJSON_GetObjectItem(message, "players");
        if (players && cJSON_IsArray(players)) {
            cJSON* player1 = cJSON_GetArrayItem(players, 0);
            cJSON* player2 = cJSON_GetArrayItem(players, 1);
            
            if (player1 && player2) {
                if (strcmp(my_username, player1->valuestring) == 0) {
                    my_color = RED;
                    currentPlayer = RED_TURN;
                } else {
                    my_color = BLUE;
                    currentPlayer = BLUE_TURN;
                }
            }
        }
        
        atomic_store(&gameStarted, 1);
        totalMoveCount = 0;
        positionHistoryCount = 0;
        moveHistoryCount = 0;
        
        updateLEDDisplay();
    }
    else if (strcmp(type_str, "your_turn") == 0) {
        safePrint("\n=== YOUR TURN ===\n");
        
        cJSON* board_json = cJSON_GetObjectItem(message, "board");
        if (board_json) {
            updateBoardFromJSON(board_json);
            printBoard();
        }
        
        atomic_store(&myTurn, 1);
        
        Move bestMove = generate_move();
        
        sendMove(bestMove.r1, bestMove.c1, bestMove.r2, bestMove.c2);
        
        atomic_store(&myTurn, 0);
    }
    else if (strcmp(type_str, "move_ok") == 0 || strcmp(type_str, "invalid_move") == 0) {
        cJSON* board_json = cJSON_GetObjectItem(message, "board");
        if (board_json) {
            updateBoardFromJSON(board_json);
            safePrint("Board updated after move feedback\n");
        }
    }
    else if (strcmp(type_str, "game_over") == 0) {
        safePrint("\n🏁 GAME OVER! 🏁\n");
        
        cJSON* scores = cJSON_GetObjectItem(message, "scores");
        if (scores && cJSON_IsObject(scores)) {
            cJSON* score = NULL;
            int my_score = 0, opponent_score = 0;
            
            cJSON_ArrayForEach(score, scores) {
                if (cJSON_IsNumber(score)) {
                    if (strcmp(score->string, my_username) == 0) {
                        my_score = (int)score->valuedouble;
                    } else {
                        opponent_score = (int)score->valuedouble;
                    }
                }
            }
            
            safePrint("\nFinal Scores:\n");
            safePrint("  You: %d\n", my_score);
            safePrint("  Opponent: %d\n", opponent_score);
            
            if (my_score > opponent_score) {
                safePrint("\n🎉 VICTORY! 🎉\n");
            } else if (opponent_score > my_score) {
                safePrint("\n😔 DEFEAT 😔\n");
            } else {
                safePrint("\n🤝 DRAW! 🤝\n");
            }
            
            if (led_initialized) {
                int red_count = countPieces(board, RED);
                int blue_count = countPieces(board, BLUE);
                show_game_over_animation(red_count, blue_count);
            }
        }
        
        atomic_store(&gameOver, 1);
        printBoard();
    }
    else if (strcmp(type_str, "pass") == 0) {
        safePrint("Turn passed\n");
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
            safePrint("\n💔 Server disconnected.\n");
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

void initializeAISystem() {
    safePrint("Initializing AI System (RPi optimized)...\n");
    
    srand(time(NULL));
    
    initZobrist();
    
    // Allocate transposition table (smaller for RPi)
    size_t ttSize = HASH_SIZE * sizeof(TTEntry);
    transpositionTable = (TTEntry*)calloc(1, ttSize);
    if (!transpositionTable) {
        safePrint("Failed to allocate transposition table\n");
        exit(1);
    }
    
    // Initialize board
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            board[i][j] = EMPTY;
        }
    }
    board[0][0] = RED;
    board[0][7] = BLUE;
    board[7][0] = BLUE;
    board[7][7] = RED;
    
    // Clear history tables
    memset(historyTable, 0, sizeof(historyTable));
    memset(killerMoves, 0, sizeof(killerMoves));
    memset(positionHistory, 0, sizeof(positionHistory));
    memset(moveHistory, 0, sizeof(moveHistory));
    
    safePrint("AI System initialized successfully!\n");
}

void cleanupAISystem() {
    if (transpositionTable) {
        free(transpositionTable);
        transpositionTable = NULL;
    }
}

void cleanup() {
    if (sockfd >= 0) {
        close(sockfd);
        sockfd = -1;
    }
    cleanupLEDDisplay();
    cleanupAISystem();
}

void sigint_handler(int sig) {
    (void)sig;
    safePrint("\n👋 Exiting...\n");
    atomic_store(&gameOver, 1);
    cleanup();
    exit(0);
}

int main(int argc, char *argv[]) {
    struct addrinfo hints, *res;
    int status;
    char server_addr[256];
    char server_port[32];
    
    strcpy(server_addr, DEFAULT_SERVER_IP);
    strcpy(server_port, DEFAULT_SERVER_PORT);
    memset(my_username, 0, sizeof(my_username));
    
    // Parse arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-ip") == 0 && i + 1 < argc) {
            strcpy(server_addr, argv[++i]);
        } else if (strcmp(argv[i], "-port") == 0 && i + 1 < argc) {
            strcpy(server_port, argv[++i]);
        } else if (strcmp(argv[i], "-username") == 0 && i + 1 < argc) {
            strcpy(my_username, argv[++i]);
        } else if (strcmp(argv[i], "-engine") == 0 && i + 1 < argc) {
            int engine = atoi(argv[++i]);
            if (engine >= 1 && engine <= 6) {
                aiEngine = (AIEngineType)engine;
            }
        } else if (strcmp(argv[i], "-no-board") == 0) {
            boardServerEnabled = 0;
        }
    }
    
    if (strlen(my_username) == 0) {
        sprintf(my_username, "TeamShannon");
    }
    
    printf("\n=== OctaFlip Client v3.0 (Optimized MCTS) ===\n");
    printf("Username: %s\n", my_username);
    printf("Server: %s:%s\n", server_addr, server_port);
    
    const char* engineNames[] = {
        "Unknown",
        "Eunsong Engine",
        "MCTS with Pure NNUE",
        "MCTS with Classic Heuristics",
        "Minimax with Hybrid Evaluation",
        "Minimax Classic",
        "Tournament Beast (Phase-Adaptive Hybrid)"
    };
    
    printf("AI Engine: %s\n", engineNames[(int)aiEngine]);
    printf("LED Display: %s\n", boardServerEnabled ? "Enabled" : "Disabled");
    printf("Time limit: %.1f seconds\n", TIME_LIMIT);
    
    #ifdef HAS_NNUE_WEIGHTS
    printf("NNUE: Enabled (Deep 4-layer network)\n");
    #else
    printf("NNUE: Disabled\n");
    #endif
    
    #ifdef HAS_OPENING_BOOK
    printf("Opening Book: Enabled\n");
    #else
    printf("Opening Book: Disabled\n");
    #endif
    
    initializeAISystem();
    initLEDDisplay();
    
    signal(SIGINT, sigint_handler);
    
    // Connect to server
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
    
    printf("\nGame session ended.\n");
    printf("Thank you for playing OctaFlip!\n");
    freeaddrinfo(res);
    cleanup();
    
    return 0;
}