CC      = gcc
EXTRA_INCLUDES = -I../include
CFLAGS += -Wall -Wextra -O2 $(EXTRA_INCLUDES)
LDFLAGS = -lgpiod

SRC_DIR = ../src
OBJ_DIR = obj

SRCS = main.c \
       $(SRC_DIR)/ili9488_spi.c \
       $(SRC_DIR)/ili9488_hal.c

OBJS = $(OBJ_DIR)/main.o \
       $(OBJ_DIR)/ili9488_spi.o \
       $(OBJ_DIR)/ili9488_hal.o

TARGET = ili9488_demo

.PHONY: all clean

all: $(OBJ_DIR) $(TARGET)

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

$(OBJ_DIR)/main.o: main.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/ili9488_spi.o: $(SRC_DIR)/ili9488_spi.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/ili9488_hal.o: $(SRC_DIR)/ili9488_hal.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(TARGET)
