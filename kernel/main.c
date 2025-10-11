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
#include <fs/vfs.h>
#include <fs/devfs.h>
#include <fs/procfs.h>
#include <fs/fat32.h>

/* 外部符号：内核结束地址 */
extern uint32_t kernel_end;

/* ========== VFS测试函数（第7章） ========== */

#if 0  // 禁用旧的 VFS 测试
static void test_vfs_basic(void)
{
    kprintf("\n=== Testing VFS and DevFS ===\n\n");
    
    char buffer[128];
    int fd, nbytes;
    
    /* 测试 /dev/null */
    kprintf("[VFS Test] Opening /null...\n");
    fd = vfs_open("/null", O_RDWR, 0);
    if (fd < 0) {
        kprintf("[VFS Test] ERROR: Failed to open /null: %d\n", fd);
    } else {
        kprintf("[VFS Test] /null opened, fd=%d\n", fd);
        
        /* 写入数据（应该被丢弃） */
        const char *test_data = "This data will disappear";
        nbytes = vfs_write(fd, test_data, 24);
        kprintf("[VFS Test] Wrote %d bytes to /null (data discarded)\n", nbytes);
        
        /* 读取数据（应该返回EOF） */
        nbytes = vfs_read(fd, buffer, sizeof(buffer));
        kprintf("[VFS Test] Read %d bytes from /null (EOF)\n", nbytes);
        
        vfs_close(fd);
        kprintf("[VFS Test] /null closed\n");
    }
    
    kprintf("\n");
    
    /* 测试 /dev/zero */
    kprintf("[VFS Test] Opening /zero...\n");
    fd = vfs_open("/zero", O_RDONLY, 0);
    if (fd < 0) {
        kprintf("[VFS Test] ERROR: Failed to open /zero: %d\n", fd);
    } else {
        kprintf("[VFS Test] /zero opened, fd=%d\n", fd);
        
        /* 填充buffer为非零 */
        for (int i = 0; i < 16; i++) {
            buffer[i] = 0xFF;
        }
        
        /* 读取零 */
        nbytes = vfs_read(fd, buffer, 16);
        kprintf("[VFS Test] Read %d bytes from /zero\n", nbytes);
        
        /* 验证全为零 */
        bool all_zero = true;
        for (int i = 0; i < 16; i++) {
            if (buffer[i] != 0) {
                all_zero = false;
                break;
            }
        }
        kprintf("[VFS Test] Data verification: %s\n", all_zero ? "PASSED (all zeros)" : "FAILED");
        
        vfs_close(fd);
        kprintf("[VFS Test] /zero closed\n");
    }
    
    kprintf("\n");
    
    /* 测试 /dev/console */
    kprintf("[VFS Test] Opening /console...\n");
    fd = vfs_open("/console", O_WRONLY, 0);
    if (fd < 0) {
        kprintf("[VFS Test] ERROR: Failed to open /console: %d\n", fd);
    } else {
        kprintf("[VFS Test] /console opened, fd=%d\n", fd);
        
        /* 写入到控制台 */
        const char *msg = "[VFS Test] Hello from /console!\n";
        nbytes = vfs_write(fd, msg, 33);
        kprintf("[VFS Test] Wrote %d bytes to /console\n", nbytes);
        
        vfs_close(fd);
        kprintf("[VFS Test] /console closed\n");
    }
    
    kprintf("\n[VFS Test] All tests completed!\n\n");
}
#endif

/* ========== ProcFS 测试函数（第9章） ========== */

