#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/select.h>
#include "cJSON.h"

// Board and game constants
#define BOARD_SIZE 8
#define RED 'R'
#define BLUE 'B'
#define EMPTY '.'
#define BLOCKED '#'
#define MAX_PLAYERS 2
#define TIMEOUT_SECONDS 5  
#define BUFFER_SIZE 4096

// Move types
#define CLONE 1
#define JUMP 2

// Player structure
typedef struct {
    char username[256];
    int sockfd;
    char color;
    int connected;
    int is_human;  
} Player;

// Game state
typedef struct {
    char board[BOARD_SIZE][BOARD_SIZE];
    Player players[MAX_PLAYERS];
    int num_players;
    int current_player;
    int game_started;
    int game_over;
    int moves_count;
    int pass_count;
    time_t last_move_time;
} GameState;

// Global game state
GameState game;
FILE* logFile = NULL;

// Function prototypes - FIXED: Added player_type parameter
void initializeBoard();
void initializeLog();
void logBoardState(const char* event);
void logMove(const char* player, int sx, int sy, int tx, int ty, const char* result);
void sendJSON(int sockfd, cJSON* json);
void handleRegister(int clientfd, const char* username, const char* player_type);
void handleMove(int clientfd, const char* username, int sx, int sy, int tx, int ty);
void broadcastGameStart();
void sendYourTurn(int player_idx);
void sendGameOver();
int isValidMove(int sx, int sy, int tx, int ty, int player_idx);
void makeMove(int sx, int sy, int tx, int ty, int player_idx);
void flipAdjacentPieces(int r, int c, char playerPiece);
int getMoveType(int sx, int sy, int tx, int ty);
cJSON* boardToJSON();
void printBoard();
void handleClientDisconnect(int clientfd);
void sendPassMessage(int player_idx);
int countPieces(char piece);
int hasValidMove(int player_idx);
int isGameOver();
void handlePass(int clientfd, const char* username);
void closeLog();

// Initialize log file
void initializeLog() {
    char filename[256];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    
    strftime(filename, sizeof(filename), "octaflip_game_%Y%m%d_%H%M%S.log", tm_info);
    logFile = fopen(filename, "w");
    
    if (logFile) {
        fprintf(logFile, "=== OctaFlip Game Log ===\n");
        fprintf(logFile, "Started at: %s", ctime(&now));
        fprintf(logFile, "========================\n\n");
        fflush(logFile);
        printf("Log file created: %s\n", filename);
    }
}

// Log board state
void logBoardState(const char* event) {
    if (!logFile) return;
    
    time_t now = time(NULL);
    char timeStr[100];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", localtime(&now));
    
    fprintf(logFile, "\n[%s] %s\n", timeStr, event);
    fprintf(logFile, "Board State:\n");
    fprintf(logFile, "  1 2 3 4 5 6 7 8\n");
    
    for (int i = 0; i < BOARD_SIZE; i++) {
        fprintf(logFile, "%d ", i + 1);
        for (int j = 0; j < BOARD_SIZE; j++) {
            fprintf(logFile, "%c ", game.board[i][j]);
        }
        fprintf(logFile, "\n");
    }
    
    fprintf(logFile, "Red: %d, Blue: %d, Empty: %d\n",
            countPieces(RED), countPieces(BLUE), countPieces(EMPTY));
    fprintf(logFile, "Move #%d, Pass count: %d\n", game.moves_count, game.pass_count);
    fprintf(logFile, "------------------------\n");
    fflush(logFile);
}

// Log move
void logMove(const char* player, int sx, int sy, int tx, int ty, const char* result) {
    if (!logFile) return;
    
    if (sx == 0 && sy == 0 && tx == 0 && ty == 0) {
        fprintf(logFile, "Player %s: PASS - %s\n", player, result);
    } else {
        fprintf(logFile, "Player %s: (%d,%d) -> (%d,%d) - %s\n", 
                player, sx, sy, tx, ty, result);
    }
    fflush(logFile);
}

// Close log file
void closeLog() {
    if (logFile) {
        fprintf(logFile, "\n=== Game Session Ended ===\n");
        fclose(logFile);
        logFile = NULL;
    }
}

