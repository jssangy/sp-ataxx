#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include "led-matrix-c.h"

#define BOARD_SIZE 8
#define CELL_SIZE 8
#define PIECE_SIZE 6
#define MATRIX_SIZE 64

// 색상 정의
#define COLOR_RED_R 255
#define COLOR_RED_G 50
#define COLOR_RED_B 50

#define COLOR_BLUE_R 50
#define COLOR_BLUE_G 50
#define COLOR_BLUE_B 255

#define COLOR_EMPTY_R 0
#define COLOR_EMPTY_G 0
#define COLOR_EMPTY_B 0

#define COLOR_BLOCKED_R 128
#define COLOR_BLOCKED_G 128
#define COLOR_BLOCKED_B 128

#define COLOR_GRID_R 100
#define COLOR_GRID_G 100
#define COLOR_GRID_B 100

// 게임 보드
char board[BOARD_SIZE][BOARD_SIZE];

// LED Matrix 관련 전역 변수
struct RGBLedMatrix *matrix = NULL;
struct LedCanvas *canvas = NULL;

// 시그널 핸들러
void signal_handler(int sig) {
    if (matrix) {
        led_matrix_delete(matrix);
    }
    exit(0);
}

// LED 매트릭스 초기화
int initLEDMatrix() {
    struct RGBLedMatrixOptions options;
    struct RGBLedRuntimeOptions rt_options;
    
    memset(&options, 0, sizeof(options));
    memset(&rt_options, 0, sizeof(rt_options));
    
    // 64x64 매트릭스 설정
    options.rows = 64;
    options.cols = 64;
    options.chain_length = 1;
    options.parallel = 1;
    options.hardware_mapping = "regular";
    options.brightness = 50;
    options.pwm_bits = 11;
    options.scan_mode = 0;
    
    // hardware pulse 비활성화 (오류 해결)
    options.disable_hardware_pulsing = 1;
    
    // Runtime 옵션
    rt_options.gpio_slowdown = 2;
    rt_options.drop_privileges = 1;
    rt_options.do_gpio_init = 1;
    
    matrix = led_matrix_create_from_options_and_rt_options(&options, &rt_options);
    
    if (matrix == NULL) {
        return -1;
    }
    
    canvas = led_matrix_get_canvas(matrix);
    return 0;
}

// 그리드 라인 그리기 (Assignment 3 명세에 따라)
void drawGridLines() {
    // Assignment 3: "Draw 1-pixel-wide horizontal and vertical lines to split 
    // the 64×64 panel into an 8×8 grid (lines at pixels 0, 8, 16, …, 64)"
    
    // 가로선 그리기 (y = 0, 8, 16, 24, 32, 40, 48, 56, 64)
    for (int i = 0; i <= BOARD_SIZE; i++) {
        int y = i * CELL_SIZE;
        if (y <= MATRIX_SIZE) {  // 64번째 라인은 매트릭스 범위를 벗어남
            for (int x = 0; x <= MATRIX_SIZE; x++) {
                led_canvas_set_pixel(canvas, x, y, COLOR_GRID_R, COLOR_GRID_G, COLOR_GRID_B);
            }
        }
    }
    
    // 세로선 그리기 (x = 0, 8, 16, 24, 32, 40, 48, 56, 64)
    for (int i = 0; i <= BOARD_SIZE; i++) {
        int x = i * CELL_SIZE;
        if (x <= MATRIX_SIZE) {  // 64번째 라인은 매트릭스 범위를 벗어남
            for (int y = 0; y <= MATRIX_SIZE; y++) {
                led_canvas_set_pixel(canvas, x, y, COLOR_GRID_R, COLOR_GRID_G, COLOR_GRID_B);
            }
        }
    }
}

// 피스 그리기 (6x6 픽셀, 셀 중앙에 위치)
void drawPiece(int row, int col, char piece) {
    // 셀의 시작 위치 (그리드 라인 고려)
    int cell_start_x = col * CELL_SIZE;
    int cell_start_y = row * CELL_SIZE;
    
    // 6x6 피스를 8x8 셀의 중앙에 배치
    // "Reserve the outermost pixel on each side of the block for the grid lines,
    // leaving a 6×6-pixel area at the center for the piece"
    int piece_start_x = cell_start_x + 1;  // 왼쪽 그리드 라인 다음
    int piece_start_y = cell_start_y + 1;  // 위쪽 그리드 라인 다음
    
    // 색상 결정
    int r = 0, g = 0, b = 0;
    int should_draw = 0;
    
    switch(piece) {
        case 'R':
            r = COLOR_RED_R;
            g = COLOR_RED_G;
            b = COLOR_RED_B;
            should_draw = 1;
            break;
        case 'B':
            r = COLOR_BLUE_R;
            g = COLOR_BLUE_G;
            b = COLOR_BLUE_B;
            should_draw = 1;
            break;
        case '.':
            // 빈 칸은 그리지 않음 (검은색 배경)
            should_draw = 0;
            break;
        case '#':
            r = COLOR_BLOCKED_R;
            g = COLOR_BLOCKED_G;
            b = COLOR_BLOCKED_B;
            should_draw = 1;
            break;
    }
    
    if (should_draw) {
        // 6x6 피스 그리기
        for (int py = 0; py < PIECE_SIZE; py++) {
            for (int px = 0; px < PIECE_SIZE; px++) {
                int x = piece_start_x + px;
                int y = piece_start_y + py;
                
                // 경계 체크
                if (x < MATRIX_SIZE && y < MATRIX_SIZE) {
                    led_canvas_set_pixel(canvas, x, y, r, g, b);
                }
            }
        }
    }
}

