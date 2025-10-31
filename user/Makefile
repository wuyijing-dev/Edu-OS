# 用户程序 Makefile

CC := gcc
LD := ld

# 用户程序编译选项
CFLAGS := -m32 -ffreestanding -nostdlib -nostdinc \
          -fno-builtin -fno-stack-protector \
          -fno-pic -fno-pie -O0 -g

# 链接选项（Linux风格）
LDFLAGS := -m elf_i386 -nostdlib \
           -T user.ld

# 库路径
USERLIB := ../userlib
LIBC := $(USERLIB)/libc.a

all: hello.elf gui_demo.elf

# Hello程序
hello.o: hello.c
	$(CC) $(CFLAGS) -I$(USERLIB) -c hello.c -o hello.o

hello.elf: hello.o $(LIBC)
	$(LD) $(LDFLAGS) hello.o $(LIBC) -o hello.elf
	@echo "✅ User program created: hello.elf"
	@readelf -l hello.elf | head -15

# GUI Demo程序
gui_demo.o: gui_demo.c
	$(CC) $(CFLAGS) -I$(USERLIB) -c gui_demo.c -o gui_demo.o

gui_demo.elf: gui_demo.o $(LIBC)
	$(LD) $(LDFLAGS) gui_demo.o $(LIBC) -o gui_demo.elf
	@echo "✅ GUI Demo created: gui_demo.elf"
	@readelf -l gui_demo.elf | head -15
	@echo ""
	@echo "📊 ELF Segments:"
	@readelf -l gui_demo.elf | grep LOAD

clean:
	rm -f *.o *.elf

.PHONY: all clean

