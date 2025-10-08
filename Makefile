# ===========================================================================
# EduOS Makefile - 第7章：虚拟文件系统（VFS）核心
# ===========================================================================
# 
# 编译内容：
#   - Stage 1: MBR引导扇区
#   - Stage 2: 加载器（切换到保护模式+分页）
#   - 内核: VGA、串口、中断、定时器、键盘、内存管理、进程管理、VFS

# 工具链
AS := nasm
CC := gcc
LD := ld
QEMU := qemu-system-i386

# 目录
BUILD_DIR := build
BOOT_DIR := boot
KERNEL_DIR := kernel
INCLUDE_DIR := include
LIB_DIR := lib

# 编译选项
ASFLAGS_BIN := -f bin
ASFLAGS_ELF := -f elf32 -g -F dwarf
CFLAGS := -m32 -ffreestanding -fno-pie -fno-pic -g -O0 \
          -Wall -Wextra -nostdlib -nostdinc -fno-builtin \
          -fno-stack-protector -I$(INCLUDE_DIR)
LDFLAGS := -m elf_i386 -nostdlib -T linker.ld

# 源文件
BOOT_STAGE1_ASM := $(BOOT_DIR)/stage1/boot_stage1.asm
BOOT_STAGE2_ASM := $(BOOT_DIR)/stage2/loader_complete.asm
KERNEL_ENTRY_ASM := $(KERNEL_DIR)/arch/i386/entry.asm
INTERRUPTS_ASM := $(KERNEL_DIR)/arch/i386/interrupts.asm
CONTEXT_SWITCH_ASM := $(KERNEL_DIR)/process/context_switch.asm

# C源文件
KERNEL_C_FILES := \
	$(KERNEL_DIR)/main.c \
	$(KERNEL_DIR)/vga.c \
	$(KERNEL_DIR)/serial.c \
	$(KERNEL_DIR)/kernel.c \
	$(KERNEL_DIR)/arch/i386/idt.c \
	$(KERNEL_DIR)/arch/i386/pic.c \
	$(KERNEL_DIR)/arch/i386/irq.c \
	$(KERNEL_DIR)/arch/i386/exception.c \
	$(KERNEL_DIR)/arch/i386/interrupt_dispatch.c \
	$(KERNEL_DIR)/drivers/timer.c \
	$(KERNEL_DIR)/drivers/keyboard.c \
	$(KERNEL_DIR)/mm/pmm.c \
	$(KERNEL_DIR)/mm/vmm.c \
	$(KERNEL_DIR)/mm/kmalloc.c \
	$(KERNEL_DIR)/process/process.c \
	$(KERNEL_DIR)/process/scheduler.c \
	$(KERNEL_DIR)/process/priority_sched.c \
	$(KERNEL_DIR)/process/mlfq_sched.c \
	$(LIB_DIR)/string.c \
	$(LIB_DIR)/libgcc_compat.c

# 目标文件
BOOT_STAGE1_BIN := $(BUILD_DIR)/boot_stage1.bin
BOOT_STAGE2_BIN := $(BUILD_DIR)/boot_stage2.bin
KERNEL_ENTRY_O := $(BUILD_DIR)/entry.o
INTERRUPTS_O := $(BUILD_DIR)/interrupts.o
CONTEXT_SWITCH_O := $(BUILD_DIR)/context_switch.o
KERNEL_C_O := $(patsubst %.c,$(BUILD_DIR)/%.o,$(notdir $(KERNEL_C_FILES)))

ALL_KERNEL_O := $(KERNEL_ENTRY_O) $(INTERRUPTS_O) $(CONTEXT_SWITCH_O) $(KERNEL_C_O)

KERNEL_ELF := $(BUILD_DIR)/kernel.elf
KERNEL_BIN := $(BUILD_DIR)/kernel.bin
OS_IMG := $(BUILD_DIR)/eduos.img