// Initialize the game board
void initializeBoard() {
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            game.board[i][j] = EMPTY;
        }
    }
    
    // Set initial pieces
    game.board[0][0] = RED;
    game.board[0][7] = BLUE;
    game.board[7][0] = BLUE;
    game.board[7][7] = RED;
}

// Convert board to JSON array
cJSON* boardToJSON() {
    cJSON* board_array = cJSON_CreateArray();
    if (!board_array) return NULL;
    
    for (int i = 0; i < BOARD_SIZE; i++) {
        char row_str[BOARD_SIZE + 1];
        for (int j = 0; j < BOARD_SIZE; j++) {
            row_str[j] = game.board[i][j];
        }
        row_str[BOARD_SIZE] = '\0';
        cJSON* row_item = cJSON_CreateString(row_str);
        if (row_item) {
            cJSON_AddItemToArray(board_array, row_item);
        }
    }
    
    return board_array;
}

// Send JSON message to client
void sendJSON(int sockfd, cJSON* json) {
    if (!json || sockfd < 0) return;
    
    char* json_str = cJSON_PrintUnformatted(json);
    if (json_str) {
        send(sockfd, json_str, strlen(json_str), MSG_NOSIGNAL);
        send(sockfd, "\n", 1, MSG_NOSIGNAL);
        printf("[SERVER->CLIENT %d] %s\n", sockfd, json_str);
        
        if (logFile) {
            fprintf(logFile, "[SERVER->CLIENT %d] %s\n", sockfd, json_str);
            fflush(logFile);
        }
        
        free(json_str);
    }
}

// Handle register request
void handleRegister(int clientfd, const char* username, const char* player_type) {
    printf("Registration request from %s (fd=%d, type=%s)\n", username, clientfd, 
           player_type ? player_type : "ai");
    
    cJSON* response = cJSON_CreateObject();
    if (!response) return;
    
    // Check if game is already running
    if (game.game_started) {
        cJSON_AddStringToObject(response, "type", "register_nack");
        cJSON_AddStringToObject(response, "reason", "game is already running");
        sendJSON(clientfd, response);
        cJSON_Delete(response);
        return;
    }
    
    // Check if username already exists
    for (int i = 0; i < game.num_players; i++) {
        if (strcmp(game.players[i].username, username) == 0) {
            cJSON_AddStringToObject(response, "type", "register_nack");
            cJSON_AddStringToObject(response, "reason", "username already exists");
            sendJSON(clientfd, response);
            cJSON_Delete(response);
            return;
        }
    }
    
    // Add player
    if (game.num_players < MAX_PLAYERS) {
        strncpy(game.players[game.num_players].username, username, sizeof(game.players[game.num_players].username) - 1);
        game.players[game.num_players].sockfd = clientfd;
        game.players[game.num_players].color = (game.num_players == 0) ? RED : BLUE;
        game.players[game.num_players].connected = 1;
        
        // Set human flag
        if (player_type && strcmp(player_type, "human") == 0) {
            game.players[game.num_players].is_human = 1;
        } else {
            game.players[game.num_players].is_human = 0;
        }
        
        game.num_players++;
        
        cJSON_AddStringToObject(response, "type", "register_ack");
        sendJSON(clientfd, response);
        cJSON_Delete(response);
        
        printf("Player %s registered as %c (%s) (total: %d)\n", username, 
               game.players[game.num_players-1].color,
               game.players[game.num_players-1].is_human ? "human" : "ai",
               game.num_players);
        
        if (logFile) {
            fprintf(logFile, "Player %s registered as %c (%s)\n", username, 
                    game.players[game.num_players-1].color,
                    game.players[game.num_players-1].is_human ? "human" : "ai");
            fflush(logFile);
        }
        
        // Start game if two players registered
        if (game.num_players == MAX_PLAYERS) {
            game.game_started = 1;
            game.current_player = 0;
            game.last_move_time = time(NULL);
            logBoardState("Game Started - Initial Board");
            broadcastGameStart();
            usleep(100000); // Small delay
            sendYourTurn(game.current_player);
        }
    }
}


