/*
 * mlfq_sched.c - 多级反馈队列（MLFQ）调度器实现
 * 
 * 自适应进程行为，动态调整优先级
 */

#include <process/mlfq_sched.h>
#include <process/scheduler.h>
#include <kernel.h>
#include <drivers/timer.h>
#include <string.h>

/* MLFQ单个队列 */
struct mlfq_queue {
    struct process *head;
    struct process *tail;
    uint32_t count;
    uint32_t time_quantum;      /* 该队列的时间片长度 */
};

/* MLFQ调度器 */
static struct {
    struct mlfq_queue levels[MLFQ_LEVELS];
    uint64_t boost_time;        /* 上次全局提升时间 */
    uint64_t boost_interval;    /* 提升间隔 */
    uint32_t total_count;       /* 总进程数 */
    
    /* 统计信息 */
    uint64_t total_switches;    /* 总切换次数 */
    uint64_t level_changes[MLFQ_LEVELS]; /* 每个级别的变化次数 */
    uint64_t boost_count;       /* 全局提升次数 */
} mlfq;

/*
 * 初始化MLFQ调度器
 */
void mlfq_init(void)
{
    memset(&mlfq, 0, sizeof(mlfq));
    
    for (int i = 0; i < MLFQ_LEVELS; i++) {
        mlfq.levels[i].head = NULL;
        mlfq.levels[i].tail = NULL;
        mlfq.levels[i].count = 0;
        
        /* 时间片随级别指数增长 */
        mlfq.levels[i].time_quantum = MLFQ_BASE_QUANTUM * (1 << i);
        
        mlfq.level_changes[i] = 0;
    }
    
    mlfq.boost_time = timer_get_ticks();
    mlfq.boost_interval = MLFQ_BOOST_INTERVAL;
    mlfq.total_count = 0;
    mlfq.total_switches = 0;
    mlfq.boost_count = 0;
    
    kprintf("[MLFQ] Initialized with %d levels\n", MLFQ_LEVELS);
    for (int i = 0; i < MLFQ_LEVELS; i++) {
        kprintf("  Level %d: quantum = %u ticks\n", 
                i, mlfq.levels[i].time_quantum);
    }
}

/*
 * 进程入队
 */
void mlfq_enqueue(struct process *proc)
{
    if (!proc) {
        return;
    }
    
    int level = proc->mlfq_level;
    
    /* 验证并修正级别范围 */
    if (level < 0) {
        level = 0;
    }
    if (level >= MLFQ_LEVELS) {
        level = MLFQ_LEVELS - 1;
    }
    proc->mlfq_level = level;
    
    struct mlfq_queue *queue = &mlfq.levels[level];
    
    /* 分配时间片 */
    proc->time_slice = queue->time_quantum;
    proc->time_slice_remaining = queue->time_quantum;
    proc->time_used = 0;
    
    /* 加入队列尾部（FIFO） */
    proc->next = NULL;
    proc->prev = queue->tail;
    
    if (queue->tail) {
        queue->tail->next = proc;
    } else {
        queue->head = proc;
    }
    queue->tail = proc;
    
    queue->count++;
    mlfq.total_count++;
}

/*
 * 进程出队
 */
void mlfq_dequeue(struct process *proc)
{
    if (!proc) {
        return;
    }
    
    int level = proc->mlfq_level;
    if (level < 0 || level >= MLFQ_LEVELS) {
        return;
    }
    
    struct mlfq_queue *queue = &mlfq.levels[level];
    
    /* 从双向链表中移除 */
    if (proc->prev) {
        proc->prev->next = proc->next;
    } else {
        queue->head = proc->next;
    }
    
    if (proc->next) {
        proc->next->prev = proc->prev;
    } else {
        queue->tail = proc->prev;
    }
    
    proc->next = NULL;
    proc->prev = NULL;
    
    /* 更新计数 */
    if (queue->count > 0) {
        queue->count--;
    }
    if (mlfq.total_count > 0) {
        mlfq.total_count--;
    }
}

/*
 * 选择下一个要运行的进程
 */
struct process *mlfq_pick_next(void)
{
    /* Linux风格：从队列中peek而不是dequeue
     * 原因：调度器会在context_switch后修改状态，不应该在pick时移除
     */
    int total_checked = 0;
    for (int level = 0; level < MLFQ_LEVELS; level++) {
        struct mlfq_queue *queue = &mlfq.levels[level];
        
        kprintf("[MLFQ_DEBUG] Level %d: count=%d, head=%p\n", level, queue->count, queue->head);
        
        if (queue->count > 0 && queue->head) {
            /* 遍历队列，跳过TERMINATED状态的进程 */
            struct process *proc = queue->head;
            while (proc) {
                total_checked++;
                kprintf("[MLFQ_DEBUG] Checking PID %u (%s): state=%d, next=%p\n",
                        proc->pid, proc->name, proc->state, proc->next);
                
                if (proc->state == PROCESS_STATE_READY || 
                    proc->state == PROCESS_STATE_RUNNING) {
                    /* Linux风格：仅返回进程，不从队列移除
                     * scheduler会在实际运行后调用mlfq_dequeue
                     */
                    kprintf("[MLFQ_DEBUG] Picked PID %u\n", proc->pid);
                    return proc;
                }
                proc = proc->next;
            }
        }
    }
    
