/*
 * priority_sched.c - 优先级调度器实现
 * 
 * 实现32级优先级调度，O(1)时间复杂度
 */

#include <process/priority_sched.h>
#include <process/scheduler.h>
#include <kernel.h>
#include <drivers/timer.h>
#include <string.h>

/* 单个优先级队列 */
struct priority_queue {
    struct process *head;
    struct process *tail;
    uint32_t count;
};

/* 优先级调度器 */
static struct {
    struct priority_queue queues[MAX_PRIORITIES];
    uint32_t bitmap;             /* 位图：标记哪些队列非空 */
    uint32_t total_count;        /* 总进程数 */
    uint64_t last_aging_time;    /* 上次老化时间 */
    uint64_t total_enqueues;     /* 统计：入队次数 */
    uint64_t total_dequeues;     /* 统计：出队次数 */
    uint64_t aging_count;        /* 统计：老化提升次数 */
} priority_sched;

/* 配置参数 */
#define AGING_INTERVAL 100       /* 每100 ticks检查一次老化 */
#define AGING_THRESHOLD 200      /* 等待超过200 ticks就提升 */
#define AGING_BOOST 2            /* 每次提升2个级别 */

/*
 * 初始化优先级调度器
 */
void priority_scheduler_init(void)
{
    memset(&priority_sched, 0, sizeof(priority_sched));
    
    for (int i = 0; i < MAX_PRIORITIES; i++) {
        priority_sched.queues[i].head = NULL;
        priority_sched.queues[i].tail = NULL;
        priority_sched.queues[i].count = 0;
    }
    
    priority_sched.bitmap = 0;
    priority_sched.total_count = 0;
    priority_sched.last_aging_time = timer_get_ticks();
    
    kprintf("[PRIORITY] Priority Scheduler initialized (%d levels)\n", MAX_PRIORITIES);
}

/*
 * 进程入队（加入优先级队列）
 */
void priority_enqueue(struct process *proc)
{
    if (!proc) {
        return;
    }
    
    int pri = proc->priority;
    
    /* 验证并修正优先级范围 */
    if (pri < 0) {
        pri = 0;
    }
    if (pri >= MAX_PRIORITIES) {
        pri = MAX_PRIORITIES - 1;
    }
    proc->priority = pri;
    
    struct priority_queue *queue = &priority_sched.queues[pri];
    
    /* 加入队列尾部（FIFO） */
    proc->next = NULL;
    proc->prev = queue->tail;
    
    if (queue->tail) {
        queue->tail->next = proc;
    } else {
        /* 队列为空，设置头指针 */
        queue->head = proc;
    }
    queue->tail = proc;
    
    /* 更新计数 */
    queue->count++;
    priority_sched.total_count++;
    priority_sched.total_enqueues++;
    
    /* 设置位图对应位 */
    priority_sched.bitmap |= (1U << pri);
    
    /* 记录入队时间（用于老化） */
    proc->wait_time = timer_get_ticks();
}

/*
 * 进程出队（从优先级队列移除）
 */
void priority_dequeue(struct process *proc)
{
    if (!proc) {
        return;
    }
    
    int pri = proc->priority;
    if (pri < 0 || pri >= MAX_PRIORITIES) {
        return;
    }
    
    struct priority_queue *queue = &priority_sched.queues[pri];
    
    /* 从双向链表中移除 */
    if (proc->prev) {
        proc->prev->next = proc->next;
    } else {
        /* 这是头节点 */
        queue->head = proc->next;
    }
    
    if (proc->next) {
        proc->next->prev = proc->prev;
    } else {
        /* 这是尾节点 */
        queue->tail = proc->prev;
    }
    
    proc->next = NULL;
    proc->prev = NULL;
    
    /* 更新计数 */
    if (queue->count > 0) {
        queue->count--;
    }
    if (priority_sched.total_count > 0) {
        priority_sched.total_count--;
    }
    priority_sched.total_dequeues++;
    
    /* 如果队列空了，清除位图对应位 */
    if (queue->count == 0) {
        priority_sched.bitmap &= ~(1U << pri);
    }
}

/*
 * 选择下一个要运行的进程（O(1)算法）
 */
struct process *priority_pick_next(void)
{
    /* 没有就绪进程 */
    if (priority_sched.bitmap == 0) {
        return NULL;
    }
    
    /* 使用__builtin_ffs找到第一个为1的位（最高优先级） */
    int highest_pri = __builtin_ffs(priority_sched.bitmap) - 1;
    
