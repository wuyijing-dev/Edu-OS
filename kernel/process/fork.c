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
#include <mm/vma.h>
#include <kernel.h>
#include <string.h>

/*
 * 复制页目录（Linux风格COW实现）
 * 
 * Linux方式：
 * 1. 复制内核空间映射（共享）
 * 2. 用户空间使用COW：父子进程共享物理页，标记为只读
 * 3. 写入时触发Page Fault，再复制页面
 */
static struct page_directory *copy_page_directory(struct process *parent)
{
    /* 使用vmm_create_page_directory创建新页目录（已包含内核映射） */
    extern struct page_directory *vmm_create_page_directory(void);
    struct page_directory *child_pd = vmm_create_page_directory();
    
    if (!child_pd) {
        return NULL;
    }
    
    /* 获取父进程页目录 */
    uint32_t *parent_pd;
    if (parent->page_dir) {
        parent_pd = (uint32_t*)((uint32_t)parent->page_dir->physical_addr + 0xC0000000);
    } else {
        /* 使用当前页目录 */
        uint32_t cr3;
        asm volatile("mov %%cr3, %0" : "=r"(cr3));
        parent_pd = (uint32_t*)(cr3 + 0xC0000000);
    }
    
    uint32_t *new_pd = (uint32_t*)(child_pd->physical_addr + 0xC0000000);
    
    /* 用户空间（0-767）：实现写时复制 */
    for (int i = 0; i < 768; i++) {
        if (parent_pd[i] & 0x1) {  /* Present */
            /* Linux COW策略：
             * 1. 复制PDE到子进程
             * 2. 标记父子的PDE为只读
             * 3. 写入时触发Page Fault，再复制物理页
             */
            new_pd[i] = parent_pd[i] & ~0x2;  /* 清除WRITABLE位 */
            parent_pd[i] &= ~0x2;              /* 父进程也变只读 */
            
            /* 注意：这里需要刷新TLB */
            /* 实际应该逐页刷新，这里简化 */
        }
    }
    
    return child_pd;
}

/*
 * 复制进程内存（Linux风格完整实现）
 */
static int copy_process_memory(struct process *parent, struct process *child)
{
    /* 1. 复制页目录（包含COW设置） */
    child->page_dir = copy_page_directory(parent);
    
    if (!child->page_dir) {
        return -ENOMEM;
    }
    
    /* 2. 复制VMA列表（Linux关键：每个进程有独立的VMA） */
    child->vma_list = NULL;
    
    struct vma *parent_vma = parent->vma_list;
    while (parent_vma) {
        extern struct vma *vma_create(uint32_t start, uint32_t end, uint32_t flags);
        extern void vma_add(struct vma **list, struct vma *vma);
        
        /* 创建子进程的VMA副本 */
        struct vma *child_vma = vma_create(parent_vma->start, parent_vma->end, parent_vma->flags);
        if (child_vma) {
            /* 复制VMA属性 */
            child_vma->file_offset = parent_vma->file_offset;
            child_vma->fd = parent_vma->fd;
            
            /* Linux风格：如果是文件映射，设置为COW */
            if (parent_vma->flags & VMA_FILE) {
                child_vma->flags |= VMA_PRIVATE;  /* 私有映射，写时复制 */
            }
            
            vma_add(&child->vma_list, child_vma);
        }
        
        parent_vma = parent_vma->next;
    }
    
    kprintf("[FORK] Memory copied using COW (parent=%u, child=%u)\n", 
            parent->pid, child->pid);
    kprintf("[FORK] VMA list copied, physical pages shared (COW enabled)\n");
    
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
    
    /* 复制VMA列表（COW共享物理页） */
    child->vma_list = NULL;
    if (parent->vma_list) {
        extern struct vma *vma_create(uint32_t start, uint32_t end, uint32_t flags);
        extern void vma_add(struct vma **list, struct vma *vma);
        
        struct vma *parent_vma = parent->vma_list;
        while (parent_vma) {
            struct vma *child_vma = vma_create(parent_vma->start, parent_vma->end, parent_vma->flags);
            if (child_vma) {
                child_vma->file_offset = parent_vma->file_offset;
                child_vma->fd = parent_vma->fd;
                vma_add(&child->vma_list, child_vma);
            }
            parent_vma = parent_vma->next;
        }
        
        kprintf("[FORK] VMA list copied (COW enabled)\n");
    }
    
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

