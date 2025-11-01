# Makefile for desktop

# 工具链配置
CC = gcc
LD = ld
CFLAGS = -m32 -ffreestanding -nostdlib -fno-builtin -fno-stack-protector \
         -I../../libc/include -I../../libc/syscall -I../../libc/string \
         -I../../libc/stdio -I../../libc/ctype -I../../libc/time \
         -I../../libc/errno -I../../libc/signal -I../../libc/pthread \
         -I../../libgui
LDFLAGS = -m elf_i386 -nostdlib -e _start

# 目标文件
TARGET = desktop.elf
OBJS = desktop.o

# 库路径
LIBC = ../../build/libc/libc.a
LIBGUI = ../../build/libgui/libgui.a

# 默认目标
all: $(TARGET)

# 链接
$(TARGET): $(OBJS) $(LIBC) $(LIBGUI)
	$(LD) $(LDFLAGS) -o $@ $(OBJS) $(LIBGUI) $(LIBC)

# 编译
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# 清理
clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean

