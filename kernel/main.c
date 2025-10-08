/*
 * main.c - EduOS 第5章：进程与线程管理
 */

#include "kernel.h"
#include "vga.h"
#include "serial.h"
#include "string.h"
#include "io.h"
#include "types.h"
#include <arch/i386/idt.h>
#include <arch/i386/irq.h>
#include <drivers/timer.h>
#include <drivers/keyboard.h>
#include <mm/pmm.h>
#include <mm/vmm.h>
#include <mm/kmalloc.h>
#include <process/process.h>
#include <process/scheduler.h>
#include <process/priority_sched.h>
#include <process/mlfq_sched.h>

/* 外部符号：内核结束地址 */
extern uint32_t kernel_end;

/* ========== 测试线程（第6章：调度算法测试） ========== */

/* CPU密集型线程（计算） */
static void cpu_intensive_thread(void)
{
    struct process *proc = process_get_current();
    kprintf("[CPU-Intensive] %s (PID %u) Started! Initial level: %d\n", 
            proc->name, proc->pid, proc->mlfq_level);
    
    uint32_t count = 0;
    uint64_t sum = 0;
    
    while (1) {
        /* 密集计算：求和 */
        for (int i = 0; i < 10000; i++) {
            sum += i;
        }
        count++;
        
        if (count % 1000 == 0) {
            kprintf("[CPU-Intensive] %s: count=%u, level=%d, tick=%llu\n",
                    proc->name, count, proc->mlfq_level, timer_get_ticks());
        }
        
        /* 运行一段时间后退出 */
        if (count >= 5000) {
            kprintf("[CPU-Intensive] %s Exiting (final level: %d)\n",
                    proc->name, proc->mlfq_level);
            process_exit(0);
        }
    }
}

/* I/O密集型线程（模拟交互） */
static void io_intensive_thread(void)
{
    struct process *proc = process_get_current();
    kprintf("[I/O-Intensive] %s (PID %u) Started! Initial level: %d\n",
            proc->name, proc->pid, proc->mlfq_level);
    
    uint32_t count = 0;
    
    while (1) {
        /* 少量计算 */
        for (int i = 0; i < 100; i++) {
            volatile int dummy = i * 2;
            (void)dummy;
        }
        
        /* 模拟I/O：主动让出CPU（短暂"等待"） */
        for (volatile int i = 0; i < 1000; i++) {
            /* 忙等待一小段时间，模拟I/O等待 */
        }
        
        count++;
        
        if (count % 500 == 0) {
            kprintf("[I/O-Intensive] %s: count=%u, level=%d, tick=%llu\n",
                    proc->name, count, proc->mlfq_level, timer_get_ticks());
        }
        
        if (count >= 3000) {
            kprintf("[I/O-Intensive] %s Exiting (final level: %d)\n",
                    proc->name, proc->mlfq_level);
            process_exit(0);
        }
    }
}

/* 优先级测试线程 */
static void priority_test_thread(void)
{
    struct process *proc = process_get_current();
    kprintf("[Priority-Test] %s (PID %u, Priority %d) Started!\n",
            proc->name, proc->pid, proc->priority);
    
    uint32_t count = 0;
    while (1) {
        count++;
        if (count % 5000000 == 0) {
            kprintf("[Priority-Test] %s (Pri %d): count=%u, tick=%llu\n",
                    proc->name, proc->priority, count / 5000000, timer_get_ticks());
        }
        
        if (count >= 30000000) {
            kprintf("[Priority-Test] %s Exiting\n", proc->name);
            process_exit(0);
        }
    }
}

/* 统计线程 */
static void stats_thread(void)
{
    kprintf("[Stats] Monitor thread started\n");
    
    uint32_t interval = 0;
    
    while (1) {
        /* 等待一段时间 */
        for (volatile uint32_t i = 0; i < 50000000; i++) {
            /* 忙等待 */
        }
        
        interval++;
        
        kprintf("\n========== Stats Report #%u (tick: %llu) ==========\n",
                interval, timer_get_ticks());
        
        /* 打印进程状态 */
        process_print_all();
        
        /* 打印MLFQ队列（如果使用MLFQ） */
        mlfq_print_queues();
        
        /* 打印调度器统计 */
        scheduler_print_stats();
        
        kprintf("==========================================\n\n");
        
        if (interval >= 3) {
            kprintf("[Stats] Monitor exiting\n");
            process_exit(0);
        }
    }
}

static void print_banner(void)
{
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLUE);
    vga_write_string("================================================================================\n");
    vga_write_string("                         EduOS Kernel v0.4.0 (Chapter 5)                       \n");
    vga_write_string("              Educational Operating System - Process Management                \n");
    vga_write_string("================================================================================\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_write_string("\n");
}

static void print_system_info(void)
{
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_write_string("[INFO] ");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_write_string("Kernel boot sequence started...\n");
    
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_write_string("[INFO] ");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_write_string("Build: ");
    vga_write_string(__DATE__);
    vga_write_string(" ");
    vga_write_string(__TIME__);
    vga_write_string("\n");
    
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_write_string("[INFO] ");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_write_string("Architecture: i386 (32-bit Protected Mode)\n\n");
}

