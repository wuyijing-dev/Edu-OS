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

/* 全局变量：下一个进程的CR3，供汇编使用 */
uint32_t g_next_cr3 = 0;

/* 当前调度策略 */
static int current_policy = SCHED_POLICY_MLFQ;

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
 * 调度器锁（简单兼容版本，逐步迁移到preempt_count）
 * Linux风格：异常处理时设置此标志，防止在Page Fault期间发生上下文切换
 */
int scheduler_locked = 0;

/*
 * Linux风格：抢占控制
 * 使用per-process的preempt_count，而不是全局锁
 */
static inline void preempt_disable(void)
{
    extern struct process *process_get_current(void);
    struct process *proc = process_get_current();
    if (proc) {
        proc->preempt_count++;
    }
    scheduler_locked = 1;  /* 兼容旧代码 */
}

static inline void preempt_enable(void)
{
    extern struct process *process_get_current(void);
    struct process *proc = process_get_current();
    if (proc && proc->preempt_count > 0) {
        proc->preempt_count--;
    }
    if (!proc || proc->preempt_count == 0) {
        scheduler_locked = 0;  /* 兼容旧代码 */
    }
}

static inline int preemptible(void)
{
    extern struct process *process_get_current(void);
    struct process *proc = process_get_current();
    
    /* 如果没有当前进程（首次调度），总是可抢占 */
    if (!proc) {
        return 1;
    }
    
    return proc->preempt_count == 0 && scheduler_locked == 0;
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
 * 禁用调度器（兼容接口）
 */
void scheduler_disable(void)
{
    preempt_disable();
}

/*
 * 启用调度器（不立即调度，兼容接口）
 */
void scheduler_enable_noschedule(void)
{
    preempt_enable();
}

/*
 * 添加进程到就绪队列（队尾）
 */
void scheduler_add_process(struct process *proc)
{
    if (!proc) {
        return;
    }
    
    /* 设置进程状态为READY */
    proc->state = PROCESS_STATE_READY;
    
    kprintf("[SCHEDULER] Adding process '%s' (PID %u), policy=%d\n", 
            proc->name, proc->pid, current_policy);
    
    /* 根据调度策略添加到相应队列 */
    if (current_policy == SCHED_POLICY_MLFQ) {
        /* MLFQ：初始化MLFQ级别并入队 */
        proc->mlfq_level = 0;  /* 从最高优先级开始 */
        extern void mlfq_enqueue(struct process *proc);
        mlfq_enqueue(proc);
        kprintf("[SCHEDULER] Added to MLFQ level 0\n");
    } else {
        /* Round Robin：加入ready_queue */
        proc->next = NULL;
        proc->prev = NULL;
        
        if (scheduler.ready_queue_tail) {
            scheduler.ready_queue_tail->next = proc;
            proc->prev = scheduler.ready_queue_tail;
            scheduler.ready_queue_tail = proc;
        } else {
            scheduler.ready_queue_head = proc;
            scheduler.ready_queue_tail = proc;
        }
        
        scheduler.ready_count++;
        kprintf("[SCHEDULER] Added to ready queue\n");
    }
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
    /* Linux风格：检查调度器状态和抢占计数 */
    if (!scheduler.enabled) {
        return;
    }
    
    /* 如果当前进程禁止抢占，不调度 */
    if (!preemptible()) {
        return;
    }
    
    /* 保存中断状态并关闭中断，避免递归调度 */
    uint32_t eflags;
    asm volatile("pushf; pop %0; cli" : "=r"(eflags));
    
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
        
        kprintf("[DEBUG] About to call context_switch...\n");
        kprintf("[DEBUG] next ptr = %p\n", next);
        kprintf("[DEBUG] next->name = %s\n", next->name);
        kprintf("[DEBUG] next->pid = %u\n", next->pid);
        kprintf("[DEBUG] next->context.eip = 0x%08x\n", next->context.eip);
        kprintf("[DEBUG] next->context.esp = 0x%08x\n", next->context.esp);
        kprintf("[DEBUG] next->context.cs = 0x%04x\n", next->context.cs);
        kprintf("[DEBUG] next->page_dir = %p\n", next->page_dir);
        
        /* 保存next指针到临时变量，防止被修改 */
        struct process *next_process = next;
        
        kprintf("[DEBUG] Calling context_switch(NULL, %p)...\n", next_process);
        context_switch(NULL, next_process);
        
        kprintf("[DEBUG] Returned from context_switch (should never happen!)\n");
        
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
    
    /* Linux风格：从MLFQ队列中移除即将运行的进程 */
    if (current_policy == SCHED_POLICY_MLFQ && next != scheduler.idle_process) {
        extern void mlfq_dequeue(struct process *proc);
        mlfq_dequeue(next);
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
    /* Linux风格：检查抢占计数 */
    if (!scheduler.enabled || !scheduler.current) {
        return;
    }
    
    /* 如果当前进程禁止抢占，不调度 */
    if (!preemptible()) {
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
 * 上下文切换（Linux风格实现）
 * 
 * 参考Linux内核的__switch_to函数
 */
void context_switch(struct process *prev, struct process *next)
{
    if (!next) {
        panic("context_switch: next is NULL");
    }
    
    /* Linux风格：如果调度器被锁定，拒绝切换（关键修复！）*/
    extern int scheduler_locked;
    if (scheduler_locked) {
        extern void serial_putc(uint16_t port, char c);
        serial_putc(0x3F8, '\n');
        serial_putc(0x3F8, '[');
        serial_putc(0x3F8, 'C');
        serial_putc(0x3F8, 'X');
        serial_putc(0x3F8, '_');
        serial_putc(0x3F8, 'B');
        serial_putc(0x3F8, 'L');
        serial_putc(0x3F8, 'O');
        serial_putc(0x3F8, 'C');
        serial_putc(0x3F8, 'K');
        serial_putc(0x3F8, ']');
        serial_putc(0x3F8, '\n');
        return;  /* 拒绝切换，保持当前进程 */
    }
    
    /* 调试：追踪所有context_switch调用 */
    extern void serial_putc(uint16_t port, char c);
    serial_putc(0x3F8, '\n');
    serial_putc(0x3F8, '[');
    serial_putc(0x3F8, 'C');
    serial_putc(0x3F8, 'X');
    serial_putc(0x3F8, ':');
    /* 打印进程名首字母 */
    if (next->name[0]) {
        serial_putc(0x3F8, next->name[0]);
    }
    serial_putc(0x3F8, ']');
    serial_putc(0x3F8, '\n');
    
    /* Linux风格：静默执行，减少调试输出 */
    #ifdef DEBUG_CONTEXT_SWITCH
    kprintf("[CONTEXT] %s -> %s\n", 
            prev ? prev->name : "kernel", next->name);
    #endif
    
    /* 准备内存上下文切换（页表）*/
    uint32_t next_cr3 = next->page_dir ? next->page_dir->physical_addr : 0;
    
    /* 更新TSS.ESP0（Linux关键步骤）
     * 用户态异常/中断时，CPU从TSS.ESP0加载内核栈
     */
    if (next->page_dir) {
        extern void tss_set_kernel_stack(uint32_t stack);
        extern void tss_verify(void);
        uint32_t tss_esp0 = next->kernel_stack + 4;
        kprintf("[CONTEXT] Setting TSS.ESP0 = 0x%08x (kernel_stack=0x%08x)\n", 
                tss_esp0, next->kernel_stack);
        tss_set_kernel_stack(tss_esp0);
        
        /* 首次切换时验证 TSS */
        if (!prev) {
            tss_verify();
        }
    }
    
    /* Linux方式：通过全局变量传递CR3给汇编代码
     * 原因：在C中切换CR3会导致栈和返回地址无效
     */
    extern uint32_t g_next_cr3;
    g_next_cr3 = next_cr3;
    
    /* Linux风格：idle进程可以没有page_dir（使用内核页表）
     * 只有用户进程需要独立页表
     */
    if (g_next_cr3 == 0 && next->pid != 1) {
        /* 非idle进程但page_dir为NULL，这是严重错误 */
        kprintf("[CONTEXT] FATAL: User process %s (PID %u) has NULL page_dir!\n", 
                next->name, next->pid);
        panic("NULL page directory for user process!");
    }
    
    if (g_next_cr3 != 0) {
        kprintf("[CONTEXT] Will switch CR3 to 0x%08x\n", g_next_cr3);
    } else {
        kprintf("[CONTEXT] Switching to %s (kernel thread, no CR3 switch)\n", next->name);
    }
    
    /* 调试：打印栈帧内容 */
    if (!prev && next->context.esp) {
        uint32_t *stack = (uint32_t*)next->context.esp;
        kprintf("[DEBUG] Stack at 0x%08x:\n", next->context.esp);
        kprintf("        [0]=0x%08x (EIP)\n", stack[0]);
        kprintf("        [1]=0x%08x (CS)\n", stack[1]);
        kprintf("        [2]=0x%08x (EFLAGS)\n", stack[2]);
        kprintf("        [3]=0x%08x (ESP)\n", stack[3]);
        kprintf("        [4]=0x%08x (SS)\n", stack[4]);
    }
    
    /* 执行上下文切换 */
    if (prev) {
        /* 正常切换：保存旧进程，恢复新进程 */
        context_switch_asm(&prev->context, &next->context);
    } else {
        /* 首次调度：只恢复新进程（不返回） */
        context_switch_asm(NULL, &next->context);
    }
    
    /* 清理 */
    g_next_cr3 = 0;
}

