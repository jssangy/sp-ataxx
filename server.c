#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "cJSON.h"

#define SIZE 8
#define MAX_CLIENTS 2
#define BUF_SIZE 8192
#define TIMEOUT_SEC 5

typedef struct {
    int sockfd;
    char username[64];
    char color;
    bool registered;
} Client;

static const int dr8[8] = { -1,-1,-1, 0, 0, 1, 1, 1 };
static const int dc8[8] = { -1, 0, 1,-1, 1,-1, 0, 1 };

char board[SIZE][SIZE];
Client clients[MAX_CLIENTS];
bool prev_pass = false;
FILE *log_file;

static bool inBounds(int r, int c) {
    return r >= 0 && r < SIZE && c >= 0 && c < SIZE;
}

static bool isClone(int dr, int dc) {
    return abs(dr) <= 1 && abs(dc) <= 1 && (abs(dr) + abs(dc) >= 1);
}

static bool isJump(int dr, int dc) {
    return (abs(dr) == 2 && dc == 0) || (dr == 0 && abs(dc) == 2) ||
           (abs(dr) == 2 && abs(dc) == 2);
}

static bool hasValidMoves(char player) {
    for (int r = 0; r < SIZE; r++) {
        for (int c = 0; c < SIZE; c++) {
            if (board[r][c] != player) continue;
            for (int dr = -2; dr <= 2; dr++) {
                for (int dc = -2; dc <= 2; dc++) {
                    if (dr == 0 && dc == 0) continue;
                    int nr = r + dr, nc = c + dc;
                    if (!inBounds(nr, nc) || board[nr][nc] != '.') continue;
                    if (isClone(dr, dc) || isJump(dr, dc)) return true;
                }
            }
        }
    }
    return false;
}

static bool applyMoveGame(char player, int r1, int c1, int r2, int c2) {
    if (r1 == 0 && c1 == 0 && r2 == 0 && c2 == 0) {
        return !hasValidMoves(player);
    }
    int sr = r1 - 1, sc = c1 - 1;
    int dr = r2 - 1, dc = c2 - 1;
    if (!inBounds(sr, sc) || !inBounds(dr, dc)) return false;
    if (board[sr][sc] != player || board[dr][dc] != '.') return false;
    int dR = dr - sr, dC = dc - sc;
    if (!(isClone(dR, dC) || isJump(dR, dC))) return false;
    board[dr][dc] = player;
    if (isJump(dR, dC)) board[sr][sc] = '.';
    char opp = (player == 'R') ? 'B' : 'R';
    for (int i = 0; i < 8; i++) {
        int fr = dr + dr8[i], fc = dc + dc8[i];
        if (inBounds(fr, fc) && board[fr][fc] == opp) {
            board[fr][fc] = player;
        }
    }
    return true;
}

static void countStones(int *r_cnt, int *b_cnt) {
    *r_cnt = *b_cnt = 0;
    for (int r = 0; r < SIZE; r++) {
        for (int c = 0; c < SIZE; c++) {
            if (board[r][c] == 'R') (*r_cnt)++;
            else if (board[r][c] == 'B') (*b_cnt)++;
        }
    }
}

static void initBoard() {
    for (int r = 0; r < SIZE; r++) {
        for (int c = 0; c < SIZE; c++) {
            board[r][c] = '.';
        }
    }
    board[0][0] = 'R';
    board[0][SIZE-1] = 'B';
    board[SIZE-1][0] = 'B';
    board[SIZE-1][SIZE-1] = 'R';
}

static void send_json(int sockfd, cJSON *j) {
    char *s = cJSON_PrintUnformatted(j);
    size_t len = strlen(s);
    char *msg = malloc(len + 2);
    memcpy(msg, s, len);
    msg[len] = '\n';
    msg[len + 1] = '\0';
    size_t sent = 0;
    while (sent < len + 1) {
        ssize_t n = send(sockfd, msg + sent, (len + 1) - sent, 0);
        if (n <= 0) break;
        sent += n;
    }
    free(s);
    free(msg);
}

static cJSON *recv_json(int sockfd) {
    char buf[BUF_SIZE];
    size_t idx = 0;
    while (idx + 1 < sizeof(buf)) {
        char ch;
        ssize_t n = recv(sockfd, &ch, 1, 0);
        if (n <= 0) return NULL;
        if (ch == '\n') break;
        buf[idx++] = ch;
    }
    buf[idx] = '\0';
    return cJSON_Parse(buf);
}

