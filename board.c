#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <signal.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "board.h"

// Only include if building with LED support
#ifdef HAS_LED_MATRIX
#include "led-matrix-c.h"
#endif

// For network mode
#ifdef ENABLE_NETWORK_MODE
#include "cJSON.h"
#endif

// Game piece definitions
#define RED 'R'
#define BLUE 'B'
#define EMPTY '.'
#define BLOCKED '#'

// Enhanced color definitions
#define COLOR_RED_R 255
#define COLOR_RED_G 30
#define COLOR_RED_B 30

#define COLOR_BLUE_R 30
#define COLOR_BLUE_G 120
#define COLOR_BLUE_B 255

#define COLOR_EMPTY_R 0
#define COLOR_EMPTY_G 0
#define COLOR_EMPTY_B 0

#define COLOR_GRID_R 80
#define COLOR_GRID_G 80
#define COLOR_GRID_B 80

#define COLOR_BLOCKED_R 80
#define COLOR_BLOCKED_G 80
#define COLOR_BLOCKED_B 80

// Grid colors (from paste.txt)
#define COLOR_GRID_OUTER_R 50
#define COLOR_GRID_OUTER_G 50
#define COLOR_GRID_OUTER_B 50

#define COLOR_GRID_INNER_R 80
#define COLOR_GRID_INNER_G 80
#define COLOR_GRID_INNER_B 80

// Team colors for animation
#define COLOR_TEAM_R 255
#define COLOR_TEAM_G 200
#define COLOR_TEAM_B 0

#define COLOR_STAR1_R 255
#define COLOR_STAR1_G 255
#define COLOR_STAR1_B 0

#define COLOR_STAR2_R 255
#define COLOR_STAR2_G 100
#define COLOR_STAR2_B 200

// Global variables
static char current_board[BOARD_SIZE][BOARD_SIZE];
static pthread_mutex_t board_mutex = PTHREAD_MUTEX_INITIALIZER;
static int running = 1;

#ifdef HAS_LED_MATRIX
static struct RGBLedMatrix *matrix = NULL;
static struct LedCanvas *canvas = NULL;
static struct LedCanvas *offscreen_canvas = NULL;

// 5x7 대문자 폰트 (A-Z)
static const uint8_t font5x7[26][7] = {
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

// 5x7 숫자 폰트 (0-9) 추가
static const uint8_t font5x7_numbers[10][7] = {
    {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}, // 0
    {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}, // 1
    {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}, // 2
    {0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E}, // 3
    {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}, // 4
    {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E}, // 5
    {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E}, // 6
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}, // 7
    {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}, // 8
    {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C}  // 9
};
#endif

// Internal function prototypes
static void display_on_matrix(void);
static int validate_board_line(const char* line, int row_num);
static void console_print_team_banner(void);
static void console_print_game_over(int red_score, int blue_score);

#ifdef HAS_LED_MATRIX
static void draw_text_manual(const char* text, int start_x, int start_y, int r, int g, int b);
static void draw_char(char ch, int x, int y, int r, int g, int b);
static void draw_score(int score, int x, int y, int r, int g, int b);
static void draw_small_star(int cx, int cy, int r, int g, int b);
static void draw_big_star(int cx, int cy, int r, int g, int b);
static void show_team_name_animation(void);
static void show_victory_animation(int winner_r, int winner_g, int winner_b);
static void swap_canvas(void);
static void draw_grid_lines(void);
static void draw_piece(int row, int col, char piece);
#endif

#ifdef STANDALONE_BUILD
static void init_board_state(void);
static void sighandler(int sig);
#endif

// Signal handler (only for standalone)
#ifdef STANDALONE_BUILD
static void sighandler(int sig) {
    (void)sig;
    running = 0;
}
#endif

