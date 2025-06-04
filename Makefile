# OctaFlip Makefile - Assignment 3 Version (Fixed)
CC = gcc
CXX = g++

# Compiler flags
CFLAGS = -Wall -Wextra -O3 -pthread
DEBUGFLAGS = -Wall -Wextra -g -O0 -pthread -DDEBUG

# Platform detection
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    # macOS
    LDFLAGS = -lm -pthread
else
    # Linux (Raspberry Pi)
    LDFLAGS = -lm -pthread
    DEBUGFLAGS += -fsanitize=address
endif

# LED Matrix configuration
LED_MATRIX_PATH = /usr/local/include
LED_MATRIX_LIB_PATH = /usr/local/lib
LED_MATRIX_LIB = -lrgbmatrix
LED_MATRIX_EXISTS := $(shell test -f $(LED_MATRIX_PATH)/led-matrix-c.h && echo yes || echo no)

# Source files
SERVER_SRC = server.c cJSON.c
CLIENT_SRC = client.c cJSON.c
BOARD_SRC = board.c

# Targets
TARGETS = server client board

# Default target
all: $(TARGETS)

# Server compilation
server: $(SERVER_SRC) cJSON.h
	@echo "Building server..."
	$(CC) $(CFLAGS) -o $@ $(SERVER_SRC) $(LDFLAGS)
	@echo "✓ Server built successfully"

# Client compilation with board integration
client: $(CLIENT_SRC) board.c board.h cJSON.h
	@echo "Building client with LED board support..."
ifeq ($(LED_MATRIX_EXISTS),yes)
	$(CC) $(CFLAGS) -DHAS_LED_MATRIX -I$(LED_MATRIX_PATH) -o $@ $(CLIENT_SRC) board.c -L$(LED_MATRIX_LIB_PATH) $(LDFLAGS) $(LED_MATRIX_LIB)
else
	$(CC) $(CFLAGS) -o $@ $(CLIENT_SRC) board.c $(LDFLAGS)
endif
	@echo "✓ Client built successfully"

# Board standalone compilation (for grading)
board: $(BOARD_SRC) board.h
	@echo "Building standalone board display..."
ifeq ($(LED_MATRIX_EXISTS),yes)
	@echo "LED Matrix library found, building with LED support..."
	$(CC) $(CFLAGS) -DSTANDALONE_BUILD -DHAS_LED_MATRIX -I$(LED_MATRIX_PATH) -o $@ $(BOARD_SRC) -L$(LED_MATRIX_LIB_PATH) $(LDFLAGS) $(LED_MATRIX_LIB)
	@echo ""
	@echo "✓ Board display built with LED support"
	@echo ""
	@echo "IMPORTANT: Run with sudo for LED access:"
	@echo "  sudo ./board                    # Read from stdin (for grading)"
	@echo "  sudo ./board -manual            # Interactive input"
	@echo ""
else
	@echo "LED Matrix library not found, building console-only version..."
	$(CC) $(CFLAGS) -DSTANDALONE_BUILD -o $@ $(BOARD_SRC) $(LDFLAGS)
	@echo "✓ Board display built (console-only)"
endif

# Network-enabled board (optional)
board-network: $(BOARD_SRC) board.h cJSON.c cJSON.h
	@echo "Building board with network support..."
ifeq ($(LED_MATRIX_EXISTS),yes)
	$(CC) $(CFLAGS) -DSTANDALONE_BUILD -DENABLE_NETWORK_MODE -DHAS_LED_MATRIX -I$(LED_MATRIX_PATH) -o $@ $(BOARD_SRC) cJSON.c -L$(LED_MATRIX_LIB_PATH) $(LDFLAGS) $(LED_MATRIX_LIB)
else
	$(CC) $(CFLAGS) -DSTANDALONE_BUILD -DENABLE_NETWORK_MODE -o $@ $(BOARD_SRC) cJSON.c $(LDFLAGS)
endif
	@echo "✓ Network board built successfully"

# Debug builds
debug: CFLAGS = $(DEBUGFLAGS)
debug: clean all
	@echo "✓ Debug build complete"

# Test board with sample input
test-board: board
	@echo "Testing board display with sample input..."
	@echo "R......B" > test_board.txt
	@echo "........" >> test_board.txt
	@echo "........" >> test_board.txt
	@echo "........" >> test_board.txt
	@echo "........" >> test_board.txt
	@echo "........" >> test_board.txt
	@echo "........" >> test_board.txt
	@echo "B......R" >> test_board.txt
ifeq ($(LED_MATRIX_EXISTS),yes)
	@echo ""
	@echo "Running with LED support (requires sudo)..."
	sudo ./board < test_board.txt || ./board < test_board.txt
else
	./board < test_board.txt
endif
	@rm -f test_board.txt

# Test LED specifically
test-led: board
ifeq ($(LED_MATRIX_EXISTS),yes)
	@echo "Testing LED matrix with demo pattern..."
	@echo "R.B.R.B." > test_led.txt
	@echo ".B.R.B.R" >> test_led.txt
	@echo "R.B.R.B." >> test_led.txt
	@echo ".B.R.B.R" >> test_led.txt
	@echo "R.B.R.B." >> test_led.txt
	@echo ".B.R.B.R" >> test_led.txt
	@echo "R.B.R.B." >> test_led.txt
	@echo ".B.R.B.R" >> test_led.txt
	sudo ./board < test_led.txt
	@rm -f test_led.txt