// Broadcast game start to both players
void broadcastGameStart() {
    cJSON* message = cJSON_CreateObject();
    if (!message) return;
    
    cJSON* players_array = cJSON_CreateArray();
    
    if (!players_array) {
        cJSON_Delete(message);
        return;
    }
    
    cJSON_AddStringToObject(message, "type", "game_start");
    
    cJSON_AddItemToArray(players_array, cJSON_CreateString(game.players[0].username));
    cJSON_AddItemToArray(players_array, cJSON_CreateString(game.players[1].username));
    cJSON_AddItemToObject(message, "players", players_array);
    
    cJSON_AddStringToObject(message, "first_player", game.players[0].username);
    
    // Don't include board in game_start (as per TA specification)
    
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (game.players[i].connected) {
            sendJSON(game.players[i].sockfd, message);
        }
    }
    
    cJSON_Delete(message);
}

// Send your_turn message
void sendYourTurn(int player_idx) {
    if (player_idx < 0 || player_idx >= MAX_PLAYERS) return;
    
    cJSON* message = cJSON_CreateObject();
    cJSON* board_json = boardToJSON();
    
    if (!message || !board_json) {
        if (message) cJSON_Delete(message);
        if (board_json) cJSON_Delete(board_json);
        return;
    }
    
    cJSON_AddStringToObject(message, "type", "your_turn");
    cJSON_AddItemToObject(message, "board", board_json);
    
    // Set timeout based on player type
    if (game.players[player_idx].is_human) {
        cJSON_AddNumberToObject(message, "timeout", 999999);  // Effectively no timeout
    } else {
        cJSON_AddNumberToObject(message, "timeout", TIMEOUT_SECONDS);
    }
    
    if (game.players[player_idx].connected) {
        sendJSON(game.players[player_idx].sockfd, message);
        game.last_move_time = time(NULL);
    }
    
    cJSON_Delete(message);
}


// Get move type
int getMoveType(int sx, int sy, int tx, int ty) {
    int dx = abs(tx - sx);
    int dy = abs(ty - sy);
    
    if (dx == 0 && dy == 0) return -1;
    
    if (dx <= 1 && dy <= 1) return CLONE;
    
    if ((dx == 2 && dy == 0) || (dx == 0 && dy == 2) || (dx == 2 && dy == 2)) return JUMP;
    
    return -1;
}

// Check if move is valid
int isValidMove(int sx, int sy, int tx, int ty, int player_idx) {
    if (sx < 0 || sx >= BOARD_SIZE || sy < 0 || sy >= BOARD_SIZE ||
        tx < 0 || tx >= BOARD_SIZE || ty < 0 || ty >= BOARD_SIZE) {
        return 0;
    }
    
    char playerPiece = game.players[player_idx].color;
    if (game.board[sx][sy] != playerPiece) {
        return 0;
    }
    
    if (game.board[tx][ty] != EMPTY) {
        return 0;
    }
    
    return getMoveType(sx, sy, tx, ty) > 0;
}

// Flip adjacent pieces
void flipAdjacentPieces(int r, int c, char playerPiece) {
    char opponentPiece = (playerPiece == RED) ? BLUE : RED;
    
    for (int dr = -1; dr <= 1; dr++) {
        for (int dc = -1; dc <= 1; dc++) {
            if (dr == 0 && dc == 0) continue;
            
            int nr = r + dr;
            int nc = c + dc;
            
            if (nr >= 0 && nr < BOARD_SIZE && nc >= 0 && nc < BOARD_SIZE) {
                if (game.board[nr][nc] == opponentPiece) {
                    game.board[nr][nc] = playerPiece;
                }
            }
        }
    }
}

// Make a move on the board
void makeMove(int sx, int sy, int tx, int ty, int player_idx) {
    char playerPiece = game.players[player_idx].color;
    int moveType = getMoveType(sx, sy, tx, ty);
    
    game.board[tx][ty] = playerPiece;
    
    if (moveType == JUMP) {
        game.board[sx][sy] = EMPTY;
    }
    
    flipAdjacentPieces(tx, ty, playerPiece);
}