// Console team banner
static void console_print_team_banner(void) {
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

// Console game over display
static void console_print_game_over(int red_score, int blue_score) {
    printf("\n");
    printf("  ╔═══════════════════════════════════════════════════════════╗\n");
    printf("  ║                     🏁 GAME OVER! 🏁                      ║\n");
    printf("  ╠═══════════════════════════════════════════════════════════╣\n");
    printf("  ║                                                           ║\n");
    printf("  ║     Red Player   : %3d pieces                             ║\n", red_score);
    printf("  ║     Blue Player  : %3d pieces                             ║\n", blue_score);
    printf("  ║                                                           ║\n");
    
    if (red_score > blue_score) {
        printf("  ║              🎉 RED PLAYER WINS! 🎉                      ║\n");
        printf("  ║                                                           ║\n");
    } else if (blue_score > red_score) {
        printf("  ║              🎉 BLUE PLAYER WINS! 🎉                     ║\n");
        printf("  ║                                                           ║\n");
    } else {
        printf("  ║                  🤝 IT'S A DRAW! 🤝                      ║\n");
        printf("  ║                                                           ║\n");
    }
    
    printf("  ║                                                           ║\n");
    printf("  ╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    printf("  Thank you for playing! - Team Shannon 💫\n");
    printf("\n");
}

#ifdef HAS_LED_MATRIX
// Swap canvas for double buffering
static void swap_canvas(void) {
    if (matrix && offscreen_canvas) {
        offscreen_canvas = led_matrix_swap_on_vsync(matrix, offscreen_canvas);
    }
}

// 단일 문자 그리기 (글자/숫자)
static void draw_char(char ch, int x, int y, int r, int g, int b) {
    if (!offscreen_canvas) return;
    
    const uint8_t* font_data = NULL;
    
    if (ch >= 'A' && ch <= 'Z') {
        font_data = font5x7[ch - 'A'];
    } else if (ch >= '0' && ch <= '9') {
        font_data = font5x7_numbers[ch - '0'];
    } else if (ch == ' ') {
        return; // 공백은 그리지 않음
    } else {
        return; // 지원하지 않는 문자
    }
    
    if (font_data) {
        for (int row = 0; row < 7; row++) {
            uint8_t row_data = font_data[row];
            for (int col = 0; col < 5; col++) {
                if (row_data & (1 << (4 - col))) {
                    int px = x + col;
                    int py = y + row;
                    if (px >= 0 && px < MATRIX_SIZE && py >= 0 && py < MATRIX_SIZE) {
                        led_canvas_set_pixel(offscreen_canvas, px, py, r, g, b);
                    }
                }
            }
        }
    }
}

// LED 매트릭스에 글자 그리기 (개선된 버전)
static void draw_text_manual(const char* text, int start_x, int start_y, int r, int g, int b) {
    if (!offscreen_canvas) return;
    
    const int char_width = 5;
    const int spacing = 1;
    int x_offset = start_x;
    
    for (int i = 0; text[i] != '\0'; i++) {
        draw_char(text[i], x_offset, start_y, r, g, b);
        x_offset += char_width + spacing;
    }
}

// 점수 표시 (두 자리 숫자)
static void draw_score(int score, int x, int y, int r, int g, int b) {
    if (!offscreen_canvas) return;
    
    if (score >= 100) score = 99; // 최대 99까지만 표시
    
    if (score >= 10) {
        // 두 자리 수
        int tens = score / 10;
        int ones = score % 10;
        draw_char('0' + tens, x, y, r, g, b);
        draw_char('0' + ones, x + 6, y, r, g, b);
    } else {
        // 한 자리 수
        draw_char('0' + score, x + 3, y, r, g, b); // 중앙 정렬
    }
}

// 작은 별 그리기
static void draw_small_star(int cx, int cy, int r, int g, int b) {
    if (!offscreen_canvas) return;
    
    if (cx >= 0 && cx < MATRIX_SIZE && cy >= 0 && cy < MATRIX_SIZE) {
        led_canvas_set_pixel(offscreen_canvas, cx, cy, r, g, b);
    }
    if (cx-1 >= 0 && cy >= 0 && cy < MATRIX_SIZE) {
        led_canvas_set_pixel(offscreen_canvas, cx-1, cy, r, g, b);
    }
    if (cx+1 < MATRIX_SIZE && cy >= 0 && cy < MATRIX_SIZE) {
        led_canvas_set_pixel(offscreen_canvas, cx+1, cy, r, g, b);
    }
    if (cx >= 0 && cx < MATRIX_SIZE && cy-1 >= 0) {
        led_canvas_set_pixel(offscreen_canvas, cx, cy-1, r, g, b);
    }
    if (cx >= 0 && cx < MATRIX_SIZE && cy+1 < MATRIX_SIZE) {
        led_canvas_set_pixel(offscreen_canvas, cx, cy+1, r, g, b);
    }
}

// 큰 별 그리기
static void draw_big_star(int cx, int cy, int r, int g, int b) {
    if (!offscreen_canvas) return;
    
    led_canvas_set_pixel(offscreen_canvas, cx, cy, r, g, b);
    
    for (int i = 1; i <= 3; i++) {
        if (cx-i >= 0) led_canvas_set_pixel(offscreen_canvas, cx-i, cy, r, g, b);
        if (cx+i < MATRIX_SIZE) led_canvas_set_pixel(offscreen_canvas, cx+i, cy, r, g, b);
        if (cy-i >= 0) led_canvas_set_pixel(offscreen_canvas, cx, cy-i, r, g, b);
        if (cy+i < MATRIX_SIZE) led_canvas_set_pixel(offscreen_canvas, cx, cy+i, r, g, b);
    }
    
    for (int i = 1; i <= 2; i++) {
        if (cx-i >= 0 && cy-i >= 0) 
            led_canvas_set_pixel(offscreen_canvas, cx-i, cy-i, r/2, g/2, b/2);
        if (cx+i < MATRIX_SIZE && cy-i >= 0) 
            led_canvas_set_pixel(offscreen_canvas, cx+i, cy-i, r/2, g/2, b/2);
        if (cx-i >= 0 && cy+i < MATRIX_SIZE) 
            led_canvas_set_pixel(offscreen_canvas, cx-i, cy+i, r/2, g/2, b/2);
        if (cx+i < MATRIX_SIZE && cy+i < MATRIX_SIZE) 
            led_canvas_set_pixel(offscreen_canvas, cx+i, cy+i, r/2, g/2, b/2);
    }
}

// Team name animation
static void show_team_name_animation(void) {
    if (!offscreen_canvas || !matrix) return;
    
    // Animation frames
    for (int frame = 0; frame < 180 && running; frame++) {
        led_canvas_clear(offscreen_canvas);
        
        // Background gradient
        for (int y = 0; y < MATRIX_SIZE; y++) {
            int bg_intensity = (y * 30) / MATRIX_SIZE;
            for (int x = 0; x < MATRIX_SIZE; x++) {
                led_canvas_set_pixel(offscreen_canvas, x, y, 
                                   bg_intensity/3, 0, bg_intensity/2);
            }
        }
        
        // Background particles
        for (int i = 0; i < 10; i++) {
            int x = (frame * 2 + i * 7) % MATRIX_SIZE;
            int y = (frame + i * 5) % MATRIX_SIZE;
            led_canvas_set_pixel(offscreen_canvas, x, y, 50, 25, 63);
        }
        
        // TEAM SHANNON text (blinking effect)
        if (frame < 60 || (frame >= 70 && frame < 130) || frame >= 140) {
            draw_text_manual("TEAM", 18, 18, COLOR_TEAM_R, COLOR_TEAM_G, COLOR_TEAM_B);
            draw_text_manual("SHANNON", 8, 28, COLOR_TEAM_R, COLOR_TEAM_G, COLOR_TEAM_B);
        }
        
        // Rotating big stars (corners)
        int star_offset = (frame % 20) < 10 ? 0 : 1;
        draw_big_star(10 + star_offset, 10, COLOR_STAR1_R, COLOR_STAR1_G, COLOR_STAR1_B);
        draw_big_star(54 - star_offset, 10, COLOR_STAR2_R, COLOR_STAR2_G, COLOR_STAR2_B);
        draw_big_star(10 + star_offset, 54, COLOR_STAR2_R, COLOR_STAR2_G, COLOR_STAR2_B);
        draw_big_star(54 - star_offset, 54, COLOR_STAR1_R, COLOR_STAR1_G, COLOR_STAR1_B);
        
        // Moving small stars
        for (int i = 0; i < 8; i++) {
            int star_x = (frame * 2 + i * 8) % MATRIX_SIZE;
            int star_y = 5 + (i % 2) * 50;
            draw_small_star(star_x, star_y, 200, 200, 100);
        }
        
        // Rainbow frame border
        for (int x = 0; x < MATRIX_SIZE; x++) {
            int rainbow = (x + frame) % 60;
            int r = rainbow < 20 ? 255 : (rainbow < 40 ? 0 : 100);
            int g = rainbow < 20 ? 0 : (rainbow < 40 ? 255 : 100);
            int b = rainbow < 20 ? 100 : (rainbow < 40 ? 100 : 255);
            
            led_canvas_set_pixel(offscreen_canvas, x, 0, r, g, b);
            led_canvas_set_pixel(offscreen_canvas, x, 1, r/2, g/2, b/2);
            led_canvas_set_pixel(offscreen_canvas, x, MATRIX_SIZE-1, r, g, b);
            led_canvas_set_pixel(offscreen_canvas, x, MATRIX_SIZE-2, r/2, g/2, b/2);
        }
        
        for (int y = 2; y < MATRIX_SIZE-2; y++) {
            int rainbow = (y + frame) % 60;
            int r = rainbow < 20 ? 255 : (rainbow < 40 ? 0 : 100);
            int g = rainbow < 20 ? 0 : (rainbow < 40 ? 255 : 100);
            int b = rainbow < 20 ? 100 : (rainbow < 40 ? 100 : 255);
            
            led_canvas_set_pixel(offscreen_canvas, 0, y, r, g, b);
            led_canvas_set_pixel(offscreen_canvas, 1, y, r/2, g/2, b/2);
            led_canvas_set_pixel(offscreen_canvas, MATRIX_SIZE-1, y, r, g, b);
            led_canvas_set_pixel(offscreen_canvas, MATRIX_SIZE-2, y, r/2, g/2, b/2);
        }
        
        swap_canvas();
        usleep(16667); // ~60fps
    }
    
    // Final static display
    led_canvas_clear(offscreen_canvas);
    
    // Background
    for (int y = 0; y < MATRIX_SIZE; y++) {
        int bg_intensity = (y * 30) / MATRIX_SIZE;
        for (int x = 0; x < MATRIX_SIZE; x++) {
            led_canvas_set_pixel(offscreen_canvas, x, y, bg_intensity/3, 0, bg_intensity/2);
        }
    }
    
    // Text
    draw_text_manual("TEAM", 18, 18, COLOR_TEAM_R, COLOR_TEAM_G, COLOR_TEAM_B);
    draw_text_manual("SHANNON", 8, 28, COLOR_TEAM_R, COLOR_TEAM_G, COLOR_TEAM_B);
    
    // Stars
    draw_big_star(10, 10, COLOR_STAR1_R, COLOR_STAR1_G, COLOR_STAR1_B);
    draw_big_star(54, 10, COLOR_STAR2_R, COLOR_STAR2_G, COLOR_STAR2_B);
    draw_big_star(10, 54, COLOR_STAR2_R, COLOR_STAR2_G, COLOR_STAR2_B);
    draw_big_star(54, 54, COLOR_STAR1_R, COLOR_STAR1_G, COLOR_STAR1_B);
    
    // Frame
    for (int x = 0; x < MATRIX_SIZE; x++) {
        led_canvas_set_pixel(offscreen_canvas, x, 0, 100, 100, 255);
        led_canvas_set_pixel(offscreen_canvas, x, MATRIX_SIZE-1, 100, 100, 255);
    }
    for (int y = 0; y < MATRIX_SIZE; y++) {
        led_canvas_set_pixel(offscreen_canvas, 0, y, 100, 100, 255);
        led_canvas_set_pixel(offscreen_canvas, MATRIX_SIZE-1, y, 100, 100, 255);
    }
    
    swap_canvas();
    sleep(1);
}

// Victory animation
static void show_victory_animation(int winner_r, int winner_g, int winner_b) {
    if (!offscreen_canvas || !matrix) return;
    
    // Fireworks effect
    for (int burst = 0; burst < 5 && running; burst++) {
        int cx = rand() % (MATRIX_SIZE - 20) + 10;
        int cy = rand() % (MATRIX_SIZE - 20) + 10;
        
        for (int radius = 0; radius < 20 && running; radius++) {
            led_canvas_clear(offscreen_canvas);
            
            // Draw expanding circle
            for (int angle = 0; angle < 360; angle += 10) {
                int x = cx + radius * cos(angle * M_PI / 180);
                int y = cy + radius * sin(angle * M_PI / 180);
                
                if (x >= 0 && x < MATRIX_SIZE && y >= 0 && y < MATRIX_SIZE) {
                    int fade = 255 - (radius * 12);
                    if (fade > 0) {
                        led_canvas_set_pixel(offscreen_canvas, x, y,
                            winner_r * fade / 255,
                            winner_g * fade / 255,
                            winner_b * fade / 255);
                    }
                }
            }
            
            // Add sparkles
            for (int i = 0; i < 5; i++) {
                int sx = cx + (rand() % (radius * 2 + 1)) - radius;
                int sy = cy + (rand() % (radius * 2 + 1)) - radius;
                if (sx >= 0 && sx < MATRIX_SIZE && sy >= 0 && sy < MATRIX_SIZE) {
                    led_canvas_set_pixel(offscreen_canvas, sx, sy, 255, 255, 255);
                }
            }
            
            swap_canvas();
            usleep(50000);
        }
    }
}

// Draw grid lines (from paste.txt approach)
static void draw_grid_lines(void) {
    if (!offscreen_canvas) return;
    
    // Grid positions: 0,7,8,15,16,23,24,31,32,39,40,47,48,55,56,63
    int grid_positions[] = {0, 7, 8, 15, 16, 23, 24, 31, 32, 39, 40, 47, 48, 55, 56, 63};
    int num_positions = sizeof(grid_positions) / sizeof(grid_positions[0]);
    
    // Draw horizontal grid lines
    for (int i = 0; i < num_positions; i++) {
        int y = grid_positions[i];
        
        // Edge lines (0, 63) are darker
        if (y == 0 || y == 63) {
            for (int x = 0; x < MATRIX_SIZE; x++) {
                led_canvas_set_pixel(offscreen_canvas, x, y, COLOR_GRID_OUTER_R, COLOR_GRID_OUTER_G, COLOR_GRID_OUTER_B);
            }
        } else {
            // Inner lines are brighter
            for (int x = 0; x < MATRIX_SIZE; x++) {
                led_canvas_set_pixel(offscreen_canvas, x, y, COLOR_GRID_INNER_R, COLOR_GRID_INNER_G, COLOR_GRID_INNER_B);
            }
        }
    }
    
    // Draw vertical grid lines
    for (int i = 0; i < num_positions; i++) {
        int x = grid_positions[i];
        
        // Edge lines (0, 63) are darker
        if (x == 0 || x == 63) {
            for (int y = 0; y < MATRIX_SIZE; y++) {
                led_canvas_set_pixel(offscreen_canvas, x, y, COLOR_GRID_OUTER_R, COLOR_GRID_OUTER_G, COLOR_GRID_OUTER_B);
            }
        } else {
            // Inner lines are brighter
            for (int y = 0; y < MATRIX_SIZE; y++) {
                led_canvas_set_pixel(offscreen_canvas, x, y, COLOR_GRID_INNER_R, COLOR_GRID_INNER_G, COLOR_GRID_INNER_B);
            }
        }
    }
}

// Draw piece (from paste.txt approach - 6x6 full fill)
static void draw_piece(int row, int col, char piece) {
    if (!offscreen_canvas || piece == EMPTY) return;
    
    // Cell start positions
    int cell_x_starts[] = {1, 9, 17, 25, 33, 41, 49, 57};
    int cell_y_starts[] = {1, 9, 17, 25, 33, 41, 49, 57};
    
    int start_x = cell_x_starts[col];
    int start_y = cell_y_starts[row];
    
    int r, g, b;
    switch(piece) {
        case RED:
            r = COLOR_RED_R;
            g = COLOR_RED_G;
            b = COLOR_RED_B;
            break;
        case BLUE:
            r = COLOR_BLUE_R;
            g = COLOR_BLUE_G;
            b = COLOR_BLUE_B;
            break;
        case BLOCKED:
            r = COLOR_BLOCKED_R;
            g = COLOR_BLOCKED_G;
            b = COLOR_BLOCKED_B;
            break;
        default:
            return;
    }
    
    // Fill 6x6 area
    for (int y = 0; y < PIECE_SIZE; y++) {
        for (int x = 0; x < PIECE_SIZE; x++) {
            led_canvas_set_pixel(offscreen_canvas, start_x + x, start_y + y, r, g, b);
        }
    }
}
#endif

// Initialize LED display (from paste.txt approach)
int init_led_display(void) {
#ifdef HAS_LED_MATRIX
    struct RGBLedMatrixOptions options;
    struct RGBLedRuntimeOptions rt_options;
    
    memset(&options, 0, sizeof(options));
    memset(&rt_options, 0, sizeof(rt_options));
    
    // Hardware options (matching paste.txt)
    options.rows = 64;
    options.cols = 64;
    options.chain_length = 1;
    options.parallel = 1;
    options.hardware_mapping = "regular";
    options.brightness = 80;
    options.disable_hardware_pulsing = 1;  // Critical from paste.txt
    
    // Runtime options (matching paste.txt)
    rt_options.gpio_slowdown = 4;
    rt_options.drop_privileges = 1;  // Changed from original board.c to match paste.txt
    
    fprintf(stderr, "Creating LED matrix with settings from paste.txt...\n");
    matrix = led_matrix_create_from_options_and_rt_options(&options, &rt_options);
    
    if (matrix == NULL) {
        fprintf(stderr, "Error: Failed to create LED matrix\n");
        fprintf(stderr, "Make sure to run with sudo\n");
        return -1;
    }
    
    fprintf(stderr, "LED matrix created successfully\n");
    
    // Get canvas
    canvas = led_matrix_get_canvas(matrix);
    offscreen_canvas = led_matrix_create_offscreen_canvas(matrix);
    
    if (!canvas || !offscreen_canvas) {
        fprintf(stderr, "Error: Failed to create canvas\n");
        led_matrix_delete(matrix);
        matrix = NULL;
        return -1;
    }
    
    // Clear display
    led_canvas_clear(canvas);
    led_canvas_clear(offscreen_canvas);
    
    fprintf(stderr, "LED display initialized successfully\n");
    return 0;
#else
    return 0;
#endif
}

// Cleanup LED display
void cleanup_led_display(void) {
#ifdef HAS_LED_MATRIX
    if (matrix) {
        fprintf(stderr, "Cleaning up LED display...\n");
        
        // Clear display before shutdown
        if (canvas) {
            led_canvas_clear(canvas);
        }
        if (offscreen_canvas) {
            led_canvas_clear(offscreen_canvas);
            swap_canvas();
        }
        
        led_matrix_delete(matrix);
        matrix = NULL;
        canvas = NULL;
        offscreen_canvas = NULL;
        
        fprintf(stderr, "LED display cleaned up\n");
    }
#endif
}

// Render board to LED matrix
void render_board_to_led(char board[BOARD_SIZE][BOARD_SIZE]) {
    pthread_mutex_lock(&board_mutex);
    
    // Copy board state
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            current_board[i][j] = board[i][j];
        }
    }
    
    // Display on matrix
    display_on_matrix();
    
    pthread_mutex_unlock(&board_mutex);
}

