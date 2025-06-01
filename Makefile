# OctaFlip Makefile with LED Board Support
CC = gcc
CXX = g++

# Compiler flags
CFLAGS = -Wall -Wextra -O3 -march=native -ffast-math -pthread
DEBUGFLAGS = -Wall -Wextra -g -O0 -pthread -DDEBUG

# macOS doesn't have -fsanitize=address by default
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    # macOS specific flags
    DEBUGFLAGS = -Wall -Wextra -g -O0 -pthread -DDEBUG
else
    # Linux specific flags
    DEBUGFLAGS = -Wall -Wextra -g -O0 -pthread -DDEBUG -fsanitize=address
endif

LDFLAGS = -lm -pthread

# LED Matrix library (adjust path as needed)
LED_MATRIX_PATH = /home/pi/rpi-rgb-led-matrix
LED_MATRIX_INCLUDE = -I$(LED_MATRIX_PATH)/include
LED_MATRIX_LIB = -L$(LED_MATRIX_PATH)/lib -lrgbmatrix

# Source files
SERVER_SRC = server.c cJSON.c
CLIENT_SRC = client.c cJSON.c
BOARD_SRC = board.c cJSON.c

# Targets
TARGETS = server client board

# Default target
all: $(TARGETS)

# Server compilation
server: $(SERVER_SRC) cJSON.h
	@echo "Building server..."
	$(CC) $(CFLAGS) -o $@ $(SERVER_SRC) $(LDFLAGS)
	@echo "✓ Server built successfully"

# Client compilation
client: $(CLIENT_SRC) cJSON.h
	@echo "Building client with advanced AI..."
	$(CC) $(CFLAGS) -o $@ $(CLIENT_SRC) $(LDFLAGS)
	@echo "✓ Client built successfully"

# Board compilation (with LED Matrix library)
board: $(BOARD_SRC) cJSON.h led-matrix-c.h
	@echo "Building LED board display..."
	@if [ -d "$(LED_MATRIX_PATH)" ]; then \
		$(CC) $(CFLAGS) $(LED_MATRIX_INCLUDE) -o $@ $(BOARD_SRC) $(LDFLAGS) $(LED_MATRIX_LIB); \
		echo "✓ Board built successfully with LED support"; \
	else \
		echo "WARNING: LED Matrix library not found at $(LED_MATRIX_PATH)"; \
		echo "Building without LED support (console only)..."; \
		$(CC) $(CFLAGS) -DNO_LED_MATRIX -o $@ $(BOARD_SRC) $(LDFLAGS); \
		echo "✓ Board built successfully (console mode only)"; \
	fi

# Debug builds
debug: CFLAGS = $(DEBUGFLAGS)
debug: clean all
	@echo "✓ Debug build complete"

# Run complete system
run-all: all
	@echo "Starting OctaFlip system..."
	@echo "1. Starting board server..."
	@./board > board.log 2>&1 &
	@BOARD_PID=$$!; \
	sleep 1; \
	echo "2. Starting game server..."; \
	./server > server.log 2>&1 & \
	SERVER_PID=$$!; \
	sleep 1; \
	echo "3. Starting AI client 1..."; \
	./client -ip 127.0.0.1 -port 8080 -username AI1 -engine 6 > ai1.log 2>&1 & \
	CLIENT1_PID=$$!; \
	sleep 0.5; \
	echo "4. Starting AI client 2..."; \
	./client -ip 127.0.0.1 -port 8080 -username AI2 -engine 13 > ai2.log 2>&1 & \
	CLIENT2_PID=$$!; \
	echo ""; \
	echo "System started!"; \
	echo "PIDs: Board=$$BOARD_PID, Server=$$SERVER_PID, AI1=$$CLIENT1_PID, AI2=$$CLIENT2_PID"; \
	echo "Logs: board.log, server.log, ai1.log, ai2.log"; \
	echo ""; \
	echo "Press Ctrl+C to stop all processes"

# Run board display only
run-board: board
	@echo "Starting board display server..."
	@echo "Clients should connect to port 8081"
	./board

# Run board in manual mode
run-board-manual: board
	@echo "Starting board display in manual input mode..."
	./board -manual

