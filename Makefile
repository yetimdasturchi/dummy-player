CC      = gcc

SRC_DIR = src
INC_DIR = inc
BIN     = player

PKG_CFLAGS = $(shell pkg-config --cflags sdl2 SDL2_ttf)
PKG_LIBS   = $(shell pkg-config --libs sdl2 SDL2_ttf)

CFLAGS  = -Wall -Wextra -std=c11 -O2 $(PKG_CFLAGS) -I$(INC_DIR)
LDFLAGS = $(PKG_LIBS) \
          -lavformat -lavcodec -lavutil -lswscale -lswresample -lm -lpthread

SRC = $(wildcard $(SRC_DIR)/*.c)

OBJ = $(SRC:$(SRC_DIR)/%.c=$(SRC_DIR)/%.o)

.PHONY: all clean

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

$(SRC_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(BIN)