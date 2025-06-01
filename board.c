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

#define COLOR_EMPTY_R 0
#define COLOR_EMPTY_G 0
#define COLOR_EMPTY_B 0

#define COLOR_BLOCKED_R 100
#define COLOR_BLOCKED_G 100
#define COLOR_BLOCKED_B 100

#define COLOR_GRID_R 50
#define COLOR_GRID_G 50
#define COLOR_GRID_B 50

#define COLOR_BG_R 0
#define COLOR_BG_G 0
#define COLOR_BG_B 0

// Team name colors
#define COLOR_TEAM_R 255
#define COLOR_TEAM_G 200
#define COLOR_TEAM_B 0

// Global game board
char board[BOARD_SIZE][BOARD_SIZE];

// LED Matrix global variables
struct RGBLedMatrix *matrix = NULL;
struct LedCanvas *canvas = NULL;

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
    printf("║   ██████╗██╗      █████╗ ██╗   ██╗██████╗ ███████╗           ║\n");
    printf("║  ██╔════╝██║     ██╔══██╗██║   ██║██╔══██╗██╔════╝           ║\n");
    printf("║  ██║     ██║     ███████║██║   ██║██║  ██║█████╗             ║\n");
    printf("║  ██║     ██║     ██╔══██║██║   ██║██║  ██║██╔══╝             ║\n");
    printf("║  ╚██████╗███████╗██║  ██║╚██████╔╝██████╔╝███████╗           ║\n");
    printf("║   ╚═════╝╚══════╝╚═╝  ╚═╝ ╚═════╝ ╚═════╝ ╚══════╝           ║\n");
    printf("║                                                               ║\n");
    printf("║                    🎮 OctaFlip LED Display 🎮                 ║\n");
    printf("║                                                               ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n");
    printf("\n");
}

// Manual text drawing function for team name
void drawTextManual(const char* text, int start_x, int start_y, int r, int g, int b) {
    // Simple 5x7 character representation
    const int char_width = 5;
    const int char_height = 7;
    const int spacing = 1;
    
    for (int i = 0; text[i] != '\0'; i++) {
        int x_offset = start_x + i * (char_width + spacing);
        
        // Simple block letters - just fill rectangles for each character
        if (text[i] != ' ') {
            for (int y = 0; y < char_height; y++) {
                for (int x = 0; x < char_width; x++) {
                    led_canvas_set_pixel(canvas, x_offset + x, start_y + y, r, g, b);
                }
            }
        }
    }
}

