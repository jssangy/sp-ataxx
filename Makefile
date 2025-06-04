CC = gcc
CFLAGS = -Wall -O2 -std=c99 -DHAS_LED_MATRIX
LDFLAGS = -lm -pthread

SRC_CLIENT = client.c board.c
OBJ_CLIENT = $(SRC_CLIENT:.c=.o)
TARGET_CLIENT = client

SRC_BOARD = board.c
OBJ_BOARD = $(SRC_BOARD:.c=.o)
TARGET_BOARD = board

.PHONY: all clean

all: $(TARGET_CLIENT) $(TARGET_BOARD)

$(TARGET_CLIENT): $(OBJ_CLIENT)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(TARGET_BOARD): $(OBJ_BOARD)
	$(CC) $(CFLAGS) -DSTANDALONE_BUILD -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o $(TARGET_CLIENT) $(TARGET_BOARD)
