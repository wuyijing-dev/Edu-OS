/*
 * IDT (中断描述符表) 实现
 * 
 * 负责初始化和管理256个中断向量
 */

#include <arch/i386/idt.h>
#include <arch/i386/pic.h>
#include <arch/i386/irq.h>
#include <string.h>
#include <kernel.h>

/* IDT表（256个条目） */
static struct idt_entry idt[IDT_ENTRIES] __attribute__((aligned(8)));

/* IDT指针（用于LIDT指令） */
static struct idt_ptr idtp;

/*
 * 设置IDT门
 * 
 * @param num: 中断向量号（0-255）
 * @param handler: 中断处理函数地址
 * @param selector: 代码段选择子（通常是0x08，内核代码段）
 * @param flags: 门属性（类型+特权级+存在位）
 */
void idt_set_gate(uint8_t num, uint32_t handler, uint16_t selector, uint8_t flags)
{
    idt[num].offset_low  = handler & 0xFFFF;
    idt[num].offset_high = (handler >> 16) & 0xFFFF;
    idt[num].selector    = selector;
    idt[num].zero        = 0;
    idt[num].type_attr   = flags;
}

/*
 * 安装ISR（中断服务程序）
 * 
 * 为CPU异常（0-31）安装处理程序
 */
static void idt_install_isrs(void)
{
    /* 设置所有ISR为中断门，DPL=0（仅内核可调用） */
    idt_set_gate(0,  (uint32_t)isr0,  0x08, IDT_ATTR_INTERRUPT);
    idt_set_gate(1,  (uint32_t)isr1,  0x08, IDT_ATTR_INTERRUPT);
    idt_set_gate(2,  (uint32_t)isr2,  0x08, IDT_ATTR_INTERRUPT);
    idt_set_gate(3,  (uint32_t)isr3,  0x08, IDT_ATTR_TRAP);      // 断点使用陷阱门
    idt_set_gate(4,  (uint32_t)isr4,  0x08, IDT_ATTR_INTERRUPT);
    idt_set_gate(5,  (uint32_t)isr5,  0x08, IDT_ATTR_INTERRUPT);
    idt_set_gate(6,  (uint32_t)isr6,  0x08, IDT_ATTR_INTERRUPT);
    idt_set_gate(7,  (uint32_t)isr7,  0x08, IDT_ATTR_INTERRUPT);
    idt_set_gate(8,  (uint32_t)isr8,  0x08, IDT_ATTR_INTERRUPT);
    idt_set_gate(9,  (uint32_t)isr9,  0x08, IDT_ATTR_INTERRUPT);
    idt_set_gate(10, (uint32_t)isr10, 0x08, IDT_ATTR_INTERRUPT);
    idt_set_gate(11, (uint32_t)isr11, 0x08, IDT_ATTR_INTERRUPT);
    idt_set_gate(12, (uint32_t)isr12, 0x08, IDT_ATTR_INTERRUPT);
    idt_set_gate(13, (uint32_t)isr13, 0x08, IDT_ATTR_INTERRUPT);
    /* Page Fault 必须允许用户态触发（DPL=3），因为按需分配需要处理用户空间缺页 */
    idt_set_gate(14, (uint32_t)isr14, 0x08, IDT_ATTR_PRESENT | IDT_ATTR_DPL3 | IDT_TYPE_INT32);
    idt_set_gate(15, (uint32_t)isr15, 0x08, IDT_ATTR_INTERRUPT);
    idt_set_gate(16, (uint32_t)isr16, 0x08, IDT_ATTR_INTERRUPT);
    idt_set_gate(17, (uint32_t)isr17, 0x08, IDT_ATTR_INTERRUPT);
    idt_set_gate(18, (uint32_t)isr18, 0x08, IDT_ATTR_INTERRUPT);
    idt_set_gate(19, (uint32_t)isr19, 0x08, IDT_ATTR_INTERRUPT);
    idt_set_gate(20, (uint32_t)isr20, 0x08, IDT_ATTR_INTERRUPT);
    idt_set_gate(21, (uint32_t)isr21, 0x08, IDT_ATTR_INTERRUPT);
    idt_set_gate(22, (uint32_t)isr22, 0x08, IDT_ATTR_INTERRUPT);
    idt_set_gate(23, (uint32_t)isr23, 0x08, IDT_ATTR_INTERRUPT);
    idt_set_gate(24, (uint32_t)isr24, 0x08, IDT_ATTR_INTERRUPT);
    idt_set_gate(25, (uint32_t)isr25, 0x08, IDT_ATTR_INTERRUPT);
    idt_set_gate(26, (uint32_t)isr26, 0x08, IDT_ATTR_INTERRUPT);
    idt_set_gate(27, (uint32_t)isr27, 0x08, IDT_ATTR_INTERRUPT);
    idt_set_gate(28, (uint32_t)isr28, 0x08, IDT_ATTR_INTERRUPT);
    idt_set_gate(29, (uint32_t)isr29, 0x08, IDT_ATTR_INTERRUPT);
    idt_set_gate(30, (uint32_t)isr30, 0x08, IDT_ATTR_INTERRUPT);
    idt_set_gate(31, (uint32_t)isr31, 0x08, IDT_ATTR_INTERRUPT);
}

