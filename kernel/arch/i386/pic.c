/*
 * 8259A PIC (可编程中断控制器) 驱动
 * 
 * 管理16个硬件中断(IRQ 0-15)
 */

#include <arch/i386/pic.h>
#include <io.h>
#include <kernel.h>

/* I/O等待宏（用于PIC操作之间的短暂延迟） */
#define IO_WAIT() outb(0x80, 0)

/*
 * PIC初始化
 * 
 * 初始化主从PIC，重映射IRQ到指定中断向量
 * 
 * @param offset1: 主PIC中断向量偏移（通常32）
 * @param offset2: 从PIC中断向量偏移（通常40）
 * 
 * 默认情况：
 * - 主PIC: IRQ0-7 → INT 8-15
 * - 从PIC: IRQ8-15 → INT 0x70-0x77
 * 
 * 重映射后：
 * - 主PIC: IRQ0-7 → INT 32-39
 * - 从PIC: IRQ8-15 → INT 40-47
 * 
 * 为什么重映射？
 * CPU异常使用INT 0-31，默认PIC使用INT 8-15会冲突！
 */
void pic_init(uint8_t offset1, uint8_t offset2)
{
    uint8_t mask1, mask2;
    
    /* 保存当前屏蔽位 */
    mask1 = inb(PIC_MASTER_DATA);
    mask2 = inb(PIC_SLAVE_DATA);
    
    /* 
     * ICW1: 初始化命令
     * 0x11 = 0001 0001b
     * - bit 0: ICW4需要
     * - bit 1: 0=级联模式
     * - bit 3: 0=边沿触发
     * - bit 4: 1=初始化
     */
    outb(PIC_MASTER_CMD, PIC_ICW1_INIT | PIC_ICW1_ICW4);
    IO_WAIT();
    outb(PIC_SLAVE_CMD, PIC_ICW1_INIT | PIC_ICW1_ICW4);
    IO_WAIT();
    
    /* 
     * ICW2: 中断向量偏移
     * 主PIC: IRQ0-7 → INT offset1-offset1+7
     * 从PIC: IRQ8-15 → INT offset2-offset2+7
     */
    outb(PIC_MASTER_DATA, offset1);
    IO_WAIT();
    outb(PIC_SLAVE_DATA, offset2);
    IO_WAIT();
    
    /* 
     * ICW3: 级联设置
     * 主PIC: bit 2设置（IRQ2连接到从PIC）
     * 从PIC: 2（从PIC的级联标识）
     */
    outb(PIC_MASTER_DATA, 0x04);    // 0000 0100b = IRQ2
    IO_WAIT();
    outb(PIC_SLAVE_DATA, 0x02);     // 从PIC级联到主PIC的IRQ2
    IO_WAIT();
    
    /* 
     * ICW4: 附加信息
     * 0x01 = 8086/88模式
     */
    outb(PIC_MASTER_DATA, PIC_ICW4_8086);
    IO_WAIT();
    outb(PIC_SLAVE_DATA, PIC_ICW4_8086);
    IO_WAIT();
    
    /* 恢复屏蔽位（或设置默认屏蔽） */
    outb(PIC_MASTER_DATA, mask1);
    outb(PIC_SLAVE_DATA, mask2);
    
    kprintf("[PIC] 8259A PIC initialized\n");
    kprintf("[PIC] Master PIC: IRQ0-7  -> INT %d-%d\n", offset1, offset1 + 7);
    kprintf("[PIC] Slave PIC:  IRQ8-15 -> INT %d-%d\n", offset2, offset2 + 7);
}

/*
 * 发送EOI (End of Interrupt) 信号
 * 
 * 必须在中断处理完成后调用，否则PIC不会处理后续中断
 * 
 * @param irq: IRQ号（0-15）
 * 
 * 注意：
 * - IRQ0-7: 只需向主PIC发送EOI
 * - IRQ8-15: 需要同时向主PIC和从PIC发送EOI
 */
void pic_send_eoi(uint8_t irq)
{
    /* 如果是从PIC的IRQ，先向从PIC发送EOI */
    if (irq >= 8) {
        outb(PIC_SLAVE_CMD, PIC_EOI);
    }
    
    /* 总是向主PIC发送EOI */
    outb(PIC_MASTER_CMD, PIC_EOI);
}

