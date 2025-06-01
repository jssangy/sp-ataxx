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

// Color definitions
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

// Team name colors (gradient effect)
#define COLOR_TEAM1_R 255
#define COLOR_TEAM1_G 100
#define COLOR_TEAM1_B 0

#define COLOR_TEAM2_R 255
#define COLOR_TEAM2_G 200
#define COLOR_TEAM2_B 0

// Global game board
char board[BOARD_SIZE][BOARD_SIZE];

// LED Matrix global variables
struct RGBLedMatrix *matrix = NULL;
struct LedCanvas *canvas = NULL;
struct LedFont *font = NULL;

// Display team name in console
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

// Show team name on LED Matrix with animation
void showTeamNameOnMatrix() {
    if (!canvas) return;
    
    // Blinking effect - flash 3 times
    for (int blink = 0; blink < 3; blink++) {
        led_canvas_clear(canvas);
        
        // Draw "TEAM SHANNON" using simple pixel blocks
        const char* text = "TEAM SHANNON";
        int text_len = strlen(text);
        
        // Simple character rendering (5x7 pixel blocks per character)
        for (int i = 0; i < text_len; i++) {
            int x_offset = 3 + i * 5;
            int y_offset = 28;
            
            // Different colors for TEAM and SHANNON
            int r = (i < 4) ? 255 : 255 - (i - 4) * 20;
            int g = (i < 4) ? 100 : 200;
            int b = 0;
            
            // Draw simple block letters
            if (text[i] != ' ') {
                for (int px = 0; px < 4; px++) {
                    for (int py = 0; py < 7; py++) {
                        led_canvas_set_pixel(canvas, x_offset + px, y_offset + py, r, g, b);
                    }
                }
            }
        }
        
        // Decorative frame
        for (int x = 0; x < MATRIX_SIZE; x++) {
            led_canvas_set_pixel(canvas, x, 0, 100, 100, 255);
            led_canvas_set_pixel(canvas, x, MATRIX_SIZE-1, 100, 100, 255);
        }
        for (int y = 0; y < MATRIX_SIZE; y++) {
            led_canvas_set_pixel(canvas, 0, y, 100, 100, 255);
            led_canvas_set_pixel(canvas, MATRIX_SIZE-1, y, 100, 100, 255);
        }
        
        usleep(500000); // 0.5 seconds on
        led_canvas_clear(canvas);
        usleep(200000); // 0.2 seconds off
    }
    
    // Keep "TEAM SHANNON" displayed
    led_canvas_clear(canvas);
    
    // Final display of team name
    const char* text = "TEAM SHANNON";
    int text_len = strlen(text);
    
    for (int i = 0; i < text_len; i++) {
        int x_offset = 3 + i * 5;
        int y_offset = 28;
        
        int r = (i < 4) ? 255 : 255 - (i - 4) * 20;
        int g = (i < 4) ? 100 : 200;
        int b = 0;
        
        if (text[i] != ' ') {
            for (int px = 0; px < 4; px++) {
                for (int py = 0; py < 7; py++) {
                    led_canvas_draw_text(canvas, x_offset + px, y_offset + py, r, g, b);
                }
            }
        }
    }
    
    // Frame
    for (int x = 0; x < MATRIX_SIZE; x++) {
        led_canvas_set_pixel(canvas, x, 0, 100, 100, 255);
        led_canvas_set_pixel(canvas, x, MATRIX_SIZE-1, 100, 100, 255);
    }
    for (int y = 0; y < MATRIX_SIZE; y++) {
        led_canvas_set_pixel(canvas, 0, y, 100, 100, 255);
        led_canvas_set_pixel(canvas, MATRIX_SIZE-1, y, 100, 100, 255);
    }
    
    // Keep displayed for 2 seconds
    usleep(2000000);
}