// Show team name on LED
void show_team_name_on_led(void) {
#ifdef HAS_LED_MATRIX
    if (!offscreen_canvas) return;
    
    show_team_name_animation();
#else
    console_print_team_banner();
#endif
}

// Game over animation
void show_game_over_animation(int red_score, int blue_score) {
#ifdef HAS_LED_MATRIX
    if (!offscreen_canvas) return;
    
    // Determine winner color
    int winner_r, winner_g, winner_b;
    
    if (red_score > blue_score) {
        winner_r = COLOR_RED_R;
        winner_g = COLOR_RED_G;
        winner_b = COLOR_RED_B;
    } else if (blue_score > red_score) {
        winner_r = COLOR_BLUE_R;
        winner_g = COLOR_BLUE_G;
        winner_b = COLOR_BLUE_B;
    } else {
        winner_r = 150;
        winner_g = 150;
        winner_b = 150;
    }
    
    // Victory animation
    show_victory_animation(winner_r, winner_g, winner_b);
    
    // Display result text
    led_canvas_clear(offscreen_canvas);
    
    // Background with winner color
    for (int y = 0; y < MATRIX_SIZE; y++) {
        for (int x = 0; x < MATRIX_SIZE; x++) {
            led_canvas_set_pixel(offscreen_canvas, x, y, 
                winner_r / 8, winner_g / 8, winner_b / 8);
        }
    }
    
    // Winner text - 두 줄로 표시
    if (red_score > blue_score) {
        draw_text_manual("RED", 22, 10, 255, 255, 255);
        draw_text_manual("WINS", 20, 20, 255, 255, 255);
    } else if (blue_score > red_score) {
        draw_text_manual("BLUE", 19, 10, 255, 255, 255);
        draw_text_manual("WINS", 20, 20, 255, 255, 255);
    } else {
        draw_text_manual("DRAW", 20, 15, 255, 255, 255);
    }
    
    // Red score (왼쪽)
    draw_text_manual("R", 10, 35, COLOR_RED_R, COLOR_RED_G, COLOR_RED_B);
    draw_score(red_score, 20, 35, 255, 255, 255);
    
    // Blue score (오른쪽) 
    draw_text_manual("B", 38, 35, COLOR_BLUE_R, COLOR_BLUE_G, COLOR_BLUE_B);
    draw_score(blue_score, 48, 35, 255, 255, 255);
    
    // Frame border
    for (int x = 0; x < MATRIX_SIZE; x++) {
        led_canvas_set_pixel(offscreen_canvas, x, 0, 255, 255, 255);
        led_canvas_set_pixel(offscreen_canvas, x, MATRIX_SIZE-1, 255, 255, 255);
    }
    for (int y = 0; y < MATRIX_SIZE; y++) {
        led_canvas_set_pixel(offscreen_canvas, 0, y, 255, 255, 255);
        led_canvas_set_pixel(offscreen_canvas, MATRIX_SIZE-1, y, 255, 255, 255);
    }
    
    swap_canvas();
    sleep(5);  // 5초간 표시
#else
    console_print_game_over(red_score, blue_score);
#endif
}

