/*
 * scheduler.c - 进程调度器实现（支持多种调度算法）
 */

#include <process/scheduler.h>
#include <process/process.h>
#include <process/mlfq_sched.h>
#include <kernel.h>
#include <string.h>

/* 调度策略 */
#define SCHED_POLICY_RR    0   /* Round Robin */
#define SCHED_POLICY_MLFQ  1   /* Multi-Level Feedback Queue */

static int current_policy = SCHED_POLICY_MLFQ;  /* 默认使用MLFQ */

/* 调度器状态 */
static struct {
    struct process *current;           /* 当前运行的进程 */
    struct process *idle_process;      /* idle进程 */
    struct process *ready_queue_head;  /* 就绪队列头 */
    struct process *ready_queue_tail;  /* 就绪队列尾 */
    uint32_t ready_count;              /* 就绪进程数 */
    uint64_t total_switches;           /* 总切换次数 */
    bool enabled;                      /* 调度器是否启用 */
} scheduler;

/* 外部汇编实现的上下文切换 */
extern void context_switch_asm(struct cpu_context *prev_ctx, struct cpu_context *next_ctx);

/*
 * idle进程（CPU空闲时运行）
 */
static void idle_thread(void)
{
    while (1) {
        asm volatile("hlt");  /* 挂起CPU，等待中断 */
    }
}

/*
 * 调度器初始化
 */
void scheduler_init(void)
{
    kprintf("[SCHEDULER] Initializing Scheduler...\n");
    
    memset(&scheduler, 0, sizeof(scheduler));
    
    scheduler.enabled = false;
    scheduler.current = NULL;
    scheduler.ready_queue_head = NULL;
    scheduler.ready_queue_tail = NULL;
    scheduler.ready_count = 0;
    scheduler.total_switches = 0;
    
    /* 创建idle进程（最低优先级，不加入就绪队列） */
    scheduler.idle_process = process_create_kernel_thread("idle", idle_thread, 0);
    if (!scheduler.idle_process) {
        panic("Failed to create idle process");
    }
    
    /* 将idle进程从就绪队列中移除（它会在没有其他进程时被选中） */
    scheduler_remove_process(scheduler.idle_process);
    
    kprintf("[SCHEDULER] Scheduler initialized\n");
    kprintf("[SCHEDULER] Idle process created (PID %u)\n", scheduler.idle_process->pid);
}

/*
 * 启用调度器
 */
void scheduler_enable(void)
{
    scheduler.enabled = true;
    kprintf("[SCHEDULER] Scheduler enabled\n");
    
    /* 立即执行第一次调度 */
    scheduler.current = NULL;  /* 标记为首次调度 */
    scheduler_schedule();
}

/*
 * 添加进程到就绪队列（队尾）
 */
void scheduler_add_process(struct process *proc)
{
    if (!proc) {
        return;
    }
    
    /* 确保进程不在队列中 */
    proc->next = NULL;
    proc->prev = NULL;
    
    if (scheduler.ready_queue_tail) {
        scheduler.ready_queue_tail->next = proc;
        proc->prev = scheduler.ready_queue_tail;
        scheduler.ready_queue_tail = proc;
    } else {
        /* 队列为空 */
        scheduler.ready_queue_head = proc;
        scheduler.ready_queue_tail = proc;
    }
    
    scheduler.ready_count++;
}

/*
 * 从就绪队列中移除进程
 */
void scheduler_remove_process(struct process *proc)
{
    if (!proc) {
        return;
    }
    
    /* 从队列中移除 */
    if (proc->prev) {
        proc->prev->next = proc->next;
    } else {
        scheduler.ready_queue_head = proc->next;
    }
    
    if (proc->next) {
        proc->next->prev = proc->prev;
    } else {
        scheduler.ready_queue_tail = proc->prev;
    }
    
    proc->next = NULL;
    proc->prev = NULL;
    
    if (scheduler.ready_count > 0) {
        scheduler.ready_count--;
    }
}

/*
 * 选择下一个要运行的进程
 */
static struct process *scheduler_pick_next(void)
{
    /* 如果就绪队列为空，返回idle进程 */
    if (scheduler.ready_queue_head == NULL) {
        return scheduler.idle_process;
    }
    
    /* 从队列头取出一个进程（FIFO） */
    struct process *next = scheduler.ready_queue_head;
    scheduler_remove_process(next);
    
    return next;
}

/*
 * 执行调度
 */