/*
 * 安装IRQ处理程序
 * 
 * 为硬件中断（32-47）安装处理程序
 */
static void idt_install_irqs(void)
{
    /* IRQ 0-15 映射到中断 32-47 */
    idt_set_gate(32, (uint32_t)irq0,  0x08, IDT_ATTR_INTERRUPT);
    idt_set_gate(33, (uint32_t)irq1,  0x08, IDT_ATTR_INTERRUPT);
    idt_set_gate(34, (uint32_t)irq2,  0x08, IDT_ATTR_INTERRUPT);
    idt_set_gate(35, (uint32_t)irq3,  0x08, IDT_ATTR_INTERRUPT);
    idt_set_gate(36, (uint32_t)irq4,  0x08, IDT_ATTR_INTERRUPT);
    idt_set_gate(37, (uint32_t)irq5,  0x08, IDT_ATTR_INTERRUPT);
    idt_set_gate(38, (uint32_t)irq6,  0x08, IDT_ATTR_INTERRUPT);
    idt_set_gate(39, (uint32_t)irq7,  0x08, IDT_ATTR_INTERRUPT);
    idt_set_gate(40, (uint32_t)irq8,  0x08, IDT_ATTR_INTERRUPT);
    idt_set_gate(41, (uint32_t)irq9,  0x08, IDT_ATTR_INTERRUPT);
    idt_set_gate(42, (uint32_t)irq10, 0x08, IDT_ATTR_INTERRUPT);
    idt_set_gate(43, (uint32_t)irq11, 0x08, IDT_ATTR_INTERRUPT);
    idt_set_gate(44, (uint32_t)irq12, 0x08, IDT_ATTR_INTERRUPT);
    idt_set_gate(45, (uint32_t)irq13, 0x08, IDT_ATTR_INTERRUPT);
    idt_set_gate(46, (uint32_t)irq14, 0x08, IDT_ATTR_INTERRUPT);
    idt_set_gate(47, (uint32_t)irq15, 0x08, IDT_ATTR_INTERRUPT);
}

/*
 * IDT初始化
 * 
 * 初始化中断描述符表并加载到CPU
 */
void idt_init(void)
{
    /* 清空IDT */
    memset(idt, 0, sizeof(idt));
    
    /* 设置IDT指针 */
    idtp.limit = sizeof(idt) - 1;
    idtp.base  = (uint32_t)&idt;
    
    /* 安装ISR和IRQ */
    idt_install_isrs();
    idt_install_irqs();
    
    /* 调试：验证 Page Fault 的 IDT 门设置 */
    kprintf("[IDT] Page Fault gate (14): offset=0x%08x, selector=0x%04x, attr=0x%02x\n",
            (idt[14].offset_high << 16) | idt[14].offset_low,
            idt[14].selector,
            idt[14].type_attr);
    
    /* 加载IDT到CPU */
    __asm__ volatile("lidt %0" : : "m"(idtp));
    
    kprintf("[IDT] Interrupt Descriptor Table initialized\n");
    kprintf("[IDT] IDT base: 0x%08x, limit: %d bytes (%d entries)\n",
            idtp.base, idtp.limit + 1, IDT_ENTRIES);
}

