/*
 * IRQ (硬件中断请求) 处理器
 * 
 * 管理硬件中断（IRQ 0-15）的注册和分发
 */

#include <arch/i386/irq.h>
#include <arch/i386/pic.h>
#include <kernel.h>

/* IRQ处理函数表 */
static irq_handler_t irq_handlers[16] = {NULL};

/* IRQ统计计数器 */
static uint64_t irq_counts[16] = {0};

/* IRQ名称表（用于调试） */
static const char *irq_names[16] = {
    "Timer (PIT)",              // IRQ0
    "Keyboard",                 // IRQ1
    "Cascade (PIC2)",           // IRQ2
    "Serial Port 2 (COM2)",     // IRQ3
    "Serial Port 1 (COM1)",     // IRQ4
    "Parallel Port 2 / Sound",  // IRQ5
    "Floppy Disk",              // IRQ6
    "Parallel Port 1 (LPT1)",   // IRQ7
    "Real-Time Clock (RTC)",    // IRQ8
    "ACPI",                     // IRQ9
    "Available",                // IRQ10
    "Available",                // IRQ11
    "PS/2 Mouse",               // IRQ12
    "Math Coprocessor",         // IRQ13
    "Primary ATA",              // IRQ14
    "Secondary ATA",            // IRQ15
};

/*
 * IRQ子系统初始化
 */
void irq_init(void)
{
    /* 初始化PIC，重映射IRQ到INT 32-47 */
    pic_init(32, 40);
    
    /* 默认屏蔽所有IRQ（除了级联IRQ2） */
    pic_set_mask(0xFFFF & ~(1 << IRQ_CASCADE));
    
    kprintf("[IRQ] IRQ subsystem initialized\n");
    kprintf("[IRQ] All IRQs masked except cascade (IRQ2)\n");
}

/*
 * 注册IRQ处理函数
 */
int irq_install_handler(uint8_t irq, irq_handler_t handler)
{
    if (irq >= 16) {
        kprintf("[IRQ] Error: Invalid IRQ number %d\n", irq);
        return -1;
    }
    
    if (irq_handlers[irq] != NULL) {
        kprintf("[IRQ] Warning: Overwriting handler for IRQ%d (%s)\n", 
                irq, irq_names[irq]);
    }
    
    irq_handlers[irq] = handler;
    kprintf("[IRQ] Installed handler for IRQ%d: %s\n", irq, irq_names[irq]);
    
    return 0;
}

/*
 * 卸载IRQ处理函数
 */
void irq_uninstall_handler(uint8_t irq)
{
    if (irq >= 16) {
        return;
    }
    
    irq_handlers[irq] = NULL;
    kprintf("[IRQ] Uninstalled handler for IRQ%d: %s\n", irq, irq_names[irq]);
}

/*
 * 启用指定IRQ
 */
void irq_enable(uint8_t irq)
{
    if (irq >= 16) {
        return;
    }
    
    pic_unmask_irq(irq);
    kprintf("[IRQ] Enabled IRQ%d: %s\n", irq, irq_names[irq]);
}

/*
 * 禁用指定IRQ
 */
void irq_disable(uint8_t irq)
{
    if (irq >= 16) {
        return;
    }
    
    pic_mask_irq(irq);
    kprintf("[IRQ] Disabled IRQ%d: %s\n", irq, irq_names[irq]);
}

/*
 * IRQ处理函数
 * 
 * 由汇编IRQ入口点调用
 */
void irq_handler(uint8_t irq, struct interrupt_frame *frame)
{
    /* 验证IRQ号 */
    if (irq >= 16) {
        kprintf("[IRQ] Error: Invalid IRQ number %d\n", irq);
        return;
    }
    
    /* 增加统计计数 */
    irq_counts[irq]++;
    
    /* 检查伪中断 */
    if (pic_is_spurious(irq)) {
        return;  // 不需要处理，也不发送EOI
    }
    
    /* 调用注册的处理函数 */
    if (irq_handlers[irq] != NULL) {
        irq_handlers[irq](frame);
    } else {
        /* 未注册处理函数（静默处理，避免刷屏） */
        static uint32_t unhandled_count[16] = {0};
        unhandled_count[irq]++;
        
        /* 仅首次报告 */
        if (unhandled_count[irq] == 1) {
            kprintf("[IRQ] Unhandled IRQ%d: %s (will not report again)\n", 
                    irq, irq_names[irq]);
        }
    }
    
    /* 发送EOI到PIC */
    pic_send_eoi(irq);
}

/*
 * 获取IRQ统计信息
 */
uint64_t irq_get_count(uint8_t irq)
{
    if (irq >= 16) {
        return 0;
    }
    return irq_counts[irq];
}

/*
 * 打印IRQ统计信息
 */
void irq_print_stats(void)
{
    kprintf("\n=== IRQ Statistics ===\n");
    kprintf("IRQ  Name                        Count         Handler\n");
    kprintf("─────────────────────────────────────────────────────────\n");
    
    for (int i = 0; i < 16; i++) {
        if (irq_counts[i] > 0 || irq_handlers[i] != NULL) {
            kprintf("%2d   %-24s  %10llu    %s\n",
                    i,
                    irq_names[i],
                    irq_counts[i],
                    irq_handlers[i] ? "Installed" : "None");
        }
    }
    
    kprintf("\n");
}

