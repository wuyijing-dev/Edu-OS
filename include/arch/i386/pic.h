#ifndef _ARCH_I386_PIC_H
#define _ARCH_I386_PIC_H

#include <types.h>

/* 8259A PIC端口地址 */
#define PIC_MASTER_CMD      0x20    // 主PIC命令端口
#define PIC_MASTER_DATA     0x21    // 主PIC数据端口
#define PIC_SLAVE_CMD       0xA0    // 从PIC命令端口
#define PIC_SLAVE_DATA      0xA1    // 从PIC数据端口

/* PIC初始化命令字(ICW) */
#define PIC_ICW1_ICW4       0x01    // ICW4存在
#define PIC_ICW1_SINGLE     0x02    // 单片模式（无ICW3）
#define PIC_ICW1_INTERVAL4  0x04    // 调用地址间隔4（8086模式）
#define PIC_ICW1_LEVEL      0x08    // 边沿触发模式
#define PIC_ICW1_INIT       0x10    // 初始化命令

#define PIC_ICW4_8086       0x01    // 8086/88 (MCS-80/85)模式
#define PIC_ICW4_AUTO       0x02    // 自动EOI
#define PIC_ICW4_BUF_SLAVE  0x08    // 缓冲模式/从
#define PIC_ICW4_BUF_MASTER 0x0C    // 缓冲模式/主
#define PIC_ICW4_SFNM       0x10    // 特殊全嵌套模式

/* PIC操作命令字(OCW) */
#define PIC_EOI             0x20    // End of Interrupt命令

/* PIC读取命令 */
#define PIC_READ_IRR        0x0A    // 读取IRR（中断请求寄存器）
#define PIC_READ_ISR        0x0B    // 读取ISR（中断服务寄存器）

/* IRQ定义 */
#define IRQ_TIMER           0       // 可编程间隔定时器(PIT)
#define IRQ_KEYBOARD        1       // 键盘
#define IRQ_CASCADE         2       // 级联到从PIC
#define IRQ_SERIAL2         3       // 串口2(COM2)
#define IRQ_SERIAL1         4       // 串口1(COM1)
#define IRQ_PARALLEL2       5       // 并口2/声卡
#define IRQ_FLOPPY          6       // 软盘控制器
#define IRQ_PARALLEL1       7       // 并口1(LPT1)
#define IRQ_RTC             8       // 实时时钟
#define IRQ_ACPI            9       // ACPI
#define IRQ_AVAILABLE1      10      // 可用
#define IRQ_AVAILABLE2      11      // 可用
#define IRQ_MOUSE           12      // PS/2鼠标
#define IRQ_FPU             13      // 数学协处理器
#define IRQ_PRIMARY_ATA     14      // 主ATA硬盘
#define IRQ_SECONDARY_ATA   15      // 从ATA硬盘

/* IRQ总数 */
#define PIC_IRQ_COUNT       16

/*
 * PIC初始化
 * 
 * @param offset1: 主PIC的中断向量偏移（通常是32）
 * @param offset2: 从PIC的中断向量偏移（通常是40）
 */
void pic_init(uint8_t offset1, uint8_t offset2);

/*
 * 发送EOI（End of Interrupt）信号
 * 
 * 必须在中断处理完成后调用，告诉PIC可以处理下一个中断
 * 
 * @param irq: IRQ号（0-15）
 */
void pic_send_eoi(uint8_t irq);

/*
 * 屏蔽IRQ
 * 
 * @param irq: 要屏蔽的IRQ号（0-15）
 */
void pic_mask_irq(uint8_t irq);

/*
 * 取消屏蔽IRQ
 * 
 * @param irq: 要取消屏蔽的IRQ号（0-15）
 */
void pic_unmask_irq(uint8_t irq);

/*
 * 设置IRQ屏蔽位
 * 
 * @param mask1: 主PIC屏蔽位（bit0-7对应IRQ0-7）
 * @param mask2: 从PIC屏蔽位（bit0-7对应IRQ8-15）
 */
void pic_set_mask(uint16_t mask);

/*
 * 读取IRQ屏蔽位
 * 
 * @return: 16位屏蔽值（低8位=主PIC，高8位=从PIC）
 */
uint16_t pic_get_mask(void);

/*
 * 禁用PIC（在使用APIC时调用）
 */
void pic_disable(void);

/*
 * 读取IRR（中断请求寄存器）
 * 
 * @return: 16位IRR值
 */
uint16_t pic_get_irr(void);

/*
 * 读取ISR（中断服务寄存器）
 * 
 * @return: 16位ISR值
 */
uint16_t pic_get_isr(void);

/*
 * 检查伪中断（Spurious IRQ）
 * 
 * IRQ7和IRQ15可能是伪中断，需要检查
 * 
 * @param irq: IRQ号
 * @return: true=伪中断，false=真实中断
 */
bool pic_is_spurious(uint8_t irq);

#endif /* _ARCH_I386_PIC_H */

