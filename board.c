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

// 다양한 색상 팔레트
#define COLOR_TEAM_R 255
#define COLOR_TEAM_G 200
#define COLOR_TEAM_B 0

#define COLOR_STAR1_R 255
#define COLOR_STAR1_G 255
#define COLOR_STAR1_B 0

#define COLOR_STAR2_R 255
#define COLOR_STAR2_G 100
#define COLOR_STAR2_B 200

#define COLOR_GRADIENT1_R 100
#define COLOR_GRADIENT1_G 50
#define COLOR_GRADIENT1_B 255

#define COLOR_GRADIENT2_R 50
#define COLOR_GRADIENT2_G 255
#define COLOR_GRADIENT2_B 100

#define COLOR_PARTICLE_R 200
#define COLOR_PARTICLE_G 100
#define COLOR_PARTICLE_B 255

// 전역 게임 보드
char board[BOARD_SIZE][BOARD_SIZE];

// LED Matrix 전역 변수
struct RGBLedMatrix *matrix = NULL;
struct LedCanvas *canvas = NULL;

// 5x7 대문자 폰트 (A-Z)
const uint8_t font5x7[26][7] = {
    {0x1E, 0x11, 0x1F, 0x11, 0x11, 0x00, 0x00}, // A
    {0x1E, 0x11, 0x1E, 0x11, 0x1E, 0x00, 0x00}, // B
    {0x0F, 0x10, 0x10, 0x10, 0x0F, 0x00, 0x00}, // C
    {0x1E, 0x11, 0x11, 0x11, 0x1E, 0x00, 0x00}, // D
    {0x1F, 0x10, 0x1E, 0x10, 0x1F, 0x00, 0x00}, // E
    {0x1F, 0x10, 0x1E, 0x10, 0x10, 0x00, 0x00}, // F
    {0x0F, 0x10, 0x13, 0x11, 0x0F, 0x00, 0x00}, // G
    {0x11, 0x11, 0x1F, 0x11, 0x11, 0x00, 0x00}, // H
    {0x0E, 0x04, 0x04, 0x04, 0x0E, 0x00, 0x00}, // I
    {0x01, 0x01, 0x01, 0x11, 0x0E, 0x00, 0x00}, // J
    {0x11, 0x12, 0x1C, 0x12, 0x11, 0x00, 0x00}, // K
    {0x10, 0x10, 0x10, 0x10, 0x1F, 0x00, 0x00}, // L
    {0x11, 0x1B, 0x15, 0x11, 0x11, 0x00, 0x00}, // M
    {0x11, 0x19, 0x15, 0x13, 0x11, 0x00, 0x00}, // N
    {0x0E, 0x11, 0x11, 0x11, 0x0E, 0x00, 0x00}, // O
    {0x1E, 0x11, 0x1E, 0x10, 0x10, 0x00, 0x00}, // P
    {0x0E, 0x11, 0x11, 0x15, 0x0E, 0x01, 0x00}, // Q
    {0x1E, 0x11, 0x1E, 0x12, 0x11, 0x00, 0x00}, // R
    {0x0F, 0x10, 0x0E, 0x01, 0x1E, 0x00, 0x00}, // S
    {0x1F, 0x04, 0x04, 0x04, 0x04, 0x00, 0x00}, // T
    {0x11, 0x11, 0x11, 0x11, 0x0E, 0x00, 0x00}, // U
    {0x11, 0x11, 0x11, 0x0A, 0x04, 0x00, 0x00}, // V
    {0x11, 0x11, 0x15, 0x1B, 0x11, 0x00, 0x00}, // W
    {0x11, 0x0A, 0x04, 0x0A, 0x11, 0x00, 0x00}, // X
    {0x11, 0x11, 0x0E, 0x04, 0x04, 0x00, 0x00}, // Y
    {0x1F, 0x02, 0x04, 0x08, 0x1F, 0x00, 0x00}  // Z
};

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

// LED 매트릭스에 글자 그리기 (manual)
void drawTextManual(const char* text, int start_x, int start_y, int r, int g, int b) {
    const int char_width = 5;
    const int spacing = 1;

    for (int i = 0; text[i] != '\0'; i++) {
        char ch = text[i];
        if (ch == ' ') {
            continue;
        }
        if (ch >= 'A' && ch <= 'Z') {
            int index = ch - 'A';
            int x_offset = start_x + i * (char_width + spacing);
            for (int y = 0; y < 7; y++) {
                uint8_t row = font5x7[index][y];
                for (int x = 0; x < char_width; x++) {
                    if (row & (1 << (4 - x))) {
                        led_canvas_set_pixel(canvas, x_offset + x, start_y + y, r, g, b);
                    }
                }
            }
        }
    }
}

// 작은 별 그리기
void drawSmallStar(int cx, int cy, int r, int g, int b) {
    led_canvas_set_pixel(canvas, cx, cy, r, g, b);
    led_canvas_set_pixel(canvas, cx-1, cy, r, g, b);
    led_canvas_set_pixel(canvas, cx+1, cy, r, g, b);
    led_canvas_set_pixel(canvas, cx, cy-1, r, g, b);
    led_canvas_set_pixel(canvas, cx, cy+1, r, g, b);
}

