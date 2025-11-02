/*
 * syscall.c - 系统调用核心实现
 */

#include <syscall.h>
#include <fs/vfs.h>
#include <kernel.h>
#include <arch/i386/idt.h>
#include <process/process.h>
#include <mm/vmm.h>

/* ========== 系统调用表 ========== */

extern syscall_func_t syscall_table[MAX_SYSCALLS];

/* ========== 地址验证 ========== */

/*
 * 用户空间地址验证函数（实现在 kernel/mm/uaccess.c）
 */
extern bool is_user_address(const void *addr);
extern bool is_user_buffer(const void *buf, size_t size);

/* ========== 系统调用分发 ========== */

/*
 * 系统调用分发器（从汇编代码调用）
 */
int syscall_dispatch(struct syscall_frame *frame)
{
    if (!frame) {
        return -EINVAL;
    }
    
    uint32_t syscall_num = frame->eax;
    
    /* 检查系统调用号 */
    if (syscall_num >= MAX_SYSCALLS) {
        kprintf("[SYSCALL] Invalid syscall number: %u\n", syscall_num);
        return -ENOSYS;
    }
    
    /* 获取系统调用函数 */
    syscall_func_t func = syscall_table[syscall_num];
    if (!func) {
        kprintf("[SYSCALL] Unimplemented syscall: %u\n", syscall_num);
        return -ENOSYS;
    }
    
    /* 调用系统调用 */
    int ret = func(
        frame->ebx,  /* arg1 */
        frame->ecx,  /* arg2 */
        frame->edx,  /* arg3 */
        frame->esi,  /* arg4 */
        frame->edi,  /* arg5 */
        frame->ebp   /* arg6 */
    );
    
    /* 返回值写入 eax */
    frame->eax = ret;
    
    return ret;
}

/* ========== 初始化 ========== */

/*
 * 初始化系统调用接口
 */
void syscall_init(void)
{
    kprintf("[SYSCALL] Initializing system call interface...\n");
    
    /* 声明汇编入口 */
    extern void syscall_handler(void);
    
    /* 注册 INT 0x80（DPL=3，用户态可调用） */
    idt_set_gate(0x80, (uint32_t)syscall_handler, 
                 0x08,  /* 内核代码段 */
                 0xEE); /* P=1, DPL=3, Type=E (32-bit interrupt gate) */
    
    kprintf("[SYSCALL] Registered INT 0x80 (DPL=3)\n");
    kprintf("[SYSCALL] %d system calls available\n", MAX_SYSCALLS);
    kprintf("[SYSCALL] System call interface initialized\n");
}