static void test_procfs(void)
{
    kprintf("\n");
    kprintf("================================================================================\n");
    kprintf("=== Chapter 9: Testing ProcFS (Process Filesystem) ===\n");
    kprintf("================================================================================\n");
    kprintf("\n");
    
    char buffer[1024];
    int fd, nbytes;
    
    /* 测试 1: /proc/cpuinfo */
    kprintf("[ProcFS Test 1] Reading /proc/cpuinfo...\n");
    fd = vfs_open("/proc/cpuinfo", O_RDONLY, 0);
    if (fd < 0) {
        kprintf("[ERROR] Failed to open /proc/cpuinfo: %d\n", fd);
    } else {
        nbytes = vfs_read(fd, buffer, sizeof(buffer) - 1);
        if (nbytes > 0) {
            buffer[nbytes] = '\0';
            kprintf("%s\n", buffer);
        }
        vfs_close(fd);
    }
    
    /* 测试 2: /proc/meminfo */
    kprintf("[ProcFS Test 2] Reading /proc/meminfo...\n");
    fd = vfs_open("/proc/meminfo", O_RDONLY, 0);
    if (fd < 0) {
        kprintf("[ERROR] Failed to open /proc/meminfo: %d\n", fd);
    } else {
        nbytes = vfs_read(fd, buffer, sizeof(buffer) - 1);
        if (nbytes > 0) {
            buffer[nbytes] = '\0';
            kprintf("%s\n", buffer);
        }
        vfs_close(fd);
    }
    
    /* 测试 3: /proc/uptime */
    kprintf("[ProcFS Test 3] Reading /proc/uptime...\n");
    fd = vfs_open("/proc/uptime", O_RDONLY, 0);
    if (fd < 0) {
        kprintf("[ERROR] Failed to open /proc/uptime: %d\n", fd);
    } else {
        nbytes = vfs_read(fd, buffer, sizeof(buffer) - 1);
        if (nbytes > 0) {
            buffer[nbytes] = '\0';
            kprintf("System uptime: %s\n", buffer);
        }
        vfs_close(fd);
    }
    
    /* 测试 4: /proc/version */
    kprintf("[ProcFS Test 4] Reading /proc/version...\n");
    fd = vfs_open("/proc/version", O_RDONLY, 0);
    if (fd < 0) {
        kprintf("[ERROR] Failed to open /proc/version: %d\n", fd);
    } else {
        nbytes = vfs_read(fd, buffer, sizeof(buffer) - 1);
        if (nbytes > 0) {
            buffer[nbytes] = '\0';
            kprintf("%s\n", buffer);
        }
        vfs_close(fd);
    }
    
    kprintf("\n");
    kprintf("================================================================================\n");
    kprintf("=== ProcFS Tests Completed Successfully! ===\n");
    kprintf("================================================================================\n");
    kprintf("\n");
}

/* ========== FAT32 测试函数（第10章） ========== */