// 큰 별 그리기
void drawBigStar(int cx, int cy, int r, int g, int b) {
    led_canvas_set_pixel(canvas, cx, cy, r, g, b);
    
    for (int i = 1; i <= 3; i++) {
        led_canvas_set_pixel(canvas, cx-i, cy, r, g, b);
        led_canvas_set_pixel(canvas, cx+i, cy, r, g, b);
        led_canvas_set_pixel(canvas, cx, cy-i, r, g, b);
        led_canvas_set_pixel(canvas, cx, cy+i, r, g, b);
    }
    
    for (int i = 1; i <= 2; i++) {
        led_canvas_set_pixel(canvas, cx-i, cy-i, r/2, g/2, b/2);
        led_canvas_set_pixel(canvas, cx+i, cy-i, r/2, g/2, b/2);
        led_canvas_set_pixel(canvas, cx-i, cy+i, r/2, g/2, b/2);
        led_canvas_set_pixel(canvas, cx+i, cy+i, r/2, g/2, b/2);
    }
}

// 배경 파티클 효과
void drawBackgroundParticles(int frame) {
    for (int i = 0; i < 10; i++) {
        int x = (frame * 2 + i * 7) % MATRIX_SIZE;
        int y = (frame + i * 5) % MATRIX_SIZE;
        led_canvas_set_pixel(canvas, x, y, 
                           COLOR_PARTICLE_R/4, COLOR_PARTICLE_G/4, COLOR_PARTICLE_B/4);
    }
}

// LED 매트릭스에 팀 이름 표시 (예쁘게 꾸미기)
void showTeamNameOnMatrix() {
    if (!canvas) return;
    
    // 애니메이션 프레임
    for (int frame = 0; frame < 180; frame++) {
        led_canvas_clear(canvas);
        
        // 배경 그라데이션
        for (int y = 0; y < MATRIX_SIZE; y++) {
            int bg_intensity = (y * 30) / MATRIX_SIZE;
            for (int x = 0; x < MATRIX_SIZE; x++) {
                led_canvas_set_pixel(canvas, x, y, 
                                   bg_intensity/3, 0, bg_intensity/2);
            }
        }
        
        // 배경 파티클
        drawBackgroundParticles(frame);
        
        // TEAM SHANNON 텍스트 (깜빡임 효과)
        if (frame < 60 || (frame >= 70 && frame < 130) || frame >= 140) {
            drawTextManual("TEAM", 18, 18, COLOR_TEAM_R, COLOR_TEAM_G, COLOR_TEAM_B);
            drawTextManual("SHANNON", 8, 28, COLOR_TEAM_R, COLOR_TEAM_G, COLOR_TEAM_B);
        }
        
        // 회전하는 큰 별들 (모서리)
        int star_offset = (frame % 20) < 10 ? 0 : 1;
        drawBigStar(10 + star_offset, 10, COLOR_STAR1_R, COLOR_STAR1_G, COLOR_STAR1_B);
        drawBigStar(54 - star_offset, 10, COLOR_STAR2_R, COLOR_STAR2_G, COLOR_STAR2_B);
        drawBigStar(10 + star_offset, 54, COLOR_STAR2_R, COLOR_STAR2_G, COLOR_STAR2_B);
        drawBigStar(54 - star_offset, 54, COLOR_STAR1_R, COLOR_STAR1_G, COLOR_STAR1_B);
        
        // 작은 별들 (움직이는 효과)
        for (int i = 0; i < 8; i++) {
            int star_x = (frame * 2 + i * 8) % MATRIX_SIZE;
            int star_y = 5 + (i % 2) * 50;
            drawSmallStar(star_x, star_y, 200, 200, 100);
        }
        
        // 프레임 (무지개 효과)
        for (int x = 0; x < MATRIX_SIZE; x++) {
            int rainbow = (x + frame) % 60;
            int r = rainbow < 20 ? 255 : (rainbow < 40 ? 0 : 100);
            int g = rainbow < 20 ? 0 : (rainbow < 40 ? 255 : 100);
            int b = rainbow < 20 ? 100 : (rainbow < 40 ? 100 : 255);
            
            led_canvas_set_pixel(canvas, x, 0, r, g, b);
            led_canvas_set_pixel(canvas, x, 1, r/2, g/2, b/2);
            led_canvas_set_pixel(canvas, x, MATRIX_SIZE-1, r, g, b);
            led_canvas_set_pixel(canvas, x, MATRIX_SIZE-2, r/2, g/2, b/2);
        }
        
        for (int y = 2; y < MATRIX_SIZE-2; y++) {
            int rainbow = (y + frame) % 60;
            int r = rainbow < 20 ? 255 : (rainbow < 40 ? 0 : 100);
            int g = rainbow < 20 ? 0 : (rainbow < 40 ? 255 : 100);
            int b = rainbow < 20 ? 100 : (rainbow < 40 ? 100 : 255);
            
            led_canvas_set_pixel(canvas, 0, y, r, g, b);
            led_canvas_set_pixel(canvas, 1, y, r/2, g/2, b/2);
            led_canvas_set_pixel(canvas, MATRIX_SIZE-1, y, r, g, b);
            led_canvas_set_pixel(canvas, MATRIX_SIZE-2, y, r/2, g/2, b/2);
        }
        
        usleep(16667); // 약 60fps
    }
    
    // 최종 정적 이미지
    led_canvas_clear(canvas);
    
    // 배경
    for (int y = 0; y < MATRIX_SIZE; y++) {
        int bg_intensity = (y * 30) / MATRIX_SIZE;
        for (int x = 0; x < MATRIX_SIZE; x++) {
            led_canvas_set_pixel(canvas, x, y, bg_intensity/3, 0, bg_intensity/2);
        }
    }
    
    // 텍스트
    drawTextManual("TEAM", 18, 18, COLOR_TEAM_R, COLOR_TEAM_G, COLOR_TEAM_B);
    drawTextManual("SHANNON", 8, 28, COLOR_TEAM_R, COLOR_TEAM_G, COLOR_TEAM_B);
    
    // 별들
    drawBigStar(10, 10, COLOR_STAR1_R, COLOR_STAR1_G, COLOR_STAR1_B);
    drawBigStar(54, 10, COLOR_STAR2_R, COLOR_STAR2_G, COLOR_STAR2_B);
    drawBigStar(10, 54, COLOR_STAR2_R, COLOR_STAR2_G, COLOR_STAR2_B);
    drawBigStar(54, 54, COLOR_STAR1_R, COLOR_STAR1_G, COLOR_STAR1_B);
    
    // 프레임
    for (int x = 0; x < MATRIX_SIZE; x++) {
        led_canvas_set_pixel(canvas, x, 0, 100, 100, 255);
        led_canvas_set_pixel(canvas, x, MATRIX_SIZE-1, 100, 100, 255);
    }
    for (int y = 0; y < MATRIX_SIZE; y++) {
        led_canvas_set_pixel(canvas, 0, y, 100, 100, 255);
        led_canvas_set_pixel(canvas, MATRIX_SIZE-1, y, 100, 100, 255);
    }
    
    usleep(1000000); // 1초간 유지
}

