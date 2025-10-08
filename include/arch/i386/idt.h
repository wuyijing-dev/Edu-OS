#ifndef _ARCH_I386_IDT_H
#define _ARCH_I386_IDT_H

#include <types.h>

/* 中断向量定义 */
#define IDT_ENTRIES     256     // IDT表项数量

/* CPU异常向量（0-31） */
#define EXCEPTION_DE    0       // 除零异常 (Divide Error)
#define EXCEPTION_DB    1       // 调试异常 (Debug)
#define EXCEPTION_NMI   2       // 不可屏蔽中断
#define EXCEPTION_BP    3       // 断点异常 (Breakpoint)
#define EXCEPTION_OF    4       // 溢出异常 (Overflow)
#define EXCEPTION_BR    5       // 边界检查异常 (Bound Range Exceeded)
#define EXCEPTION_UD    6       // 无效操作码 (Invalid Opcode)
#define EXCEPTION_NM    7       // 设备不可用 (Device Not Available)
#define EXCEPTION_DF    8       // 双重故障 (Double Fault)
#define EXCEPTION_CSO   9       // 协处理器段越界 (Coprocessor Segment Overrun)
#define EXCEPTION_TS    10      // 无效TSS (Invalid TSS)
#define EXCEPTION_NP    11      // 段不存在 (Segment Not Present)
#define EXCEPTION_SS    12      // 栈段故障 (Stack-Segment Fault)
#define EXCEPTION_GP    13      // 通用保护故障 (General Protection Fault)
#define EXCEPTION_PF    14      // 缺页异常 (Page Fault)
#define EXCEPTION_RESERVED 15   // Intel保留
#define EXCEPTION_MF    16      // 浮点异常 (x87 FPU Error)
#define EXCEPTION_AC    17      // 对齐检查 (Alignment Check)
#define EXCEPTION_MC    18      // 机器检查 (Machine Check)
#define EXCEPTION_XM    19      // SIMD浮点异常 (SIMD Floating-Point)
#define EXCEPTION_VE    20      // 虚拟化异常 (Virtualization)
#define EXCEPTION_CP    21      // 控制保护异常 (Control Protection)

/* 硬件中断向量（32-47，IRQ0-IRQ15） */
#define IRQ_BASE        32      // IRQ重映射基址
#define IRQ0            32      // 定时器
#define IRQ1            33      // 键盘
#define IRQ2            34      // 级联到从PIC
#define IRQ3            35      // 串口2
#define IRQ4            36      // 串口1
#define IRQ5            37      // 并口2/声卡
#define IRQ6            38      // 软盘控制器
#define IRQ7            39      // 并口1
#define IRQ8            40      // 实时时钟(RTC)
#define IRQ9            41      // ACPI
#define IRQ10           42      // 可用
#define IRQ11           43      // 可用
#define IRQ12           44      // PS/2鼠标
#define IRQ13           45      // 数学协处理器
#define IRQ14           46      // 主IDE
#define IRQ15           47      // 从IDE

/* 系统调用向量 */
#define INT_SYSCALL     0x80    // 系统调用中断号

/* IDT门类型 */
#define IDT_TYPE_TASK       0x5     // 任务门
#define IDT_TYPE_INT16      0x6     // 16位中断门
#define IDT_TYPE_TRAP16     0x7     // 16位陷阱门
#define IDT_TYPE_INT32      0xE     // 32位中断门
#define IDT_TYPE_TRAP32     0xF     // 32位陷阱门

/* IDT门属性 */
#define IDT_ATTR_PRESENT    0x80    // P位：存在位
#define IDT_ATTR_DPL0       0x00    // DPL=0 (内核特权级)
#define IDT_ATTR_DPL1       0x20    // DPL=1
#define IDT_ATTR_DPL2       0x40    // DPL=2
#define IDT_ATTR_DPL3       0x60    // DPL=3 (用户特权级)

/* 常用门属性组合 */
#define IDT_ATTR_INTERRUPT  (IDT_ATTR_PRESENT | IDT_ATTR_DPL0 | IDT_TYPE_INT32)
#define IDT_ATTR_TRAP       (IDT_ATTR_PRESENT | IDT_ATTR_DPL0 | IDT_TYPE_TRAP32)
#define IDT_ATTR_SYSCALL    (IDT_ATTR_PRESENT | IDT_ATTR_DPL3 | IDT_TYPE_TRAP32)

