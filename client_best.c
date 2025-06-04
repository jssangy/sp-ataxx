#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#define MAX_MOVES 128
#define BOARD_SIZE 8
#define MAX_DEPTH 5
#define TIME_LIMIT 5
#define MAX_THREADS 4


char initial_board[8][9] = {
    "R......B",
    "........",
    "........",
    "........",
    "........",
    "........",
    "........",
    "B......R"
};

int move_dir[8][2] = {
    {-1, -1}, {-1, 0}, {-1, 1},
    { 0, -1},          { 0, 1},
    { 1, -1}, { 1, 0}, { 1, 1}
};

typedef struct {
    int sx, sy, tx, ty;
} Move;

typedef struct {
    char board[8][8];
    Move move;
    char player;
    int score;
} ThreadData;

time_t start_time;

int in_bounds(int r, int c) {
    return r >= 0 && r < 8 && c >= 0 && c < 8;
}

void copy_board(char dst[8][8], char src[8][8]) {
    for (int i = 0; i < 8; i++)
        for (int j = 0; j < 8; j++)
            dst[i][j] = src[i][j];
}

int valid_move(char board[8][8], int r1, int c1, int r2, int c2, char player) {
    if (!in_bounds(r1, c1) || !in_bounds(r2, c2)) return 0;
    if (board[r1][c1] != player || board[r2][c2] != '.') return 0;
    int dr = abs(r2 - r1), dc = abs(c2 - c1);
    if ((dr == 1 || dr == 0) && (dc == 1 || dc == 0) && !(dr == 0 && dc == 0)) return 1;
    if ((dr == 2 || dr == 0) && (dc == 2 || dc == 0) && !(dr == 0 && dc == 0)) return 2;
    return 0;
}

void apply_move(char board[8][8], int sx, int sy, int tx, int ty, char player) {
    int is_jump = abs(tx - sx) > 1 || abs(ty - sy) > 1;
    if (is_jump) board[sx][sy] = '.';
    board[tx][ty] = player;
    char opp = (player == 'R') ? 'B' : 'R';
    for (int i = 0; i < 8; i++) {
        int nr = tx + move_dir[i][0];
        int nc = ty + move_dir[i][1];
        if (in_bounds(nr, nc) && board[nr][nc] == opp)
            board[nr][nc] = player;
    }
}

int evaluate(char board[8][8], char player) {
    char opp = (player == 'R') ? 'B' : 'R';
    int p = 0, o = 0;
    for (int i = 0; i < 8; i++)
        for (int j = 0; j < 8; j++) {
            if (board[i][j] == player) p++;
            else if (board[i][j] == opp) o++;
        }
    return p - o;
}

int generate_moves(char board[8][8], char player, Move *moves) {
    int count = 0;
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            if (board[i][j] != player) continue;
            for (int d = 0; d < 8; d++) {
                for (int dist = 1; dist <= 2; dist++) {
                    int ni = i + move_dir[d][0] * dist;
                    int nj = j + move_dir[d][1] * dist;
                    if (in_bounds(ni, nj) && board[ni][nj] == '.') {
                        moves[count++] = (Move){i, j, ni, nj};
                    }
                }
            }
        }
    }
    return count;
}

int negamax(char board[8][8], char player, int depth, int alpha, int beta) {
    if (time(NULL) - start_time >= TIME_LIMIT || depth == 0)
        return evaluate(board, player);

    Move moves[MAX_MOVES];
    int n = generate_moves(board, player, moves);
    if (n == 0) return evaluate(board, player);

    int best = -9999;
    for (int i = 0; i < n; i++) {
        char sim[8][8];
        copy_board(sim, board);
        apply_move(sim, moves[i].sx, moves[i].sy, moves[i].tx, moves[i].ty, player);
        int score = -negamax(sim, (player == 'R') ? 'B' : 'R', depth - 1, -beta, -alpha);
        if (score > best) best = score;
        if (best > alpha) alpha = best;
        if (alpha >= beta) break;
    }
    return best;
}

