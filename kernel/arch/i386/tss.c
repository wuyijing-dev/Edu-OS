/*
 * tss.c - 任务状态段（Task State Segment）
 * 
 * TSS 用于特权级切换时保存/恢复状态
 */

#include <kernel.h>
#include <string.h>

/* TSS 结构 */
struct tss_entry {
    uint32_t prev_tss;
    uint32_t esp0;       /* 内核栈指针（Ring 0） */
    uint32_t ss0;        /* 内核栈段（Ring 0） */
    uint32_t esp1;
    uint32_t ss1;
    uint32_t esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint32_t es;
    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t fs;
    uint32_t gs;
    uint32_t ldt;
    uint16_t trap;
    uint16_t iomap_base;
} __attribute__((packed));

/* 全局 TSS */
static struct tss_entry tss;

/* 内核栈 */
static uint8_t kernel_stack[8192] __attribute__((aligned(16)));

/*
 * 动态设置 GDT 中的 TSS 描述符
 */
static void update_gdt_tss(void)
{
    uint32_t base = (uint32_t)&tss;
    
    /* GDT 基址（从 GDTR 读取） */
    struct {
        uint16_t limit;
        uint32_t base;
    } __attribute__((packed)) gdtr;
    
    asm volatile("sgdt %0" : "=m"(gdtr));
    
    /* GDT[5] = TSS 描述符（偏移 40 字节） */
    uint8_t *tss_desc = (uint8_t*)(gdtr.base + 40);
    
    /* 填入 TSS 基址 */
    tss_desc[2] = (base) & 0xFF;
    tss_desc[3] = (base >> 8) & 0xFF;
    tss_desc[4] = (base >> 16) & 0xFF;
    tss_desc[7] = (base >> 24) & 0xFF;
    
    kprintf("[TSS] Updated GDT TSS descriptor with base 0x%08x\n", base);
}

/*
 * 初始化 TSS
 */
void tss_init(void)
{
    /* 清零 TSS */
    memset(&tss, 0, sizeof(tss));
    
    /* 设置内核栈 */
    tss.ss0 = 0x10;  // 内核数据段
    tss.esp0 = (uint32_t)kernel_stack + sizeof(kernel_stack);  // 栈顶
    
    /* 设置段寄存器 */
    tss.cs = 0x08;   // 内核代码段
    tss.ss = 0x10;
    tss.ds = 0x10;
    tss.es = 0x10;
    tss.fs = 0x10;
    tss.gs = 0x10;
    
    kprintf("[TSS] Initialized at 0x%08x\n", (uint32_t)&tss);
    kprintf("[TSS] Kernel stack: 0x%08x\n", tss.esp0);
    
    /* 动态更新 GDT 中的 TSS 描述符 */
    update_gdt_tss();
    
    /* 加载 TSS 到 TR 寄存器 */
    asm volatile("ltr %%ax" : : "a"(0x28));  // 0x28 = GDT[5]
    
    kprintf("[TSS] TSS loaded into TR register\n");
}

/*
 * 获取 TSS 地址（用于 GDT）
 */
uint32_t tss_get_address(void)
{
    return (uint32_t)&tss;
}

/*
 * 获取 TSS 大小
 */
uint32_t tss_get_size(void)
{
    return sizeof(tss);
}

/*
 * 设置内核栈
 */
void tss_set_kernel_stack(uint32_t stack)
{
    tss.esp0 = stack;
}

