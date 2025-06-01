#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include "led-matrix-c.h"
#include "cJSON.h"

#define BOARD_SIZE 8
#define CELL_SIZE 8
#define PIECE_SIZE 6
#define MATRIX_SIZE 64
#define BOARD_PORT 8081
#define BUFFER_SIZE 4096

// Color definitions (same as before)
#define COLOR_RED_R 255
#define COLOR_RED_G 0
#define COLOR_RED_B 0

#define COLOR_BLUE_R 0
#define COLOR_BLUE_G 0
#define COLOR_BLUE_B 255

#define COLOR_EMPTY_R 30
#define COLOR_EMPTY_G 30
#define COLOR_EMPTY_B 30

#define COLOR_BLOCKED_R 100
#define COLOR_BLOCKED_G 100
#define COLOR_BLOCKED_B 100

#define COLOR_GRID_R 50
#define COLOR_GRID_G 50
#define COLOR_GRID_B 50

#define COLOR_BG_R 0
#define COLOR_BG_G 0
#define COLOR_BG_B 0

#define COLOR_TEAM1_R 255
#define COLOR_TEAM1_G 100
#define COLOR_TEAM1_B 0

#define COLOR_TEAM2_R 255
#define COLOR_TEAM2_G 200
#define COLOR_TEAM2_B 0

// Global variables
char board[BOARD_SIZE][BOARD_SIZE];
struct RGBLedMatrix *matrix = NULL;
struct LedCanvas *canvas = NULL;
struct LedFont *font = NULL;
pthread_mutex_t board_mutex = PTHREAD_MUTEX_INITIALIZER;
int server_running = 1;
int display_mode = 0; // 0 = network mode, 1 = manual input mode

// Function prototypes
void displayTeamName();
void showTeamNameOnMatrix();
int initLEDMatrix();
void drawGridLines();
void drawPiece(int row, int col, char piece);
void displayBoardOnMatrix();
void printBoard();
void updateBoardFromJSON(cJSON* board_array);
void* networkServerThread(void* arg);
void handleBoardUpdate(const char* json_str);
void signalHandler(int sig);

// Display team name in console (same as before)
void displayTeamName() {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║                                                               ║\n");
    printf("║  ████████╗███████╗ █████╗ ███╗   ███╗                        ║\n");
    printf("║  ╚══██╔══╝██╔════╝██╔══██╗████╗ ████║                        ║\n");
    printf("║     ██║   █████╗  ███████║██╔████╔██║                        ║\n");
    printf("║     ██║   ██╔══╝  ██╔══██║██║╚██╔╝██║                        ║\n");
    printf("║     ██║   ███████╗██║  ██║██║ ╚═╝ ██║                        ║\n");
    printf("║     ╚═╝   ╚══════╝╚═╝  ╚═╝╚═╝     ╚═╝                        ║\n");
    printf("║                                                               ║\n");
    printf("║  ███████╗██╗  ██╗ █████╗ ███╗   ██╗███╗   ██╗ ██████╗ ███╗   ██╗║\n");
    printf("║  ██╔════╝██║  ██║██╔══██╗████╗  ██║████╗  ██║██╔═══██╗████╗  ██║║\n");
    printf("║  ███████╗███████║███████║██╔██╗ ██║██╔██╗ ██║██║   ██║██╔██╗ ██║║\n");
    printf("║  ╚════██║██╔══██║██╔══██║██║╚██╗██║██║╚██╗██║██║   ██║██║╚██╗██║║\n");
    printf("║  ███████║██║  ██║██║  ██║██║ ╚████║██║ ╚████║╚██████╔╝██║ ╚████║║\n");
    printf("║  ╚══════╝╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═══╝╚═╝  ╚═══╝ ╚═════╝ ╚═╝  ╚═══╝║\n");
    printf("║                                                               ║\n");
    printf("║          🎮 OctaFlip LED Display Server v2.0 🎮               ║\n");
    printf("║                                                               ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n");
    printf("\n");
}

