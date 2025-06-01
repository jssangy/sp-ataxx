# OctaFlip Makefile (macOS Compatible)
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

# Source files
SERVER_SRC = server.c cJSON.c
CLIENT_SRC = client.c cJSON.c

# Targets
TARGETS = server client

# Default target
all: $(TARGETS)

# Server compilation
server: $(SERVER_SRC) cJSON.h
	@echo "Building server..."
	$(CC) $(CFLAGS) -o $@ $(SERVER_SRC) $(LDFLAGS)
	@echo "✓ Server built successfully"

# Client compilation (without OpenMP for macOS compatibility)
client: $(CLIENT_SRC) cJSON.h
	@echo "Building client with advanced AI..."
	$(CC) $(CFLAGS) -o $@ $(CLIENT_SRC) $(LDFLAGS)
	@echo "✓ Client built successfully"

# Debug builds
debug: CFLAGS = $(DEBUGFLAGS)
debug: clean all
	@echo "✓ Debug build complete"

# Quick test match
test: all
	@echo "Running quick test match..."
	@./server > test_server.log 2>&1 &
	@sleep 0.5
	@./client -username TestAI1 -engine 1 > test_ai1.log 2>&1 &
	@sleep 0.3
	@./client -username TestAI2 -engine 6 > test_ai2.log 2>&1 &
	@echo "Test match running... Check test_*.log files"

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
	@cp server client ~/bin/
	@echo "✓ Installed to ~/bin"
	@echo "Add ~/bin to your PATH to use from anywhere"

# Uninstall
uninstall:
	@echo "Uninstalling OctaFlip..."
	@rm -f ~/bin/server ~/bin/client
	@echo "✓ Uninstalled"

# Quick match between two AI engines
quick-match: all
	@echo "Starting quick match..."
	@./server > server.log 2>&1 &
	@SERVER_PID=$$!; \
	sleep 0.5; \
	./client -ip 127.0.0.1 -port 8080 -username AI1 -engine 1 > ai1.log 2>&1 & \
	CLIENT1_PID=$$!; \
	sleep 0.3; \
	./client -ip 127.0.0.1 -port 8080 -username AI2 -engine 6 > ai2.log 2>&1 & \
	CLIENT2_PID=$$!; \
	echo "Match started. PIDs: Server=$$SERVER_PID, AI1=$$CLIENT1_PID, AI2=$$CLIENT2_PID"; \
	echo "Logs: server.log, ai1.log, ai2.log"; \
	echo "Press Ctrl+C to stop"

# Help
help:
	@echo "OctaFlip Makefile Commands:"
	@echo "  make              - Build all targets"
	@echo "  make debug        - Build with debug symbols"
	@echo "  make clean        - Remove executables"
	@echo "  make clean-all    - Remove executables and logs"
	@echo "  make test         - Run a quick test match"
	@echo "  make quick-match  - Run a match between two AIs"
	@echo "  make memcheck     - Check memory leaks (requires valgrind)"
	@echo "  make install      - Install to ~/bin"
	@echo "  make uninstall    - Remove from ~/bin"
	@echo "  make help         - Show this help"

.PHONY: all debug clean clean-logs clean-all test memcheck install uninstall quick-match help