// Count pieces
int countPieces(char piece) {
    int count = 0;
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (game.board[i][j] == piece) {
                count++;
            }
        }
    }
    return count;
}

// Check if player has valid moves
int hasValidMove(int player_idx) {
    if (player_idx < 0 || player_idx >= MAX_PLAYERS) return 0;
    
    char playerPiece = game.players[player_idx].color;
    
    for (int sx = 0; sx < BOARD_SIZE; sx++) {
        for (int sy = 0; sy < BOARD_SIZE; sy++) {
            if (game.board[sx][sy] == playerPiece) {
                for (int tx = 0; tx < BOARD_SIZE; tx++) {
                    for (int ty = 0; ty < BOARD_SIZE; ty++) {
                        if (isValidMove(sx, sy, tx, ty, player_idx)) {
                            return 1;
                        }
                    }
                }
            }
        }
    }
    return 0;
}

// Check if game is over
int isGameOver() {
    if (countPieces(EMPTY) == 0) return 1;
    if (countPieces(RED) == 0 || countPieces(BLUE) == 0) return 1;
    if (game.pass_count >= 2) return 1;
    
    return 0;
}

// Handle pass
void handlePass(int clientfd, const char* username) {
    int player_idx = -1;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (game.players[i].sockfd == clientfd) {
            player_idx = i;
            break;
        }
    }
    
    if (player_idx == -1 || player_idx != game.current_player) {
        cJSON* response = cJSON_CreateObject();
        if (response) {
            cJSON_AddStringToObject(response, "type", "invalid_move");
            cJSON_AddStringToObject(response, "reason", "not your turn");
            sendJSON(clientfd, response);
            cJSON_Delete(response);
        }
        return;
    }
    
    cJSON* response = cJSON_CreateObject();
    cJSON* board_json = boardToJSON();
    
    if (!response || !board_json) {
        if (response) cJSON_Delete(response);
        if (board_json) cJSON_Delete(board_json);
        return;
    }
    
    if (hasValidMove(player_idx)) {
        // Has valid moves, cannot pass
        cJSON_AddStringToObject(response, "type", "invalid_move");
        cJSON_AddItemToObject(response, "board", board_json);
        cJSON_AddStringToObject(response, "next_player", game.players[game.current_player].username);
        
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (game.players[i].connected) {
                sendJSON(game.players[i].sockfd, response);
            }
        }
        
        logMove(username, 0, 0, 0, 0, "Invalid pass - has valid moves");
    } else {
        // Valid pass
        game.pass_count++;
        game.moves_count++;
        
        cJSON_AddStringToObject(response, "type", "move_ok");
        cJSON_AddItemToObject(response, "board", board_json);
        cJSON_AddStringToObject(response, "next_player", game.players[1 - game.current_player].username);
        
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (game.players[i].connected) {
                sendJSON(game.players[i].sockfd, response);
            }
        }
        
        logMove(username, 0, 0, 0, 0, "Valid pass");
        logBoardState("After Pass");
        
        game.current_player = 1 - game.current_player;
        game.last_move_time = time(NULL);
        
        if (isGameOver()) {
            sendGameOver();
        } else {
            sendYourTurn(game.current_player);
        }
    }
    
    cJSON_Delete(response);
}