// Initialize LED Matrix
int initLEDMatrix() {
    struct RGBLedMatrixOptions options;
    struct RGBLedRuntimeOptions rt_options;
    
    memset(&options, 0, sizeof(options));
    memset(&rt_options, 0, sizeof(rt_options));
    
    // 64x64 matrix configuration
    options.rows = 64;
    options.cols = 64;
    options.chain_length = 1;
    options.parallel = 1;
    options.hardware_mapping = "regular";
    options.brightness = 80;
    
    // hardware pulse 비활성화 (오류 해결)
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

// Draw grid lines
void drawGridLines() {
    // Draw horizontal lines (0, 8, 16, ..., 64)
    for (int i = 0; i <= BOARD_SIZE; i++) {
        int y = i * CELL_SIZE;
        for (int x = 0; x < MATRIX_SIZE; x++) {
            led_canvas_set_pixel(canvas, x, y, COLOR_GRID_R, COLOR_GRID_G, COLOR_GRID_B);
        }
    }
    
    // Draw vertical lines (0, 8, 16, ..., 64)
    for (int i = 0; i <= BOARD_SIZE; i++) {
        int x = i * CELL_SIZE;
        for (int y = 0; y < MATRIX_SIZE; y++) {
            led_canvas_set_pixel(canvas, x, y, COLOR_GRID_R, COLOR_GRID_G, COLOR_GRID_B);
        }
    }
}

// Draw piece function
void drawPiece(int row, int col, char piece) {
    // Calculate cell starting position
    int start_x = col * CELL_SIZE + 1;  // +1 for border
    int start_y = row * CELL_SIZE + 1;
    
    // Determine color
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
    
    // Draw 6x6 piece
    for (int py = 0; py < PIECE_SIZE; py++) {
        for (int px = 0; px < PIECE_SIZE; px++) {
            int x = start_x + px;
            int y = start_y + py;
            
            // Special patterns for each piece type
            if (piece == 'R') {
                // Red piece as circle
                int cx = PIECE_SIZE / 2;
                int cy = PIECE_SIZE / 2;
                int dx = px - cx;
                int dy = py - cy;
                if (dx*dx + dy*dy <= (PIECE_SIZE/2) * (PIECE_SIZE/2)) {
                    led_canvas_set_pixel(canvas, x, y, r, g, b);
                }
            } else if (piece == 'B') {
                // Blue piece as diamond
                int cx = PIECE_SIZE / 2;
                int cy = PIECE_SIZE / 2;
                if (abs(px - cx) + abs(py - cy) <= PIECE_SIZE/2) {
                    led_canvas_set_pixel(canvas, x, y, r, g, b);
                }
            } else if (piece == '.') {
                // Empty space as small dot
                if (px == PIECE_SIZE/2 && py == PIECE_SIZE/2) {
                    led_canvas_set_pixel(canvas, x, y, r, g, b);
                }
            } else if (piece == '#') {
                // Blocked space as X
                if (px == py || px == PIECE_SIZE - 1 - py) {
                    led_canvas_set_pixel(canvas, x, y, r, g, b);
                }
            }
        }
    }
}

// Display board on LED Matrix
void displayBoardOnMatrix() {
    if (!canvas) return;
    
    // Clear background
    led_canvas_clear(canvas);
    
    // Draw grid lines
    drawGridLines();
    
    // Draw each piece
    for (int row = 0; row < BOARD_SIZE; row++) {
        for (int col = 0; col < BOARD_SIZE; col++) {
            drawPiece(row, col, board[row][col]);
        }
    }
}

// Print board state to console
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

// Validate board input
int validateBoardInput(const char* input) {
    // Check if exactly 64 characters
    int len = strlen(input);
    if (len != BOARD_SIZE * BOARD_SIZE) {
        return 0;
    }
    
    // Check if each character is valid
    for (int i = 0; i < len; i++) {
        char c = input[i];
        if (c != 'R' && c != 'B' && c != '.' && c != '#') {
            return 0;
        }
    }
    
    return 1;
}

// Main function
int main() {
    // Display team name
    displayTeamName();
    
    // Initialize LED Matrix
    printf("Initializing LED Matrix...\n");
    if (initLEDMatrix() != 0) {
        printf("Warning: Unable to initialize LED matrix. Running in console mode.\n");
    } else {
        printf("LED Matrix initialized successfully!\n");
        // Show team name animation
        showTeamNameOnMatrix();
    }
    
    // Board input instructions
    printf("\n=== Board State Input ===\n");
    printf("Please enter the 8x8 board state.\n");
    printf("Available characters:\n");
    printf("  R : Red player's piece\n");
    printf("  B : Blue player's piece\n");
    printf("  . : Empty cell\n");
    printf("  # : Blocked cell\n");
    printf("\nExample input:\n");
    printf("R......B........................................................B......R\n");
    printf("\nEnter 64 characters without spaces in a single line:\n");
    
    // Get board input
    char input[BOARD_SIZE * BOARD_SIZE + 10];  // Include extra space
    while (1) {
        printf("> ");
        if (fgets(input, sizeof(input), stdin) == NULL) {
            printf("Input error occurred.\n");
            continue;
        }
        
        // Remove newline character
        input[strcspn(input, "\n")] = 0;
        
        // Validate input
        if (!validateBoardInput(input)) {
            printf("Invalid input. Please enter exactly 64 valid characters (R,B,.,#).\n");
            continue;
        }
        
        // Store input in board
        for (int i = 0; i < BOARD_SIZE; i++) {
            for (int j = 0; j < BOARD_SIZE; j++) {
                board[i][j] = input[i * BOARD_SIZE + j];
            }
        }
        
        break;
    }
    
    // Output board state
    printf("\nInput Board:\n");
    printBoard();
    
    // Display on LED Matrix
    if (canvas) {
        printf("Displaying board on LED Matrix...\n");
        displayBoardOnMatrix();
        
        // Wait for user to exit
        printf("Press Enter to exit...\n");
        getchar();
    }
    
    // Cleanup
    if (matrix) {
        led_matrix_delete(matrix);
    }
    
    printf("\nProgram terminated.\n");
    printf("Team Shannon - OctaFlip LED Display\n");
    
    return 0;
}