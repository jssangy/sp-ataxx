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

// 팀 이름 색상 (그라디언트 효과)
#define COLOR_TEAM1_R 255
#define COLOR_TEAM1_G 100
#define COLOR_TEAM1_B 0

#define COLOR_TEAM2_R 255
#define COLOR_TEAM2_G 200
#define COLOR_TEAM2_B 0

// 게임 보드 전역 변수
char board[BOARD_SIZE][BOARD_SIZE];

// LED Matrix 관련 전역 변수
struct RGBLedMatrix *matrix = NULL;
struct LedCanvas *canvas = NULL;

// 팀 이름 표시 함수
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

// LED 매트릭스에 팀 이름 표시 (애니메이션)
void showTeamNameOnMatrix() {
    if (!canvas) return;
    
    led_canvas_clear(canvas);
    
    // 간단한 "TEAM SHANNON" 텍스트를 픽셀로 표시
    const char* text = "TEAM SHANNON";
    int text_len = strlen(text);
    int start_x = 5;
    int y = 28;
    
    // 깜빡이는 효과
    for (int blink = 0; blink < 3; blink++) {
        // 텍스트 표시
        for (int i = 0; i < text_len; i++) {
            int x = start_x + i * 5;
            // 간단한 블록 문자로 표시
            for (int px = 0; px < 4; px++) {
                for (int py = 0; py < 7; py++) {
                    int r = COLOR_TEAM1_R - (i * 10);
                    int g = COLOR_TEAM1_G + (i * 10);
                    int b = COLOR_TEAM1_B;
                    led_canvas_set_pixel(canvas, x + px, y + py, r, g, b);
                }
            }
        }
        
        // 장식 프레임
        for (int x = 0; x < MATRIX_SIZE; x++) {
            led_canvas_set_pixel(canvas, x, 0, 100, 100, 255);
            led_canvas_set_pixel(canvas, x, MATRIX_SIZE-1, 100, 100, 255);
        }
        for (int y = 0; y < MATRIX_SIZE; y++) {
            led_canvas_set_pixel(canvas, 0, y, 100, 100, 255);
            led_canvas_set_pixel(canvas, MATRIX_SIZE-1, y, 100, 100, 255);
        }
        
        usleep(500000); // 0.5초
        led_canvas_clear(canvas);
        usleep(200000); // 0.2초
    }
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
    options.brightness = 80;
    
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

// 그리드 라인 그리기
void drawGridLines() {
    // 가로선 그리기 (0, 8, 16, ..., 64)
    for (int i = 0; i <= BOARD_SIZE; i++) {
        int y = i * CELL_SIZE;
        for (int x = 0; x < MATRIX_SIZE; x++) {
            led_canvas_set_pixel(canvas, x, y, COLOR_GRID_R, COLOR_GRID_G, COLOR_GRID_B);
        }
    }
    
    // 세로선 그리기 (0, 8, 16, ..., 64)
    for (int i = 0; i <= BOARD_SIZE; i++) {
        int x = i * CELL_SIZE;
        for (int y = 0; y < MATRIX_SIZE; y++) {
            led_canvas_set_pixel(canvas, x, y, COLOR_GRID_R, COLOR_GRID_G, COLOR_GRID_B);
        }
    }
}

// 피스 그리기 함수
void drawPiece(int row, int col, char piece) {
    // 셀의 시작 위치 계산
    int start_x = col * CELL_SIZE + 1;  // +1은 경계선 때문
    int start_y = row * CELL_SIZE + 1;
    
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
    
    // 6x6 피스 그리기
    for (int py = 0; py < PIECE_SIZE; py++) {
        for (int px = 0; px < PIECE_SIZE; px++) {
            int x = start_x + px;
            int y = start_y + py;
            
            // 피스별 특별한 패턴 추가
            if (piece == 'R') {
                // Red 피스는 원형으로
                int cx = PIECE_SIZE / 2;
                int cy = PIECE_SIZE / 2;
                int dx = px - cx;
                int dy = py - cy;
                if (dx*dx + dy*dy <= (PIECE_SIZE/2) * (PIECE_SIZE/2)) {
                    led_canvas_set_pixel(canvas, x, y, r, g, b);
                }
            } else if (piece == 'B') {
                // Blue 피스는 다이아몬드 형태로
                int cx = PIECE_SIZE / 2;
                int cy = PIECE_SIZE / 2;
                if (abs(px - cx) + abs(py - cy) <= PIECE_SIZE/2) {
                    led_canvas_set_pixel(canvas, x, y, r, g, b);
                }
            } else if (piece == '.') {
                // 빈 공간은 작은 점으로
                if (px == PIECE_SIZE/2 && py == PIECE_SIZE/2) {
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

// LED 매트릭스에 보드 표시
void displayBoardOnMatrix() {
    if (!canvas) return;
    
    // 배경 클리어
    led_canvas_clear(canvas);
    
    // 그리드 라인 그리기
    drawGridLines();
    
    // 각 피스 그리기
    for (int row = 0; row < BOARD_SIZE; row++) {
        for (int col = 0; col < BOARD_SIZE; col++) {
            drawPiece(row, col, board[row][col]);
        }
    }
}

// 보드 상태를 콘솔에 출력
void printBoard() {
    printf("\n현재 보드 상태:\n");
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

// 보드 입력 검증
int validateBoardInput(const char* input) {
    // 정확히 64개의 문자인지 확인
    int len = strlen(input);
    if (len != BOARD_SIZE * BOARD_SIZE) {
        return 0;
    }
    
    // 각 문자가 유효한지 확인
    for (int i = 0; i < len; i++) {
        char c = input[i];
        if (c != 'R' && c != 'B' && c != '.' && c != '#') {
            return 0;
        }
    }
    
    return 1;
}

// 메인 함수
int main() {
    // 팀 이름 표시
    displayTeamName();
    
    // LED 매트릭스 초기화
    printf("LED 매트릭스 초기화 중...\n");
    if (initLEDMatrix() != 0) {
        printf("경고: LED 매트릭스를 초기화할 수 없습니다. 콘솔 모드로 실행합니다.\n");
    } else {
        printf("LED 매트릭스 초기화 완료!\n");
        // 팀 이름 애니메이션 표시
        showTeamNameOnMatrix();
    }
    
    // 보드 입력 안내
    printf("\n=== 보드 상태 입력 ===\n");
    printf("8x8 보드를 입력하세요.\n");
    printf("사용 가능한 문자:\n");
    printf("  R : Red 플레이어의 말\n");
    printf("  B : Blue 플레이어의 말\n");
    printf("  . : 빈 칸\n");
    printf("  # : 막힌 칸\n");
    printf("\n예시 입력:\n");
    printf("R......B........................................................B......R\n");
    printf("\n64개의 문자를 공백 없이 한 줄로 입력하세요:\n");
    
    // 보드 입력 받기
    char input[BOARD_SIZE * BOARD_SIZE + 10];  // 여유 공간 포함
    while (1) {
        printf("> ");
        if (fgets(input, sizeof(input), stdin) == NULL) {
            printf("입력 오류가 발생했습니다.\n");
            continue;
        }
        
        // 개행 문자 제거
        input[strcspn(input, "\n")] = 0;
        
        // 입력 검증
        if (!validateBoardInput(input)) {
            printf("잘못된 입력입니다. 정확히 64개의 유효한 문자(R,B,.,#)를 입력해주세요.\n");
            continue;
        }
        
        // 보드에 입력 저장
        for (int i = 0; i < BOARD_SIZE; i++) {
            for (int j = 0; j < BOARD_SIZE; j++) {
                board[i][j] = input[i * BOARD_SIZE + j];
            }
        }
        
        break;
    }
    
    // 보드 상태 출력
    printf("\n입력된 보드:\n");
    printBoard();
    
    // LED 매트릭스에 표시
    if (canvas) {
        printf("LED 매트릭스에 보드를 표시합니다...\n");
        displayBoardOnMatrix();
        
        // 계속 표시하기 위해 대기
        printf("종료하려면 Enter를 누르세요...\n");
        getchar();
    }
    
    // 정리
    if (matrix) {
        led_matrix_delete(matrix);
    }
    
    printf("\n프로그램을 종료합니다.\n");
    printf("Team Shannon - OctaFlip LED Display\n");
    
    return 0;
}