// Handle move request
void handleMove(int clientfd, const char* username, int sx, int sy, int tx, int ty) {
    int player_idx = -1;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (game.players[i].sockfd == clientfd) {
            player_idx = i;
            break;
        }
    }
    
    if (player_idx == -1 || player_idx != game.current_player) {
        cJSON* response = cJSON_CreateObject();
        if (response) {
            cJSON_AddStringToObject(response, "type", "invalid_move");
            cJSON_AddStringToObject(response, "reason", "not your turn");
            sendJSON(clientfd, response);
            cJSON_Delete(response);
        }
        return;
    }
    
    // Check for pass move
    if (sx == 0 && sy == 0 && tx == 0 && ty == 0) {
        handlePass(clientfd, username);
        return;
    }
    
    // Convert from 1-indexed to 0-indexed
    sx--; sy--; tx--; ty--;
    
    cJSON* response = cJSON_CreateObject();
    if (!response) return;
    
    cJSON* board_json = NULL;
    int next_player = 1 - game.current_player;
    
    if (isValidMove(sx, sy, tx, ty, player_idx)) {
        makeMove(sx, sy, tx, ty, player_idx);
        board_json = boardToJSON();
        
        if (board_json) {
            cJSON_AddStringToObject(response, "type", "move_ok");
            cJSON_AddItemToObject(response, "board", board_json);
            cJSON_AddStringToObject(response, "next_player", game.players[next_player].username);
            
            logMove(username, sx+1, sy+1, tx+1, ty+1, "Valid move");
            logBoardState("After Move");
            
            game.pass_count = 0;  // Reset pass counter
            game.current_player = next_player;
            game.moves_count++;
            game.last_move_time = time(NULL);
            
            for (int i = 0; i < MAX_PLAYERS; i++) {
                if (game.players[i].connected) {
                    sendJSON(game.players[i].sockfd, response);
                }
            }
            
            printBoard();
            
            if (isGameOver()) {
                sendGameOver();
            } else {
                sendYourTurn(game.current_player);
            }
        }
    } else {
        board_json = boardToJSON();
        
        if (board_json) {
            cJSON_AddStringToObject(response, "type", "invalid_move");
            cJSON_AddItemToObject(response, "board", board_json);
            cJSON_AddStringToObject(response, "next_player", game.players[game.current_player].username);
            
            logMove(username, sx+1, sy+1, tx+1, ty+1, "Invalid move");
            
            for (int i = 0; i < MAX_PLAYERS; i++) {
                if (game.players[i].connected) {
                    sendJSON(game.players[i].sockfd, response);
                }
            }
        }
    }
    
    cJSON_Delete(response);
}

// Send pass message when timeout occurs
void sendPassMessage(int player_idx) {
    cJSON* message = cJSON_CreateObject();
    if (!message) return;
    
    int next_player = 1 - player_idx;
    
    cJSON_AddStringToObject(message, "type", "pass");
    cJSON_AddStringToObject(message, "next_player", game.players[next_player].username);
    
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (game.players[i].connected) {
            sendJSON(game.players[i].sockfd, message);
        }
    }
    
    cJSON_Delete(message);
}

// Send game over message
void sendGameOver() {
    cJSON* message = cJSON_CreateObject();
    cJSON* scores = cJSON_CreateObject();
    
    if (!message || !scores) {
        if (message) cJSON_Delete(message);
        if (scores) cJSON_Delete(scores);
        return;
    }
    
    int red_count = countPieces(RED);
    int blue_count = countPieces(BLUE);
    
    for (int i = 0; i < MAX_PLAYERS; i++) {
        int player_score = (game.players[i].color == RED) ? red_count : blue_count;
        cJSON_AddNumberToObject(scores, game.players[i].username, player_score);
    }
    
    cJSON_AddStringToObject(message, "type", "game_over");
    cJSON_AddItemToObject(message, "scores", scores);
    
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (game.players[i].connected) {
            sendJSON(game.players[i].sockfd, message);
        }
    }
    
    printf("\n=== GAME OVER ===\n");
    printf("Red: %d, Blue: %d\n", red_count, blue_count);
    printf("Total moves: %d\n", game.moves_count);
    
    logBoardState("Game Over - Final Board");
    if (logFile) {
        fprintf(logFile, "\nFinal Score: Red=%d, Blue=%d\n", red_count, blue_count);
        fprintf(logFile, "Total moves: %d\n", game.moves_count);
    }
    
    cJSON_Delete(message);
    
    game.game_started = 0;
    game.game_over = 1;

    fflush(logFile);  // 버퍼 강제 플러시
    usleep(100000);   // 0.1초 대기

}

