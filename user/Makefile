# 用户程序 Makefile

CC := gcc
LD := ld

# 用户程序编译选项
CFLAGS := -m32 -ffreestanding -nostdlib -nostdinc \
          -fno-builtin -fno-stack-protector \
          -fno-pic -fno-pie -O0 -g

# 链接选项
LDFLAGS := -m elf_i386 -nostdlib \
           -Ttext=0x08000000 \
           -e _start

all: hello.elf

hello.o: hello.c
	$(CC) $(CFLAGS) -c hello.c -o hello.o

hello.elf: hello.o
	$(LD) $(LDFLAGS) hello.o -o hello.elf
	@echo "User program created: hello.elf"
	@readelf -h hello.elf
	@readelf -l hello.elf

clean:
	rm -f *.o *.elf

.PHONY: all clean

