# ===========================================================================
# EduOS User Space - libc Makefile
# ===========================================================================

CC := gcc
AR := ar
CFLAGS := -m32 -ffreestanding -nostdlib -nostdinc -fno-builtin \
          -fno-stack-protector -O2 -Wall \
          -I. -Istdio -Istdlib -Istring -Isyscall -Ierrno

BUILD_DIR := ../build/libc

SOURCES := stdio/stdio.c stdlib/stdlib.c \
           string/string.c string/str.c string/memcpy.c string/strdup.c \
           syscall/syscall.c \
           ctype/ctype.c time/time.c errno/errno.c

OBJECTS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(notdir $(SOURCES)))

all: $(BUILD_DIR)/libc.a

$(BUILD_DIR)/libc.a: $(OBJECTS)
	@echo "Creating libc.a..."
	$(AR) rcs $@ $^
	@echo "✓ libc.a created"

$(BUILD_DIR)/stdio.o: stdio/stdio.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/stdlib.o: stdlib/stdlib.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/string.o: string/string.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/syscall.o: syscall/syscall.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/str.o: string/str.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/memcpy.o: string/memcpy.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/ctype.o: ctype/ctype.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/time.o: time/time.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/strdup.o: string/strdup.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/errno.o: errno/errno.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean

