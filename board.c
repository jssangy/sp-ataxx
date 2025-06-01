#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "led-matrix-c.h"

#define BOARD_SIZE 8
#define CELL_SIZE 8
#define PIECE_SIZE 6
#define MATRIX_SIZE 64

// 색상 정의
#define COLOR_RED_R 255
#define COLOR_RED_G 0
#define COLOR_RED_B 0

#define COLOR_BLUE_R 0
#define COLOR_BLUE_G 100
#define COLOR_BLUE_B 255

#define COLOR_EMPTY_R 0
#define COLOR_EMPTY_G 0
#define COLOR_EMPTY_B 0

// 그리드 색상
#define COLOR_GRID_OUTER_R 50
#define COLOR_GRID_OUTER_G 50
#define COLOR_GRID_OUTER_B 50

#define COLOR_GRID_INNER_R 80
#define COLOR_GRID_INNER_G 80
#define COLOR_GRID_INNER_B 80

// 팀 이름 색상
#define COLOR_TEAM_R 255
#define COLOR_TEAM_G 200
#define COLOR_TEAM_B 0

// 장식 색상
#define COLOR_STAR_R 255
#define COLOR_STAR_G 255
#define COLOR_STAR_B 0

#define COLOR_FRAME_R 100
#define COLOR_FRAME_G 100
#define COLOR_FRAME_B 255

// 전역 게임 보드
char board[BOARD_SIZE][BOARD_SIZE];

// LED Matrix 전역 변수
struct RGBLedMatrix *matrix = NULL;
struct LedCanvas *canvas = NULL;
struct LedFont *font = NULL;

// 콘솔에 팀 이름 표시
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
    printf("║                    🎮 OctaFlip LED Display 🎮                 ║\n");
    printf("║                                                               ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n");
    printf("\n");
}

// 별 그리기 함수
void drawStar(int cx, int cy, int size) {
    // 간단한 별 모양
    for (int i = -size; i <= size; i++) {
        led_canvas_set_pixel(canvas, cx + i, cy, COLOR_STAR_R, COLOR_STAR_G, COLOR_STAR_B);
        led_canvas_set_pixel(canvas, cx, cy + i, COLOR_STAR_R, COLOR_STAR_G, COLOR_STAR_B);
    }
    for (int i = -size/2; i <= size/2; i++) {
        led_canvas_set_pixel(canvas, cx + i, cy + i, COLOR_STAR_R, COLOR_STAR_G, COLOR_STAR_B);
        led_canvas_set_pixel(canvas, cx + i, cy - i, COLOR_STAR_R, COLOR_STAR_G, COLOR_STAR_B);
    }
}

// LED 매트릭스에 팀 이름 표시 (예쁘게 꾸미기)
void showTeamNameOnMatrix() {
    if (!canvas) return;
    
    // 폰트 로드 시도
    if (!font) {
        font = load_font("/home/pi/rpi-rgb-led-matrix/fonts/7x13.bdf");
        if (!font) {
            printf("Warning: Could not load font file. Using fallback display.\n");
        }
    }
    
    // 깜빡이는 효과 - 3번 반복
    for (int blink = 0; blink < 3; blink++) {
        led_canvas_clear(canvas);
        
        if (font) {
            // 텍스트 표시
            draw_text(canvas, font, 5, 32, COLOR_TEAM_R, COLOR_TEAM_G, COLOR_TEAM_B, 
                      "TEAM SHANNON", 0);
        } else {
            // 폰트 없이 색깔 블록으로 표시
            for (int y = 25; y < 40; y++) {
                for (int x = 8; x < 56; x++) {
                    led_canvas_set_pixel(canvas, x, y, COLOR_TEAM_R, COLOR_TEAM_G, COLOR_TEAM_B);
                }
            }
        }
        
        // 모서리 별 장식
        drawStar(10, 10, 3);
        drawStar(54, 10, 3);
        drawStar(10, 54, 3);
        drawStar(54, 54, 3);
        
        // 프레임 (그라데이션 효과)
        for (int x = 0; x < MATRIX_SIZE; x++) {
            int gradient = (x * 255) / MATRIX_SIZE;
            led_canvas_set_pixel(canvas, x, 0, gradient, gradient/2, 255);
            led_canvas_set_pixel(canvas, x, MATRIX_SIZE-1, 255-gradient, (255-gradient)/2, 255);
        }
        for (int y = 1; y < MATRIX_SIZE-1; y++) {
            int gradient = (y * 255) / MATRIX_SIZE;
            led_canvas_set_pixel(canvas, 0, y, gradient, gradient/2, 255);
            led_canvas_set_pixel(canvas, MATRIX_SIZE-1, y, 255-gradient, (255-gradient)/2, 255);
        }
        
        usleep(500000); // 0.5초 켜짐
        led_canvas_clear(canvas);
        usleep(200000); // 0.2초 꺼짐
    }
    
    // 최종 표시
    led_canvas_clear(canvas);
    
    if (font) {
        draw_text(canvas, font, 5, 32, COLOR_TEAM_R, COLOR_TEAM_G, COLOR_TEAM_B, 
                  "TEAM SHANNON", 0);
    } else {
        for (int y = 25; y < 40; y++) {
            for (int x = 8; x < 56; x++) {
                led_canvas_set_pixel(canvas, x, y, COLOR_TEAM_R, COLOR_TEAM_G, COLOR_TEAM_B);
            }
        }
    }
    
    // 장식 유지
    drawStar(10, 10, 3);
    drawStar(54, 10, 3);
    drawStar(10, 54, 3);
    drawStar(54, 54, 3);
    
    // 프레임
    for (int x = 0; x < MATRIX_SIZE; x++) {
        int gradient = (x * 255) / MATRIX_SIZE;
        led_canvas_set_pixel(canvas, x, 0, gradient, gradient/2, 255);
        led_canvas_set_pixel(canvas, x, MATRIX_SIZE-1, 255-gradient, (255-gradient)/2, 255);
    }
    for (int y = 1; y < MATRIX_SIZE-1; y++) {
        int gradient = (y * 255) / MATRIX_SIZE;
        led_canvas_set_pixel(canvas, 0, y, gradient, gradient/2, 255);
        led_canvas_set_pixel(canvas, MATRIX_SIZE-1, y, 255-gradient, (255-gradient)/2, 255);
    }
    
    // 2초간 표시
    usleep(2000000);
}

