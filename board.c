#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

// rpi-rgb-led-matrix 라이브러리 헤더
#include "led-matrix-c.h"

// 보드 및 디스플레이 상수
#define BOARD_SIZE 8
#define MATRIX_SIZE 64
#define CELL_SIZE 8
#define PIECE_SIZE 6
#define PIECE_OFFSET 1  // (8-6)/2 = 1

// 게임 피스 정의
#define RED 'R'
#define BLUE 'B'
#define EMPTY '.'
#define BLOCKED '#'

// 색상 정의 (RGB)
typedef struct {
    unsigned char r, g, b;
} Color;

const Color COLOR_RED = {255, 0, 0};
const Color COLOR_BLUE = {0, 0, 255};
const Color COLOR_EMPTY = {0, 0, 0};
const Color COLOR_BLOCKED = {64, 64, 64};
const Color COLOR_GRID = {32, 32, 32};
const Color COLOR_RED_DIM = {128, 0, 0};      // 어두운 빨강
const Color COLOR_BLUE_DIM = {0, 0, 128};     // 어두운 파랑

// 전역 변수
static struct RGBLedMatrix *matrix = NULL;
static struct LedCanvas *canvas = NULL;
static volatile int running = 1;
static char board[BOARD_SIZE][BOARD_SIZE];

// 시그널 핸들러
void signal_handler(int sig) {
    (void)sig;
    running = 0;
}

// 보드 초기화
void initializeBoard() {
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            board[i][j] = EMPTY;
        }
    }
    // 초기 위치 설정
    board[0][0] = RED;
    board[0][7] = BLUE;
    board[7][0] = BLUE;
    board[7][7] = RED;
}

// 입력에서 보드 읽기
int readBoardFromInput() {
    printf("Enter the board state (8 lines, 8 characters each):\n");
    printf("Use R for Red, B for Blue, . for Empty, # for Blocked\n");
    
    for (int i = 0; i < BOARD_SIZE; i++) {
        char line[256];
        if (fgets(line, sizeof(line), stdin) == NULL) {
            fprintf(stderr, "Error reading input\n");
            return -1;
        }
        
        // 줄바꿈 문자 제거
        line[strcspn(line, "\n")] = 0;
        
        // 입력 길이 확인
        if (strlen(line) != BOARD_SIZE) {
            fprintf(stderr, "Error: Line %d must have exactly %d characters\n", i+1, BOARD_SIZE);
            return -1;
        }
        
        // 유효한 문자 확인 및 보드에 복사
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (line[j] != RED && line[j] != BLUE && 
                line[j] != EMPTY && line[j] != BLOCKED) {
                fprintf(stderr, "Error: Invalid character '%c' at position (%d,%d)\n", 
                        line[j], i+1, j+1);
                return -1;
            }
            board[i][j] = line[j];
        }
    }
    
    return 0;
}

// 보드 상태 출력 (디버깅용)
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

// 그리드 라인 그리기
void drawGridLines() {
    // 수직선 그리기 (0, 8, 16, 24, 32, 40, 48, 56, 64)
    for (int i = 0; i <= BOARD_SIZE; i++) {
        int x = i * CELL_SIZE;
        for (int y = 0; y < MATRIX_SIZE; y++) {
            led_canvas_set_pixel(canvas, x, y, 
                               COLOR_GRID.r, COLOR_GRID.g, COLOR_GRID.b);
        }
    }
    
    // 수평선 그리기
    for (int i = 0; i <= BOARD_SIZE; i++) {
        int y = i * CELL_SIZE;
        for (int x = 0; x < MATRIX_SIZE; x++) {
            led_canvas_set_pixel(canvas, x, y, 
                               COLOR_GRID.r, COLOR_GRID.g, COLOR_GRID.b);
        }
    }
}