/* 
 * IDT门描述符结构（8字节）
 * 
 * 63                                                                  32
 * ┌──────────────────────────────────────────────────────────────────────┐
 * │                     Offset 31:16                                     │
 * └──────────────────────────────────────────────────────────────────────┘
 * 31       23       15       7       0
 * ┌─────────┬───────┬────────┬────────┐
 * │ Offset  │   P   │  Type  │ Offset │
 * │  15:0   │ DPL 0 │ Flags  │ 31:16  │
 * └─────────┴───────┴────────┴────────┘
 */
struct idt_entry {
    uint16_t offset_low;        // 中断处理函数地址低16位
    uint16_t selector;          // 内核代码段选择子
    uint8_t  zero;              // 保留，必须为0
    uint8_t  type_attr;         // 类型和属性
    uint16_t offset_high;       // 中断处理函数地址高16位
} __attribute__((packed));

/* IDT指针结构（用于LIDT指令） */
struct idt_ptr {
    uint16_t limit;             // IDT大小 - 1
    uint32_t base;              // IDT基址
} __attribute__((packed));

/*
 * 中断帧结构
 * 
 * 保存中断发生时的完整CPU状态，用于中断返回时恢复
 */
struct interrupt_frame {
    /* 段寄存器（手动保存） */
    uint32_t gs, fs, es, ds;
    
    /* 通用寄存器（使用pusha保存） */
    uint32_t edi, esi, ebp, esp_dummy;
    uint32_t ebx, edx, ecx, eax;
    
    /* 中断号和错误码（ISR压入） */
    uint32_t int_no;            // 中断向量号
    uint32_t err_code;          // 错误码（部分异常有）
    
    /* CPU自动保存的寄存器 */
    uint32_t eip;               // 返回地址
    uint32_t cs;                // 代码段
    uint32_t eflags;            // 标志寄存器
    uint32_t user_esp;          // 用户栈指针（特权级改变时）
    uint32_t ss;                // 栈段（特权级改变时）
} __attribute__((packed));

/* 中断处理函数类型 */
typedef void (*interrupt_handler_t)(struct interrupt_frame *frame);

/* IDT初始化 */
void idt_init(void);

/* 设置IDT门 */
void idt_set_gate(uint8_t num, uint32_t handler, uint16_t selector, uint8_t flags);

/* ISR函数声明（汇编实现） */
extern void isr0(void);     // 除零异常
extern void isr1(void);     // 调试异常
extern void isr2(void);     // 不可屏蔽中断
extern void isr3(void);     // 断点异常
extern void isr4(void);     // 溢出异常
extern void isr5(void);     // 边界检查
extern void isr6(void);     // 无效操作码
extern void isr7(void);     // 设备不可用
extern void isr8(void);     // 双重故障
extern void isr9(void);     // 协处理器段越界
extern void isr10(void);    // 无效TSS
extern void isr11(void);    // 段不存在
extern void isr12(void);    // 栈段故障
extern void isr13(void);    // 通用保护故障
extern void isr14(void);    // 缺页异常
extern void isr15(void);    // Intel保留
extern void isr16(void);    // 浮点异常
extern void isr17(void);    // 对齐检查
extern void isr18(void);    // 机器检查
extern void isr19(void);    // SIMD浮点异常
extern void isr20(void);    // 虚拟化异常
extern void isr21(void);    // 控制保护异常
extern void isr22(void);    // 保留
extern void isr23(void);    // 保留
extern void isr24(void);    // 保留
extern void isr25(void);    // 保留
extern void isr26(void);    // 保留
extern void isr27(void);    // 保留
extern void isr28(void);    // 保留
extern void isr29(void);    // 保留
extern void isr30(void);    // 保留
extern void isr31(void);    // 保留

/* IRQ函数声明（汇编实现） */
extern void irq0(void);     // 定时器
extern void irq1(void);     // 键盘
extern void irq2(void);     // 级联
extern void irq3(void);     // 串口2
extern void irq4(void);     // 串口1
extern void irq5(void);     // 并口2/声卡
extern void irq6(void);     // 软盘
extern void irq7(void);     // 并口1
extern void irq8(void);     // 实时时钟
extern void irq9(void);     // ACPI
extern void irq10(void);    // 可用
extern void irq11(void);    // 可用
extern void irq12(void);    // PS/2鼠标
extern void irq13(void);    // 协处理器
extern void irq14(void);    // 主IDE
extern void irq15(void);    // 从IDE

#endif /* _ARCH_I386_IDT_H */