# 颜色
RED := \033[0;31m
GREEN := \033[0;32m
YELLOW := \033[1;33m
BLUE := \033[0;34m
CYAN := \033[0;36m
NC := \033[0m

# 默认目标
.PHONY: all clean run debug help info

all: $(BUILD_DIR) $(OS_IMG)
	@echo -e "$(GREEN)=========================================$(NC)"
	@echo -e "$(GREEN) EduOS Chapter 4 构建成功！$(NC)"
	@echo -e "$(GREEN)=========================================$(NC)"
	@echo -e "$(CYAN)提示：运行 'make run' 启动虚拟机$(NC)"

# 创建构建目录
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

# ===========================================================================
# Stage 1: MBR引导扇区 (512字节)
# ===========================================================================
$(BOOT_STAGE1_BIN): $(BOOT_STAGE1_ASM) | $(BUILD_DIR)
	@echo -e "$(BLUE)编译 Stage 1 (MBR)...$(NC)"
	$(AS) $(ASFLAGS_BIN) $< -o $@
	@size=$$(stat -c%s $@); \
	if [ $$size -ne 512 ]; then \
		echo -e "$(RED)错误: Stage 1 大小为 $$size 字节（必须是512字节）$(NC)"; \
		exit 1; \
	fi
	@echo -e "$(GREEN)✓ Stage 1 编译完成 (512字节)$(NC)"

# ===========================================================================
# Stage 2: 加载器 (切换到保护模式)
# ===========================================================================
$(BOOT_STAGE2_BIN): $(BOOT_STAGE2_ASM) | $(BUILD_DIR)
	@echo -e "$(BLUE)编译 Stage 2 (Loader)...$(NC)"
	$(AS) $(ASFLAGS_BIN) $< -o $@
	@size=$$(stat -c%s $@); \
	echo -e "$(GREEN)✓ Stage 2 编译完成 ($$size字节)$(NC)"

# ===========================================================================
# 内核入口和中断处理 (汇编)
# ===========================================================================
$(KERNEL_ENTRY_O): $(KERNEL_ENTRY_ASM) | $(BUILD_DIR)
	@echo -e "$(BLUE)编译内核入口 (entry.asm)...$(NC)"
	$(AS) $(ASFLAGS_ELF) $< -o $@

$(INTERRUPTS_O): $(INTERRUPTS_ASM) | $(BUILD_DIR)
	@echo -e "$(BLUE)编译中断处理程序 (interrupts.asm)...$(NC)"
	$(AS) $(ASFLAGS_ELF) $< -o $@

$(CONTEXT_SWITCH_O): $(CONTEXT_SWITCH_ASM) | $(BUILD_DIR)
	@echo -e "$(BLUE)编译上下文切换 (context_switch.asm)...$(NC)"
	$(AS) $(ASFLAGS_ELF) $< -o $@

# ===========================================================================
# 内核C文件编译规则
# ===========================================================================
$(BUILD_DIR)/main.o: $(KERNEL_DIR)/main.c | $(BUILD_DIR)
	@echo -e "$(BLUE)编译 $<$(NC)"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/vga.o: $(KERNEL_DIR)/vga.c | $(BUILD_DIR)
	@echo -e "$(BLUE)编译 $<$(NC)"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/serial.o: $(KERNEL_DIR)/serial.c | $(BUILD_DIR)
	@echo -e "$(BLUE)编译 $<$(NC)"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel.o: $(KERNEL_DIR)/kernel.c | $(BUILD_DIR)
	@echo -e "$(BLUE)编译 $<$(NC)"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/string.o: $(LIB_DIR)/string.c | $(BUILD_DIR)
	@echo -e "$(BLUE)编译 $<$(NC)"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/libgcc_compat.o: $(LIB_DIR)/libgcc_compat.c | $(BUILD_DIR)
	@echo -e "$(BLUE)编译 $< (64位运算库)$(NC)"
	$(CC) $(CFLAGS) -c $< -o $@

# 第3章新增的中断系统文件
$(BUILD_DIR)/idt.o: $(KERNEL_DIR)/arch/i386/idt.c | $(BUILD_DIR)
	@echo -e "$(BLUE)编译 $<$(NC)"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/pic.o: $(KERNEL_DIR)/arch/i386/pic.c | $(BUILD_DIR)
	@echo -e "$(BLUE)编译 $<$(NC)"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/irq.o: $(KERNEL_DIR)/arch/i386/irq.c | $(BUILD_DIR)
	@echo -e "$(BLUE)编译 $<$(NC)"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/exception.o: $(KERNEL_DIR)/arch/i386/exception.c | $(BUILD_DIR)
	@echo -e "$(BLUE)编译 $<$(NC)"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/interrupt_dispatch.o: $(KERNEL_DIR)/arch/i386/interrupt_dispatch.c | $(BUILD_DIR)
	@echo -e "$(BLUE)编译 $<$(NC)"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/timer.o: $(KERNEL_DIR)/drivers/timer.c | $(BUILD_DIR)
	@echo -e "$(BLUE)编译 $<$(NC)"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/keyboard.o: $(KERNEL_DIR)/drivers/keyboard.c | $(BUILD_DIR)
	@echo -e "$(BLUE)编译 $<$(NC)"
	$(CC) $(CFLAGS) -c $< -o $@

# 第4章新增的内存管理文件
$(BUILD_DIR)/pmm.o: $(KERNEL_DIR)/mm/pmm.c | $(BUILD_DIR)
	@echo -e "$(BLUE)编译 $< (PMM)$(NC)"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/vmm.o: $(KERNEL_DIR)/mm/vmm.c | $(BUILD_DIR)
	@echo -e "$(BLUE)编译 $< (VMM)$(NC)"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kmalloc.o: $(KERNEL_DIR)/mm/kmalloc.c | $(BUILD_DIR)
	@echo -e "$(BLUE)编译 $< (kmalloc)$(NC)"
	$(CC) $(CFLAGS) -c $< -o $@

# 第5章新增的进程管理文件
$(BUILD_DIR)/process.o: $(KERNEL_DIR)/process/process.c | $(BUILD_DIR)
	@echo -e "$(BLUE)编译 $< (Process Manager)$(NC)"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/scheduler.o: $(KERNEL_DIR)/process/scheduler.c | $(BUILD_DIR)
	@echo -e "$(BLUE)编译 $< (Scheduler)$(NC)"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/priority_sched.o: $(KERNEL_DIR)/process/priority_sched.c | $(BUILD_DIR)
	@echo -e "$(BLUE)编译 $< (Priority Scheduler)$(NC)"
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/mlfq_sched.o: $(KERNEL_DIR)/process/mlfq_sched.c | $(BUILD_DIR)
	@echo -e "$(BLUE)编译 $< (MLFQ Scheduler)$(NC)"
	$(CC) $(CFLAGS) -c $< -o $@

# ===========================================================================
# 链接内核ELF
# ===========================================================================
$(KERNEL_ELF): $(ALL_KERNEL_O) linker.ld | $(BUILD_DIR)
	@echo -e "$(BLUE)链接内核 ELF...$(NC)"
	$(LD) $(LDFLAGS) $(ALL_KERNEL_O) -o $@
	@echo -e "$(GREEN)✓ 内核 ELF 链接完成$(NC)"

# ===========================================================================
# 提取内核二进制
# ===========================================================================
$(KERNEL_BIN): $(KERNEL_ELF) | $(BUILD_DIR)
	@echo -e "$(BLUE)提取内核二进制文件...$(NC)"
	objcopy -O binary $< $@
	@size=$$(stat -c%s $@); \
	echo -e "$(GREEN)✓ 内核二进制提取完成 ($$size字节)$(NC)"

# ===========================================================================
# 创建最终OS镜像
# ===========================================================================
$(OS_IMG): $(BOOT_STAGE1_BIN) $(BOOT_STAGE2_BIN) $(KERNEL_BIN) | $(BUILD_DIR)
	@echo -e "$(YELLOW)=========================================$(NC)"
	@echo -e "$(YELLOW) 创建 EduOS 启动镜像$(NC)"
	@echo -e "$(YELLOW)=========================================$(NC)"
	
	# 创建1.44MB软盘镜像
	dd if=/dev/zero of=$@ bs=512 count=2880 2>/dev/null
	
	# 写入Stage 1（扇区0）
	dd if=$(BOOT_STAGE1_BIN) of=$@ conv=notrunc bs=512 seek=0 2>/dev/null
	
	# 写入Stage 2（扇区1-4）
	dd if=$(BOOT_STAGE2_BIN) of=$@ conv=notrunc bs=512 seek=1 2>/dev/null
	
	# 写入内核（扇区5+）
	dd if=$(KERNEL_BIN) of=$@ conv=notrunc seek=5 bs=512 2>/dev/null
	
	@echo -e "$(GREEN)=========================================$(NC)"
	@echo -e "$(GREEN)✓ 磁盘镜像创建完成$(NC)"
	@echo -e "$(CYAN)镜像布局:$(NC)"
	@echo -e "$(CYAN)  扇区 0:   Stage 1 引导扇区 (512字节)$(NC)"
	@echo -e "$(CYAN)  扇区 1-4: Stage 2 加载器$(NC)"
	@echo -e "$(CYAN)  扇区 5+:  内核$(NC)"
	@echo -e "$(GREEN)=========================================$(NC)"

# ===========================================================================
# 运行QEMU
# ===========================================================================
run: $(OS_IMG)
	@echo -e "$(YELLOW)=========================================$(NC)"
	@echo -e "$(YELLOW) 启动 EduOS (Chapter 4)$(NC)"
	@echo -e "$(YELLOW)=========================================$(NC)"
	@echo -e "$(CYAN)提示：内存管理系统已启用$(NC)"
	@echo -e "$(CYAN)      分页机制、物理内存管理、动态内存分配$(NC)"
	@echo ""
	$(QEMU) -drive format=raw,file=$(OS_IMG),if=floppy \
		-serial stdio -m 128M

# ===========================================================================
# 调试模式
# ===========================================================================
debug: $(OS_IMG)
	@echo -e "$(YELLOW)=========================================$(NC)"
	@echo -e "$(YELLOW) 调试模式启动 EduOS$(NC)"
	@echo -e "$(YELLOW)=========================================$(NC)"
	@echo -e "$(CYAN)GDB调试提示：$(NC)"
	@echo -e "$(CYAN)  1. 在另一个终端运行: gdb $(KERNEL_ELF)$(NC)"
	@echo -e "$(CYAN)  2. 在GDB中运行: target remote localhost:1234$(NC)"
	@echo -e "$(CYAN)  3. 设置断点: break kernel_main$(NC)"
	@echo -e "$(CYAN)  4. 继续执行: continue$(NC)"
	@echo ""
	$(QEMU) -drive format=raw,file=$(OS_IMG),if=floppy \
		-serial stdio -m 128M -s -S

# ===========================================================================
# 清理
# ===========================================================================
clean:
	@echo -e "$(RED)清理构建文件...$(NC)"
	rm -rf $(BUILD_DIR)
	@echo -e "$(GREEN)✓ 清理完成$(NC)"

# ===========================================================================
# 显示信息
# ===========================================================================
info:
	@echo -e "$(CYAN)=========================================$(NC)"
	@echo -e "$(CYAN) EduOS Chapter 5: 进程与线程管理$(NC)"
	@echo -e "$(CYAN)=========================================$(NC)"
	@echo ""
	@echo -e "$(YELLOW)编译的文件:$(NC)"
	@echo -e "  • Bootloader: Stage 1 + Stage 2 (支持分页)"
	@echo -e "  • 内核入口: entry.asm (高半核)"
	@echo -e "  • 中断处理: interrupts.asm (ISR + IRQ)"
	@echo -e "  • 上下文切换: context_switch.asm"
	@echo -e "  • 内核代码: main.c, vga.c, serial.c, kernel.c"
	@echo -e "  • 中断系统: idt.c, pic.c, irq.c, exception.c"
	@echo -e "  • 驱动程序: timer.c (支持调度), keyboard.c"
	@echo -e "  • 内存管理: pmm.c, vmm.c, kmalloc.c"
	@echo -e "  • 进程管理: process.c, scheduler.c"
	@echo -e "  • 库函数: string.c, libgcc_compat.c"
	@echo ""
	@echo -e "$(YELLOW)新增功能:$(NC)"
	@echo -e "  ✓ 进程控制块 (PCB)"
	@echo -e "  ✓ 进程调度器 - 时间片轮转算法"
	@echo -e "  ✓ CPU上下文切换"
	@echo -e "  ✓ 内核线程创建和管理"
	@echo -e "  ✓ 进程生命周期管理"
	@echo -e "  ✓ 多任务并发执行"
	@echo -e "  ✓ idle进程"
	@echo ""
	@echo -e "$(YELLOW)学习目标:$(NC)"
	@echo -e "  1. 理解进程的概念和状态"
	@echo -e "  2. 实现进程控制块（PCB）"
	@echo -e "  3. 掌握上下文切换机制"
	@echo -e "  4. 实现进程调度算法"
	@echo -e "  5. 理解多任务并发"
	@echo -e "  6. 实现内核线程"
	@echo ""
	@echo -e "$(YELLOW)编译命令:$(NC)"
	@echo -e "  make        - 编译所有文件"
	@echo -e "  make run    - 编译并运行"
	@echo -e "  make debug  - 调试模式启动"
	@echo -e "  make clean  - 清理构建文件"
	@echo -e "  make info   - 显示本帮助"
	@echo ""

help: info

# ===========================================================================
# 依赖关系
# ===========================================================================
.PHONY: all clean run debug help info