// Internal: Display board on matrix
static void display_on_matrix(void) {
#ifdef HAS_LED_MATRIX
    if (!offscreen_canvas) return;
    
    // Clear canvas
    led_canvas_clear(offscreen_canvas);
    
    // Draw grid first
    draw_grid_lines();
    
    // Draw all pieces
    for (int row = 0; row < BOARD_SIZE; row++) {
        for (int col = 0; col < BOARD_SIZE; col++) {
            draw_piece(row, col, current_board[row][col]);
        }
    }
    
    // Swap buffers
    swap_canvas();
#else
    // Console display
    printf("\n  1 2 3 4 5 6 7 8\n");
    for (int i = 0; i < BOARD_SIZE; i++) {
        printf("%d ", i + 1);
        for (int j = 0; j < BOARD_SIZE; j++) {
            printf("%c ", current_board[i][j]);
        }
        printf("\n");
    }
    printf("\nRed (R): 🔴  Blue (B): 🔵  Empty (.): ⚪  Blocked (#): ⬛\n");
#endif
}

#ifdef STANDALONE_BUILD
// Internal: Initialize board state
static void init_board_state(void) {
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            current_board[i][j] = EMPTY;
        }
    }
    current_board[0][0] = RED;
    current_board[0][7] = BLUE;
    current_board[7][0] = BLUE;
    current_board[7][7] = RED;
}
#endif

