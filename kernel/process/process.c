/*
 * process.c - 进程管理实现
 */

#include <process/process.h>
#include <process/scheduler.h>
#include <mm/kmalloc.h>
#include <string.h>
#include <kernel.h>
#include <drivers/timer.h>

/* 进程计数器和PID分配 */
static uint32_t next_pid = 1;
static uint32_t process_count = 0;

/* 所有进程链表（双向链表）*/
struct process *process_list_head = NULL;  /* 导出给其他模块使用 */
static struct process *process_list_tail = NULL;

/* 当前运行进程 */
static struct process *current_process = NULL;

/*
 * 进程管理器初始化
 */
void process_init(void)
{
    kprintf("[PROCESS] Initializing Process Manager...\n");
    
    next_pid = 1;
    process_count = 0;
    process_list_head = NULL;
    process_list_tail = NULL;
    current_process = NULL;
    
    kprintf("[PROCESS] Process Manager initialized\n\n");
}

/*
 * 添加进程到链表
 */
static void add_process_to_list(struct process *proc)
{
    if (!proc) {
        return;
    }
    
    proc->next = NULL;
    proc->prev = NULL;
    
    if (!process_list_head) {
        /* 第一个进程 */
        process_list_head = proc;
        process_list_tail = proc;
    } else {
        /* 添加到尾部 */
        process_list_tail->next = proc;
        proc->prev = process_list_tail;
        process_list_tail = proc;
    }
    
    process_count++;
}

/*
 * 从链表移除进程
 */
static void remove_process_from_list(struct process *proc)
{
    if (!proc) {
        return;
    }
    
    if (proc->prev) {
        proc->prev->next = proc->next;
    } else {
        /* 是头节点 */
        process_list_head = proc->next;
    }
    
    if (proc->next) {
        proc->next->prev = proc->prev;
    } else {
        /* 是尾节点 */
        process_list_tail = proc->prev;
    }
    
    proc->next = NULL;
    proc->prev = NULL;
    process_count--;
}

/*
 * 分配新的PID
 */
uint32_t process_allocate_pid(void)
{
    return next_pid++;
}

/*
 * 进程状态转换
 */
void process_set_state(struct process *proc, process_state_t new_state)
{
    if (!proc) {
        return;
    }
    
    process_state_t old_state = proc->state;
    proc->state = new_state;
    
    // 可以在这里添加状态转换日志
    // kprintf("[PROCESS] %s: %s -> %s\n", proc->name,
    //         process_state_to_string(old_state),
    //         process_state_to_string(new_state));
}

/*
 * 状态转字符串
 */
const char *process_state_to_string(process_state_t state)
{
    switch (state) {
        case PROCESS_STATE_NEW:        return "NEW";
        case PROCESS_STATE_READY:      return "READY";
        case PROCESS_STATE_RUNNING:    return "RUNNING";
        case PROCESS_STATE_BLOCKED:    return "BLOCKED";
        case PROCESS_STATE_TERMINATED: return "TERMINATED";
        default:                       return "UNKNOWN";
    }
}

/*
 * 内核线程入口包装器
 */
static void kernel_thread_entry(void)
{
    /* 获取当前进程 */
    struct process *current = scheduler_get_current();
    if (!current) {
        panic("kernel_thread_entry: no current process");
    }
    
    /* 从栈中获取实际的入口函数（我们会在创建时将其放在栈上） */
    void (*entry)(void) = (void (*)(void))current->context.ebx;
    
    /* 启用中断 */
    asm volatile("sti");
    
    /* 调用实际的入口函数 */
    if (entry) {
        entry();
    }
    
    /* 如果函数返回，退出进程 */
    process_exit(0);
}

/*
 * 创建内核线程
 */