// LED 매트릭스에 보드 표시
void displayBoardOnMatrix() {
    if (!canvas) return;
    
    // 배경을 검은색으로 클리어
    led_canvas_clear(canvas);
    
    // 각 피스 그리기 (그리드 라인 그리기 전에)
    for (int row = 0; row < BOARD_SIZE; row++) {
        for (int col = 0; col < BOARD_SIZE; col++) {
            drawPiece(row, col, board[row][col]);
        }
    }
    
    // 그리드 라인을 마지막에 그려서 피스 위에 표시
    drawGridLines();
}

// 보드 입력 읽기
int readBoardInput() {
    printf("Please enter the 8x8 board state:\n");
    printf("Enter 8 lines, each with 8 characters (R, B, ., or #):\n\n");
    
    for (int i = 0; i < BOARD_SIZE; i++) {
        char line[BOARD_SIZE + 10];  // 여유 공간
        
        printf("Row %d: ", i + 1);
        fflush(stdout);
        
        if (fgets(line, sizeof(line), stdin) == NULL) {
            fprintf(stderr, "Error reading input\n");
            return -1;
        }
        
        // 개행 문자 제거
        line[strcspn(line, "\n")] = '\0';
        line[strcspn(line, "\r")] = '\0';
        
        // 길이 확인
        if (strlen(line) != BOARD_SIZE) {
            fprintf(stderr, "Error: Row %d must have exactly %d characters\n", i + 1, BOARD_SIZE);
            return -1;
        }
        
        // 유효한 문자인지 확인하고 보드에 저장
        for (int j = 0; j < BOARD_SIZE; j++) {
            char c = line[j];
            if (c != 'R' && c != 'B' && c != '.' && c != '#') {
                fprintf(stderr, "Error: Invalid character '%c' at position (%d,%d)\n", c, i + 1, j + 1);
                return -1;
            }
            board[i][j] = c;
        }
    }
    
    return 0;
}

// 보드 상태 출력 (디버깅용)
void printBoard() {
    printf("\nCurrent board state:\n");
    printf("  ");
    for (int j = 0; j < BOARD_SIZE; j++) {
        printf("%d ", j + 1);
    }
    printf("\n");
    
    for (int i = 0; i < BOARD_SIZE; i++) {
        printf("%d ", i + 1);
        for (int j = 0; j < BOARD_SIZE; j++) {
            printf("%c ", board[i][j]);
        }
        printf("\n");
    }
    printf("\n");
}

// 메인 함수
int main(int argc, char *argv[]) {
    // 시그널 핸들러 설정
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    printf("=== OctaFlip LED Matrix Display ===\n");
    printf("Assignment 3 - Team Shannon\n\n");
    
    // 보드 입력 읽기
    if (readBoardInput() != 0) {
        fprintf(stderr, "Failed to read board input\n");
        return 1;
    }
    
    // 입력된 보드 확인
    printBoard();
    
    // LED 매트릭스 초기화
    printf("Initializing LED matrix...\n");
    if (initLEDMatrix() != 0) {
        fprintf(stderr, "Failed to initialize LED matrix\n");
        fprintf(stderr, "Make sure you're running with sudo and have the RGB LED Matrix connected\n");
        return 1;
    }
    
    printf("LED matrix initialized successfully\n");
    
    // 보드를 LED 매트릭스에 표시
    displayBoardOnMatrix();
    printf("Board displayed on LED matrix\n");
    
    // 계속 표시하기 위해 대기
    printf("\nPress Ctrl+C to exit...\n");
    
    // 무한 루프 (Ctrl+C로 종료)
    while (1) {
        sleep(1);
    }
    
    // 정리 (실제로는 시그널 핸들러에서 처리됨)
    if (matrix) {
        led_matrix_delete(matrix);
    }
    
    return 0;
}