// Handle client disconnect
void handleClientDisconnect(int clientfd) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (game.players[i].sockfd == clientfd) {
            game.players[i].connected = 0;
            printf("Player %s disconnected\n", game.players[i].username);
            
            if (logFile) {
                fprintf(logFile, "Player %s disconnected\n", game.players[i].username);
                fflush(logFile);
            }
            
            // Check if both disconnected
            int connected_count = 0;
            for (int j = 0; j < MAX_PLAYERS; j++) {
                if (game.players[j].connected) connected_count++;
            }
            
            if (connected_count == 0 && game.game_started) {
                printf("Both players disconnected. Game terminated.\n");
                sendGameOver();
            } else if (game.game_started && i == game.current_player) {
                // Pass turn to other player
                game.pass_count++;
                game.current_player = 1 - i;
                if (game.players[game.current_player].connected) {
                    sendPassMessage(i);
                    sendYourTurn(game.current_player);
                }
            }
            break;
        }
    }
}

// Print board
void printBoard() {
    printf("\nCurrent Board:\n");
    printf("  1 2 3 4 5 6 7 8\n");
    for (int i = 0; i < BOARD_SIZE; i++) {
        printf("%d ", i + 1);
        for (int j = 0; j < BOARD_SIZE; j++) {
            printf("%c ", game.board[i][j]);
        }
        printf("\n");
    }
    printf("Red: %d, Blue: %d, Empty: %d\n", 
           countPieces(RED), countPieces(BLUE), countPieces(EMPTY));
    printf("\n");
}