void scheduler_schedule(void)
{
    if (!scheduler.enabled) {
        return;
    }
    
    struct process *prev = scheduler.current;
    struct process *next = NULL;
    
    /* 根据调度策略选择下一个进程 */
    if (current_policy == SCHED_POLICY_MLFQ) {
        /* MLFQ：检查全局提升 */
        mlfq_boost_all();
        
        /* 从MLFQ队列选择 */
        next = mlfq_pick_next();
    } else {
        /* Round Robin */
        next = scheduler_pick_next();
    }
    
    if (!next) {
        /* 没有可运行的进程，使用idle */
        next = scheduler.idle_process;
    }
    
    /* 如果是首次调度（prev为NULL），直接启动第一个进程 */
    if (!prev) {
        process_set_state(next, PROCESS_STATE_RUNNING);
        next->time_used = 0;
        scheduler.current = next;
        scheduler.total_switches++;
        
        kprintf("[SCHEDULER] Starting first process: %s (PID %u)\n", 
                next->name, next->pid);
        
        /* 首次调度，直接跳转到新进程 */
        context_switch(NULL, next);
        
        /* 永远不应该返回 */
        panic("scheduler_schedule: returned from first context switch");
    }
    
    /* 如果当前进程还在运行状态，将其加回就绪队列 */
    if (prev->state == PROCESS_STATE_RUNNING) {
        process_set_state(prev, PROCESS_STATE_READY);
        prev->time_used = 0;  /* 重置时间片 */
        
        /* 放回就绪队列（如果不是idle进程且不是已终止的） */
        if (prev != scheduler.idle_process) {
            if (current_policy == SCHED_POLICY_MLFQ) {
                /* MLFQ：重新入队（可能改变级别） */
                mlfq_enqueue(prev);
            } else {
                /* Round Robin */
                scheduler_add_process(prev);
            }
        }
    }
    
    /* 新进程开始运行 */
    process_set_state(next, PROCESS_STATE_RUNNING);
    next->time_used = 0;
    scheduler.current = next;
    
    /* 执行上下文切换 */
    if (prev != next) {
        scheduler.total_switches++;
        
        // kprintf("[SCHEDULER] Switch: %s -> %s (total: %llu)\n",
        //         prev->name,
        //         next->name,
        //         scheduler.total_switches);
        
        context_switch(prev, next);
    }
}

/*
 * 主动让出CPU
 */
void scheduler_yield(void)
{
    scheduler_schedule();
}

/*
 * 定时器Tick（由定时器中断调用）
 */
void scheduler_tick(void)
{
    if (!scheduler.enabled || !scheduler.current) {
        return;
    }
    
    struct process *current = scheduler.current;
    
    if (current_policy == SCHED_POLICY_MLFQ) {
        /* MLFQ策略：处理时间片和优先级调整 */
        mlfq_tick(current);
        
        /* 检查是否需要调度 */
        if (current->time_slice_remaining == 0) {
            scheduler_schedule();
        }
    } else {
        /* Round Robin策略 */
        current->time_used++;
        
        /* 时间片用完，触发调度 */
        if (current->time_used >= current->time_slice) {
            scheduler_schedule();
        }
    }
}

/*
 * 获取当前进程
 */
struct process *scheduler_get_current(void)
{
    return scheduler.current;
}

/*
 * 打印调度器统计
 */
void scheduler_print_stats(void)
{
    kprintf("\n=== Scheduler Statistics ===\n");
    kprintf("Current Process:  %s (PID %u)\n",
            scheduler.current ? scheduler.current->name : "NULL",
            scheduler.current ? scheduler.current->pid : 0);
    kprintf("Ready Queue:      %u processes\n", scheduler.ready_count);
    kprintf("Total Switches:   %llu\n", scheduler.total_switches);
    kprintf("Scheduler:        %s\n", scheduler.enabled ? "ENABLED" : "DISABLED");
    kprintf("\n");
}

/*
 * 打印就绪队列
 */
void scheduler_print_ready_queue(void)
{
    kprintf("\n=== Ready Queue (%u processes) ===\n", scheduler.ready_count);
    
    struct process *proc = scheduler.ready_queue_head;
    int index = 0;
    
    while (proc) {
        kprintf("[%d] %s (PID %u, Pri %u)\n",
                index++, proc->name, proc->pid, proc->priority);
        proc = proc->next;
    }
    
    kprintf("\n");
}

/*
 * 上下文切换（C接口）
 */
void context_switch(struct process *prev, struct process *next)
{
    if (!next) {
        panic("context_switch: next is NULL");
    }
    
    /* 更新运行时间统计 */
    if (prev) {
        prev->total_runtime += prev->time_used;
    }
    
    /* 切换页目录（如果进程有独立的地址空间） */
    if (next->page_dir) {
        extern void vmm_switch_page_directory(struct page_directory *pd);
        vmm_switch_page_directory(next->page_dir);
        
        /* 调试输出（可以注释） */
        // kprintf("[SCHEDULER] Switched to PD 0x%08x for process %s\n", 
        //         next->page_dir->physical_addr, next->name);
    }
    
    /* 调用汇编实现的上下文切换 */
    if (prev) {
        context_switch_asm(&prev->context, &next->context);
    } else {
        /* 首次调度，直接加载新进程上下文 */
        context_switch_asm(NULL, &next->context);
    }
}