// LED Matrix 초기화
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

// 그리드 라인 그리기 (가장자리 1픽셀, 내부 2픽셀)
void drawGridLines() {
    // 가로선 그리기
    for (int i = 0; i <= BOARD_SIZE; i++) {
        int y = i * CELL_SIZE;
        
        if (i == 0 || i == BOARD_SIZE) {
            // 가장자리는 1픽셀
            for (int x = 0; x < MATRIX_SIZE; x++) {
                led_canvas_set_pixel(canvas, x, y, COLOR_GRID_OUTER_R, COLOR_GRID_OUTER_G, COLOR_GRID_OUTER_B);
            }
        } else {
            // 내부는 2픽셀
            for (int x = 0; x < MATRIX_SIZE; x++) {
                led_canvas_set_pixel(canvas, x, y, COLOR_GRID_INNER_R, COLOR_GRID_INNER_G, COLOR_GRID_INNER_B);
                if (y < MATRIX_SIZE - 1) {
                    led_canvas_set_pixel(canvas, x, y + 1, COLOR_GRID_INNER_R, COLOR_GRID_INNER_G, COLOR_GRID_INNER_B);
                }
            }
        }
    }
    
    // 세로선 그리기
    for (int i = 0; i <= BOARD_SIZE; i++) {
        int x = i * CELL_SIZE;
        
        if (i == 0 || i == BOARD_SIZE) {
            // 가장자리는 1픽셀
            for (int y = 0; y < MATRIX_SIZE; y++) {
                led_canvas_set_pixel(canvas, x, y, COLOR_GRID_OUTER_R, COLOR_GRID_OUTER_G, COLOR_GRID_OUTER_B);
            }
        } else {
            // 내부는 2픽셀
            for (int y = 0; y < MATRIX_SIZE; y++) {
                led_canvas_set_pixel(canvas, x, y, COLOR_GRID_INNER_R, COLOR_GRID_INNER_G, COLOR_GRID_INNER_B);
                if (x < MATRIX_SIZE - 1) {
                    led_canvas_set_pixel(canvas, x + 1, y, COLOR_GRID_INNER_R, COLOR_GRID_INNER_G, COLOR_GRID_INNER_B);
                }
            }
        }
    }
}

// 말 그리기 (6x6 전체 채우기)
void drawPiece(int row, int col, char piece) {
    if (piece == '.') return; // 빈 공간은 아무것도 그리지 않음
    
    // 셀의 시작 위치 계산
    int cell_start_x = col * CELL_SIZE;
    int cell_start_y = row * CELL_SIZE;
    
    // 6x6 영역의 시작 위치
    int start_x, start_y;
    
    // 가장자리 셀인지 확인하여 오프셋 계산
    if (row == 0) {
        start_y = cell_start_y + 1;
    } else if (row == BOARD_SIZE - 1) {
        start_y = cell_start_y + 1;
    } else {
        start_y = cell_start_y + 2;
    }
    
    if (col == 0) {
        start_x = cell_start_x + 1;
    } else if (col == BOARD_SIZE - 1) {
        start_x = cell_start_x + 1;
    } else {
        start_x = cell_start_x + 2;
    }
    
    // 색상 결정
    int r, g, b;
    if (piece == 'R') {
        r = COLOR_RED_R;
        g = COLOR_RED_G;
        b = COLOR_RED_B;
    } else if (piece == 'B') {
        r = COLOR_BLUE_R;
        g = COLOR_BLUE_G;
        b = COLOR_BLUE_B;
    } else if (piece == '#') {
        r = COLOR_GRID_INNER_R;
        g = COLOR_GRID_INNER_G;
        b = COLOR_GRID_INNER_B;
    } else {
        return;
    }
    
    // 6x6 영역을 완전히 채우기
    for (int py = 0; py < PIECE_SIZE; py++) {
        for (int px = 0; px < PIECE_SIZE; px++) {
            led_canvas_set_pixel(canvas, start_x + px, start_y + py, r, g, b);
        }
    }
}

