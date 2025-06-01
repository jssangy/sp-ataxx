#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "led-matrix-c.h"

#define BOARD_SIZE 8
#define CELL_SIZE 8      // 각 셀은 8x8 픽셀
#define PIECE_SIZE 6     // 내부 피스는 6x6 픽셀
#define MATRIX_SIZE 64   // 전체 매트릭스 크기

// 색상 정의
#define COLOR_RED_R 255
#define COLOR_RED_G 0
#define COLOR_RED_B 0

#define COLOR_BLUE_R 0
#define COLOR_BLUE_G 100
#define COLOR_BLUE_B 255

#define COLOR_EMPTY_R 20
#define COLOR_EMPTY_G 20
#define COLOR_EMPTY_B 20

#define COLOR_BLOCKED_R 100
#define COLOR_BLOCKED_G 50
#define COLOR_BLOCKED_B 0

#define COLOR_GRID_R 80
#define COLOR_GRID_G 80
#define COLOR_GRID_B 80

#define COLOR_BG_R 0
#define COLOR_BG_G 0
#define COLOR_BG_B 0

// 팀 이름 색상
#define COLOR_TEAM_R 255
#define COLOR_TEAM_G 215
#define COLOR_TEAM_B 0

// 게임 보드 전역 변수
char board[BOARD_SIZE][BOARD_SIZE];

// LED Matrix 관련 전역 변수
struct RGBLedMatrix *matrix = NULL;
struct LedCanvas *canvas = NULL;