    /* 所有队列都空或只有TERMINATED进程 */
    kprintf("[MLFQ_DEBUG] No READY process (checked %d total)\n", total_checked);
    return NULL;
}

/*
 * 定时器tick处理（核心逻辑）
 */
void mlfq_tick(struct process *proc)
{
    if (!proc || proc->state != PROCESS_STATE_RUNNING) {
        return;
    }
    
    proc->time_used++;
    
    /* 减少剩余时间片 */
    if (proc->time_slice_remaining > 0) {
        proc->time_slice_remaining--;
    }
    
    /* 检查是否用完时间片 */
    if (proc->time_slice_remaining == 0) {
        /* 规则4A：用完时间片，降级 */
        int old_level = proc->mlfq_level;
        int new_level = old_level + 1;
        
        if (new_level >= MLFQ_LEVELS) {
            new_level = MLFQ_LEVELS - 1;
        }
        
        if (new_level != old_level) {
            proc->mlfq_level = new_level;
            mlfq.level_changes[old_level]++;
            
            // kprintf("[MLFQ] %s (PID %u): Level %d -> %d (used quantum)\n",
            //         proc->name, proc->pid, old_level, new_level);
        }
        
        /* 标记需要重新调度 */
        proc->time_slice_remaining = 0;
    }
}

/*
 * 进程主动让出CPU
 */
void mlfq_yield(struct process *proc)
{
    if (!proc) {
        return;
    }
    
    /* 规则4B：主动让出，保持优先级 */
    /* mlfq_level不变 */
    
    /* 重置时间片（下次运行时重新分配） */
    proc->time_slice_remaining = 0;
    proc->time_used = 0;
}

/*
 * 全局提升（防饿死）
 */
void mlfq_boost_all(void)
{
    uint64_t current_time = timer_get_ticks();
    
    /* 检查是否到了提升时间 */
    if (current_time - mlfq.boost_time < mlfq.boost_interval) {
        return;
    }
    
    kprintf("[MLFQ] Global boost: moving all processes to level 0\n");
    
    mlfq.boost_time = current_time;
    mlfq.boost_count++;
    
    /* 从低级别到高级别遍历（避免重复处理） */
    for (int level = MLFQ_LEVELS - 1; level > 0; level--) {
        struct mlfq_queue *queue = &mlfq.levels[level];
        
        while (queue->count > 0 && queue->head) {
            struct process *proc = queue->head;
            
            /* 从当前队列移除 */
            mlfq_dequeue(proc);
            
            /* 提升到最高级别 */
            proc->mlfq_level = 0;
            
            /* 加入最高级别队列 */
            mlfq_enqueue(proc);
        }
    }
}

/*
 * 打印MLFQ统计信息
 */
void mlfq_print_stats(void)
{
    kprintf("\n=== MLFQ Scheduler Statistics ===\n");
    kprintf("Total Processes:    %u\n", mlfq.total_count);
    kprintf("Total Switches:     %llu\n", mlfq.total_switches);
    kprintf("Global Boosts:      %llu\n", mlfq.boost_count);
    kprintf("\nLevel Distribution:\n");
    
    for (int i = 0; i < MLFQ_LEVELS; i++) {
        kprintf("  Level %d [Q=%3u]: %u processes, %llu changes\n",
                i, mlfq.levels[i].time_quantum,
                mlfq.levels[i].count,
                mlfq.level_changes[i]);
    }
    kprintf("\n");
}

/*
 * 打印所有MLFQ队列
 */
void mlfq_print_queues(void)
{
    kprintf("\n=== MLFQ Queues ===\n");
    
    for (int level = 0; level < MLFQ_LEVELS; level++) {
        struct mlfq_queue *queue = &mlfq.levels[level];
        
        if (queue->count == 0) {
            continue;
        }
        
        kprintf("Level %d [Q=%u, Count=%u]: ",
                level, queue->time_quantum, queue->count);
        
        struct process *proc = queue->head;
        int shown = 0;
        
        while (proc && shown < 5) {
            kprintf("%s(PID %u) ", proc->name, proc->pid);
            proc = proc->next;
            shown++;
        }
        
        if (queue->count > 5) {
            kprintf("... (+%u more)", queue->count - 5);
        }
        
        kprintf("\n");
    }
    kprintf("\n");
}

/*
 * 获取指定级别的进程数
 */
int mlfq_get_level_count(int level)
{
    if (level < 0 || level >= MLFQ_LEVELS) {
        return 0;
    }
    return mlfq.levels[level].count;
}

/*
 * 获取指定级别的时间片长度
 */
uint32_t mlfq_get_level_quantum(int level)
{
    if (level < 0 || level >= MLFQ_LEVELS) {
        return 0;
    }
    return mlfq.levels[level].time_quantum;
}