static int setupListener(const char *port) {
    struct addrinfo hints, *res, *p;
    int listener, yes = 1;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if (getaddrinfo(NULL, port, &hints, &res) != 0) exit(EXIT_FAILURE);
    for (p = res; p; p = p->ai_next) {
        if ((listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0)
            continue;
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
        if (bind(listener, p->ai_addr, p->ai_addrlen) == 0) break;
        close(listener);
    }
    if (!p) exit(EXIT_FAILURE);
    freeaddrinfo(res);
    if (listen(listener, MAX_CLIENTS) < 0) exit(EXIT_FAILURE);
    return listener;
}

static void addBoardToJSON(cJSON *obj) {
    cJSON *arr = cJSON_CreateArray();
    for (int r = 0; r < SIZE; r++) {
        char row[SIZE + 1];
        for (int c = 0; c < SIZE; c++) {
            row[c] = board[r][c];
        }
        row[SIZE] = '\0';
        cJSON_AddItemToArray(arr, cJSON_CreateString(row));
    }
    cJSON_AddItemToObject(obj, "board", arr);
}

int main(int argc, char *argv[]) {
    if (argc != 2) return EXIT_FAILURE;
    log_file = fopen("log.txt", "w");
    signal(SIGPIPE, SIG_IGN);
    initBoard();
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].registered = false;
    }
    int listener = setupListener(argv[1]);
    int connected = 0;
    while (connected < MAX_CLIENTS) {
        int fd = accept(listener, NULL, NULL);
        if (fd < 0) continue;
        clients[connected++].sockfd = fd;
    }
    int regs = 0;
    while (regs < MAX_CLIENTS) {
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].registered) continue;
            cJSON *msg = recv_json(clients[i].sockfd);
            if (!msg) continue;
            cJSON *t = cJSON_GetObjectItem(msg, "type");
            if (t && strcmp(t->valuestring, "register") == 0) {
                strcpy(clients[i].username, cJSON_GetObjectItem(msg, "username")->valuestring);
                clients[i].color = (regs == 0 ? 'R' : 'B');
                cJSON *ack = cJSON_CreateObject();
                cJSON_AddStringToObject(ack, "type", "register_ack");
                send_json(clients[i].sockfd, ack);
                cJSON_Delete(ack);
                clients[i].registered = true;
                fprintf(log_file, "[Registration ] %s registered\n", clients[i].username);
                regs++;
            } else {
                cJSON *nack = cJSON_CreateObject();
                cJSON_AddStringToObject(nack, "type", "register_nack");
                cJSON_AddStringToObject(nack, "reason", "invalid");
                send_json(clients[i].sockfd, nack);
                cJSON_Delete(nack);
            }
            cJSON_Delete(msg);
        }
    }
    {
        cJSON *gs = cJSON_CreateObject();
        cJSON_AddStringToObject(gs, "type", "game_start");
        cJSON *players = cJSON_CreateStringArray(
            (const char*[]){clients[0].username, clients[1].username}, 2
        );
        cJSON_AddItemToObject(gs, "players", players);
        cJSON_AddStringToObject(gs, "first_player", clients[0].username);
        addBoardToJSON(gs);
        fprintf(log_file, "[Game Start   ] Board:\n");
        for (int r = 0; r < SIZE; r++) {
            fprintf(log_file, "%.*s\n", SIZE, board[r]);
        }
        for (int i = 0; i < MAX_CLIENTS; i++) {
            send_json(clients[i].sockfd, gs);
        }
        cJSON_Delete(gs);
    }
    int turn = 0;
    while (1) {
        Client *cur = &clients[turn];
        Client *nxt = &clients[1 - turn];
        cJSON *yt = cJSON_CreateObject();
        cJSON_AddStringToObject(yt, "type", "your_turn");
        addBoardToJSON(yt);
        cJSON_AddNumberToObject(yt, "timeout", TIMEOUT_SEC);
        send_json(cur->sockfd, yt);
        fprintf(log_file, "[Your Turn    ] %s\n", cur->username);
        for (int r = 0; r < SIZE; r++) {
            fprintf(log_file, "%.*s\n", SIZE, board[r]);
        }
        cJSON_Delete(yt);

        fd_set rfds;
        struct timeval tv = { TIMEOUT_SEC, 0 };
        FD_ZERO(&rfds);
        FD_SET(cur->sockfd, &rfds);
        int sel = select(cur->sockfd + 1, &rfds, NULL, NULL, &tv);
        bool did_pass = false;
        cJSON *req = NULL;
        if (sel <= 0) {
            did_pass = true;
        } else {
            req = recv_json(cur->sockfd);
            if (!req) did_pass = true;
        }

        if (did_pass) {
            cJSON *ps = cJSON_CreateObject();
            cJSON_AddStringToObject(ps, "type", "pass");
            cJSON_AddStringToObject(ps, "next_player", nxt->username);
            send_json(cur->sockfd, ps);
            fprintf(log_file, "[Move Received] %s -> pass\n", cur->username);
            fprintf(log_file, "[Move OK      ] pass\n");
            cJSON_Delete(ps);
            if (prev_pass) break;
            prev_pass = true;
        } else {
            prev_pass = false;
            int sx = cJSON_GetObjectItem(req, "sx")->valueint;
            int sy = cJSON_GetObjectItem(req, "sy")->valueint;
            int tx = cJSON_GetObjectItem(req, "tx")->valueint;
            int ty = cJSON_GetObjectItem(req, "ty")->valueint;
            bool isPass = (sx == 0 && sy == 0 && tx == 0 && ty == 0);
            if (isPass) {
                fprintf(log_file, "[Move Received] %s -> pass\n", cur->username);
            } else {
                fprintf(log_file, "[Move Received] %s -> (%d,%d)->(%d,%d)\n",
                        cur->username, sx, sy, tx, ty);
            }
            bool ok = applyMoveGame(cur->color, sx, sy, tx, ty);
            cJSON_Delete(req);
            cJSON *res = cJSON_CreateObject();
            if (ok) {
                if (isPass) {
                    cJSON_AddStringToObject(res, "type", "pass");
                    if (prev_pass) {
                        cJSON_Delete(res);
                        break;
                    }
                    prev_pass = true;
                    fprintf(log_file, "[Move OK      ] pass\n");
                } else {
                    cJSON_AddStringToObject(res, "type", "move_ok");
                    prev_pass = false;
                    fprintf(log_file, "[Move OK      ] New Board:\n");
                    for (int r = 0; r < SIZE; r++) {
                        fprintf(log_file, "%.*s\n", SIZE, board[r]);
                    }
                }
            } else {
                cJSON_AddStringToObject(res, "type", "invalid_move");
                fprintf(log_file, "[Move OK      ] invalid_move\n");
            }
            addBoardToJSON(res);
            cJSON_AddStringToObject(res, "next_player", nxt->username);
            send_json(cur->sockfd, res);
            cJSON_Delete(res);
        }

        int r_cnt, b_cnt;
        countStones(&r_cnt, &b_cnt);
        if (prev_pass) break;
        if (r_cnt == 0 || b_cnt == 0 || r_cnt + b_cnt == SIZE * SIZE) break;
        turn = 1 - turn;
    }

    {
        int r_cnt, b_cnt;
        countStones(&r_cnt, &b_cnt);
        cJSON *go = cJSON_CreateObject();
        cJSON_AddStringToObject(go, "type", "game_over");
        cJSON *scores = cJSON_CreateObject();
        cJSON_AddNumberToObject(scores, clients[0].username,
                               (clients[0].color == 'R') ? r_cnt : b_cnt);
        cJSON_AddNumberToObject(scores, clients[1].username,
                               (clients[1].color == 'R') ? r_cnt : b_cnt);
        cJSON_AddItemToObject(go, "scores", scores);
        fprintf(log_file, "[Game Over    ] Scores: %s=%d, %s=%d\n",
                clients[0].username, (clients[0].color == 'R') ? r_cnt : b_cnt,
                clients[1].username, (clients[1].color == 'R') ? r_cnt : b_cnt);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            send_json(clients[i].sockfd, go);
            close(clients[i].sockfd);
        }
        cJSON_Delete(go);
    }

    fclose(log_file);
    close(listener);
    return 0;
}