/*
 * 屏蔽IRQ
 * 
 * 禁用指定IRQ（不再接收该中断）
 * 
 * @param irq: IRQ号（0-15）
 */
void pic_mask_irq(uint8_t irq)
{
    uint16_t port;
    uint8_t value;
    
    if (irq < 8) {
        port = PIC_MASTER_DATA;
    } else {
        port = PIC_SLAVE_DATA;
        irq -= 8;
    }
    
    value = inb(port) | (1 << irq);
    outb(port, value);
}

/*
 * 取消屏蔽IRQ
 * 
 * 启用指定IRQ（开始接收该中断）
 * 
 * @param irq: IRQ号（0-15）
 */
void pic_unmask_irq(uint8_t irq)
{
    uint16_t port;
    uint8_t value;
    
    if (irq < 8) {
        port = PIC_MASTER_DATA;
    } else {
        port = PIC_SLAVE_DATA;
        irq -= 8;
    }
    
    value = inb(port) & ~(1 << irq);
    outb(port, value);
}

/*
 * 设置IRQ屏蔽位
 * 
 * 批量设置屏蔽寄存器
 * 
 * @param mask: 16位屏蔽值
 *              低8位 = 主PIC (IRQ0-7)
 *              高8位 = 从PIC (IRQ8-15)
 */
void pic_set_mask(uint16_t mask)
{
    outb(PIC_MASTER_DATA, mask & 0xFF);
    outb(PIC_SLAVE_DATA, (mask >> 8) & 0xFF);
}

/*
 * 读取IRQ屏蔽位
 * 
 * @return: 16位屏蔽值
 */
uint16_t pic_get_mask(void)
{
    uint16_t mask;
    mask = inb(PIC_MASTER_DATA);
    mask |= inb(PIC_SLAVE_DATA) << 8;
    return mask;
}

/*
 * 禁用PIC
 * 
 * 屏蔽所有IRQ（在使用APIC时调用）
 */
void pic_disable(void)
{
    outb(PIC_MASTER_DATA, 0xFF);
    outb(PIC_SLAVE_DATA, 0xFF);
    kprintf("[PIC] PIC disabled (all IRQs masked)\n");
}

/*
 * 读取IRR (Interrupt Request Register)
 * 
 * IRR显示哪些IRQ正在等待处理
 * 
 * @return: 16位IRR值（bit N = IRQ N状态）
 */
uint16_t pic_get_irr(void)
{
    outb(PIC_MASTER_CMD, PIC_READ_IRR);
    outb(PIC_SLAVE_CMD, PIC_READ_IRR);
    return ((uint16_t)inb(PIC_SLAVE_CMD) << 8) | inb(PIC_MASTER_CMD);
}

/*
 * 读取ISR (In-Service Register)
 * 
 * ISR显示哪些IRQ正在被处理
 * 
 * @return: 16位ISR值（bit N = IRQ N状态）
 */
uint16_t pic_get_isr(void)
{
    outb(PIC_MASTER_CMD, PIC_READ_ISR);
    outb(PIC_SLAVE_CMD, PIC_READ_ISR);
    uint16_t master = inb(PIC_MASTER_CMD);
    uint16_t slave = inb(PIC_SLAVE_CMD);
    return (slave << 8) | master;
}

/*
 * 检查伪中断 (Spurious IRQ)
 * 
 * IRQ7和IRQ15可能是伪中断（硬件噪声引起）
 * 如果是伪中断，不应发送EOI
 * 
 * @param irq: IRQ号
 * @return: true=伪中断，false=真实中断
 */
bool pic_is_spurious(uint8_t irq)
{
    uint16_t isr;
    
    /* 只有IRQ7和IRQ15可能是伪中断 */
    if (irq == 7) {
        isr = pic_get_isr();
        /* 检查主PIC的bit 7 */
        if (!(isr & 0x80)) {
            kprintf("[PIC] Spurious IRQ7 detected\n");
            return true;
        }
    } else if (irq == 15) {
        isr = pic_get_isr();
        /* 检查从PIC的bit 7 (ISR的bit 15) */
        if (!(isr & 0x8000)) {
            kprintf("[PIC] Spurious IRQ15 detected\n");
            /* 仍需向主PIC发送EOI */
            outb(PIC_MASTER_CMD, PIC_EOI);
            return true;
        }
    }
    
    return false;
}