// LED Matrix 초기화
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

// 그리드 라인 그리기 (지정된 위치에)
void drawGridLines() {
    // 그리드 위치: 0,7,8,15,16,23,24,31,32,39,40,47,48,55,56,63
    int grid_positions[] = {0, 7, 8, 15, 16, 23, 24, 31, 32, 39, 40, 47, 48, 55, 56, 63};
    int num_positions = sizeof(grid_positions) / sizeof(grid_positions[0]);
    
    // 가로선 그리기
    for (int i = 0; i < num_positions; i++) {
        int y = grid_positions[i];
        
        // 가장자리 (0, 63)는 1픽셀
        if (y == 0 || y == 63) {
            for (int x = 0; x < MATRIX_SIZE; x++) {
                led_canvas_set_pixel(canvas, x, y, COLOR_GRID_OUTER_R, COLOR_GRID_OUTER_G, COLOR_GRID_OUTER_B);
            }
        } else {
            // 내부는 더 밝은 색
            for (int x = 0; x < MATRIX_SIZE; x++) {
                led_canvas_set_pixel(canvas, x, y, COLOR_GRID_INNER_R, COLOR_GRID_INNER_G, COLOR_GRID_INNER_B);
            }
        }
    }
    
    // 세로선 그리기
    for (int i = 0; i < num_positions; i++) {
        int x = grid_positions[i];
        
        // 가장자리 (0, 63)는 1픽셀
        if (x == 0 || x == 63) {
            for (int y = 0; y < MATRIX_SIZE; y++) {
                led_canvas_set_pixel(canvas, x, y, COLOR_GRID_OUTER_R, COLOR_GRID_OUTER_G, COLOR_GRID_OUTER_B);
            }
        } else {
            // 내부는 더 밝은 색
            for (int y = 0; y < MATRIX_SIZE; y++) {
                led_canvas_set_pixel(canvas, x, y, COLOR_GRID_INNER_R, COLOR_GRID_INNER_G, COLOR_GRID_INNER_B);
            }
        }
    }
}

// 말 그리기 (6x6 전체 채우기)
void drawPiece(int row, int col, char piece) {
    if (piece == '.') return; // 빈 공간은 아무것도 그리지 않음
    
    // 각 셀의 시작 위치 계산
    int cell_x_starts[] = {1, 9, 17, 25, 33, 41, 49, 57};
    int cell_y_starts[] = {1, 9, 17, 25, 33, 41, 49, 57};
    
    int start_x = cell_x_starts[col];
    int start_y = cell_y_starts[row];
    
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
    
    led_canvas_clear(canvas);
    drawGridLines();
    
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
    if (matrix) {
        led_matrix_delete(matrix);
    }
    
    printf("\nProgram terminated.\n");
    printf("Team Shannon - OctaFlip LED Display\n");
    
    return 0;
}