int main(int argc, char *argv[]) {
    // Suppress unused parameter warnings
    (void)argc;
    (void)argv;
    
    struct addrinfo hints, *res;
    int sockfd, clientfd, status;
    char buffer[BUFFER_SIZE];
    struct timeval timeout;
    fd_set read_fds, master_fds;
    int maxfd;
    
    // Initialize game state
    memset(&game, 0, sizeof(game));
    initializeBoard();
    initializeLog();
    
    // Set up socket
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    
    status = getaddrinfo(NULL, "8080", &hints, &res);
    if (status != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        exit(1);
    }
    
    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd == -1) {
        perror("socket error");
        exit(1);
    }
    
    int reuse = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof reuse);
    
    status = bind(sockfd, res->ai_addr, res->ai_addrlen);
    if (status == -1) {
        perror("bind error");
        exit(1);
    }
    
    status = listen(sockfd, 5);
    if (status == -1) {
        perror("listen error");
        exit(1);
    }
    
    printf("=== OctaFlip Server ===\n");
    printf("Listening on port 8080...\n");
    printBoard();
    
    FD_ZERO(&master_fds);
    FD_SET(sockfd, &master_fds);
    maxfd = sockfd;
    
    char recv_buffer[BUFFER_SIZE * 2];  // Larger buffer for partial messages
    int recv_len[FD_SETSIZE] = {0};    // Track received data per socket
    
    // Main server loop
    while (1) {
        read_fds = master_fds;
        
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        status = select(maxfd + 1, &read_fds, NULL, NULL, &timeout);
        
        if (status == -1) {
            perror("select error");
            continue;
        }
        
        // Check for timeout
        if (game.game_started && !game.game_over) {
            time_t current_time = time(NULL);
            
            // Only check timeout for AI players
            if (!game.players[game.current_player].is_human && 
                current_time - game.last_move_time >= TIMEOUT_SECONDS) {
                printf("Timeout for player %s\n", game.players[game.current_player].username);
                
                logMove(game.players[game.current_player].username, 0, 0, 0, 0, "Timeout - forced pass");
                
                game.pass_count++;
                sendPassMessage(game.current_player);
                game.current_player = 1 - game.current_player;
                game.last_move_time = current_time;
                
                if (isGameOver()) {
                    sendGameOver();
                } else {
                    sendYourTurn(game.current_player);
                }
            }
        }
        
        // Check all sockets
        for (int i = 0; i <= maxfd; i++) {
            if (FD_ISSET(i, &read_fds)) {
                if (i == sockfd) {
                    // New connection
                    clientfd = accept(sockfd, NULL, NULL);
                    if (clientfd == -1) {
                        perror("accept error");
                        continue;
                    }
                    
                    printf("New connection (fd=%d)\n", clientfd);
                    
                    FD_SET(clientfd, &master_fds);
                    if (clientfd > maxfd) maxfd = clientfd;
                    recv_len[clientfd] = 0;
                    
                } else {
                    // Data from client
                    memset(buffer, 0, BUFFER_SIZE);
                    int bytes_received = recv(i, buffer, BUFFER_SIZE - 1, 0);
                    
                    if (bytes_received <= 0) {
                        close(i);
                        FD_CLR(i, &master_fds);
                        handleClientDisconnect(i);
                        recv_len[i] = 0;
                    } else {
                        // Append to existing buffer
                        if (recv_len[i] + bytes_received < BUFFER_SIZE * 2) {
                            memcpy(recv_buffer + recv_len[i], buffer, bytes_received);
                            recv_len[i] += bytes_received;
                            recv_buffer[recv_len[i]] = '\0';
                            
                            // Process complete messages
                            char* start = recv_buffer;
                            char* newline = NULL;
                            
                            while ((newline = strchr(start, '\n')) != NULL) {
                                *newline = '\0';
                                
                                printf("[CLIENT %d->SERVER] %s\n", i, start);
                                if (logFile) {
                                    fprintf(logFile, "[CLIENT %d->SERVER] %s\n", i, start);
                                    fflush(logFile);
                                }
                                
                                // Parse JSON
                                cJSON* json = cJSON_Parse(start);
                                if (json) {
                                    cJSON* type = cJSON_GetObjectItem(json, "type");
                                    if (type && cJSON_IsString(type)) {
                                        if (strcmp(type->valuestring, "register") == 0) {
                                            cJSON* username = cJSON_GetObjectItem(json, "username");
                                            cJSON* player_type = cJSON_GetObjectItem(json, "player_type");
                                            
                                            if (username && cJSON_IsString(username)) {
                                                const char* type_str = NULL;
                                                if (player_type && cJSON_IsString(player_type)) {
                                                    type_str = player_type->valuestring;
                                                }
                                                handleRegister(i, username->valuestring, type_str);
                                            }
                                        } else if (strcmp(type->valuestring, "move") == 0) {
                                            cJSON* username = cJSON_GetObjectItem(json, "username");
                                            cJSON* sx = cJSON_GetObjectItem(json, "sx");
                                            cJSON* sy = cJSON_GetObjectItem(json, "sy");
                                            cJSON* tx = cJSON_GetObjectItem(json, "tx");
                                            cJSON* ty = cJSON_GetObjectItem(json, "ty");
                                            
                                            if (username && cJSON_IsString(username) &&
                                                sx && cJSON_IsNumber(sx) &&
                                                sy && cJSON_IsNumber(sy) &&
                                                tx && cJSON_IsNumber(tx) &&
                                                ty && cJSON_IsNumber(ty)) {
                                                handleMove(i, username->valuestring,
                                                         (int)sx->valuedouble, (int)sy->valuedouble,
                                                         (int)tx->valuedouble, (int)ty->valuedouble);
                                            }
                                        }
                                    }
                                    cJSON_Delete(json);
                                }
                                
                                start = newline + 1;
                            }
                            
                            // Move remaining data to beginning
                            int remaining = recv_len[i] - (start - recv_buffer);
                            if (remaining > 0) {
                                memmove(recv_buffer, start, remaining);
                            }
                            recv_len[i] = remaining;
                        }
                    }
                }
            }
        }
        
        // Exit if game is over and no players connected
        if (game.game_over) {
            int connected = 0;
            for (int i = 0; i < MAX_PLAYERS; i++) {
                if (game.players[i].connected) connected++;
            }
            if (connected == 0) {
                printf("Game ended and all players disconnected. Shutting down server.\n");
                
                // 로그 파일 안전하게 닫기
                if (logFile) {
                    fprintf(logFile, "\nGame completed successfully\n");
                    fflush(logFile);
                    closeLog();
                }
                usleep(500000);  // 0.5초 대기
                break;
            }
        }
    }
    
    // Clean up
    closeLog();
    freeaddrinfo(res);
    close(sockfd);
    
    return 0;
}