// 원형 피스 그리기 (6x6 영역에)
void drawCirclePiece(int cellX, int cellY, Color color, Color dimColor) {
    int centerX = cellX * CELL_SIZE + CELL_SIZE / 2;
    int centerY = cellY * CELL_SIZE + CELL_SIZE / 2;
    
    // 6x6 영역에 원 그리기
    for (int dy = -PIECE_SIZE/2; dy <= PIECE_SIZE/2; dy++) {
        for (int dx = -PIECE_SIZE/2; dx <= PIECE_SIZE/2; dx++) {
            int x = centerX + dx;
            int y = centerY + dy;
            
            // 경계 확인
            if (x <= cellX * CELL_SIZE || x >= (cellX + 1) * CELL_SIZE ||
                y <= cellY * CELL_SIZE || y >= (cellY + 1) * CELL_SIZE) {
                continue;
            }
            
            // 원 내부인지 확인 (반지름 약 2.5)
            float dist_sq = dx * dx + dy * dy;
            if (dist_sq <= 6.25) {  // 2.5^2
                // 가장자리는 어둡게
                if (dist_sq > 4.0) {
                    led_canvas_set_pixel(canvas, x, y, 
                                       dimColor.r, dimColor.g, dimColor.b);
                } else {
                    led_canvas_set_pixel(canvas, x, y, 
                                       color.r, color.g, color.b);
                }
            }
        }
    }
}

// 사각형 피스 그리기 (대체 디자인)
void drawSquarePiece(int cellX, int cellY, Color color) {
    int startX = cellX * CELL_SIZE + PIECE_OFFSET;
    int startY = cellY * CELL_SIZE + PIECE_OFFSET;
    
    for (int dy = 0; dy < PIECE_SIZE; dy++) {
        for (int dx = 0; dx < PIECE_SIZE; dx++) {
            led_canvas_set_pixel(canvas, startX + dx, startY + dy, 
                               color.r, color.g, color.b);
        }
    }
}

// 다이아몬드 피스 그리기 (또 다른 대체 디자인)
void drawDiamondPiece(int cellX, int cellY, Color color, Color dimColor) {
    int centerX = cellX * CELL_SIZE + CELL_SIZE / 2;
    int centerY = cellY * CELL_SIZE + CELL_SIZE / 2;
    
    // 다이아몬드 모양 그리기
    for (int dy = -PIECE_SIZE/2; dy <= PIECE_SIZE/2; dy++) {
        for (int dx = -PIECE_SIZE/2; dx <= PIECE_SIZE/2; dx++) {
            int x = centerX + dx;
            int y = centerY + dy;
            
            // 경계 확인
            if (x <= cellX * CELL_SIZE || x >= (cellX + 1) * CELL_SIZE ||
                y <= cellY * CELL_SIZE || y >= (cellY + 1) * CELL_SIZE) {
                continue;
            }
            
            // 다이아몬드 내부인지 확인
            if (abs(dx) + abs(dy) <= PIECE_SIZE/2) {
                // 가장자리는 어둡게
                if (abs(dx) + abs(dy) == PIECE_SIZE/2) {
                    led_canvas_set_pixel(canvas, x, y, 
                                       dimColor.r, dimColor.g, dimColor.b);
                } else {
                    led_canvas_set_pixel(canvas, x, y, 
                                       color.r, color.g, color.b);
                }
            }
        }
    }
}

// X 표시 그리기 (블록된 셀용)
void drawBlockedCell(int cellX, int cellY) {
    int startX = cellX * CELL_SIZE + PIECE_OFFSET;
    int startY = cellY * CELL_SIZE + PIECE_OFFSET;
    
    // X 모양 그리기
    for (int i = 0; i < PIECE_SIZE; i++) {
        // 대각선 \
        led_canvas_set_pixel(canvas, startX + i, startY + i, 
                           COLOR_BLOCKED.r, COLOR_BLOCKED.g, COLOR_BLOCKED.b);
        // 대각선 /
        led_canvas_set_pixel(canvas, startX + PIECE_SIZE - 1 - i, startY + i, 
                           COLOR_BLOCKED.r, COLOR_BLOCKED.g, COLOR_BLOCKED.b);
    }
}