// Initialize LED Matrix (same as before)
int initLEDMatrix() {
    struct RGBLedMatrixOptions options;
    struct RGBLedRuntimeOptions rt_options;
    
    memset(&options, 0, sizeof(options));
    memset(&rt_options, 0, sizeof(rt_options));
    
    options.rows = 64;
    options.cols = 64;
    options.chain_length = 1;
    options.parallel = 1;
    options.hardware_mapping = "regular";
    options.brightness = 80;
    options.disable_hardware_pulsing = 1;
    
    rt_options.gpio_slowdown = 4;
    rt_options.drop_privileges = 1;
    
    matrix = led_matrix_create_from_options_and_rt_options(&options, &rt_options);
    
    if (matrix == NULL) {
        fprintf(stderr, "Error: Failed to create LED matrix\n");
        return -1;
    }
    
    canvas = led_matrix_get_canvas(matrix);
    return 0;
}

// All display functions remain the same
void showTeamNameOnMatrix() {
    if (!canvas) return;
    
    for (int blink = 0; blink < 3; blink++) {
        led_canvas_clear(canvas);
        
        const char* text = "TEAM SHANNON";
        int text_len = strlen(text);
        
        for (int i = 0; i < text_len; i++) {
            int x_offset = 3 + i * 5;
            int y_offset = 28;
            
            int r = (i < 4) ? 255 : 255 - (i - 4) * 20;
            int g = (i < 4) ? 100 : 200;
            int b = 0;
            
            if (text[i] != ' ') {
                for (int px = 0; px < 4; px++) {
                    for (int py = 0; py < 7; py++) {
                        led_canvas_set_pixel(canvas, x_offset + px, y_offset + py, r, g, b);
                    }
                }
            }
        }
        
        usleep(500000);
        led_canvas_clear(canvas);
        usleep(200000);
    }
}

void drawGridLines() {
    for (int i = 0; i <= BOARD_SIZE; i++) {
        int y = i * CELL_SIZE;
        for (int x = 0; x < MATRIX_SIZE; x++) {
            led_canvas_set_pixel(canvas, x, y, COLOR_GRID_R, COLOR_GRID_G, COLOR_GRID_B);
        }
    }
    
    for (int i = 0; i <= BOARD_SIZE; i++) {
        int x = i * CELL_SIZE;
        for (int y = 0; y < MATRIX_SIZE; y++) {
            led_canvas_set_pixel(canvas, x, y, COLOR_GRID_R, COLOR_GRID_G, COLOR_GRID_B);
        }
    }
}

void drawPiece(int row, int col, char piece) {
    int start_x = col * CELL_SIZE + 1;
    int start_y = row * CELL_SIZE + 1;
    
    int r, g, b;
    switch(piece) {
        case 'R':
            r = COLOR_RED_R;
            g = COLOR_RED_G;
            b = COLOR_RED_B;
            break;
        case 'B':
            r = COLOR_BLUE_R;
            g = COLOR_BLUE_G;
            b = COLOR_BLUE_B;
            break;
        case '.':
            r = COLOR_EMPTY_R;
            g = COLOR_EMPTY_G;
            b = COLOR_EMPTY_B;
            break;
        case '#':
            r = COLOR_BLOCKED_R;
            g = COLOR_BLOCKED_G;
            b = COLOR_BLOCKED_B;
            break;
        default:
            r = g = b = 0;
    }
    
    for (int py = 0; py < PIECE_SIZE; py++) {
        for (int px = 0; px < PIECE_SIZE; px++) {
            int x = start_x + px;
            int y = start_y + py;
            
            if (piece == 'R') {
                int cx = PIECE_SIZE / 2;
                int cy = PIECE_SIZE / 2;
                int dx = px - cx;
                int dy = py - cy;
                if (dx*dx + dy*dy <= (PIECE_SIZE/2) * (PIECE_SIZE/2)) {
                    led_canvas_set_pixel(canvas, x, y, r, g, b);
                }
            } else if (piece == 'B') {
                int cx = PIECE_SIZE / 2;
                int cy = PIECE_SIZE / 2;
                if (abs(px - cx) + abs(py - cy) <= PIECE_SIZE/2) {
                    led_canvas_set_pixel(canvas, x, y, r, g, b);
                }
            } else if (piece == '.') {
                if (px == PIECE_SIZE/2 && py == PIECE_SIZE/2) {
                    led_canvas_set_pixel(canvas, x, y, r, g, b);
                }
            } else if (piece == '#') {
                if (px == py || px == PIECE_SIZE - 1 - py) {
                    led_canvas_set_pixel(canvas, x, y, r, g, b);
                }
            }
        }
    }
}

