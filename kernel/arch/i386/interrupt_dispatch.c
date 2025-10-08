/*
 * 中断分发器
 * 
 * 统一的中断处理入口点，负责分发异常和IRQ
 */

#include <arch/i386/idt.h>
#include <arch/i386/irq.h>
#include <kernel.h>

/* 前向声明 */
extern void exception_handler(struct interrupt_frame *frame);
extern void irq_handler(uint8_t irq, struct interrupt_frame *frame);

/*
 * 主中断处理函数
 * 
 * 所有中断（异常和IRQ）的C语言入口点
 * 由汇编ISR/IRQ stub调用
 */
void interrupt_handler(struct interrupt_frame *frame)
{
    uint32_t int_no = frame->int_no;
    
    /* CPU异常（INT 0-31） */
    if (int_no < 32) {
        exception_handler(frame);
        return;
    }
    
    /* 硬件中断（INT 32-47，IRQ 0-15） */
    if (int_no >= 32 && int_no < 48) {
        uint8_t irq = int_no - 32;
        irq_handler(irq, frame);
        return;
    }
    
    /* 未定义的中断向量 */
    kprintf("[INT] Unhandled interrupt: %d\n", int_no);
}