# Quick test (server + 2 AIs without board)
test: server client
	@echo "Running quick test match..."
	@./server > test_server.log 2>&1 &
	@sleep 0.5
	@./client -username TestAI1 -engine 1 > test_ai1.log 2>&1 &
	@sleep 0.3
	@./client -username TestAI2 -engine 6 > test_ai2.log 2>&1 &
	@echo "Test match running... Check test_*.log files"

# Quick match with board display
match-with-board: all
	@echo "Starting match with LED board display..."
	@./board > board.log 2>&1 &
	@BOARD_PID=$$!; \
	sleep 1; \
	./server > server.log 2>&1 & \
	SERVER_PID=$$!; \
	sleep 0.5; \
	./client -ip 127.0.0.1 -port 8080 -username Player1 -engine 13 > player1.log 2>&1 & \
	CLIENT1_PID=$$!; \
	sleep 0.3; \
	./client -ip 127.0.0.1 -port 8080 -username Player2 -engine 6 > player2.log 2>&1 & \
	CLIENT2_PID=$$!; \
	echo "Match started with board display!"; \
	echo "Watch the LED matrix for live updates!"; \
	echo "PIDs: Board=$$BOARD_PID, Server=$$SERVER_PID"; \
	echo "Press Ctrl+C to stop"

# Memory check
memcheck: debug
	@echo "Running memory check on client..."
	@if command -v valgrind >/dev/null 2>&1; then \
		valgrind --leak-check=full --show-leak-kinds=all ./client -username TestAI -engine 6 & \
		sleep 2; \
		killall client 2>/dev/null || true; \
	else \
		echo "Valgrind not installed. Skipping memory check."; \
	fi

# Clean build files
clean:
	@echo "Cleaning build files..."
	@rm -f $(TARGETS) *.o core gmon.out
	@echo "✓ Clean complete"

# Clean logs
clean-logs:
	@echo "Cleaning log files..."
	@rm -f *.log
	@echo "✓ Logs cleaned"

# Clean everything
clean-all: clean clean-logs
	@echo "✓ Full clean complete"

# Installation
install: all
	@echo "Installing OctaFlip..."
	@mkdir -p ~/bin
	@cp server client board ~/bin/
	@echo "✓ Installed to ~/bin"
	@echo "Add ~/bin to your PATH to use from anywhere"

# Uninstall
uninstall:
	@echo "Uninstalling OctaFlip..."
	@rm -f ~/bin/server ~/bin/client ~/bin/board
	@echo "✓ Uninstalled"

# Kill all OctaFlip processes
killall:
	@echo "Stopping all OctaFlip processes..."
	@pkill -f "./board" 2>/dev/null || true
	@pkill -f "./server" 2>/dev/null || true
	@pkill -f "./client" 2>/dev/null || true
	@echo "✓ All processes stopped"

# Help
help:
	@echo "OctaFlip Makefile Commands:"
	@echo "  make              - Build all targets"
	@echo "  make debug        - Build with debug symbols"
	@echo "  make clean        - Remove executables"
	@echo "  make clean-all    - Remove executables and logs"
	@echo ""
	@echo "Running commands:"
	@echo "  make run-all      - Run complete system (board+server+2 AIs)"
	@echo "  make run-board    - Run board display server only"
	@echo "  make run-board-manual - Run board in manual input mode"
	@echo "  make test         - Run quick test (no board)"
	@echo "  make match-with-board - Run match with LED display"
	@echo ""
	@echo "Other commands:"
	@echo "  make memcheck     - Check memory leaks"
	@echo "  make install      - Install to ~/bin"
	@echo "  make uninstall    - Remove from ~/bin"
	@echo "  make killall      - Stop all OctaFlip processes"
	@echo "  make help         - Show this help"
	@echo ""
	@echo "LED Matrix Setup:"
	@echo "  Edit LED_MATRIX_PATH in Makefile to point to your"
	@echo "  rpi-rgb-led-matrix installation directory"

.PHONY: all debug clean clean-logs clean-all test memcheck install uninstall \
        run-all run-board run-board-manual match-with-board killall help