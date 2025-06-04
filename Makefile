# OctaFlip Makefile - Assignment 3
CC = gcc
CXX = g++

# Compiler flags (conditionally extended below)
CFLAGS = -O3 -pthread
LDFLAGS = -lm -pthread

# LED Matrix paths
LED_MATRIX_PATH = ./rpi-rgb-led-matrix/include
LED_MATRIX_LIB_PATH = ./rpi-rgb-led-matrix/lib

# Detect LED support
LED_EXISTS := $(shell test -f $(LED_MATRIX_PATH)/led-matrix-c.h && echo yes || echo no)
ifeq ($(LED_EXISTS),no)
    LED_MATRIX_PATH = /usr/local/include
    LED_MATRIX_LIB_PATH = /usr/local/lib
    LED_EXISTS := $(shell test -f $(LED_MATRIX_PATH)/led-matrix-c.h && echo yes || echo no)
endif

# Detect NNUE support
NNUE_EXISTS := $(shell test -f nnue_weights_generated.h && echo yes || echo no)

# Add flags based on detection
ifeq ($(LED_EXISTS),yes)
    CFLAGS += -DHAS_LED_MATRIX -I$(LED_MATRIX_PATH)
    LDFLAGS += -L$(LED_MATRIX_LIB_PATH) -lrgbmatrix -lrt
endif
ifeq ($(NNUE_EXISTS),yes)
    CFLAGS += -DHAS_NNUE_WEIGHTS
endif

# Default target
all: server client board

# Server (C only)
server: server.c cJSON.c cJSON.h
	@echo "Building server..."
	$(CC) $(CFLAGS) server.c cJSON.c -o server $(LDFLAGS)

# Client target
client: client.o board.o cJSON.o
ifeq ($(LED_EXISTS),yes)
	@echo "Building client with LED support (linked with G++)..."
	$(CXX) $(CFLAGS) -o client client.o board.o cJSON.o $(LDFLAGS)
else
	@echo "Building client without LED support..."
	$(CC) $(CFLAGS) -o client client.o board.o cJSON.o $(LDFLAGS)
endif

client.o: client.c board.h
	$(CC) $(CFLAGS) -c client.c -o client.o

board.o: board.c board.h
	$(CC) $(CFLAGS) -c board.c -o board.o

cJSON.o: cJSON.c cJSON.h
	$(CC) $(CFLAGS) -c cJSON.c -o cJSON.o

# Board viewer (standalone)
board: board.c board.h
ifeq ($(LED_EXISTS),yes)
	@echo "Building standalone board with LED using G++..."
	$(CXX) $(CFLAGS) -DSTANDALONE_BUILD -o board board.c $(LDFLAGS)
else
	@echo "Building standalone board without LED..."
	$(CXX) $(CFLAGS) -DSTANDALONE_BUILD -o board board.c $(LDFLAGS)
endif

# Client no LED (forced)
client-no-led:
	@echo "Forcing client build without LED..."
	$(CC) -O3 -pthread -o client-no-led client.c board.c cJSON.c $(LDFLAGS)

# Generate NNUE weights
nnue:
	@echo "Generating NNUE weights..."
	python3 train_octoflip.py

# Clean
clean:
	rm -f server client board client-no-led *.o

# Deep clean
deepclean: clean
	rm -f nnue_weights_generated.h best_nnue_deep.pkl.gz training_stats_deep.json

# Install LED lib
install-led-lib:
	@echo "Cloning and building LED matrix library..."
	git clone https://github.com/hzeller/rpi-rgb-led-matrix.git
	cd rpi-rgb-led-matrix && make

# Help
help:
	@echo "OctaFlip Build Targets"
	@echo "======================="
	@echo "make                - Build everything"
	@echo "make server         - Build server only"
	@echo "make client         - Build client with optional LED/NNUE"
	@echo "make board          - Build standalone board viewer"
	@echo "make nnue           - Generate NNUE weights"
	@echo "make install-led-lib - Clone and build LED lib"
	@echo "make clean          - Clean builds"
	@echo "make deepclean      - Clean + remove NNUE generated files"
	@echo "make help           - Show this message"
	@echo ""
	@echo "Auto-config:"
	@echo "  LED support: $(LED_EXISTS)"
	@echo "  NNUE weights: $(NNUE_EXISTS)"

# Status Check
check:
	@echo "System Check:"
	@echo "LED Matrix library: $(LED_EXISTS)"
ifeq ($(LED_EXISTS),yes)
	@echo "  Include: $(LED_MATRIX_PATH)"
	@echo "  Library: $(LED_MATRIX_LIB_PATH)"
	@ls -la $(LED_MATRIX_LIB_PATH)/librgbmatrix* 2>/dev/null || echo "  ⚠️  Library missing"
endif
	@echo "NNUE weights: $(NNUE_EXISTS)"
ifeq ($(NNUE_EXISTS),no)
	@echo "  Run 'make nnue' to generate"
endif
	@echo "Python3: $(shell which python3 > /dev/null && echo Found || echo Not found)"
	@echo "GCC: $(shell $(CC) --version | head -n1)"
	@echo "G++: $(shell $(CXX) --version | head -n1)"

.PHONY: all clean deepclean help check install-led-lib nnue client-no-led