void *thread_func(void *arg) {
    ThreadData *data = (ThreadData *)arg;
    char sim[8][8];
    copy_board(sim, data->board);
    apply_move(sim, data->move.sx, data->move.sy, data->move.tx, data->move.ty, data->player);
    data->score = -negamax(sim, (data->player == 'R') ? 'B' : 'R', MAX_DEPTH - 1, -10000, 10000);
    return NULL;
}

void move_generate(char board[8][8], char player, int *sx, int *sy, int *tx, int *ty) {
    Move moves[MAX_MOVES];
    int n = generate_moves(board, player, moves);
    if (n == 0) {
        *sx = *sy = *tx = *ty = 0;
        return;
    }

    start_time = time(NULL);

    pthread_t threads[MAX_THREADS];
    ThreadData data[MAX_THREADS];
    int best_score = -9999, best_idx = 0;

    for (int i = 0; i < n; i += MAX_THREADS) {
        int batch = (i + MAX_THREADS > n) ? (n - i) : MAX_THREADS;

        for (int j = 0; j < batch; j++) {
            copy_board(data[j].board, board);
            data[j].move = moves[i + j];
            data[j].player = player;
            data[j].score = -9999;
            pthread_create(&threads[j], NULL, thread_func, &data[j]);
        }

        for (int j = 0; j < batch; j++) {
            pthread_join(threads[j], NULL);
            if (data[j].score > best_score) {
                best_score = data[j].score;
                best_idx = i + j;
            }
        }
    }

    *sx = moves[best_idx].sx + 1;
    *sy = moves[best_idx].sy + 1;
    *tx = moves[best_idx].tx + 1;
    *ty = moves[best_idx].ty + 1;
}

int main() {
    char board[8][8];
    for (int i = 0; i < 8; i++)
        for (int j = 0; j < 8; j++)
            board[i][j] = initial_board[i][j];

    char current_player = 'R';
    char opponent = 'B';

    int consecutive_passes = 0;

    while (1) {
        int sx = 0, sy = 0, tx = 0, ty = 0;

        if (current_player == 'R') {
            // 사용자 입력
            printf("Enter your move (sx sy tx ty): ");
            scanf("%d %d %d %d", &sx, &sy, &tx, &ty);
        } else {
            // AI가 수 생성
            move_generate(board, current_player, &sx, &sy, &tx, &ty);
            if (sx == 0 && sy == 0 && tx == 0 && ty == 0) {
                printf("AI (%c) passes.\n", current_player);
                consecutive_passes++;
            } else {
                printf("AI (%c) plays: %d %d %d %d\n", current_player, sx, sy, tx, ty);
                consecutive_passes = 0;
            }
        }

        // 패스 판별 (사람이 패스하면 0 0 0 0 입력해야 함)
        if (sx == 0 && sy == 0 && tx == 0 && ty == 0) {
            printf("%c passes.\n", current_player);
            consecutive_passes++;
        } else {
            int mv = valid_move(board, sx - 1, sy - 1, tx - 1, ty - 1, current_player);
            if (mv == 1 || mv == 2) {
                apply_move(board, sx - 1, sy - 1, tx - 1, ty - 1, current_player);
                if (mv == 2) board[sx - 1][sy - 1] = '.';
            } else {
                printf("Invalid move! Try again.\n");
                if (current_player == 'R') continue; // 사용자 재입력 유도
                else consecutive_passes++; // AI가 invalid 수를 낼 일은 없지만 예외 처리
            }
        }

        // 보드 출력
        for (int i = 0; i < 8; i++) {
            for (int j = 0; j < 8; j++)
                printf("%c", board[i][j]);
            printf("\n");
        }
        printf("\n");

        // 종료 조건
        if (consecutive_passes >= 2) {
            printf("Both players passed. Game over.\n");
            break;
        }

        // 턴 전환
        char temp = current_player;
        current_player = opponent;
        opponent = temp;
    }

    return 0;
}
