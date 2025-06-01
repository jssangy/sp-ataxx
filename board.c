#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include "led-matrix-c.h"

#define BOARD_SIZE 8
#define CELL_SIZE 8
#define PIECE_SIZE 6
#define PIECE_OFFSET 1

// 색상 정의
#define RED_R 255
#define RED_G 0
#define RED_B 0

#define BLUE_R 0
#define BLUE_G 0
#define BLUE_B 255

#define EMPTY_R 0
#define EMPTY_G 0
#define EMPTY_B 0

#define BLOCKED_R 64
#define BLOCKED_G 64
#define BLOCKED_B 64

#define GRID_R 128
#define GRID_G 128
#define GRID_B 128

// 전역 변수
struct RGBLedMatrix *matrix = NULL;
struct LedCanvas *canvas = NULL;
volatile sig_atomic_t interrupt_received = 0;

// 시그널 핸들러
static void InterruptHandler(int signo) {
    interrupt_received = 1;
}

// 격자선 그리기
void draw_grid(struct LedCanvas *canvas) {
    // 수평선 그리기 (0, 8, 16, ..., 64)
    for (int y = 0; y <= 64; y += CELL_SIZE) {
        for (int x = 0; x < 64; x++) {
            led_canvas_set_pixel(canvas, x, y, GRID_R, GRID_G, GRID_B);
        }
    }
    
    // 수직선 그리기 (0, 8, 16, ..., 64)
    for (int x = 0; x <= 64; x += CELL_SIZE) {
        for (int y = 0; y < 64; y++) {
            led_canvas_set_pixel(canvas, x, y, GRID_R, GRID_G, GRID_B);
        }
    }
}

// 말 그리기 (6x6 픽셀)
void draw_piece(struct LedCanvas *canvas, int row, int col, char piece) {
    int start_x = col * CELL_SIZE + PIECE_OFFSET;
    int start_y = row * CELL_SIZE + PIECE_OFFSET;
    
    uint8_t r = 0, g = 0, b = 0;
    
    switch (piece) {
        case 'R':
            r = RED_R; g = RED_G; b = RED_B;
            break;
        case 'B':
            r = BLUE_R; g = BLUE_G; b = BLUE_B;
            break;
        case '.':
            r = EMPTY_R; g = EMPTY_G; b = EMPTY_B;
            break;
        case '#':
            r = BLOCKED_R; g = BLOCKED_G; b = BLOCKED_B;
            break;
        default:
            return;
    }
    
    // 6x6 픽셀 영역 채우기
    for (int y = 0; y < PIECE_SIZE; y++) {
        for (int x = 0; x < PIECE_SIZE; x++) {
            led_canvas_set_pixel(canvas, start_x + x, start_y + y, r, g, b);
        }
    }
    
    // 빈 칸인 경우 작은 점을 중앙에 표시 (선택사항)
    if (piece == '.') {
        int center_x = start_x + PIECE_SIZE / 2;
        int center_y = start_y + PIECE_SIZE / 2;
        led_canvas_set_pixel(canvas, center_x, center_y, 32, 32, 32);
    }
}

// 보드 상태 그리기
void draw_board(struct LedCanvas *canvas, char board[BOARD_SIZE][BOARD_SIZE]) {
    // 먼저 캔버스를 검은색으로 지우기
    led_canvas_clear(canvas);
    
    // 격자선 그리기
    draw_grid(canvas);
    
    // 각 셀에 말 그리기
    for (int row = 0; row < BOARD_SIZE; row++) {
        for (int col = 0; col < BOARD_SIZE; col++) {
            draw_piece(canvas, row, col, board[row][col]);
        }
    }
}

// 보드 입력 받기
int read_board_input(char board[BOARD_SIZE][BOARD_SIZE]) {
    printf("Please enter the 8x8 board state:\n");
    printf("Use R for Red, B for Blue, . for Empty, # for Blocked\n");
    printf("Enter 8 lines with 8 characters each:\n\n");
    
    for (int i = 0; i < BOARD_SIZE; i++) {
        char line[256];
        printf("Row %d: ", i + 1);
        
        if (fgets(line, sizeof(line), stdin) == NULL) {
            printf("Error reading input\n");
            return 0;
        }
        
        // 개행 문자 제거
        line[strcspn(line, "\n")] = '\0';
        
        // 입력 검증
        if (strlen(line) != BOARD_SIZE) {
            printf("Error: Row must have exactly %d characters\n", BOARD_SIZE);
            i--; // 다시 입력받기
            continue;
        }
        
        // 유효한 문자인지 확인
        int valid = 1;
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (line[j] != 'R' && line[j] != 'B' && 
                line[j] != '.' && line[j] != '#') {
                printf("Error: Invalid character '%c'. Use only R, B, ., or #\n", line[j]);
                valid = 0;
                break;
            }
        }
        
        if (!valid) {
            i--; // 다시 입력받기
            continue;
        }
        
        // 보드에 복사
        for (int j = 0; j < BOARD_SIZE; j++) {
            board[i][j] = line[j];
        }
    }
    
    return 1;
}

// 보드 출력 (콘솔)
void print_board(char board[BOARD_SIZE][BOARD_SIZE]) {
    printf("\nCurrent Board:\n");
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

int main(int argc, char *argv[]) {
    // LED Matrix 옵션 설정
    struct RGBLedMatrixOptions options;
    struct RGBLedRuntimeOptions rt_options;
    
    memset(&options, 0, sizeof(options));
    memset(&rt_options, 0, sizeof(rt_options));
    
    options.rows = 64;
    options.cols = 64;
    options.chain_length = 1;
    options.parallel = 1;
    options.hardware_mapping = "regular";
    options.brightness = 50;
    
    rt_options.gpio_slowdown = 4;
    rt_options.drop_privileges = 1;
    
    // 시그널 핸들러 설정
    signal(SIGTERM, InterruptHandler);
    signal(SIGINT, InterruptHandler);
    
    // LED Matrix 생성
    matrix = led_matrix_create_from_options_and_rt_options(&options, &rt_options);
    if (matrix == NULL) {
        fprintf(stderr, "Error: Failed to create LED matrix\n");
        fprintf(stderr, "Make sure to run with sudo\n");
        return 1;
    }
    
    canvas = led_matrix_get_canvas(matrix);
    
    printf("=== OctaFlip LED Matrix Display ===\n");
    printf("64x64 LED Panel initialized\n");
    printf("Press Ctrl+C to exit\n\n");
    
    // 보드 상태 저장용 배열
    char board[BOARD_SIZE][BOARD_SIZE];
    
    // 메인 루프
    while (!interrupt_received) {
        // 보드 입력 받기
        if (!read_board_input(board)) {
            break;
        }
        
        // 콘솔에 보드 출력
        print_board(board);
        
        // LED Matrix에 보드 그리기
        draw_board(canvas, board);
        
        printf("Board displayed on LED Matrix\n");
        printf("\nPress Enter to input a new board state, or Ctrl+C to exit...");
        
        // 입력 대기
        char c;
        while ((c = getchar()) != '\n' && !interrupt_received) {
            // 버퍼 비우기
        }
        
        if (interrupt_received) break;
        
        printf("\n");
    }
    
    // 정리
    printf("\nCleaning up...\n");
    led_canvas_clear(canvas);
    led_matrix_delete(matrix);
    
    return 0;
}