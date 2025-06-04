# OctaFlip Makefile - Assignment 3
CC = gcc
CXX = g++

# Compiler flags
CFLAGS = -O3 -pthread
LDFLAGS = -lm -pthread

# LED Matrix paths
LED_MATRIX_PATH = ./rpi-rgb-led-matrix/include
LED_MATRIX_LIB_PATH = ./rpi-rgb-led-matrix/lib

# Check if LED Matrix exists
LED_EXISTS := $(shell test -f $(LED_MATRIX_PATH)/led-matrix-c.h && echo yes || echo no)
ifeq ($(LED_EXISTS),no)
    LED_MATRIX_PATH = /usr/local/include
    LED_MATRIX_LIB_PATH = /usr/local/lib
    LED_EXISTS := $(shell test -f $(LED_MATRIX_PATH)/led-matrix-c.h && echo yes || echo no)
endif

# Check if NNUE weights exist
NNUE_EXISTS := $(shell test -f nnue_weights_generated.h && echo yes || echo no)

# Default target
all: server client board

# Server (C only)
server: server.c cJSON.c cJSON.h
	@echo "Building server with GCC..."
	$(CC) $(CFLAGS) server.c cJSON.c -o server $(LDFLAGS)

# Client (C only)
# Client with LED support (compile with gcc, link with g++)
client: client.o board.o cJSON.o
ifeq ($(LED_EXISTS),yes)
	@echo "Building client with LED support (C compiled, C++ linked)..."
	$(CXX) -o client client.o board.o cJSON.o \
		-DHAS_LED_MATRIX \
		-I$(LED_MATRIX_PATH) -L$(LED_MATRIX_LIB_PATH) \
		-lrgbmatrix -lrt -lm -lpthread
else
	@echo "Building client without LED support (all C)..."
	$(CC) $(CFLAGS) client.o board.o cJSON.o -o client $(LDFLAGS)
endif

client.o: client.c board.h
	$(CC) $(CFLAGS) -c client.c -o client.o

board.o: board.c board.h
	$(CC) $(CFLAGS) -DHAS_LED_MATRIX -c board.c -o board.o

cJSON.o: cJSON.c cJSON.h
	$(CC) $(CFLAGS) -c cJSON.c -o cJSON.o


ifeq ($(NNUE_EXISTS),yes)
	@echo "NNUE weights found - AI will use neural network evaluation"
else
	@echo "Warning: NNUE weights not found - AI will use classical evaluation"
	@echo "Run 'python3 train_octoflip.py' to generate NNUE weights"
endif

# Board (C++ only)
board: board.c board.h
ifeq ($(LED_EXISTS),yes)
	@echo "Building standalone board with LED support using G++..."
	$(CXX) $(CFLAGS) board.c -o board -DSTANDALONE_BUILD -DHAS_LED_MATRIX \
		-I$(LED_MATRIX_PATH) -L$(LED_MATRIX_LIB_PATH) \
		-lrgbmatrix -lrt -lm -lpthread
else
	@echo "Building standalone board without LED support using G++..."
	$(CXX) $(CFLAGS) board.c -o board -DSTANDALONE_BUILD $(LDFLAGS)
endif

# Client without LED (for testing)
client-no-led: client.c board.c cJSON.c board.h cJSON.h
	@echo "Building client without LED support (forced) using GCC..."
	$(CC) $(CFLAGS) client.c board.c cJSON.c -o client-no-led $(LDFLAGS)

# Generate NNUE weights
nnue:
	@echo "Generating NNUE weights (this may take a while)..."
	python3 train_octoflip.py

# Clean
clean:
	rm -f server client board client-no-led *.o

# Deep clean (including generated files)
deepclean: clean
	rm -f nnue_weights_generated.h best_nnue_deep.pkl.gz training_stats_deep.json

# Install LED library
install-led-lib:
	@echo "Installing RGB LED Matrix library..."
	git clone https://github.com/hzeller/rpi-rgb-led-matrix.git
	cd rpi-rgb-led-matrix && make
	@echo "LED library installed locally"

# Help
help:
	@echo "OctaFlip Build System"
	@echo "===================="
	@echo ""
	@echo "Main targets:"
	@echo "  make              # Build all components"
	@echo "  make server       # Build game server only (GCC)"
	@echo "  make client       # Build game client only (GCC)"
	@echo "  make board        # Build standalone board (G++)"
	@echo ""
	@echo "Additional targets:"
	@echo "  make client-no-led    # Build client without LED support"
	@echo "  make nnue            # Generate NNUE weights for AI"
	@echo "  make install-led-lib # Download and build LED library"
	@echo "  make clean           # Remove executables"
	@echo "  make deepclean       # Remove all generated files"
	@echo ""
	@echo "Running:"
	@echo "  ./server                                          # Start game server"
	@echo "  ./client -ip 10.8.128.233 -port 8080 -username NAME  # Connect to server"
	@echo "  sudo ./client [options]                           # With LED support"
	@echo "  sudo ./board                                      # Test LED display"
	@echo ""
	@echo "Current configuration:"
	@echo "  LED support: $(LED_EXISTS)"
ifeq ($(LED_EXISTS),yes)
	@echo "  LED library path: $(LED_MATRIX_PATH)"
endif
	@echo "  NNUE weights: $(NNUE_EXISTS)"

# Check system
check:
	@echo "System Check"
	@echo "============"
	@echo "LED Matrix library: $(LED_EXISTS)"
ifeq ($(LED_EXISTS),yes)
	@echo "  Include: $(LED_MATRIX_PATH)"
	@echo "  Library: $(LED_MATRIX_LIB_PATH)"
	@ls -la $(LED_MATRIX_LIB_PATH)/librgbmatrix* 2>/dev/null || echo "  Warning: Library files not found"
else
	@echo "  Not found - run 'make install-led-lib' to install"
endif
	@echo ""
	@echo "NNUE weights: $(NNUE_EXISTS)"
ifeq ($(NNUE_EXISTS),no)
	@echo "  Run 'make nnue' to generate (takes ~1 hour for quick test)"
endif
	@echo ""
	@echo "Python3: $(shell which python3 > /dev/null && echo "Found" || echo "Not found")"
	@echo "GCC: $(shell $(CC) --version | head -n1)"
	@echo "G++: $(shell $(CXX) --version | head -n1)"

.PHONY: all clean deepclean help check install-led-lib nnue client-no-led