else
	@echo "LED Matrix library not installed"
endif

# Test game
test-game: server client
	@echo "Starting test game..."
	@./server > server.log 2>&1 &
	@sleep 1
	@./client -username TestAI1 -engine 1 > ai1.log 2>&1 &
	@sleep 0.5
	@./client -username TestAI2 -engine 6 > ai2.log 2>&1 &
	@echo "Game running... Check *.log files"

# Clean
clean:
	@echo "Cleaning build files..."
	@rm -f $(TARGETS) board-network *.o core gmon.out
	@echo "✓ Clean complete"

# Clean logs
clean-logs:
	@echo "Cleaning log files..."
	@rm -f *.log test_board.txt test_led.txt
	@echo "✓ Logs cleaned"

# Full clean
clean-all: clean clean-logs
	@echo "✓ Full clean complete"

# Kill processes
kill-all:
	@echo "Stopping all OctaFlip processes..."
	@killall server 2>/dev/null || true
	@killall client 2>/dev/null || true
	@killall board 2>/dev/null || true
	@echo "✓ All processes stopped"

# Check LED matrix installation
check-led:
	@echo "Checking LED Matrix installation..."
	@echo -n "Header file: "
	@test -f $(LED_MATRIX_PATH)/led-matrix-c.h && echo "✓ Found" || echo "✗ Not found"
	@echo -n "Library file: "
	@test -f $(LED_MATRIX_LIB_PATH)/librgbmatrix.so && echo "✓ Found" || echo "✗ Not found"
	@echo -n "GPIO access: "
	@test -w /dev/gpiomem && echo "✓ Available" || echo "✗ Need sudo"
	@echo ""
	@echo "If LED Matrix is not found, install it:"
	@echo "  git clone https://github.com/hzeller/rpi-rgb-led-matrix"
	@echo "  cd rpi-rgb-led-matrix"
	@echo "  make"
	@echo "  sudo make install"

# Submission package
package:
	@echo "Creating submission package..."
	@mkdir -p octaflip_submission
	@cp client.c board.c board.h cJSON.c cJSON.h octaflip_submission/
	@cp Makefile octaflip_submission/
	@echo "# Compilation Instructions" > octaflip_submission/README.txt
	@echo "" >> octaflip_submission/README.txt
	@echo "## Using Makefile (Recommended):" >> octaflip_submission/README.txt
	@echo "make              # Build all components" >> octaflip_submission/README.txt
	@echo "make board        # Build standalone board for LED testing" >> octaflip_submission/README.txt
	@echo "sudo ./board      # Run with LED support (grading mode)" >> octaflip_submission/README.txt
	@echo "" >> octaflip_submission/README.txt
	@echo "## Manual compilation:" >> octaflip_submission/README.txt
	@echo "gcc -O3 -pthread client.c board.c cJSON.c -o client -lm" >> octaflip_submission/README.txt
	@echo "" >> octaflip_submission/README.txt
	@echo "## For LED matrix support:" >> octaflip_submission/README.txt
	@echo "gcc -O3 -pthread -DSTANDALONE_BUILD -DHAS_LED_MATRIX board.c -o board -lm -lrgbmatrix" >> octaflip_submission/README.txt
	@echo "sudo ./board" >> octaflip_submission/README.txt
	@echo "" >> octaflip_submission/README.txt
	@echo "## Team: Shannon" >> octaflip_submission/README.txt
	@zip -r hw4_teamleader_studentnumber_Shannon.zip octaflip_submission/
	@rm -rf octaflip_submission/
	@echo "✓ Package created: hw4_teamleader_studentnumber_Shannon.zip"

# Help
help:
	@echo "OctaFlip Makefile - Assignment 3"
	@echo ""
	@echo "Main targets:"
	@echo "  make              - Build all (server, client, board)"
	@echo "  make server       - Build game server"
	@echo "  make client       - Build AI client with LED support"
	@echo "  make board        - Build standalone board display"
	@echo ""
	@echo "Testing:"
	@echo "  make test-board   - Test board with sample input"
	@echo "  make test-led     - Test LED with pattern"
	@echo "  make test-game    - Run a test game"
	@echo "  make check-led    - Check LED matrix installation"
	@echo ""
	@echo "Utilities:"
	@echo "  make debug        - Build with debug symbols"
	@echo "  make clean        - Remove executables"
	@echo "  make clean-all    - Remove executables and logs"
	@echo "  make kill-all     - Stop all processes"
	@echo "  make package      - Create submission zip file"
	@echo ""
	@echo "Board usage:"
	@echo "  sudo ./board           - Read 8 lines from stdin (for grading)"
	@echo "  sudo ./board -manual   - Interactive input mode"
	@echo "  sudo ./board -network  - Network server mode (if enabled)"
	@echo ""
	@echo "LED Matrix Notes:"
	@echo "  - Must run with sudo for GPIO access"
	@echo "  - Hardware pulse is disabled to avoid permission issues"
	@echo "  - If problems persist, try: --led-slowdown-gpio=5"

.PHONY: all debug clean clean-logs clean-all test-board test-led test-game kill-all package help check-led