// Internal: Validate board line
static int validate_board_line(const char* line, int row_num) {
    int len = strlen(line);
    
    // Remove newline
    while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
        len--;
    }
    
    if (len != BOARD_SIZE) {
        fprintf(stderr, "Error: Row %d must have exactly %d characters (found %d)\n", 
                row_num, BOARD_SIZE, len);
        return 0;
    }
    
    for (int i = 0; i < BOARD_SIZE; i++) {
        if (line[i] != RED && line[i] != BLUE && 
            line[i] != EMPTY && line[i] != BLOCKED) {
            fprintf(stderr, "Error: Invalid character '%c' at position %d in row %d\n", 
                    line[i], i+1, row_num);
            return 0;
        }
    }
    
    return 1;
}

// Standalone mode - read from stdin (FOR GRADING)
int run_board_standalone(void) {
    char line[BOARD_SIZE + 10];
    
    console_print_team_banner();
    
    // Show input instructions
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
    printf("Enter 8 rows (8 characters each):\n");
    
    // Read 8 lines
    for (int i = 0; i < BOARD_SIZE; i++) {
        if (fgets(line, sizeof(line), stdin) == NULL) {
            fprintf(stderr, "Error reading line %d\n", i + 1);
            return 1;
        }
        
        // Remove newline
        int len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';
        
        // Validate line
        if (!validate_board_line(line, i + 1)) {
            return 1;
        }
        
        // Copy to board
        for (int j = 0; j < BOARD_SIZE; j++) {
            current_board[i][j] = line[j];
        }
    }
    
    printf("\nBoard loaded successfully!\n");
    
#ifdef HAS_LED_MATRIX
    // Show team name animation first
    printf("Displaying team name animation...\n");
    show_team_name_animation();
#endif
    
    // Display on LED matrix
    printf("Displaying board state...\n");
    display_on_matrix();
    
    // Keep display on for a while
    printf("\nBoard displayed. Showing for 10 seconds...\n");
    sleep(10);
    
    return 0;
}