struct process *process_create_kernel_thread(
    const char *name,
    void (*entry)(void),
    uint32_t priority)
{
    if (!entry) {
        kprintf("[PROCESS] ERROR: entry is NULL\n");
        return NULL;
    }
    
    /* 1. 分配PCB */
    struct process *proc = (struct process*)kmalloc(sizeof(struct process));
    if (!proc) {
        kprintf("[PROCESS] ERROR: Failed to allocate PCB\n");
        return NULL;
    }
    
    memset(proc, 0, sizeof(struct process));
    
    /* 2. 设置基本信息 */
    proc->pid = process_allocate_pid();
    strncpy(proc->name, name, 31);
    proc->name[31] = '\0';
    proc->state = PROCESS_STATE_NEW;
    proc->priority = priority;
    proc->time_slice = 10;  /* 10 ticks */
    proc->time_slice_remaining = 10;
    proc->start_time = timer_get_ticks();
    
    /* MLFQ相关 */
    proc->mlfq_level = 0;  /* 新进程从最高优先级开始 */
    
    /* 3. 分配内核栈（4KB） */
    proc->kernel_stack_size = 4096;
    proc->kernel_stack = (uint32_t)kmalloc(proc->kernel_stack_size);
    if (!proc->kernel_stack) {
        kprintf("[PROCESS] ERROR: Failed to allocate kernel stack\n");
        kfree(proc);
        return NULL;
    }
    
    /* 在栈底放置魔数（用于检测栈溢出） */
    uint32_t *stack_bottom = (uint32_t*)proc->kernel_stack;
    *stack_bottom = 0xDEADBEEF;
    
    /* 4. 设置初始上下文 */
    memset(&proc->context, 0, sizeof(struct cpu_context));
    
    /* 栈顶 */
    uint32_t stack_top = proc->kernel_stack + proc->kernel_stack_size;
    
    /* 初始化上下文，使其看起来像是从中断返回 */
    proc->context.esp = stack_top - 64;  /* 预留一些空间 */
    proc->context.eip = (uint32_t)kernel_thread_entry;
    proc->context.eflags = 0x202;  /* IF=1（允许中断） */
    proc->context.cs = 0x08;       /* 内核代码段 */
    proc->context.ss = 0x10;       /* 内核数据段 */
    proc->context.user_esp = 0;    /* 内核线程不需要用户栈 */
    
    /* 将实际的入口函数地址存储在ebx中 */
    proc->context.ebx = (uint32_t)entry;
    
    /* 5. 内核线程使用内核页目录 */
    proc->page_dir = NULL;  /* NULL表示使用当前页目录 */
    
    /* 6. 添加到进程列表 */
    proc->next = process_list_head;
    if (process_list_head) {
        process_list_head->prev = proc;
    }
    process_list_head = proc;
    process_count++;
    
    /* 7. 添加到调度器就绪队列 */
    process_set_state(proc, PROCESS_STATE_READY);
    scheduler_add_process(proc);
    
    kprintf("[PROCESS] Created kernel thread: %s (PID %u, Priority %u)\n",
            proc->name, proc->pid, proc->priority);
    
    return proc;
}

/*
 * 销毁进程
 */
void process_destroy(struct process *proc)
{
    if (!proc) {
        return;
    }
    
    kprintf("[PROCESS] Destroying process: %s (PID %u)\n", proc->name, proc->pid);
    
    /* 从进程列表中移除 */
    if (proc->prev) {
        proc->prev->next = proc->next;
    } else {
        process_list_head = proc->next;
    }
    
    if (proc->next) {
        proc->next->prev = proc->prev;
    }
    
    /* 释放内核栈 */
    if (proc->kernel_stack) {
        kfree((void*)proc->kernel_stack);
    }
    
    /* 清理VMA链表 */
    if (proc->vma_list) {
        extern void vma_destroy_all(struct vma *list);
        vma_destroy_all(proc->vma_list);
        proc->vma_list = NULL;
    }
    
    /* 释放页目录（如果有独立的） */
    if (proc->page_dir) {
        vmm_destroy_page_directory(proc->page_dir);
        proc->page_dir = NULL;
    }
    
    /* 释放PCB */
    kfree(proc);
    
    process_count--;
}

/*
 * 进程退出
 */
void process_exit(uint32_t exit_code)
{
    struct process *current = scheduler_get_current();
    if (!current) {
        panic("process_exit: no current process");
    }
    
    kprintf("[PROCESS] Process %s (PID %u) exiting with code %u\n",
            current->name, current->pid, exit_code);
    
    current->exit_code = exit_code;
    process_set_state(current, PROCESS_STATE_TERMINATED);
    
    /* 从调度器中移除 */
    scheduler_remove_process(current);
    
    /* 触发调度，切换到其他进程 */
    scheduler_schedule();
    
    /* 永远不应该到这里 */
    panic("process_exit: returned from schedule");
}

/*
 * 打印进程信息
 */
void process_print_info(struct process *proc)
{
    if (!proc) {
        kprintf("Process: NULL\n");
        return;
    }
    
    kprintf("Process: %s\n", proc->name);
    kprintf("  PID:      %u\n", proc->pid);
    kprintf("  State:    %s\n", process_state_to_string(proc->state));
    kprintf("  Priority: %u\n", proc->priority);
    kprintf("  EIP:      0x%08x\n", proc->context.eip);
    kprintf("  ESP:      0x%08x\n", proc->context.esp);
    kprintf("  Time:     %u/%u ticks\n", proc->time_used, proc->time_slice);
    kprintf("  Runtime:  %llu ticks\n", proc->total_runtime);
}

/*
 * 打印所有进程
 */
void process_print_all(void)
{
    kprintf("\n=== All Processes (%u total) ===\n", process_count);
    
    struct process *proc = process_list_head;
    while (proc) {
        kprintf("PID %3u: %-20s [%s] Pri:%u Time:%llu\n",
                proc->pid, proc->name,
                process_state_to_string(proc->state),
                proc->priority, proc->total_runtime);
        proc = proc->next;
    }
    
    kprintf("\n");
}

/*
 * 获取当前进程
 */
struct process *process_get_current(void)
{
    return scheduler_get_current();
}

/*
 * 设置当前进程
 */
void process_set_current(struct process *proc)
{
    // 由调度器管理
    (void)proc;
}

/*
 * 根据 PID 查找进程
 */
struct process *process_find_by_pid(pid_t pid)
{
    struct process *proc = process_list_head;
    
    while (proc) {
        if (proc->pid == pid) {
            return proc;
        }
        proc = proc->next;
    }
    
    return NULL;
}

