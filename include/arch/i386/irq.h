#ifndef _ARCH_I386_IRQ_H
#define _ARCH_I386_IRQ_H

#include <types.h>
#include <arch/i386/idt.h>

/* IRQ处理函数类型 */
typedef void (*irq_handler_t)(struct interrupt_frame *frame);

/*
 * IRQ子系统初始化
 * 
 * 初始化PIC，安装IRQ处理程序，启用中断
 */
void irq_init(void);

/*
 * 注册IRQ处理函数
 * 
 * @param irq: IRQ号（0-15）
 * @param handler: 处理函数指针
 * @return: 0=成功，-1=失败
 */
int irq_install_handler(uint8_t irq, irq_handler_t handler);

/*
 * 卸载IRQ处理函数
 * 
 * @param irq: IRQ号（0-15）
 */
void irq_uninstall_handler(uint8_t irq);

/*
 * 启用指定IRQ
 * 
 * @param irq: IRQ号（0-15）
 */
void irq_enable(uint8_t irq);

/*
 * 禁用指定IRQ
 * 
 * @param irq: IRQ号（0-15）
 */
void irq_disable(uint8_t irq);

/*
 * 禁用所有中断（CLI指令）
 */
static inline void irq_disable_all(void)
{
    __asm__ volatile("cli");
}

/*
 * 启用所有中断（STI指令）
 */
static inline void irq_enable_all(void)
{
    __asm__ volatile("sti");
}

/*
 * 保存中断状态并禁用中断
 * 
 * @return: 之前的EFLAGS值
 */
static inline uint32_t irq_save_disable(void)
{
    uint32_t flags;
    __asm__ volatile(
        "pushfl\n"
        "popl %0\n"
        "cli\n"
        : "=r"(flags)
        :
        : "memory"
    );
    return flags;
}

/*
 * 恢复之前保存的中断状态
 * 
 * @param flags: 之前保存的EFLAGS值
 */
static inline void irq_restore(uint32_t flags)
{
    __asm__ volatile(
        "pushl %0\n"
        "popfl\n"
        :
        : "r"(flags)
        : "memory", "cc"
    );
}

/*
 * 获取IRQ统计信息
 * 
 * @param irq: IRQ号（0-15）
 * @return: 该IRQ触发的次数
 */
uint64_t irq_get_count(uint8_t irq);

/*
 * 打印IRQ统计信息
 */
void irq_print_stats(void);

#endif /* _ARCH_I386_IRQ_H */