void displayBoardOnMatrix() {
    if (!canvas) return;
    
    pthread_mutex_lock(&board_mutex);
    
    led_canvas_clear(canvas);
    drawGridLines();
    
    for (int row = 0; row < BOARD_SIZE; row++) {
        for (int col = 0; col < BOARD_SIZE; col++) {
            drawPiece(row, col, board[row][col]);
        }
    }
    
    pthread_mutex_unlock(&board_mutex);
}

void printBoard() {
    printf("\nCurrent Board State:\n");
    printf("  1 2 3 4 5 6 7 8\n");
    for (int i = 0; i < BOARD_SIZE; i++) {
        printf("%d ", i + 1);
        for (int j = 0; j < BOARD_SIZE; j++) {
            printf("%c ", board[i][j]);
        }
        printf("\n");
    }
    printf("\n");
}

// Update board from JSON
void updateBoardFromJSON(cJSON* board_array) {
    if (!board_array || !cJSON_IsArray(board_array)) return;
    
    pthread_mutex_lock(&board_mutex);
    
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
    
    pthread_mutex_unlock(&board_mutex);
    
    printf("Board updated from network\n");
    printBoard();
    displayBoardOnMatrix();
}

// Handle board update message
void handleBoardUpdate(const char* json_str) {
    cJSON* json = cJSON_Parse(json_str);
    if (!json) {
        printf("Failed to parse JSON: %s\n", json_str);
        return;
    }
    
    cJSON* type = cJSON_GetObjectItem(json, "type");
    if (type && cJSON_IsString(type) && strcmp(type->valuestring, "board_update") == 0) {
        cJSON* board_array = cJSON_GetObjectItem(json, "board");
        if (board_array) {
            updateBoardFromJSON(board_array);
        }
    }
    
    cJSON_Delete(json);
}

// Network server thread
void* networkServerThread(void* arg) {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];
    char recv_buffer[BUFFER_SIZE * 2];
    int recv_len = 0;
    
    // Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return NULL;
    }
    
    // Allow reuse
    int reuse = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    
    // Bind
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(BOARD_PORT);
    
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_fd);
        return NULL;
    }
    
    // Listen
    if (listen(server_fd, 5) < 0) {
        perror("listen");
        close(server_fd);
        return NULL;
    }
    
    printf("Board server listening on port %d...\n", BOARD_PORT);
    
    fd_set master_fds, read_fds;
    int maxfd = server_fd;
    
    FD_ZERO(&master_fds);
    FD_SET(server_fd, &master_fds);
    
    struct timeval timeout;
    
    while (server_running) {
        read_fds = master_fds;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int status = select(maxfd + 1, &read_fds, NULL, NULL, &timeout);
        
        if (status < 0) {
            perror("select");
            continue;
        }
        
        for (int i = 0; i <= maxfd; i++) {
            if (FD_ISSET(i, &read_fds)) {
                if (i == server_fd) {
                    // New connection
                    client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
                    if (client_fd < 0) {
                        perror("accept");
                        continue;
                    }
                    
                    printf("Client connected from %s:%d\n", 
                           inet_ntoa(client_addr.sin_addr), 
                           ntohs(client_addr.sin_port));
                    
                    FD_SET(client_fd, &master_fds);
                    if (client_fd > maxfd) maxfd = client_fd;
                    
                } else {
                    // Data from client
                    memset(buffer, 0, BUFFER_SIZE);
                    int bytes = recv(i, buffer, BUFFER_SIZE - 1, 0);
                    
                    if (bytes <= 0) {
                        printf("Client disconnected\n");
                        close(i);
                        FD_CLR(i, &master_fds);
                    } else {
                        // Process messages
                        if (recv_len + bytes < BUFFER_SIZE * 2) {
                            memcpy(recv_buffer + recv_len, buffer, bytes);
                            recv_len += bytes;
                            recv_buffer[recv_len] = '\0';
                            
                            char* start = recv_buffer;
                            char* newline = NULL;
                            
                            while ((newline = strchr(start, '\n')) != NULL) {
                                *newline = '\0';
                                handleBoardUpdate(start);
                                start = newline + 1;
                            }
                            
                            int remaining = recv_len - (start - recv_buffer);
                            if (remaining > 0) {
                                memmove(recv_buffer, start, remaining);
                            }
                            recv_len = remaining;
                        }
                    }
                }
            }
        }
    }
    
    close(server_fd);
    return NULL;
}