static void print_success_message(void)
{
    vga_write_string("\n");
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_write_string("================================================================================\n");
    vga_write_string("                    Kernel initialized successfully!                           \n");
    vga_write_string("================================================================================\n");
    
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_write_string("\nWelcome to EduOS Chapter 4: Memory Management\n\n");
    vga_write_string("Achievements unlocked:\n");
    vga_write_string("  [X] Physical Memory Manager (PMM)\n");
    vga_write_string("  [X] Virtual Memory Manager (VMM)\n");
    vga_write_string("  [X] Paging (Page Directory + Page Tables)\n");
    vga_write_string("  [X] Kernel Heap Allocator (kmalloc/kfree)\n");
    vga_write_string("  [X] Higher-Half Kernel (3GB+)\n");
    vga_write_string("  [X] Page Fault Handler\n\n");
    
    vga_set_color(VGA_COLOR_BROWN, VGA_COLOR_BLACK);
    vga_write_string("Next chapter: Process & Thread Management\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_write_string("\n");
}

/* 测试内存管理 */
static void test_memory(void)
{
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_write_string("\n=== Memory Management Test ===\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    
    /* 测试kmalloc */
    kprintf("Testing kmalloc...\n");
    
    void *ptr1 = kmalloc(128);
    kprintf("  Allocated 128 bytes at: 0x%08x\n", (uint32_t)ptr1);
    
    void *ptr2 = kmalloc(256);
    kprintf("  Allocated 256 bytes at: 0x%08x\n", (uint32_t)ptr2);
    
    void *ptr3 = kmalloc(512);
    kprintf("  Allocated 512 bytes at: 0x%08x\n", (uint32_t)ptr3);
    
    /* 测试内存写入 */
    if (ptr1) {
        char *test = (char*)ptr1;
        for (int i = 0; i < 128; i++) {
            test[i] = (char)i;
        }
        kprintf("  Memory write test: OK\n");
    }
    
    /* 打印统计 */
    kmalloc_print_stats();
    
    /* 测试kfree */
    kprintf("Testing kfree...\n");
    kfree(ptr2);
    kprintf("  Freed 256 bytes\n");
    
    /* 再次打印统计 */
    kmalloc_print_stats();
    
    /* 验证堆完整性 */
    if (kmalloc_verify_heap()) {
        kprintf("Heap integrity: OK\n");
    } else {
        kprintf("Heap integrity: FAILED!\n");
    }
    
    kprintf("\nMemory test completed\n\n");
}

/* 测试键盘输入 */
static void test_keyboard(void)
{
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_write_string("\n=== Interactive Keyboard Test ===\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_write_string("Type something (Ctrl+C to exit, Ctrl+L to clear):\n> ");
    
    while (1) {
        char ch = keyboard_getchar();
        
        /* Ctrl+C: 退出 */
        if (ch == 3) {
            vga_write_string("\n\nKeyboard test terminated.\n\n");
            break;
        }
        
        /* 回车 */
        if (ch == '\n') {
            vga_write_string("\n> ");
        }
        /* 退格 */
        else if (ch == '\b') {
            vga_putchar('\b');
            vga_putchar(' ');
            vga_putchar('\b');
        }
        /* 普通字符 */
        else {
            vga_putchar(ch);
        }
    }
}

/* 测试定时器 */
static void test_timer(void)
{
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_write_string("\n=== Timer Test ===\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_write_string("Waiting 5 seconds");
    
    for (int i = 0; i < 5; i++) {
        timer_wait_ms(1000);
        vga_putchar('.');
    }
    
    vga_write_string(" Done!\n");
    timer_print_info();
}

/* 测试除零异常 */
static void test_exception(void)
{
    vga_set_color(VGA_COLOR_BROWN, VGA_COLOR_BLACK);
    vga_write_string("\n[WARNING] ");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_write_string("To test CPU exceptions, uncomment test code in main.c\n");
    vga_write_string("Example tests available:\n");
    vga_write_string("  - Division by zero (#DE)\n");
    vga_write_string("  - Invalid opcode (#UD)\n");
    vga_write_string("  - General protection fault (#GP)\n");
    vga_write_string("  - Page fault (#PF)\n\n");
    
    /* 
     * 取消注释以测试异常：
     * 
     * 1. 测试除零异常
     * volatile int x = 10;
     * volatile int y = 0;
     * volatile int z = x / y;
     * 
     * 2. 测试无效操作码
     * __asm__ volatile("ud2");
     * 
     * 3. 测试通用保护故障（访问空指针）
     * volatile int *ptr = (int*)0x0;
     * volatile int val = *ptr;
     * 
     * 4. 测试缺页异常
     * volatile int *ptr = (int*)0x80000000;
     * volatile int val = *ptr;
     */
}

// 内核主函数
void kernel_main(void)
{
    // 初始化VGA和串口
    vga_init();
    serial_init(COM1);
    serial_puts(COM1, "\n=== EduOS Kernel v0.3.0 (Chapter 4) ===\n");
    serial_puts(COM1, "Serial port initialized successfully.\n");
    
    // 显示banner
    print_banner();
    print_system_info();
    
    // 初始化中断系统
    kprintf("[INIT] Initializing Interrupt Descriptor Table...\n");
    idt_init();
    
    kprintf("[INIT] Initializing IRQ subsystem...\n");
    irq_init();
    
    kprintf("[INIT] Initializing Timer (PIT 8253/8254)...\n");
    timer_init(TIMER_FREQUENCY_HZ);
    
    kprintf("[INIT] Initializing Keyboard (PS/2)...\n");
    keyboard_init();
    
    kprintf("[INIT] Enabling interrupts...\n");
    irq_enable_all();
    kprintf("[INIT] Interrupts enabled (IF flag set)\n\n");
    
    // ========== 第4章：初始化内存管理 ==========
    
    kprintf("[INIT] Initializing Memory Management...\n\n");
    
    /* 获取内核结束地址（物理地址） */
    uint32_t kernel_end_phys = (uint32_t)&kernel_end;
    kprintf("[INIT] Kernel end (physical): 0x%08x\n\n", kernel_end_phys);
    
    /* 初始化物理内存管理器（假设128MB RAM） */
    uint32_t total_memory = 128 * 1024 * 1024;  // 128MB
    pmm_init(total_memory, 0x100000, kernel_end_phys);
    
    /* 打印物理内存统计 */
    pmm_print_stats();
    
    /* 初始化虚拟内存管理器（启用分页） */
    vmm_init(kernel_end_phys);
    
    /* 初始化内核堆（在高地址空间） */
    uint32_t heap_start = 0xC0400000;  // 4MB后开始
    uint32_t heap_size = 16 * 1024 * 1024;  // 16MB堆
    kmalloc_init(heap_start, heap_size);
    
    kprintf("[INIT] Memory Management initialized successfully!\n\n");
    
    /* 初始化进程管理器 */
    process_init();
    scheduler_init();
    
    /* 初始化高级调度器 */
    kprintf("[INIT] Initializing advanced schedulers...\n");
    priority_scheduler_init();
    mlfq_init();
    kprintf("\n");
    
    /* ========== 恢复MLFQ测试（之前工作正常） ========== */
    kprintf("=== Chapter 6: Testing MLFQ Scheduler ===\n\n");
    
    /* 创建测试线程 */
    kprintf("[INIT] Creating test threads for MLFQ...\n");
    
    /* CPU密集型进程（会逐渐降到低优先级） */
    struct process *cpu1 = process_create_kernel_thread("cpu_worker1", cpu_intensive_thread, 10);
    struct process *cpu2 = process_create_kernel_thread("cpu_worker2", cpu_intensive_thread, 10);
    
    /* I/O密集型进程（会保持在高优先级） */
    struct process *io1 = process_create_kernel_thread("io_worker1", io_intensive_thread, 10);
    struct process *io2 = process_create_kernel_thread("io_worker2", io_intensive_thread, 10);
    
    /* 统计监控线程 */
    struct process *stats = process_create_kernel_thread("monitor", stats_thread, 10);
    
    /* 将所有进程加入MLFQ（从最高级别开始） */
    if (cpu1) { cpu1->mlfq_level = 0; mlfq_enqueue(cpu1); }
    if (cpu2) { cpu2->mlfq_level = 0; mlfq_enqueue(cpu2); }
    if (io1) { io1->mlfq_level = 0; mlfq_enqueue(io1); }
    if (io2) { io2->mlfq_level = 0; mlfq_enqueue(io2); }
    if (stats) { stats->mlfq_level = 0; mlfq_enqueue(stats); }
    
    kprintf("[INIT] Created 5 test threads:\n");
    kprintf("  - 2x CPU-intensive (will sink to low priority)\n");
    kprintf("  - 2x I/O-intensive (will stay at high priority)\n");
    kprintf("  - 1x Stats monitor\n");
    kprintf("\n");
    
    // 打印系统信息
    kprintf("\n=== System Status ===\n");
    pmm_print_stats();
    kmalloc_print_stats();
    process_print_all();
    scheduler_print_stats();
    
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_write_string("\n================================================================================\n");
    vga_write_string("           System initialized! Starting multitasking...                        \n");
    vga_write_string("================================================================================\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    
    kprintf("\n[INIT] Enabling scheduler and multitasking...\n");
    extern void scheduler_enable(void);
    scheduler_enable();
    
    /* 启用中断，让定时器开始触发调度 */
    kprintf("[INIT] Enabling interrupts...\n");
    asm volatile("sti");
    
    kprintf("[INIT] Multitasking started! Threads will execute concurrently.\n\n");
    
    /* 主线程变为idle循环 */
    while (1) {
        asm volatile("hlt");  // 等待中断
    }
}
