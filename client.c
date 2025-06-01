#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "cJSON.h"

#define BUF_SIZE    8192
#define BOARD_SIZE  8

typedef struct {
    int sx, sy, tx, ty;
} Move;

int connect_to_server(const char *hostname, const char *port) {
    struct addrinfo hints, *res, *p;
    int sockfd, status;
    memset(&hints, 0, sizeof hints);
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if ((status = getaddrinfo(hostname, port, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        exit(EXIT_FAILURE);
    }
    for (p = res; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
            continue;
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == 0)
            break;
        close(sockfd);
    }
    if (p == NULL) {
        fprintf(stderr, "Failed to connect to server\n");
        exit(EXIT_FAILURE);
    }
    freeaddrinfo(res);
    return sockfd;
}

void send_json(int sockfd, cJSON *j) {
    char *s = cJSON_PrintUnformatted(j);
    size_t len = strlen(s);
    char *msg = malloc(len + 2);
    memcpy(msg, s, len);
    msg[len] = '\n';
    msg[len + 1] = '\0';
    size_t sent = 0;
    while (sent < len + 1) {
        ssize_t n = send(sockfd, msg + sent, (len + 1) - sent, 0);
        if (n <= 0) {
            perror("send error");
            free(s);
            free(msg);
            exit(EXIT_FAILURE);
        }
        sent += n;
    }
    free(s);
    free(msg);
}

cJSON *recv_json(int sockfd) {
    char buf[BUF_SIZE];
    size_t idx = 0;
    char ch;
    while (idx + 1 < BUF_SIZE) {
        ssize_t n = recv(sockfd, &ch, 1, 0);
        if (n == 0) return NULL;
        if (n < 0) {
            perror("recv error");
            exit(EXIT_FAILURE);
        }
        if (ch == '\n') break;
        buf[idx++] = ch;
    }
    buf[idx] = '\0';
    cJSON *j = cJSON_Parse(buf);
    if (!j) {
        fprintf(stderr, "JSON parse error: %s\n", buf);
        exit(EXIT_FAILURE);
    }
    return j;
}

void print_board(cJSON *board) {
    if (!cJSON_IsArray(board)) return;
    int rows = cJSON_GetArraySize(board);
    for (int i = 0; i < rows; i++) {
        cJSON *row = cJSON_GetArrayItem(board, i);
        if (cJSON_IsString(row) && row->valuestring) {
            printf("%s\n", row->valuestring);
        }
    }
    printf("\n");
}

Move move_generate(char board[BOARD_SIZE][BOARD_SIZE], char color) {
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            if (board[r][c] != color) continue;
            for (int dr = -1; dr <= 1; dr++) {
                for (int dc = -1; dc <= 1; dc++) {
                    if (dr == 0 && dc == 0) continue;
                    int nr = r + dr, nc = c + dc;
                    if (nr >= 0 && nr < BOARD_SIZE && nc >= 0 && nc < BOARD_SIZE) {
                        if (board[nr][nc] == '.') {
                            return (Move){r + 1, c + 1, nr + 1, nc + 1};
                        }
                    }
                }
            }
            for (int dr = -2; dr <= 2; dr += 2) {
                for (int dc = -2; dc <= 2; dc += 2) {
                    if (dr == 0 && dc == 0) continue;
                    int nr = r + dr, nc = c + dc;
                    if (nr >= 0 && nr < BOARD_SIZE && nc >= 0 && nc < BOARD_SIZE) {
                        if (board[nr][nc] == '.') {
                            return (Move){r + 1, c + 1, nr + 1, nc + 1};
                        }
                    }
                }
            }
        }
    }
    return (Move){0, 0, 0, 0};
}