// Signal handler
void signalHandler(int sig) {
    printf("\nShutting down board server...\n");
    server_running = 0;
}

// Manual input mode
void manualInputMode() {
    printf("\n=== Manual Input Mode ===\n");
    printf("Please enter the 8x8 board state.\n");
    printf("Available characters:\n");
    printf("  R : Red player's piece\n");
    printf("  B : Blue player's piece\n");
    printf("  . : Empty cell\n");
    printf("  # : Blocked cell\n");
    printf("\nEnter 8 lines, each with 8 characters:\n");
    
    char line[BOARD_SIZE + 10];
    
    for (int i = 0; i < BOARD_SIZE; i++) {
        int valid_input = 0;
        
        while (!valid_input) {
            printf("Row %d: ", i + 1);
            
            if (fgets(line, sizeof(line), stdin) == NULL) {
                printf("Error reading input. Please try again.\n");
                continue;
            }
            
            int len = strlen(line);
            if (len > 0 && line[len - 1] == '\n') {
                line[len - 1] = '\0';
                len--;
            }
            
            if (len > 0 && line[len - 1] == '\r') {
                line[len - 1] = '\0';
                len--;
            }
            
            if (len != BOARD_SIZE) {
                printf("Invalid line length. Each line must have exactly %d characters.\n", BOARD_SIZE);
                continue;
            }
            
            int has_invalid_char = 0;
            for (int j = 0; j < BOARD_SIZE; j++) {
                if (line[j] != 'R' && line[j] != 'B' && line[j] != '.' && line[j] != '#') {
                    has_invalid_char = 1;
                    break;
                }
            }
            
            if (has_invalid_char) {
                printf("Invalid characters. Use only R, B, ., or #.\n");
                continue;
            }
            
            valid_input = 1;
        }
        
        pthread_mutex_lock(&board_mutex);
        for (int j = 0; j < BOARD_SIZE; j++) {
            board[i][j] = line[j];
        }
        pthread_mutex_unlock(&board_mutex);
    }
    
    printf("\nInput Board:\n");
    printBoard();
    
    if (canvas) {
        printf("Displaying board on LED Matrix...\n");
        displayBoardOnMatrix();
    }
}

// Main function
int main(int argc, char *argv[]) {
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-manual") == 0) {
            display_mode = 1;
        }
    }
    
    // Display team name
    displayTeamName();
    
    // Initialize LED Matrix
    printf("Initializing LED Matrix...\n");
    if (initLEDMatrix() != 0) {
        printf("Warning: Unable to initialize LED matrix. Running in console mode.\n");
    } else {
        printf("LED Matrix initialized successfully!\n");
        showTeamNameOnMatrix();
    }
    
    // Initialize board
    pthread_mutex_lock(&board_mutex);
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            board[i][j] = EMPTY;
        }
    }
    board[0][0] = RED;
    board[0][7] = BLUE;
    board[7][0] = BLUE;
    board[7][7] = RED;
    pthread_mutex_unlock(&board_mutex);
    
    // Set up signal handler
    signal(SIGINT, signalHandler);
    
    if (display_mode == 0) {
        // Network mode
        printf("\n=== Network Mode ===\n");
        printf("Waiting for board updates from OctaFlip client...\n");
        printf("Make sure the client is configured to send updates to port %d\n", BOARD_PORT);
        
        pthread_t server_thread;
        if (pthread_create(&server_thread, NULL, networkServerThread, NULL) != 0) {
            perror("pthread_create");
            if (matrix) led_matrix_delete(matrix);
            return 1;
        }
        
        // Display initial board
        displayBoardOnMatrix();
        
        // Wait for server thread
        pthread_join(server_thread, NULL);
        
    } else {
        // Manual input mode
        manualInputMode();
        
        printf("Press Enter to exit...\n");
        getchar();
    }
    
    // Cleanup
    if (font) {
        delete_font(font);
    }
    if (matrix) {
        led_matrix_delete(matrix);
    }
    
    printf("\nProgram terminated.\n");
    printf("Team Shannon - OctaFlip LED Display Server\n");
    
    return 0;
}