// Show team name on LED Matrix with animation
void showTeamNameOnMatrix() {
    if (!canvas) return;
    
    // Blinking effect - flash 3 times
    for (int blink = 0; blink < 3; blink++) {
        led_canvas_clear(canvas);
        
        // Draw "TEAM CLAUDE" manually centered
        drawTextManual("TEAM", 15, 20, COLOR_TEAM_R, COLOR_TEAM_G, COLOR_TEAM_B);
        drawTextManual("CLAUDE", 10, 35, COLOR_TEAM_R, COLOR_TEAM_G, COLOR_TEAM_B);
        
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
    
    // Keep "TEAM CLAUDE" displayed until board input
    led_canvas_clear(canvas);
    drawTextManual("TEAM", 15, 20, COLOR_TEAM_R, COLOR_TEAM_G, COLOR_TEAM_B);
    drawTextManual("CLAUDE", 10, 35, COLOR_TEAM_R, COLOR_TEAM_G, COLOR_TEAM_B);
    
    // Frame
    for (int x = 0; x < MATRIX_SIZE; x++) {
        led_canvas_set_pixel(canvas, x, 0, 100, 100, 255);
        led_canvas_set_pixel(canvas, x, MATRIX_SIZE-1, 100, 100, 255);
    }
    for (int y = 0; y < MATRIX_SIZE; y++) {
        led_canvas_set_pixel(canvas, 0, y, 100, 100, 255);
        led_canvas_set_pixel(canvas, MATRIX_SIZE-1, y, 100, 100, 255);
    }
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
    
    // Disable hardware pulsing
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

// Draw grid lines - 1 pixel wide at positions 0, 8, 16, ..., 64
void drawGridLines() {
    // Draw horizontal lines
    for (int i = 0; i <= BOARD_SIZE; i++) {
        int y = i * CELL_SIZE;
        for (int x = 0; x < MATRIX_SIZE; x++) {
            led_canvas_set_pixel(canvas, x, y, COLOR_GRID_R, COLOR_GRID_G, COLOR_GRID_B);
        }
    }
    
    // Draw vertical lines
    for (int i = 0; i <= BOARD_SIZE; i++) {
        int x = i * CELL_SIZE;
        for (int y = 0; y < MATRIX_SIZE; y++) {
            led_canvas_set_pixel(canvas, x, y, COLOR_GRID_R, COLOR_GRID_G, COLOR_GRID_B);
        }
    }
}

// Draw piece function - fill 6x6 area with solid color
void drawPiece(int row, int col, char piece) {
    // Calculate cell starting position (skip the grid line)
    int start_x = col * CELL_SIZE + 1;
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
            // Empty cell - leave black (no drawing needed)
            return;
        case '#':
            r = COLOR_BLOCKED_R;
            g = COLOR_BLOCKED_G;
            b = COLOR_BLOCKED_B;
            break;
        default:
            return;
    }
    
    // Fill the entire 6x6 area with solid color
    for (int py = 0; py < PIECE_SIZE; py++) {
        for (int px = 0; px < PIECE_SIZE; px++) {
            int x = start_x + px;
            int y = start_y + py;
            led_canvas_set_pixel(canvas, x, y, r, g, b);
        }
    }
}

// Display board on LED Matrix
void displayBoardOnMatrix() {
    if (!canvas) return;
    
    // Clear background
    led_canvas_clear(canvas);
    
    // Draw grid lines first
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
        // Show team name animation and keep it displayed
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
    
    // Bulk input option
    printf("\nYou can paste all 8 rows at once (8 characters per row):\n");
    printf("Example:\n");
    printf("R......B\n");
    printf("........\n");
    printf("........\n");
    printf("........\n");
    printf("........\n");
    printf("........\n");
    printf("........\n");
    printf("B......R\n");
    printf("\nEnter your board state below:\n");
    
    // Get board input - bulk style
    char board_lines[BOARD_SIZE][BOARD_SIZE + 2];  // +2 for newline and null
    
    // Read all 8 lines
    for (int i = 0; i < BOARD_SIZE; i++) {
        if (fgets(board_lines[i], sizeof(board_lines[i]), stdin) == NULL) {
            printf("Error reading input. Please restart and try again.\n");
            if (matrix) led_matrix_delete(matrix);
            return 1;
        }
        
        // Remove newline character
        int len = strlen(board_lines[i]);
        if (len > 0 && board_lines[i][len - 1] == '\n') {
            board_lines[i][len - 1] = '\0';
            len--;
        }
        
        // Additional check for carriage return
        if (len > 0 && board_lines[i][len - 1] == '\r') {
            board_lines[i][len - 1] = '\0';
            len--;
        }
        
        // Validate line length
        if (len != BOARD_SIZE) {
            printf("Error: Row %d has %d characters, expected %d.\n", i+1, len, BOARD_SIZE);
            if (matrix) led_matrix_delete(matrix);
            return 1;
        }
        
        // Validate characters
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (board_lines[i][j] != 'R' && board_lines[i][j] != 'B' && 
                board_lines[i][j] != '.' && board_lines[i][j] != '#') {
                printf("Error: Invalid character '%c' at row %d, column %d.\n", 
                       board_lines[i][j], i+1, j+1);
                if (matrix) led_matrix_delete(matrix);
                return 1;
            }
            
            // Copy to board
            board[i][j] = board_lines[i][j];
        }
    }
    
    // Output board state
    printf("\nBoard successfully loaded!\n");
    printBoard();
    
    // Display on LED Matrix
    if (canvas) {
        printf("Displaying board on LED Matrix...\n");
        displayBoardOnMatrix();
        
        // Wait for user to exit
        printf("\nPress Enter to exit...\n");
        getchar();
    }
    
    // Cleanup
    if (matrix) {
        led_matrix_delete(matrix);
    }
    
    printf("\nProgram terminated.\n");
    printf("Team Claude - OctaFlip LED Display\n");
    
    return 0;
}