// Network server mode (if enabled)
int run_board_network_server(void) {
#ifdef ENABLE_NETWORK_MODE
    printf("Network server mode...\n");
    // Implementation here if needed
    return 0;
#else
    fprintf(stderr, "Network mode not enabled in this build\n");
    return 1;
#endif
}

// Manual input mode
int run_board_manual_input(void) {
    console_print_team_banner();
    
    printf("\n=== Manual Input Mode ===\n");
    printf("Enter board state (8 lines, 8 characters each):\n");
    printf("Use: R=Red, B=Blue, .=Empty, #=Blocked\n\n");
    
    char line[BOARD_SIZE + 10];
    
    for (int i = 0; i < BOARD_SIZE; i++) {
        printf("Row %d: ", i + 1);
        
        if (fgets(line, sizeof(line), stdin) == NULL) {
            fprintf(stderr, "Error reading input\n");
            return 1;
        }
        
        // Remove newline
        int len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';
        
        // Validate and copy
        if (!validate_board_line(line, i + 1)) {
            i--; // Retry this line
            continue;
        }
        
        for (int j = 0; j < BOARD_SIZE; j++) {
            current_board[i][j] = line[j];
        }
    }
    
    printf("\nBoard loaded successfully!\n");
    
#ifdef HAS_LED_MATRIX
    // Show team name animation
    show_team_name_animation();
#endif
    
    // Display on LED matrix
    display_on_matrix();
    
    printf("\nPress Enter to exit...\n");
    getchar();
    
    return 0;
}

