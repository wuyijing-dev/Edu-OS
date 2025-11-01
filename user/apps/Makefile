# ===========================================================================
# EduOS Apps Makefile
# ===========================================================================

CC := gcc
LD := ld
CFLAGS := -m32 -ffreestanding -nostdlib -nostdinc -fno-builtin \
          -fno-stack-protector -O2 -Wall -I../libc -I../libgui
LDFLAGS := -m elf_i386 -nostdlib -T ../user.ld

LIBC := ../build/libc/libc.a
LIBGUI := ../build/libgui/libgui.a

BUILD_DIR := ../build/apps

all: $(BUILD_DIR)/desktop.elf

# 桌面程序
$(BUILD_DIR)/desktop.elf: desktop/desktop.c $(LIBC) $(LIBGUI) | $(BUILD_DIR)
	@echo "Building desktop.elf..."
	$(CC) $(CFLAGS) -c desktop/desktop.c -o $(BUILD_DIR)/desktop.o
	$(LD) $(LDFLAGS) $(BUILD_DIR)/desktop.o $(LIBGUI) $(LIBC) -o $@
	@echo "✓ desktop.elf created"

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean

