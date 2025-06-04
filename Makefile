# OctaFlip Makefile - Assignment 3
CC = gcc
CXX = g++

# Compiler flags
CFLAGS = -O3 -pthread
LDFLAGS = -lm -pthread

# LED Matrix paths
LED_MATRIX_PATH = ./rpi-rgb-led-matrix/include
LED_MATRIX_LIB_PATH = ./rpi-rgb-led-matrix/lib

# Check if LED Matrix exists locally first, then system paths
LED_EXISTS := $(shell test -f $(LED_MATRIX_PATH)/led-matrix-c.h && echo yes || echo no)
ifeq ($(LED_EXISTS),no)
    LED_MATRIX_PATH = /usr/local/include
    LED_MATRIX_LIB_PATH = /usr/local/lib
    LED_EXISTS := $(shell test -f $(LED_MATRIX_PATH)/led-matrix-c.h && echo yes || echo no)
endif

# Default target
all: server client board

# Server
server: server.c cJSON.c cJSON.h
	$(CC) $(CFLAGS) server.c cJSON.c -o server $(LDFLAGS)

# Client  
client: client.c board.c cJSON.c board.h cJSON.h
	$(CC) $(CFLAGS) client.c board.c cJSON.c -o client $(LDFLAGS)

# Board (standalone for LED testing)
board: board.c board.h
ifeq ($(LED_EXISTS),yes)
	$(CXX) -o board board.c -DSTANDALONE_BUILD -DHAS_LED_MATRIX -I$(LED_MATRIX_PATH) -L$(LED_MATRIX_LIB_PATH) -lrgbmatrix -lrt -lm -lpthread
else
	$(CC) $(CFLAGS) -DSTANDALONE_BUILD board.c -o board $(LDFLAGS)
endif

# Clean
clean:
	rm -f server client board *.o

# Help
help:
	@echo "Usage:"
	@echo "  make              # Build all components"
	@echo "  make board        # Build standalone board for LED testing"
	@echo "  make clean        # Remove executables"
	@echo ""
	@echo "To run board with LED:"
	@echo "  sudo ./board      # Read 8 lines from stdin"
	@echo ""
	@echo "Manual compilation (as per README):"
	@echo "  gcc -O3 -pthread client.c board.c cJSON.c -o client -lm"
	@echo ""
	@echo "For LED support:"
	@echo "  g++ -o board board.c -DSTANDALONE_BUILD -DHAS_LED_MATRIX \\"
	@echo "      -I./rpi-rgb-led-matrix/include -L./rpi-rgb-led-matrix/lib \\"
	@echo "      -lrgbmatrix -lrt -lm -lpthread"

.PHONY: all clean help