// Main function - for standalone execution
#ifdef STANDALONE_BUILD
int main(int argc, char *argv[]) {
    int ret = 0;
    
    // Signal handling
    signal(SIGINT, sighandler);
    signal(SIGTERM, sighandler);
    
    // Show startup message
    fprintf(stderr, "=== OctaFlip LED Display v3.0 ===\n");
    fprintf(stderr, "Team Shannon\n");
    fprintf(stderr, "Initializing...\n\n");
    
    // Initialize LED display
    if (init_led_display() != 0) {
        fprintf(stderr, "Warning: LED matrix initialization failed\n");
        fprintf(stderr, "Continuing with console display only\n");
    } else {
        fprintf(stderr, "LED matrix initialized successfully\n");
    }
    
    // Show team name
    show_team_name_on_led();
    
    // Initialize board
    init_board_state();
    
    if (argc == 1) {
        // Default: read from stdin (for grading) - Assignment 3 requirement
        ret = run_board_standalone();
    } else if (argc > 1) {
        if (strcmp(argv[1], "-network") == 0) {
            ret = run_board_network_server();
        } else if (strcmp(argv[1], "-manual") == 0) {
            ret = run_board_manual_input();
        } else if (strcmp(argv[1], "-help") == 0) {
            printf("Usage: %s [-network | -manual | -help]\n", argv[0]);
            printf("  Default   : Read 8 lines from stdin (for grading)\n");
            printf("  -manual   : Interactive input mode\n");
            printf("  -network  : Network server mode (if enabled)\n");
            printf("  -help     : Show this help message\n");
            ret = 0;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[1]);
            fprintf(stderr, "Usage: %s [-network | -manual | -help]\n", argv[0]);
            fprintf(stderr, "Default: read 8 lines from stdin\n");
            ret = 1;
        }
    }
    
    // Cleanup
    cleanup_led_display();
    
    fprintf(stderr, "\nProgram finished\n");
    
    return ret;
}
#endif