// 보드를 LED 매트릭스에 표시
void displayBoardOnMatrix() {
    if (!canvas) return;
    
    // 배경 지우기
    led_canvas_clear(canvas);
    
    // 그리드 라인 그리기
    drawGridLines();
    
    // 각 말 그리기
    for (int row = 0; row < BOARD_SIZE; row++) {
        for (int col = 0; col < BOARD_SIZE; col++) {
            drawPiece(row, col, board[row][col]);
        }
    }
}

// 콘솔에 보드 상태 출력
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

// 메인 함수
int main() {
    // 팀 이름 표시
    displayTeamName();
    
    // LED Matrix 초기화
    printf("Initializing LED Matrix...\n");
    if (initLEDMatrix() != 0) {
        printf("Warning: Unable to initialize LED matrix. Running in console mode.\n");
    } else {
        printf("LED Matrix initialized successfully!\n");
        // 팀 이름 애니메이션 표시
        showTeamNameOnMatrix();
    }
    
    // 보드 입력 안내
    printf("\n=== Board State Input ===\n");
    printf("Please enter the 8x8 board state.\n");
    printf("Available characters:\n");
    printf("  R : Red player's piece\n");
    printf("  B : Blue player's piece\n");
    printf("  . : Empty cell\n");
    printf("  # : Blocked cell\n");
    printf("\n=== EXAMPLE FORMAT ===\n");
    printf("R......B\n");
    printf("........\n");
    printf("........\n");
    printf("........\n");
    printf("........\n");
    printf("........\n");
    printf("........\n");
    printf("B......R\n");
    printf("===================\n\n");
    printf("Paste your 8 rows below (exactly 8 characters per row WITHOUT SPACES):\n");
    printf("↓ ↓ ↓ ↓ ↓ ↓ ↓ ↓ ↓ ↓ ↓ ↓ ↓ ↓ ↓ ↓ ↓ ↓ ↓ ↓ ↓ ↓ ↓ ↓ ↓ ↓ ↓ ↓ ↓ ↓ ↓ ↓\n");
    printf("--------------------------------------------------------------\n");
    
    // Bulk 입력 받기 (8줄을 한번에)
    char board_lines[BOARD_SIZE][BOARD_SIZE + 10];
    
    for (int i = 0; i < BOARD_SIZE; i++) {
        if (fgets(board_lines[i], sizeof(board_lines[i]), stdin) == NULL) {
            printf("Error reading input. Returning to main menu.\n");
            if (matrix) led_matrix_delete(matrix);
            return 1;
        }
        
        // 개행 문자 제거
        int len = strlen(board_lines[i]);
        if (len > 0 && board_lines[i][len - 1] == '\n') {
            board_lines[i][len - 1] = '\0';
            len--;
        }
        if (len > 0 && board_lines[i][len - 1] == '\r') {
            board_lines[i][len - 1] = '\0';
            len--;
        }
        
        // 라인 길이 확인
        if (len != BOARD_SIZE) {
            printf("Invalid line length for row %d. Each row must have exactly %d characters.\n", i+1, BOARD_SIZE);
            printf("Returning to main menu.\n");
            if (matrix) led_matrix_delete(matrix);
            return 1;
        }
        
        // 유효한 문자 확인
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board_lines[i][j] != 'R' && board_lines[i][j] != 'B' && 
                board_lines[i][j] != '.' && board_lines[i][j] != '#') {
                printf("Invalid character '%c' at row %d, column %d. Use only R, B, ., or #.\n",
                       board_lines[i][j], i+1, j+1);
                printf("Returning to main menu.\n");
                if (matrix) led_matrix_delete(matrix);
                return 1;
            }
            
            // 보드에 복사
            board[i][j] = board_lines[i][j];
        }
    }
    
    printf("--------------------------------------------------------------\n");
    
    // 보드 상태 출력
    printf("\nInput Board:\n");
    printBoard();
    
    // LED 매트릭스에 표시
    if (canvas) {
        printf("Displaying board on LED Matrix...\n");
        displayBoardOnMatrix();
        
        // 종료 대기
        printf("Press Enter to exit...\n");
        getchar();
    }
    
    // 정리
    if (font) {
        delete_font(font);
    }
    if (matrix) {
        led_matrix_delete(matrix);
    }
    
    printf("\nProgram terminated.\n");
    printf("Team Shannon - OctaFlip LED Display\n");
    
    return 0;
}