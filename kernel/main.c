/*
 * main.c - EduOS 第5章：进程与线程管理
 */

#include "kernel.h"
#include "vga.h"
#include "serial.h"
#include "string.h"
#include "io.h"
#include "types.h"
#include <arch/i386/gdt.h>
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
#include <drivers/block.h>
#include <drivers/ramdisk.h>
#include <drivers/bga.h>
#include <syscall.h>
#include <sync/mutex.h>
#include <sync/semaphore.h>
#include <sync/spinlock.h>

/* 外部符号：内核结束地址 */
extern uint32_t kernel_end;

/* 外部函数：IDE 驱动 */
extern void ide_init(void);

/* 外部函数：系统调用 */
extern void syscall_init(void);

/* 外部函数：TSS */
extern void tss_init(void);
extern uint32_t tss_get_address(void);
extern uint32_t tss_get_size(void);

/* 外部函数：用户态和ELF */
extern void test_user_mode(void);
extern int elf_load(const char *path, uint32_t *entry);
extern int elf_exec(const char *path);

/* 外部函数：进程管理 */
extern int do_fork(void);

// 内核主函数
void kernel_main(void)
{
    // 初始化VGA和串口
    vga_init();
    serial_init(COM1);
    
    // 重新建立GDT（在内核虚拟地址空间）
    // 必须在使用用户模式之前完成
    gdt_init();
    
    // 初始化中断系统（静默）
    idt_init();
    irq_init();
    timer_init(TIMER_FREQUENCY_HZ);
    keyboard_init();
    irq_enable_all();
    
    // ========== 第4章：初始化内存管理（静默） ==========
    
    /* 获取内核结束地址（物理地址） */
    uint32_t kernel_end_phys = (uint32_t)&kernel_end;
    
    /* 初始化物理内存管理器（假设128MB RAM） */
    uint32_t total_memory = 128 * 1024 * 1024;  // 128MB
    pmm_init(total_memory, 0x100000, kernel_end_phys);
    
    /* 初始化虚拟内存管理器（启用分页） */
    vmm_init(kernel_end_phys);
    
    /* 初始化内核堆（在高地址空间） */
    uint32_t heap_start = 0xC0400000;  // 4MB后开始
    uint32_t heap_size = 16 * 1024 * 1024;  // 16MB堆
    kmalloc_init(heap_start, heap_size);
    
    /* 初始化kmap（高端内存临时映射）*/
    extern void kmap_init(void);
    kmap_init();
    
    /* 初始化页面回收系统（LRU）*/
    extern void page_reclaim_init(void);
    page_reclaim_init();
    
    /* 初始化OOM killer */
    extern void oom_init(void);
    oom_init();
    
    /* 初始化VFS缓冲区池（避免Page Fault中的内存分配）*/
    extern void vfs_buffer_init(void);
    vfs_buffer_init();
    
    // ========== 第7章：初始化VFS和DevFS（静默初始化） ==========
    
    /* 初始化 VFS 核心 */
    vfs_init();
    
    /* 初始化 DevFS */
    int ret = devfs_init();
    if (ret < 0) {
        panic("Failed to initialize DevFS");
    }
    
    /* 声明设备初始化函数 */
    extern int dev_null_init(void);
    extern int dev_zero_init(void);
    extern int dev_console_init(void);
    extern int dev_mouse_init(void);
    extern void mouse_init(void);
    
    /* 注册标准设备 */
    dev_null_init();
    
    dev_zero_init();
    
    dev_console_init();
    
    /* Linux风格：初始化输入子系统 */
    extern int input_subsystem_init(void);
    extern int input_devfs_init(void);
    input_subsystem_init();
    input_devfs_init();
    
    /* POSIX：初始化共享内存子系统 */
    extern int shm_init(void);
    shm_init();
    
    /* 初始化鼠标驱动和设备 */
    mouse_init();
    dev_mouse_init();
    
    /* 初始化网络子系统 */
    extern void netdev_init(void);
    extern int rtl8139_init(void);
    
    kprintf("[NET] Initializing network subsystem...\n");
    netdev_init();
    
    kprintf("[NET] Scanning for RTL8139 network card...\n");
    ret = rtl8139_init();
    if (ret == 0) {
        kprintf("[NET] ✓ RTL8139 network card initialized\n");
        kprintf("[NET] Network Configuration:\n");
        kprintf("[NET]   IP Address:  10.0.2.15\n");
        kprintf("[NET]   Netmask:     255.255.255.0\n");
        kprintf("[NET]   Gateway:     10.0.2.2\n");
        kprintf("[NET]   Protocol:    IPv4, ARP, ICMP\n");
        kprintf("[NET] ℹ️  Test with: ping 10.0.2.15 (from host)\n");
    } else {
        kprintf("[NET] RTL8139 network card not found (ret=%d)\n", ret);
    }
    
    /* 初始化进程管理器（静默） */
    process_init();
    scheduler_init();
    priority_scheduler_init();
    mlfq_init();
    
    // 初始化 TSS（用户态必需，会自动加载到 TR）
    tss_init();
    
    // 静默初始化其他子系统
    syscall_init();
    ret = procfs_init();
    (void)ret;
    
    /* 静默初始化 FAT32 */
    block_init();
    ide_init();
    fat32_init();
    
    /* 尝试挂载 FAT32 */
    extern struct block_device *block_get_device(const char *name);
    struct block_device *bdev = block_get_device("hdb");
    if (!bdev) {
        bdev = block_get_device("hdc");
    }
    if (bdev) {
        fat32_mount(bdev);
    }
    
    /* 显示简洁的系统状态 */
    kprintf("\n");
    kprintf("╔════════════════════════════════════════════════════════════╗\n");
    kprintf("║          EduOS Kernel Initialized Successfully            ║\n");
    kprintf("╚════════════════════════════════════════════════════════════╝\n");
    kprintf("\n");
    kprintf("[System Status]\n");
    kprintf("  ✅ VFS + DevFS + ProcFS\n");
    kprintf("  ✅ FAT32 Filesystem\n");
    kprintf("  ✅ System Calls (INT 0x80)\n");
    kprintf("  ✅ GDT User Segments (Ring 3)\n");
    kprintf("\n");
    
    /* ========== BGA 图形驱动初始化 ========== */
    kprintf("\n[Init] BGA Graphics Driver\n");
    kprintf("================================================================================\n");
    kprintf("\n");
    
    extern int bga_init(void);
    extern int dev_fb_init(void);
    
    if (bga_init() == 0) {
        /* 注册/dev/fb0设备 */
        dev_fb_init();
        
        kprintf("      ✅ BGA initialized: 1024x768x32\n");
        kprintf("      ✅ /dev/fb0 registered\n");
        kprintf("\n");
        
        /* 加载GUI演示程序 */
        kprintf("[Load] GUI Demo Program\n");
        kprintf("================================================================================\n");
        kprintf("\n");
        
        extern struct fat32_fs_info *fat32_get_fs(void);
        struct fat32_fs_info *fs = fat32_get_fs();
        
        if (fs) {
            /* 使用Linux风格的按需加载版本 */
            extern pid_t create_user_process_lazy(const char *name, const char *elf_path);
            
            /* 加载网络测试程序 */
            kprintf("      Loading /nettest.elf (Network Test Program)...\n");
            pid_t nettest_pid = create_user_process_lazy("nettest", "/nettest.elf");
            
            if (nettest_pid > 0) {
                kprintf("      ✅ Network Test loaded (PID %u)\n", nettest_pid);
            } else {
                kprintf("      ⚠️  Failed to load nettest.elf\n");
            }
            
            /* 加载桌面程序 */
            kprintf("      Loading /desktop.elf (GUI Desktop)...\n");
            pid_t desktop_pid = create_user_process_lazy("desktop", "/desktop.elf");
            
            if (desktop_pid > 0) {
                kprintf("      ✅ Desktop loaded (PID %u)\n", desktop_pid);
                kprintf("\n");
                kprintf("╔════════════════════════════════════════════════════════════╗\n");
                kprintf("║           EduOS - Ready to Launch!                       ║\n");
                kprintf("║  Programs: nettest (PID %u), desktop (PID %u)            ║\n", nettest_pid, desktop_pid);
                kprintf("║  Test with: ping 10.0.2.15 (from host)                  ║\n");
                kprintf("╚════════════════════════════════════════════════════════════╝\n");
                kprintf("\n");
                
                /* 启动调度器执行用户程序 */
                kprintf("[Scheduler] Starting multitasking...\n");
                kprintf("      Enabling interrupts and scheduler...\n");
                
                extern void scheduler_enable(void);
                extern void scheduler_schedule(void);
                
                scheduler_enable();
                
                kprintf("      ⚡ Performing first context switch...\n");
                kprintf("      ℹ️  Interrupts will be enabled after switch\n");
                kprintf("\n");
                
                /* 确保中断关闭，避免首次调度中被打断 */
                asm volatile("cli");
                
                /* 主动进行第一次调度（不会返回） */
                scheduler_schedule();
                
                /* 永远不应该到达这里 */
                kprintf("      ❌ ERROR: Returned from scheduler!\n");
                while (1) {
                    asm volatile("hlt");
                }
            } else {
                kprintf("      ❌ Failed to load desktop.elf\n");
            }
        } else {
            kprintf("      ⚠️  No FAT32 disk mounted\n");
        }
        
        kprintf("\n");
    } else {
        kprintf("      ⚠️  BGA not available (fallback to VGA text mode)\n");
    }
    
    /* 简单的 idle 循环 */
    while (1) {
        asm volatile("hlt");  // 等待中断（低功耗）
    }
}
