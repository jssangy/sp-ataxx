#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "led-matrix-c.h"

#define BOARD_SIZE 8
#define CELL_SIZE 8
#define MATRIX_SIZE 64

// 색상 정의
#define COLOR_RED_R 255
#define COLOR_RED_G 0
#define COLOR_RED_B 0

#define COLOR_BLUE_R 0
#define COLOR_BLUE_G 0
#define COLOR_BLUE_B 255

#define COLOR_EMPTY_R 0
#define COLOR_EMPTY_G 0
#define COLOR_EMPTY_B 0

#define COLOR_GRID_R 50
#define COLOR_GRID_G 50
#define COLOR_GRID_B 50

#define COLOR_TEAM_R 255
#define COLOR_TEAM_G 200
#define COLOR_TEAM_B 0

// 전역 변수
char board[BOARD_SIZE][BOARD_SIZE];
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

// 콘솔 출력용 팀 이름
void displayTeamName() {
    printf("\n╔════════════════════════════════════════════════════╗\n");
    printf("║             TEAM CLAUDE - OctaFlip Display         ║\n");
    printf("╚════════════════════════════════════════════════════╝\n");
}

// LED 매트릭스에 글자 그리기
void drawTextManual(const char* text, int start_x, int start_y, int r, int g, int b) {
    const int char_width = 5;
    const int spacing = 1;

    for (int i = 0; text[i] != '\0'; i++) {
        char ch = text[i];
        if (ch == ' ') continue;
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

// 그리드 선 그리기
void drawGridLines() {
    for (int i = 0; i <= MATRIX_SIZE; i += CELL_SIZE) {
        for (int j = 0; j < MATRIX_SIZE; j++) {
            led_canvas_set_pixel(canvas, i, j, COLOR_GRID_R, COLOR_GRID_G, COLOR_GRID_B);
            led_canvas_set_pixel(canvas, j, i, COLOR_GRID_R, COLOR_GRID_G, COLOR_GRID_B);
        }
    }
}

// 말 출력
void drawPiece(int row, int col, char piece) {
    int start_x = col * CELL_SIZE + 2;
    int start_y = row * CELL_SIZE + 2;
    int color_r, color_g, color_b;

    if (piece == 'R') {
        color_r = COLOR_RED_R;
        color_g = COLOR_RED_G;
        color_b = COLOR_RED_B;
    } else if (piece == 'B') {
        color_r = COLOR_BLUE_R;
        color_g = COLOR_BLUE_G;
        color_b = COLOR_BLUE_B;
    } else {
        return;
    }

    for (int y = start_y; y < start_y + 4; y++) {
        for (int x = start_x; x < start_x + 4; x++) {
            led_canvas_set_pixel(canvas, x, y, color_r, color_g, color_b);
        }
    }
}

// 보드 LED 매트릭스에 표시
void displayBoardOnMatrix() {
    led_canvas_clear(canvas);
    drawGridLines();
    for (int row = 0; row < BOARD_SIZE; row++) {
        for (int col = 0; col < BOARD_SIZE; col++) {
            drawPiece(row, col, board[row][col]);
        }
    }
}

// 보드 초기화 및 콘솔 출력
void printBoard() {
    printf("\n    A B C D E F G H\n");
    printf("   -----------------\n");
    for (int i = 0; i < BOARD_SIZE; i++) {
        printf(" %d|", i + 1);
        for (int j = 0; j < BOARD_SIZE; j++) {
            printf(" %c", board[i][j] ? board[i][j] : '.');
        }
        printf("\n");
    }
}

// 팀 이름 LED 매트릭스에 표시
void showTeamNameOnMatrix() {
    for (int i = 0; i < 3; i++) {
        led_canvas_clear(canvas);
        drawTextManual("TEAM", 18, 20, COLOR_TEAM_R, COLOR_TEAM_G, COLOR_TEAM_B);
        drawTextManual("CLAUDE", 13, 35, COLOR_TEAM_R, COLOR_TEAM_G, COLOR_TEAM_B);
        usleep(500000);
        led_canvas_clear(canvas);
        usleep(200000);
    }
    drawTextManual("TEAM", 18, 20, COLOR_TEAM_R, COLOR_TEAM_G, COLOR_TEAM_B);
    drawTextManual("CLAUDE", 13, 35, COLOR_TEAM_R, COLOR_TEAM_G, COLOR_TEAM_B);
}

// LED 매트릭스 초기화
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

// 메인 함수
int main() {
    displayTeamName();

    if (initLEDMatrix() != 0) return EXIT_FAILURE;

    showTeamNameOnMatrix();

    memset(board, 0, sizeof(board));
    board[3][3] = 'R';
    board[3][4] = 'B';
    board[4][3] = 'B';
    board[4][4] = 'R';

    printBoard();
    displayBoardOnMatrix();

    sleep(10); // 화면 유지

    led_matrix_delete(matrix);
    return EXIT_SUCCESS;
}