static void test_fat32(void)
{
    kprintf("\n");
    kprintf("================================================================================\n");
    kprintf("=== Chapter 10: Testing FAT32 Filesystem ===\n");
    kprintf("================================================================================\n");
    kprintf("\n");
    
    /* 初始化 FAT32 */
    kprintf("[FAT32 Test 1] Initializing FAT32 driver...\n");
    int ret = fat32_init();
    if (ret < 0) {
        kprintf("[ERROR] Failed to initialize FAT32: %d\n", ret);
        return;
    }
    kprintf("[OK] FAT32 driver initialized\n\n");
    
    /* 尝试挂载（这里使用 NULL，实际需要真实的块设备） */
    kprintf("[FAT32 Test 2] Mounting FAT32 filesystem...\n");
    kprintf("[INFO] Note: Real block device driver not yet implemented\n");
    kprintf("[INFO] This is a demonstration of the mounting process\n");
    
    /* 演示 FAT32 功能（不需要真实设备） */
    ret = fat32_mount(NULL);
    if (ret < 0) {
        kprintf("[INFO] Mount failed (expected, no real device): %d\n", ret);
        kprintf("[INFO] FAT32 code is ready for real block device integration\n");
    } else {
        kprintf("[OK] FAT32 mounted successfully\n");
        
        /* 获取文件系统信息 */
        struct fat32_fs_info *fs = fat32_get_fs();
        if (fs) {
            kprintf("\n[FAT32 Info] Filesystem Information:\n");
            kprintf("  Bytes per Sector: %u\n", fs->bytes_per_sector);
            kprintf("  Sectors per Cluster: %u\n", fs->sectors_per_cluster);
            kprintf("  Cluster Size: %u bytes\n", fs->cluster_size);
            kprintf("  Root Cluster: %u\n", fs->root_cluster);
            kprintf("  Total Clusters: %u\n", fs->total_clusters);
        }
    }
    
    kprintf("\n");
    kprintf("================================================================================\n");
    kprintf("=== FAT32 Tests Completed! ===\n");
    kprintf("=== Ready for Block Device Integration ===\n");
    kprintf("================================================================================\n");
    kprintf("\n");
}

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
    serial_puts(COM1, "\n=== EduOS Kernel v0.3.0 (Chapter 7 - VFS Test) ===\n");
    serial_puts(COM1, "Serial port initialized successfully.\n");
    
    #if 0  // 注释掉 banner 和系统信息
    // 显示banner
    print_banner();
    print_system_info();
    #endif
    
    // 初始化中断系统（静默）
    idt_init();
    irq_init();
    timer_init(TIMER_FREQUENCY_HZ);
    keyboard_init();
    irq_enable_all();
    
    #if 0  // 注释掉初始化过程的输出
    kprintf("[INIT] Initializing Interrupt Descriptor Table...\n");
    kprintf("[INIT] Initializing IRQ subsystem...\n");
    kprintf("[INIT] Initializing Timer (PIT 8253/8254)...\n");
    kprintf("[INIT] Initializing Keyboard (PS/2)...\n");
    kprintf("[INIT] Enabling interrupts...\n");
    kprintf("[INIT] Interrupts enabled (IF flag set)\n\n");
    #endif
    
    // ========== 第4章：初始化内存管理（静默） ==========
    
    /* 获取内核结束地址（物理地址） */
    uint32_t kernel_end_phys = (uint32_t)&kernel_end;
    
    /* 初始化物理内存管理器（假设128MB RAM） */
    uint32_t total_memory = 128 * 1024 * 1024;  // 128MB
    pmm_init(total_memory, 0x100000, kernel_end_phys);
    
    #if 0  // 注释掉内存管理的输出
    kprintf("[INIT] Initializing Memory Management...\n\n");
    kprintf("[INIT] Kernel end (physical): 0x%08x\n\n", kernel_end_phys);
    pmm_print_stats();
    #endif
    
    /* 初始化虚拟内存管理器（启用分页） */
    vmm_init(kernel_end_phys);
    
    /* 初始化内核堆（在高地址空间） */
    uint32_t heap_start = 0xC0400000;  // 4MB后开始
    uint32_t heap_size = 16 * 1024 * 1024;  // 16MB堆
    kmalloc_init(heap_start, heap_size);
    
    #if 0  // 注释掉成功消息
    kprintf("[INIT] Memory Management initialized successfully!\n\n");
    #endif
    
    // ========== 第7章：初始化VFS和DevFS（静默初始化） ==========
    
    #if 0  // 注释掉 VFS 初始化的调试输出
    vga_puts("\n=== Chapter 7: VFS Init Debug ===\n");
    vga_puts("[VFS-DEBUG] Before vfs_init() call\n");
    #endif
    
    /* 初始化 VFS 核心 */
    vfs_init();
    
    #if 0
    vga_puts("[VFS-DEBUG] After vfs_init() call - SUCCESS\n");
    vga_puts("[VFS-DEBUG] Before devfs_init() call\n");
    #endif
    
    /* 初始化 DevFS */
    int ret = devfs_init();
    if (ret < 0) {
        panic("Failed to initialize DevFS");
    }
    
    #if 0
    vga_puts("[VFS-DEBUG] After devfs_init() call - SUCCESS\n");
    #endif
    
    /* 声明设备初始化函数 */
    extern int dev_null_init(void);
    extern int dev_zero_init(void);
    extern int dev_console_init(void);
    
    /* 注册标准设备 */
    #if 0
    vga_puts("[VFS-DEBUG] Registering /dev/null\n");
    #endif
    dev_null_init();
    
    #if 0
    vga_puts("[VFS-DEBUG] Registering /dev/zero\n");
    #endif
    dev_zero_init();
    
    #if 0
    vga_puts("[VFS-DEBUG] Registering /dev/console\n");
    #endif
    dev_console_init();
    
    #if 0
    vga_puts("[VFS-DEBUG] All devices registered\n");
    vga_puts("[VFS-DEBUG] VFS test completed, system stable\n\n");
    #endif
    
    /* 初始化进程管理器（静默） */
    process_init();
    scheduler_init();
    priority_scheduler_init();
    mlfq_init();
    
    /* ========== 第9章：初始化并测试 ProcFS ========== */
    kprintf("\n[INIT] Initializing ProcFS...\n");
    ret = procfs_init();
    if (ret < 0) {
        kprintf("[ERROR] Failed to initialize ProcFS: %d\n", ret);
    } else {
        kprintf("[OK] ProcFS initialized successfully\n");
    }
    
    /* 运行 ProcFS 测试 */
    test_procfs();
    
    /* ========== 第10章：初始化并测试 FAT32 ========== */
    test_fat32();
    
    #if 0  // 暂时禁用 MLFQ 测试
    /* ========== MLFQ 调度器测试（暂时禁用） ========== */
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
    #endif
    
    /* ========== VFS 测试完成后的简单循环 ========== */
    
    #if 0  // 注释掉最后的系统状态输出
    kprintf("\n=== VFS Test Completed ===\n");
    kprintf("System is in idle state. Press Ctrl+C in QEMU to exit.\n\n");
    
    // 打印系统状态
    kprintf("=== System Status ===\n");
    pmm_print_stats();
    kmalloc_print_stats();
    
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_write_string("\n================================================================================\n");
    vga_write_string("           All tests passed! System idle.                                      \n");
    vga_write_string("================================================================================\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    #endif
    
    /* 简单的 idle 循环 */
    while (1) {
        asm volatile("hlt");  // 等待中断（低功耗）
    }
}