    if (highest_pri < 0 || highest_pri >= MAX_PRIORITIES) {
        kprintf("[PRIORITY] ERROR: Invalid priority %d from bitmap 0x%08x\n",
                highest_pri, priority_sched.bitmap);
        return NULL;
    }
    
    /* 从该优先级队列取出第一个进程 */
    struct priority_queue *queue = &priority_sched.queues[highest_pri];
    struct process *next = queue->head;
    
    if (!next) {
        /* 不应该发生：位图说有，但队列为空（数据不一致） */
        kprintf("[PRIORITY] WARNING: Bitmap inconsistency at level %d\n", 
                highest_pri);
        
        /* 修复位图 */
        priority_sched.bitmap &= ~(1U << highest_pri);
        
        /* 递归重试 */
        return priority_pick_next();
    }
    
    /* 从队列中移除 */
    priority_dequeue(next);
    
    return next;
}

/*
 * 优先级老化（防止低优先级进程饿死）
 */
void priority_aging(void)
{
    uint64_t current_time = timer_get_ticks();
    
    /* 检查是否到了老化检查时间 */
    if (current_time - priority_sched.last_aging_time < AGING_INTERVAL) {
        return;
    }
    
    priority_sched.last_aging_time = current_time;
    
    /* 遍历所有优先级队列（从低到高优先级） */
    for (int pri = MAX_PRIORITIES - 1; pri > 0; pri--) {
        struct priority_queue *queue = &priority_sched.queues[pri];
        
        if (queue->count == 0) {
            continue;
        }
        
        struct process *proc = queue->head;
        
        while (proc) {
            /* 保存next指针，因为进程可能会被移动 */
            struct process *next = proc->next;
            
            /* 检查等待时间 */
            uint64_t wait_time = current_time - proc->wait_time;
            
            if (wait_time > AGING_THRESHOLD) {
                /* 计算新优先级（提升） */
                int new_pri = pri - AGING_BOOST;
                if (new_pri < 0) {
                    new_pri = 0;
                }
                
                if (new_pri != pri) {
                    /* 从当前队列移除 */
                    priority_dequeue(proc);
                    
                    /* 修改优先级 */
                    int old_pri = proc->priority;
                    proc->priority = new_pri;
                    proc->boost_count++;
                    
                    /* 加入新队列 */
                    priority_enqueue(proc);
                    
                    priority_sched.aging_count++;
                    
                    /* 可选：打印调试信息 */
                    // kprintf("[AGING] %s (PID %u): %d -> %d (waited %llu ticks)\n",
                    //         proc->name, proc->pid, old_pri, new_pri, wait_time);
                }
            }
            
            proc = next;
        }
    }
}

/*
 * 打印优先级调度器统计信息
 */
void priority_print_stats(void)
{
    kprintf("\n=== Priority Scheduler Statistics ===\n");
    kprintf("Total Processes:    %u\n", priority_sched.total_count);
    kprintf("Active Priorities:  ");
    
    int active_count = 0;
    for (int i = 0; i < MAX_PRIORITIES; i++) {
        if (priority_sched.queues[i].count > 0) {
            kprintf("%d ", i);
            active_count++;
        }
    }
    if (active_count == 0) {
        kprintf("(none)");
    }
    kprintf("\n");
    
    kprintf("Bitmap:             0x%08x\n", priority_sched.bitmap);
    kprintf("Total Enqueues:     %llu\n", priority_sched.total_enqueues);
    kprintf("Total Dequeues:     %llu\n", priority_sched.total_dequeues);
    kprintf("Aging Boosts:       %llu\n", priority_sched.aging_count);
    kprintf("\n");
}

/*
 * 打印所有优先级队列
 */
void priority_print_queues(void)
{
    kprintf("\n=== Priority Queues ===\n");
    
    for (int pri = 0; pri < MAX_PRIORITIES; pri++) {
        struct priority_queue *queue = &priority_sched.queues[pri];
        
        if (queue->count == 0) {
            continue;
        }
        
        kprintf("Priority %2d [%u]: ", pri, queue->count);
        
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
 * 获取指定优先级队列的进程数
 */
int priority_get_queue_count(int priority)
{
    if (priority < 0 || priority >= MAX_PRIORITIES) {
        return 0;
    }
    return priority_sched.queues[priority].count;
}

/*
 * 检查指定优先级队列是否为空
 */
bool priority_is_queue_empty(int priority)
{
    if (priority < 0 || priority >= MAX_PRIORITIES) {
        return true;
    }
    return (priority_sched.queues[priority].count == 0);
}