int main(int argc, char *argv[]) {
    if (argc != 7) {
        fprintf(stderr, "Usage: %s -ip <server_ip> -port <server_port> -username <username>\n", argv[0]);
        return EXIT_FAILURE;
    }
    char ip[64] = {0}, port[16] = {0}, username[64] = {0};
    for (int i = 1; i < argc; i += 2) {
        if (strcmp(argv[i], "-ip") == 0) {
            strncpy(ip, argv[i + 1], sizeof(ip) - 1);
        } else if (strcmp(argv[i], "-port") == 0) {
            strncpy(port, argv[i + 1], sizeof(port) - 1);
        } else if (strcmp(argv[i], "-username") == 0) {
            strncpy(username, argv[i + 1], sizeof(username) - 1);
        } else {
            fprintf(stderr, "Unknown option %s\n", argv[i]);
            return EXIT_FAILURE;
        }
    }
    int sockfd = connect_to_server(ip, port);

    cJSON *reg = cJSON_CreateObject();
    cJSON_AddStringToObject(reg, "type", "register");
    cJSON_AddStringToObject(reg, "username", username);
    send_json(sockfd, reg);
    cJSON_Delete(reg);

    cJSON *msg = recv_json(sockfd);
    if (!msg) {
        close(sockfd);
        return EXIT_FAILURE;
    }
    const char *type = cJSON_GetObjectItem(msg, "type")->valuestring;
    if (strcmp(type, "register_nack") == 0) {
        const char *reason = cJSON_GetObjectItem(msg, "reason")->valuestring;
        fprintf(stderr, "Registration failed: %s\n", reason);
        cJSON_Delete(msg);
        close(sockfd);
        return EXIT_FAILURE;
    }
    cJSON_Delete(msg);

    msg = recv_json(sockfd);
    if (!msg) {
        close(sockfd);
        return EXIT_FAILURE;
    }
    type = cJSON_GetObjectItem(msg, "type")->valuestring;
    if (strcmp(type, "game_start") != 0) {
        cJSON_Delete(msg);
        close(sockfd);
        return EXIT_FAILURE;
    }
    cJSON *players     = cJSON_GetObjectItem(msg, "players");
    cJSON *firstPlayer = cJSON_GetObjectItem(msg, "first_player");
    char myColor = (strcmp(firstPlayer->valuestring, username) == 0) ? 'R' : 'B';
    cJSON *boardArr = cJSON_GetObjectItem(msg, "board");
    print_board(boardArr);
    cJSON_Delete(msg);

    char board[BOARD_SIZE][BOARD_SIZE];
    while ((msg = recv_json(sockfd)) != NULL) {
        type = cJSON_GetObjectItem(msg, "type")->valuestring;

        if (strcmp(type, "your_turn") == 0) {
            cJSON *bArr = cJSON_GetObjectItem(msg, "board");
            for (int i = 0; i < BOARD_SIZE; i++) {
                const char *row = cJSON_GetArrayItem(bArr, i)->valuestring;
                memcpy(board[i], row, BOARD_SIZE);
            }
            Move m = move_generate(board, myColor);
            cJSON *mv = cJSON_CreateObject();
            cJSON_AddStringToObject(mv, "type",     "move");
            cJSON_AddStringToObject(mv, "username", username);
            cJSON_AddNumberToObject(mv, "sx",       m.sx);
            cJSON_AddNumberToObject(mv, "sy",       m.sy);
            cJSON_AddNumberToObject(mv, "tx",       m.tx);
            cJSON_AddNumberToObject(mv, "ty",       m.ty);
            send_json(sockfd, mv);
            cJSON_Delete(mv);

        } else if (strcmp(type, "move_ok") == 0) {
            cJSON *bArr = cJSON_GetObjectItem(msg, "board");
            print_board(bArr);

        } else if (strcmp(type, "invalid_move") == 0) {
            cJSON *reason = cJSON_GetObjectItem(msg, "reason");
            if (reason) {
                printf("Invalid move: %s\n", reason->valuestring);
            } else {
                cJSON *bArr = cJSON_GetObjectItem(msg, "board");
                print_board(bArr);
            }

        } else if (strcmp(type, "pass") == 0) {
            // server handles turning to next player

        } else if (strcmp(type, "game_over") == 0) {
            cJSON *scores = cJSON_GetObjectItem(msg, "scores");
            cJSON *item;
            cJSON_ArrayForEach(item, scores) {
                printf("%s: %d\n", item->string, item->valueint);
            }
            cJSON_Delete(msg);
            break;
        }
        cJSON_Delete(msg);
    }
    close(sockfd);
    return EXIT_SUCCESS;
}
