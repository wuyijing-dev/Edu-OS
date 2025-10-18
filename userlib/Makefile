# Makefile for EduOS User Library (libc)

CC = gcc
AR = ar
CFLAGS = -m32 -ffreestanding -nostdlib -nostdinc -fno-builtin -fno-stack-protector -O2 -Wall -I.
ARFLAGS = rcs

# 源文件
SRCS = string.c stdio.c stdlib.c syscall.c
OBJS = $(SRCS:.c=.o)

# 目标库
TARGET = libc.a

all: $(TARGET)

$(TARGET): $(OBJS)
	@echo "Creating static library: $@"
	$(AR) $(ARFLAGS) $@ $^
	@echo "✓ libc.a created successfully"

%.o: %.c
	@echo "Compiling: $<"
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
