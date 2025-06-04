// board.h - OctaFlip LED Display Interface
// Team Shannon - Assignment 3
#ifndef BOARD_H
#define BOARD_H

#include <stdint.h>

// Board and display constants
#define BOARD_SIZE 8
#define MATRIX_SIZE 64
#define CELL_SIZE 8
#define PIECE_SIZE 6

// Board display functions for client integration
int init_led_display(void);
void cleanup_led_display(void);
void render_board_to_led(char board[BOARD_SIZE][BOARD_SIZE]);
void show_team_name_on_led(void);
void show_game_over_animation(int red_score, int blue_score);

// Standalone mode functions
int run_board_standalone(void);
int run_board_network_server(void);
int run_board_manual_input(void);

#endif // BOARD_H