// 사용자 정의 문자열 관련 함수들
int my_strlen(const char* str) {
    int len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

int my_strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

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
    
    // "TEAM SHANNON" 텍스트를 중앙에 표시
    const char* text = "TEAM SHANNON";
    int text_len = my_strlen(text);
    
    // 텍스트를 여러 줄로 표시
    const char* team = "TEAM";
    const char* shannon = "SHANNON";
    
    // 깜빡이는 효과와 함께 표시
    for (int frame = 0; frame < 5; frame++) {
        led_canvas_clear(canvas);
        
        // 프레임 그리기
        for (int i = 0; i < MATRIX_SIZE; i++) {
            // 상단 프레임
            led_canvas_set_pixel(canvas, i, 0, 100, 100, 255);
            led_canvas_set_pixel(canvas, i, 1, 100, 100, 255);
            // 하단 프레임
            led_canvas_set_pixel(canvas, i, MATRIX_SIZE-2, 100, 100, 255);
            led_canvas_set_pixel(canvas, i, MATRIX_SIZE-1, 100, 100, 255);
            // 좌측 프레임
            led_canvas_set_pixel(canvas, 0, i, 100, 100, 255);
            led_canvas_set_pixel(canvas, 1, i, 100, 100, 255);
            // 우측 프레임
            led_canvas_set_pixel(canvas, MATRIX_SIZE-2, i, 100, 100, 255);
            led_canvas_set_pixel(canvas, MATRIX_SIZE-1, i, 100, 100, 255);
        }
        
        // TEAM 텍스트 (위쪽)
        int y1 = 20;
        for (int i = 0; i < 4; i++) {
            int x = 15 + i * 10;
            // 각 문자를 5x7 블록으로 표시
            for (int px = 0; px < 5; px++) {
                for (int py = 0; py < 7; py++) {
                    if (frame % 2 == 0) {  // 깜빡임 효과
                        led_canvas_set_pixel(canvas, x + px, y1 + py, 
                                           COLOR_TEAM_R, COLOR_TEAM_G, COLOR_TEAM_B);
                    }
                }
            }
        }
        
        // SHANNON 텍스트 (아래쪽)
        int y2 = 35;
        for (int i = 0; i < 7; i++) {  // SHANNON은 7글자
            int x = 5 + i * 8;
            // 각 문자를 5x7 블록으로 표시
            for (int px = 0; px < 5; px++) {
                for (int py = 0; py < 7; py++) {
                    if (frame % 2 == 0) {  // 깜빡임 효과
                        led_canvas_set_pixel(canvas, x + px, y2 + py, 
                                           COLOR_TEAM_R - i*20, COLOR_TEAM_G, COLOR_TEAM_B + i*20);
                    }
                }
            }
        }
        
        // 장식 효과
        if (frame % 2 == 0) {
            // 별 효과
            for (int i = 0; i < 10; i++) {
                int x = rand() % MATRIX_SIZE;
                int y = rand() % MATRIX_SIZE;
                led_canvas_set_pixel(canvas, x, y, 255, 255, 255);
            }
        }
        
        usleep(400000); // 0.4초
    }
    
    // 마지막에 전체 화면 플래시
    led_canvas_fill(canvas, 255, 255, 255);
    usleep(100000);
    led_canvas_clear(canvas);
    usleep(200000);
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

// 셀 그리기 함수 (8x8 픽셀, 내부 6x6은 색상, 나머지는 테두리)
void drawCell(int row, int col, char piece) {
    // 셀의 시작 위치 계산
    int start_x = col * CELL_SIZE;
    int start_y = row * CELL_SIZE;
    
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
    
    // 8x8 픽셀 그리기
    for (int py = 0; py < CELL_SIZE; py++) {
        for (int px = 0; px < CELL_SIZE; px++) {
            int x = start_x + px;
            int y = start_y + py;
            
            // 테두리인지 확인 (첫 번째와 마지막 픽셀)
            if (px == 0 || px == CELL_SIZE-1 || py == 0 || py == CELL_SIZE-1) {
                // 테두리는 그리드 색상으로
                led_canvas_set_pixel(canvas, x, y, COLOR_GRID_R, COLOR_GRID_G, COLOR_GRID_B);
            } else {
                // 내부 6x6 영역
                if (piece == 'R') {
                    // Red 피스는 원형으로
                    int cx = 3;
                    int cy = 3;
                    int dx = (px - 1) - cx;
                    int dy = (py - 1) - cy;
                    if (dx*dx + dy*dy <= 9) {  // 반지름 3
                        led_canvas_set_pixel(canvas, x, y, r, g, b);
                    } else {
                        led_canvas_set_pixel(canvas, x, y, COLOR_BG_R, COLOR_BG_G, COLOR_BG_B);
                    }
                } else if (piece == 'B') {
                    // Blue 피스는 사각형으로
                    led_canvas_set_pixel(canvas, x, y, r, g, b);
                } else if (piece == '.') {
                    // 빈 공간은 중앙에 작은 점
                    if ((px == 3 || px == 4) && (py == 3 || py == 4)) {
                        led_canvas_set_pixel(canvas, x, y, r, g, b);
                    } else {
                        led_canvas_set_pixel(canvas, x, y, COLOR_BG_R, COLOR_BG_G, COLOR_BG_B);
                    }
                } else if (piece == '#') {
                    // 막힌 공간은 X 표시
                    if ((px == py) || (px == CELL_SIZE - 1 - py)) {
                        led_canvas_set_pixel(canvas, x, y, r, g, b);
                    } else {
                        led_canvas_set_pixel(canvas, x, y, COLOR_BG_R, COLOR_BG_G, COLOR_BG_B);
                    }
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
    
    // 각 셀 그리기 (테두리 포함)
    for (int row = 0; row < BOARD_SIZE; row++) {
        for (int col = 0; col < BOARD_SIZE; col++) {
            drawCell(row, col, board[row][col]);
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

// 입력 버퍼 비우기
void clearInputBuffer() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

// 메뉴 표시
void displayMenu() {
    printf("\n===== OctaFlip LED Display Menu =====\n");
    printf("1. 한 줄로 입력 (64개 문자)\n");
    printf("2. 8줄로 입력 (줄별로)\n");
    printf("3. 붙여넣기 모드 (8줄 한번에)\n");
    printf("4. 예제 보드 사용\n");
    printf("0. 종료\n");
    printf("선택: ");
}

// 한 줄 입력 모드
int inputSingleLine() {
    printf("\n64개의 문자를 공백 없이 한 줄로 입력하세요:\n");
    printf("예시: R......B........................................................B......R\n");
    printf("> ");
    
    char input[BOARD_SIZE * BOARD_SIZE + 10];
    if (fgets(input, sizeof(input), stdin) == NULL) {
        printf("입력 오류가 발생했습니다.\n");
        return 0;
    }
    
    // 개행 문자 제거
    input[strcspn(input, "\n")] = 0;
    
    // 길이 확인
    if (my_strlen(input) != BOARD_SIZE * BOARD_SIZE) {
        printf("잘못된 입력입니다. 정확히 64개의 문자를 입력해주세요.\n");
        return 0;
    }
    
    // 유효성 검사
    for (int i = 0; i < BOARD_SIZE * BOARD_SIZE; i++) {
        char c = input[i];
        if (c != 'R' && c != 'B' && c != '.' && c != '#') {
            printf("잘못된 문자 '%c'가 포함되어 있습니다.\n", c);
            return 0;
        }
    }
    
    // 보드에 저장
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            board[i][j] = input[i * BOARD_SIZE + j];
        }
    }
    
    return 1;
}

// 줄별 입력 모드
int inputLineByLine() {
    printf("\n8x8 보드를 한 줄씩 입력하세요 (각 줄은 8개 문자):\n");
    
    char line[20];
    for (int i = 0; i < BOARD_SIZE; i++) {
        printf("줄 %d: ", i + 1);
        
        if (fgets(line, sizeof(line), stdin) == NULL) {
            printf("입력 오류가 발생했습니다.\n");
            return 0;
        }
        
        // 개행 문자 제거
        line[strcspn(line, "\n")] = 0;
        
        // 길이 확인
        if (my_strlen(line) != BOARD_SIZE) {
            printf("각 줄은 정확히 8개의 문자여야 합니다.\n");
            return 0;
        }
        
        // 유효성 검사 및 저장
        for (int j = 0; j < BOARD_SIZE; j++) {
            char c = line[j];
            if (c != 'R' && c != 'B' && c != '.' && c != '#') {
                printf("잘못된 문자 '%c'가 포함되어 있습니다.\n", c);
                return 0;
            }
            board[i][j] = c;
        }
    }
    
    return 1;
}

// 붙여넣기 모드
int inputPasteMode() {
    printf("\n=== 붙여넣기 모드 ===\n");
    printf("8줄을 한번에 붙여넣으세요:\n");
    printf("예시:\n");
    printf("R......B\n");
    printf("........\n");
    printf("........\n");
    printf("........\n");
    printf("........\n");
    printf("........\n");
    printf("........\n");
    printf("B......R\n");
    printf("\n아래에 붙여넣으세요:\n");
    printf("--------------------\n");
    
    char line[20];
    for (int i = 0; i < BOARD_SIZE; i++) {
        if (fgets(line, sizeof(line), stdin) == NULL) {
            printf("입력 오류가 발생했습니다.\n");
            return 0;
        }
        
        // 개행 문자 제거
        line[strcspn(line, "\n")] = 0;
        
        // 길이 확인
        if (my_strlen(line) != BOARD_SIZE) {
            printf("줄 %d가 8개 문자가 아닙니다.\n", i + 1);
            return 0;
        }
        
        // 유효성 검사 및 저장
        for (int j = 0; j < BOARD_SIZE; j++) {
            char c = line[j];
            if (c != 'R' && c != 'B' && c != '.' && c != '#') {
                printf("줄 %d에 잘못된 문자 '%c'가 있습니다.\n", i + 1, c);
                return 0;
            }
            board[i][j] = c;
        }
    }
    
    return 1;
}

// 예제 보드 설정
void setExampleBoard() {
    // 초기 상태 설정
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            board[i][j] = '.';
        }
    }
    
    // 모서리에 말 배치
    board[0][0] = 'R';
    board[0][7] = 'B';
    board[7][0] = 'B';
    board[7][7] = 'R';
    
    printf("\n예제 보드가 설정되었습니다.\n");
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
    
    // 보드 입력
    int validInput = 0;
    while (!validInput) {
        displayMenu();
        
        int choice;
        if (scanf("%d", &choice) != 1) {
            clearInputBuffer();
            printf("잘못된 입력입니다.\n");
            continue;
        }
        clearInputBuffer();
        
        switch (choice) {
            case 1:
                validInput = inputSingleLine();
                break;
            case 2:
                validInput = inputLineByLine();
                break;
            case 3:
                validInput = inputPasteMode();
                break;
            case 4:
                setExampleBoard();
                validInput = 1;
                break;
            case 0:
                printf("\n프로그램을 종료합니다.\n");
                if (matrix) {
                    led_matrix_delete(matrix);
                }
                return 0;
            default:
                printf("잘못된 선택입니다.\n");
        }
    }
    
    // 보드 상태 출력
    printf("\n입력된 보드:\n");
    printBoard();
    
    // LED 매트릭스에 표시
    if (canvas) {
        printf("LED 매트릭스에 보드를 표시합니다...\n");
        displayBoardOnMatrix();
        
        // 게임 정보 표시
        int red_count = 0, blue_count = 0, empty_count = 0, blocked_count = 0;
        for (int i = 0; i < BOARD_SIZE; i++) {
            for (int j = 0; j < BOARD_SIZE; j++) {
                switch(board[i][j]) {
                    case 'R': red_count++; break;
                    case 'B': blue_count++; break;
                    case '.': empty_count++; break;
                    case '#': blocked_count++; break;
                }
            }
        }
        
        printf("\n=== 보드 상태 ===\n");
        printf("Red 말: %d개\n", red_count);
        printf("Blue 말: %d개\n", blue_count);
        printf("빈 칸: %d개\n", empty_count);
        printf("막힌 칸: %d개\n", blocked_count);
        
        // 계속 표시하기 위해 대기
        printf("\n종료하려면 Enter를 누르세요...\n");
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