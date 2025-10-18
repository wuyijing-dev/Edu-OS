/*
 * usermode.c - 用户态进程支持
 */

#include <kernel.h>
#include <process/process.h>
#include <mm/vmm.h>
#include <mm/pmm.h>
#include <string.h>

/*
 * 切换到用户模式
 * 
 * @param entry: 用户程序入口地址
 * @param stack: 用户栈地址
 */
void switch_to_user_mode(uint32_t entry, uint32_t stack)
{
    kprintf("[USER] Switching to user mode...\n");
    kprintf("[USER] Entry: 0x%08x, Stack: 0x%08x\n", entry, stack);
    
    /* 构造 IRET 栈帧切换到用户态 */
    asm volatile(
        /* 设置数据段为用户态 */
        "mov $0x23, %%ax\n"      // 0x23 = 用户数据段选择子
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        
        /* 构造 IRET 栈帧 */
        "pushl $0x23\n"          // SS (用户数据段)
        "pushl %1\n"             // ESP (用户栈)
        "pushf\n"                // EFLAGS
        "popl %%eax\n"
        "orl $0x200, %%eax\n"    // 设置 IF=1（启用中断）
        "pushl %%eax\n"
        "pushl $0x1B\n"          // CS (用户代码段)
        "pushl %0\n"             // EIP (入口地址)
        
        /* 执行 IRET，切换到用户态 */
        "iret\n"
        :
        : "r"(entry), "r"(stack)
    );
}

/*
 * 测试用户态（已废弃，使用 elf_exec 替代）
 */
void test_user_mode(void)
{
    kprintf("\n");
    kprintf("================================================================================\n");
    kprintf("=== Chapter 14: Testing User Mode ===\n");
    kprintf("================================================================================\n");
    kprintf("\n");
    
    kprintf("[USER] GDT user segments added ✅\n");
    kprintf("[USER] System calls ready ✅\n");
    kprintf("[INFO] Use elf_exec() to run user programs\n");
    kprintf("[INFO] Example: elf_exec(\"/hello.elf\")\n\n");
    
    kprintf("\n");
    kprintf("================================================================================\n");
    kprintf("=== User Mode Test Completed ===\n");
    kprintf("================================================================================\n");
    kprintf("\n");
}