// 보드를 LED Matrix에 표시
void displayBoard() {
    // 캔버스 클리어
    led_canvas_clear(canvas);
    
    // 그리드 라인 그리기
    drawGridLines();
    
    // 각 셀에 피스 그리기
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            switch (board[i][j]) {
                case RED:
                    drawCirclePiece(j, i, COLOR_RED, COLOR_RED_DIM);
                    break;
                case BLUE:
                    drawCirclePiece(j, i, COLOR_BLUE, COLOR_BLUE_DIM);
                    break;
                case BLOCKED:
                    drawBlockedCell(j, i);
                    break;
                case EMPTY:
                    // 빈 셀은 그리지 않음
                    break;
            }
        }
    }
    
    // 변경사항을 실제 LED에 적용
    canvas = led_matrix_swap_on_vsync(matrix, canvas);
}

// LED Matrix 초기화
int initializeLEDMatrix() {
    struct RGBLedMatrixOptions options;
    struct RGBLedRuntimeOptions runtime;
    
    memset(&options, 0, sizeof(options));
    memset(&runtime, 0, sizeof(runtime));
    
    // LED Matrix 옵션 설정
    options.rows = 64;
    options.cols = 64;
    options.chain_length = 1;
    options.parallel = 1;
    options.hardware_mapping = "regular";
    options.pwm_bits = 11;
    options.brightness = 100;
    options.scan_mode = 0;
    options.pwm_lsb_nanoseconds = 130;
    options.led_rgb_sequence = "RGB";
    options.pixel_mapper_config = NULL;
    options.panel_type = NULL;
    
    runtime.gpio_slowdown = 1;
    runtime.daemon = 0;
    runtime.drop_privileges = 1;
    
    // Matrix 생성
    matrix = led_matrix_create_from_options(&options, &runtime);
    if (matrix == NULL) {
        fprintf(stderr, "Error: Failed to create LED matrix\n");
        return -1;
    }
    
    // Canvas 생성
    canvas = led_matrix_create_offscreen_canvas(matrix);
    if (canvas == NULL) {
        fprintf(stderr, "Error: Failed to create canvas\n");
        led_matrix_delete(matrix);
        return -1;
    }
    
    return 0;
}

// 메인 함수
int main(int argc, char *argv[]) {
    printf("=== OctaFlip LED Matrix Display ===\n\n");
    
    // 시그널 핸들러 설정
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // LED Matrix 초기화
    if (initializeLEDMatrix() < 0) {
        fprintf(stderr, "Failed to initialize LED Matrix\n");
        return 1;
    }
    
    printf("LED Matrix initialized successfully\n");
    
    // 명령행 인자로 테스트 모드 확인
    if (argc > 1 && strcmp(argv[1], "-test") == 0) {
        printf("Test mode: Using default board\n");
        initializeBoard();
    } else {
        // 사용자로부터 보드 상태 입력받기
        if (readBoardFromInput() < 0) {
            led_matrix_delete(matrix);
            return 1;
        }
    }
    
    // 입력받은 보드 상태 출력
    printBoard();
    
    // LED Matrix에 표시
    printf("Displaying board on LED Matrix...\n");
    displayBoard();
    
    // 사용자가 종료할 때까지 대기
    printf("\nPress Ctrl+C to exit\n");
    
    while (running) {
        usleep(100000);  // 100ms 대기
    }
    
    // 정리
    printf("\nCleaning up...\n");
    led_canvas_clear(canvas);
    led_matrix_delete(matrix);
    
    printf("Goodbye!\n");
    return 0;
}

// 보드 상태 업데이트 함수 (client.c에서 호출 가능)
void updateBoard(char newBoard[BOARD_SIZE][BOARD_SIZE]) {
    memcpy(board, newBoard, sizeof(board));
    displayBoard();
}

// 개별 셀 업데이트 함수
void updateCell(int row, int col, char piece) {
    if (row >= 0 && row < BOARD_SIZE && col >= 0 && col < BOARD_SIZE) {
        board[row][col] = piece;
        displayBoard();
    }
}

// 피스 개수 카운트 함수
int countPieces(char piece) {
    int count = 0;
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board[i][j] == piece) {
                count++;
            }
        }
    }
    return count;
}

// 게임 상태 표시 함수
void displayGameStatus() {
    int redCount = countPieces(RED);
    int blueCount = countPieces(BLUE);
    int emptyCount = countPieces(EMPTY);
    
    printf("Game Status - Red: %d, Blue: %d, Empty: %d\n", 
           redCount, blueCount, emptyCount);
}