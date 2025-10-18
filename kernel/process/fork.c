/*
 * fork.c - 进程复制（fork 系统调用）
 * 
 * 基于 Linux fork 实现
 */

#include <process/process.h>
#include <fs/vfs.h>
#include <mm/vmm.h>
#include <mm/pmm.h>
#include <mm/kmalloc.h>
#include <kernel.h>
#include <string.h>

/*
 * 复制页目录（写时复制优化版）
 */
static struct page_directory *copy_page_directory(struct process *parent)
{
    /* 分配新页目录 */
    uint32_t pd_phys = pmm_alloc_frame();
    if (!pd_phys) {
        return NULL;
    }
    
    uint32_t *new_pd = (uint32_t*)(pd_phys + 0xC0000000);
    memset(new_pd, 0, 4096);
    
    /* 获取父进程页目录 */
    uint32_t *parent_pd;
    if (parent->page_dir) {
        parent_pd = (uint32_t*)((uint32_t)parent->page_dir + 0xC0000000);
    } else {
        /* 使用当前页目录 */
        asm volatile("mov %%cr3, %0" : "=r"(parent_pd));
        parent_pd = (uint32_t*)((uint32_t)parent_pd + 0xC0000000);
    }
    
    /* 复制内核空间映射（768-1023） */
    for (int i = 768; i < 1024; i++) {
        new_pd[i] = parent_pd[i];
    }
    
    /* 用户空间（0-767）：写时复制 */
    for (int i = 0; i < 768; i++) {
        if (parent_pd[i] & 0x1) {  /* Present */
            /* 标记为只读（写时复制） */
            new_pd[i] = parent_pd[i] & ~0x2;  /* 清除 WRITABLE 位 */
            parent_pd[i] &= ~0x2;              /* 父进程也变只读 */
        }
    }
    
    return (struct page_directory*)pd_phys;
}

/*
 * 复制进程内存
 */
static int copy_process_memory(struct process *parent, struct process *child)
{
    /* 复制页目录 */
    child->page_dir = copy_page_directory(parent);
    
    if (!child->page_dir) {
        return -ENOMEM;
    }
    
    /* Copy-On-Write已在copy_page_directory中实现 */
    /* 父子进程共享只读页，写入时触发Page Fault并复制 */
    
    kprintf("[FORK] Memory copied using COW (parent=%u, child=%u)\n", 
            parent->pid, child->pid);
    
    return 0;
}

/*
 * fork 系统调用
 * 
 * @return: 父进程返回子进程 PID，子进程返回 0
 */
int do_fork(void)
{
    struct process *parent = process_get_current();
    
    if (!parent) {
        kprintf("[FORK] No current process\n");
        return -ESRCH;
    }
    
    kprintf("[FORK] Forking process %s (PID %u)\n", parent->name, parent->pid);
    
    /* 分配子进程结构 */
    struct process *child = kmalloc(sizeof(struct process));
    if (!child) {
        return -ENOMEM;
    }
    
    /* 复制父进程信息 */
    memcpy(child, parent, sizeof(struct process));
    
    /* 分配新 PID */
    child->pid = process_allocate_pid();
    
    /* 设置父子关系 */
    child->parent = parent;
    
    /* 子进程的名称 */
    snprintf(child->name, sizeof(child->name), "%s-child", parent->name);
    
    /* 设置状态 */
    child->state = PROCESS_STATE_READY;
    
    /* 分配内核栈 */
    child->kernel_stack = (uint32_t)kmalloc(8192);  // 8KB 栈
    if (!child->kernel_stack) {
        kfree(child);
        return -ENOMEM;
    }
    
    child->kernel_stack_size = 8192;
    
    /* 复制地址空间 */
    if (copy_process_memory(parent, child) < 0) {
        kfree((void*)child->kernel_stack);
        kfree(child);
        return -ENOMEM;
    }
    
    /* 复制 CPU 上下文（重要！） */
    memcpy(&child->context, &parent->context, sizeof(struct cpu_context));
    
    /* 子进程的返回值 = 0 */
    child->context.eax = 0;
    
    /* 加入调度器 */
    extern void scheduler_add_process(struct process *proc);
    scheduler_add_process(child);
    
    kprintf("[FORK] Created child process PID %u, added to scheduler\n", child->pid);
    
    /* 父进程返回子进程 PID */
    return child->pid;
}

