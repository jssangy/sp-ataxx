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

#define COLOR_EMPTY_R 30
#define COLOR_EMPTY_G 30
#define COLOR_EMPTY_B 30

#define COLOR_BLOCKED_R 100
#define COLOR_BLOCKED_G 100
#define COLOR_BLOCKED_B 100

// 그리드 색상 (내부는 더 밝게)
#define COLOR_GRID_OUTER_R 50
#define COLOR_GRID_OUTER_G 50
#define COLOR_GRID_OUTER_B 50

#define COLOR_GRID_INNER_R 80
#define COLOR_GRID_INNER_G 80
#define COLOR_GRID_INNER_B 80

#define COLOR_BG_R 0
#define COLOR_BG_G 0
#define COLOR_BG_B 0

// 팀 이름 색상
#define COLOR_TEAM_R 255
#define COLOR_TEAM_G 200
#define COLOR_TEAM_B 0

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

// LED 매트릭스에 글자 그리기 (paste.txt 방식)
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

// 팀 이름을 LED 매트릭스에 표시
void showTeamNameOnMatrix() {
    if (!canvas) return;
    
    // 깜빡이는 효과 - 3번 반복
    for (int blink = 0; blink < 3; blink++) {
        led_canvas_clear(canvas);
        
        // "TEAM" 표시 (위치 조정)
        drawTextManual("TEAM", 18, 20, COLOR_TEAM_R, COLOR_TEAM_G, COLOR_TEAM_B);
        // "SHANNON" 표시 (위치 조정)
        drawTextManual("SHANNON", 8, 35, COLOR_TEAM_R, COLOR_TEAM_G, COLOR_TEAM_B);
        
        // 장식용 프레임
        for (int x = 0; x < MATRIX_SIZE; x++) {
            led_canvas_set_pixel(canvas, x, 0, 100, 100, 255);
            led_canvas_set_pixel(canvas, x, MATRIX_SIZE-1, 100, 100, 255);
        }
        for (int y = 0; y < MATRIX_SIZE; y++) {
            led_canvas_set_pixel(canvas, 0, y, 100, 100, 255);
            led_canvas_set_pixel(canvas, MATRIX_SIZE-1, y, 100, 100, 255);
        }
        
        usleep(500000); // 0.5초 켜짐
        led_canvas_clear(canvas);
        usleep(200000); // 0.2초 꺼짐
    }
    
    // 마지막에 계속 표시
    led_canvas_clear(canvas);
    drawTextManual("TEAM", 18, 20, COLOR_TEAM_R, COLOR_TEAM_G, COLOR_TEAM_B);
    drawTextManual("SHANNON", 8, 35, COLOR_TEAM_R, COLOR_TEAM_G, COLOR_TEAM_B);
    
    // 프레임
    for (int x = 0; x < MATRIX_SIZE; x++) {
        led_canvas_set_pixel(canvas, x, 0, 100, 100, 255);
        led_canvas_set_pixel(canvas, x, MATRIX_SIZE-1, 100, 100, 255);
    }
    for (int y = 0; y < MATRIX_SIZE; y++) {
        led_canvas_set_pixel(canvas, 0, y, 100, 100, 255);
        led_canvas_set_pixel(canvas, MATRIX_SIZE-1, y, 100, 100, 255);
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
    
    // hardware pulse 비활성화
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

// 그리드 라인 그리기 (내부 2픽셀, 가장자리 1픽셀)
void drawGridLines() {
    // 가로선 그리기
    for (int i = 0; i <= BOARD_SIZE; i++) {
        int y = i * CELL_SIZE;
        if (y >= MATRIX_SIZE) {
            y = MATRIX_SIZE - 1;
        }

        if (i == 0 || i == BOARD_SIZE) {
            // 가장자리는 1픽셀
            for (int x = 0; x < MATRIX_SIZE; x++) {
                led_canvas_set_pixel(canvas, x, y, COLOR_GRID_OUTER_R, COLOR_GRID_OUTER_G, COLOR_GRID_OUTER_B);
            }
        } else {
            // 내부는 2픽셀
            for (int x = 0; x < MATRIX_SIZE; x++) {
                led_canvas_set_pixel(canvas, x, y, COLOR_GRID_INNER_R, COLOR_GRID_INNER_G, COLOR_GRID_INNER_B);
                if (y + 1 < MATRIX_SIZE) {
                    led_canvas_set_pixel(canvas, x, y + 1, COLOR_GRID_INNER_R, COLOR_GRID_INNER_G, COLOR_GRID_INNER_B);
                }
            }
        }
    }

    // 세로선 그리기
    for (int i = 0; i <= BOARD_SIZE; i++) {
        int x = i * CELL_SIZE;
        if (x >= MATRIX_SIZE) {
            x = MATRIX_SIZE - 1;
        }

        if (i == 0 || i == BOARD_SIZE) {
            // 가장자리는 1픽셀
            for (int y = 0; y < MATRIX_SIZE; y++) {
                led_canvas_set_pixel(canvas, x, y, COLOR_GRID_OUTER_R, COLOR_GRID_OUTER_G, COLOR_GRID_OUTER_B);
            }
        } else {
            // 내부는 2픽셀
            for (int y = 0; y < MATRIX_SIZE; y++) {
                led_canvas_set_pixel(canvas, x, y, COLOR_GRID_INNER_R, COLOR_GRID_INNER_G, COLOR_GRID_INNER_B);
                if (x + 1 < MATRIX_SIZE) {
                    led_canvas_set_pixel(canvas, x + 1, y, COLOR_GRID_INNER_R, COLOR_GRID_INNER_G, COLOR_GRID_INNER_B);
                }
            }
        }
    }
}


// 말 그리기 (6x6 전체 채우기)
void drawPiece(int row, int col, char piece) {
    // 셀의 시작 위치 계산
    int cell_start_x = col * CELL_SIZE;
    int cell_start_y = row * CELL_SIZE;
    
    // 6x6 영역의 시작 위치 (셀 내부 중앙)
    int start_x, start_y;
    
    // 가장자리 셀인지 확인
    if (row == 0 || row == BOARD_SIZE - 1) {
        start_y = (row == 0) ? cell_start_y + 1 : cell_start_y + 1;
    } else {
        start_y = cell_start_y + 2; // 내부 셀은 2픽셀 그리드
    }
    
    if (col == 0 || col == BOARD_SIZE - 1) {
        start_x = (col == 0) ? cell_start_x + 1 : cell_start_x + 1;
    } else {
        start_x = cell_start_x + 2; // 내부 셀은 2픽셀 그리드
    }
    
    // 색상 결정
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
    
    // 6x6 영역 채우기
    for (int py = 0; py < PIECE_SIZE; py++) {
        for (int px = 0; px < PIECE_SIZE; px++) {
            int x = start_x + px;
            int y = start_y + py;
            
            if (piece == 'R') {
                // 빨간색은 원형으로
                int cx = PIECE_SIZE / 2;
                int cy = PIECE_SIZE / 2;
                int dx = px - cx;
                int dy = py - cy;
                if (dx*dx + dy*dy <= 9) { // 반지름 3픽셀
                    led_canvas_set_pixel(canvas, x, y, r, g, b);
                }
            } else if (piece == 'B') {
                // 파란색은 다이아몬드
                int cx = PIECE_SIZE / 2;
                int cy = PIECE_SIZE / 2;
                if (abs(px - cx) + abs(py - cy) <= 3) {
                    led_canvas_set_pixel(canvas, x, y, r, g, b);
                }
            } else if (piece == '.') {
                // 빈 공간은 중앙에 작은 점
                if (px >= 2 && px <= 3 && py >= 2 && py <= 3) {
                    led_canvas_set_pixel(canvas, x, y, r, g, b);
                }
            } else if (piece == '#') {
                // 막힌 공간은 X 표시
                if (px == py || px == PIECE_SIZE - 1 - py) {
                    led_canvas_set_pixel(canvas, x, y, r, g, b);
                }
            }
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
    printf("\nEnter 8 lines, each with 8 characters:\n");
    
    // 보드 입력 받기
    char line[BOARD_SIZE + 10];
    
    for (int i = 0; i < BOARD_SIZE; i++) {
        int valid_input = 0;
        
        while (!valid_input) {
            printf("Row %d: ", i + 1);
            
            if (fgets(line, sizeof(line), stdin) == NULL) {
                printf("Error reading input. Please try again.\n");
                continue;
            }
            
            // 개행 문자 제거
            int len = strlen(line);
            if (len > 0 && line[len - 1] == '\n') {
                line[len - 1] = '\0';
                len--;
            }
            
            // 캐리지 리턴 제거 (Windows)
            if (len > 0 && line[len - 1] == '\r') {
                line[len - 1] = '\0';
                len--;
            }
            
            // 라인 길이 검증
            if (len != BOARD_SIZE) {
                printf("Invalid line length. Each line must have exactly %d characters.\n", BOARD_SIZE);
                continue;
            }
            
            // 문자 검증
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
            
            // 유효한 입력
            valid_input = 1;
        }
        
        // 보드에 복사
        for (int j = 0; j < BOARD_SIZE; j++) {
            board[i][j] = line[j];
        }
    }
    
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