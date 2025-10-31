#include <arch/i386/gdt.h>
#include <kernel.h>
#include <string.h>

/* GDT表 - 放在内核虚拟地址空间 */
#define GDT_ENTRIES 6
static struct gdt_entry gdt_entries[GDT_ENTRIES];
static struct gdt_ptr gdt_ptr;

/* 外部汇编函数，用于加载GDT */
extern void gdt_flush(uint32_t gdt_ptr_addr);

/**
 * 设置GDT表项
 * 
 * @param num 表项编号
 * @param base 段基址
 * @param limit 段界限
 * @param access 访问权限字节
 * @param gran 粒度字节
 */
void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran)
{
    /* 设置段基址 */
    gdt_entries[num].base_low    = (base & 0xFFFF);
    gdt_entries[num].base_middle = (base >> 16) & 0xFF;
    gdt_entries[num].base_high   = (base >> 24) & 0xFF;
    
    /* 设置段界限 */
    gdt_entries[num].limit_low   = (limit & 0xFFFF);
    gdt_entries[num].granularity = (limit >> 16) & 0x0F;
    
    /* 设置粒度标志 */
    gdt_entries[num].granularity |= gran & 0xF0;
    
    /* 设置访问字节 */
    gdt_entries[num].access = access;
}

/**
 * 更新TSS段描述符
 */
void gdt_update_tss(uint32_t tss_base, uint32_t tss_limit)
{
    /* TSS段：索引5, DPL=0, 类型=可用TSS(0x89) */
    gdt_set_gate(5, tss_base, tss_limit, 0x89, 0x40);
}

/**
 * 初始化GDT
 * 
 * 在内核虚拟地址空间重新建立GDT，替换bootloader的GDT
 */
void gdt_init(void)
{
    kprintf("[GDT] Initializing Global Descriptor Table...\n");
    
    /* 设置GDT指针 */
    gdt_ptr.limit = (sizeof(struct gdt_entry) * GDT_ENTRIES) - 1;
    gdt_ptr.base  = (uint32_t)&gdt_entries;
    
    kprintf("[GDT] GDT base: 0x%08x, limit: %d bytes (%d entries)\n",
            gdt_ptr.base, gdt_ptr.limit + 1, GDT_ENTRIES);
    
    /* 
     * 设置GDT表项
     * 
     * 访问字节格式: P(1) DPL(2) S(1) Type(4)
     * 粒度字节格式: G(1) D/B(1) L(1) AVL(1) Limit[19:16](4)
     * 
     * P   = Present (1=在内存中)
     * DPL = Descriptor Privilege Level (0=Ring 0, 3=Ring 3)
     * S   = Descriptor Type (0=系统段, 1=代码/数据段)
     * Type = 段类型:
     *   代码段: 1010 = 可执行,可读
     *   数据段: 0010 = 可读写
     * 
     * G   = Granularity (0=字节, 1=4KB)
     * D/B = Default operation size (0=16位, 1=32位)
     */
    
    /* 0: NULL段 */
    gdt_set_gate(0, 0, 0, 0, 0);
    
    /* 1: 内核代码段 (0x08)
     * Base=0, Limit=0xFFFFF (4GB with G=1)
     * Access=0x9A: P=1, DPL=00, S=1, Type=1010 (代码段,可执行,可读)
     * Gran=0xCF: G=1, D=1, L=0, AVL=0, Limit[19:16]=0xF
     */
    gdt_set_gate(1, 0, 0xFFFFF, 0x9A, 0xCF);
    
    /* 2: 内核数据段 (0x10)
     * Base=0, Limit=0xFFFFF (4GB with G=1)
     * Access=0x92: P=1, DPL=00, S=1, Type=0010 (数据段,可读写)
     * Gran=0xCF: G=1, D=1, L=0, AVL=0, Limit[19:16]=0xF
     */
    gdt_set_gate(2, 0, 0xFFFFF, 0x92, 0xCF);
    
    /* 3: 用户代码段 (0x18, RPL=3后为0x1B)
     * Base=0, Limit=0xFFFFF (4GB with G=1)
     * Access=0xFA: P=1, DPL=11, S=1, Type=1010 (代码段,可执行,可读,Ring 3)
     * Gran=0xCF: G=1, D=1, L=0, AVL=0, Limit[19:16]=0xF
     */
    gdt_set_gate(3, 0, 0xFFFFF, 0xFA, 0xCF);
    
    /* 4: 用户数据段 (0x20, RPL=3后为0x23)
     * Base=0, Limit=0xFFFFF (4GB with G=1)
     * Access=0xF2: P=1, DPL=11, S=1, Type=0010 (数据段,可读写,Ring 3)
     * Gran=0xCF: G=1, D=1, L=0, AVL=0, Limit[19:16]=0xF
     */
    gdt_set_gate(4, 0, 0xFFFFF, 0xF2, 0xCF);
    
    /* 5: TSS段 (0x28) - 稍后由TSS模块更新 */
    gdt_set_gate(5, 0, 0, 0, 0);
    
    /* 加载新的GDT */
    kprintf("[GDT] Loading new GDT at 0x%08x...\n", gdt_ptr.base);
    gdt_flush((uint32_t)&gdt_ptr);
    
    kprintf("[GDT] GDT loaded successfully\n");
    kprintf("[GDT] Segment selectors:\n");
    kprintf("      Kernel Code: 0x%02x\n", GDT_KERNEL_CODE);
    kprintf("      Kernel Data: 0x%02x\n", GDT_KERNEL_DATA);
    kprintf("      User Code:   0x%02x (with RPL=3: 0x%02x)\n", 
            GDT_USER_CODE, GDT_USER_CODE | 3);
    kprintf("      User Data:   0x%02x (with RPL=3: 0x%02x)\n", 
            GDT_USER_DATA, GDT_USER_DATA | 3);
    kprintf("      TSS:         0x%02x\n", GDT_TSS);
}
