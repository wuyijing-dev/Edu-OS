/**
 * rt_sched.c - 实时调度器实现
 * 
 * Linux风格的实时调度，支持SCHED_FIFO和SCHED_RR
 */

#include <process/rt_sched.h>
#include <process/process.h>
#include <kernel.h>
#include <string.h>

/**
 * 全局实时运行队列
 */
static struct rt_rq g_rt_rq;

/**
 * 设置位图中的某一位
 */
static inline void set_bit(int nr, unsigned long *addr)
{
    addr[nr / 32] |= (1UL << (nr % 32));
}

/**
 * 清除位图中的某一位
 */
static inline void clear_bit(int nr, unsigned long *addr)
{
    addr[nr / 32] &= ~(1UL << (nr % 32));
}

/**
 * 测试位图中的某一位
 */
static inline int test_bit(int nr, const unsigned long *addr)
{
    return (addr[nr / 32] & (1UL << (nr % 32))) != 0;
}

/**
 * 查找第一个设置的位（ffs - find first set）
 */
static inline int find_first_bit(const unsigned long *addr, int size)
{
    int i, j;
    for (i = 0; i < (size + 31) / 32; i++) {
        if (addr[i]) {
            for (j = 0; j < 32; j++) {
                if (addr[i] & (1UL << j)) {
                    return i * 32 + j;
                }
            }
        }
    }
    return size;
}

/**
 * 实时调度器初始化
 */
void rt_sched_init(void)
{
    kprintf("[RT] Initializing Real-Time Scheduler...\n");
    
    memset(&g_rt_rq, 0, sizeof(g_rt_rq));
    
    /* 初始化所有优先级队列 */
    for (int i = 0; i < MAX_RT_PRIO; i++) {
        INIT_LIST_HEAD(&g_rt_rq.active.queue[i]);
    }
    
    /* 清空位图 */
    for (int i = 0; i < 4; i++) {
        g_rt_rq.active.bitmap[i] = 0;
    }
    
    g_rt_rq.active.nr_running = 0;
    g_rt_rq.rt_nr_running = 0;
    g_rt_rq.highest_prio = MAX_RT_PRIO;  /* 无实时进程 */
    g_rt_rq.rt_time = 0;
    g_rt_rq.rt_throttled = 0;
    
    kprintf("[RT] Real-Time scheduler initialized\n");
    kprintf("[RT]   Priority range: %d-%d (higher is better)\n", 
            RT_PRIO_MIN, RT_PRIO_MAX);
    kprintf("[RT]   RR timeslice: %d ms\n", RR_TIMESLICE);
}

/**
 * 查找最高优先级
 */
int rt_find_highest_prio(struct rt_prio_array *array)
{
    int idx = find_first_bit(array->bitmap, MAX_RT_PRIO);
    return idx < MAX_RT_PRIO ? idx : MAX_RT_PRIO;
}

/**
 * 将进程加入实时运行队列
 */
void rt_enqueue_task(struct process *proc)
{
    if (!proc) {
        return;
    }
    
    int prio = proc->rt_priority;
    
    if (!rt_prio_valid(prio)) {
        kprintf("[RT] ERROR: Invalid RT priority %d for process '%s'\n",
                prio, proc->name);
        return;
    }
    
    /* 注意：这里应该使用专用的list_head字段，暂时使用强制转换 */
    /* TODO: 在process结构中添加rt_list_head字段 */
    struct list_head *node = (struct list_head *)&proc->next;
    list_add_tail(node, &g_rt_rq.active.queue[prio]);
    
    /* 设置位图 */
    set_bit(prio, g_rt_rq.active.bitmap);
    
    /* 更新统计 */
    g_rt_rq.active.nr_running++;
    g_rt_rq.rt_nr_running++;
    
    /* 更新最高优先级 */
    if (prio < g_rt_rq.highest_prio) {
        g_rt_rq.highest_prio = prio;
    }
    
    kprintf("[RT] Enqueued '%s' (PID %u, prio=%d, policy=%s)\n",
            proc->name, proc->pid, prio,
            proc->policy == SCHED_FIFO ? "FIFO" : "RR");
}

/**
 * 从实时运行队列中移除进程
 */
void rt_dequeue_task(struct process *proc)
{
    if (!proc) {
        return;
    }
    
    int prio = proc->rt_priority;
    
    if (!rt_prio_valid(prio)) {
        return;
    }
    
    /* 从队列中移除 */
    struct list_head *node = (struct list_head *)&proc->next;
    list_del(node);
    
    /* 如果该优先级队列为空，清除位图 */
    if (list_empty(&g_rt_rq.active.queue[prio])) {
        clear_bit(prio, g_rt_rq.active.bitmap);
    }
    
    /* 更新统计 */
    if (g_rt_rq.active.nr_running > 0) {
        g_rt_rq.active.nr_running--;
    }
    if (g_rt_rq.rt_nr_running > 0) {
        g_rt_rq.rt_nr_running--;
    }
    
    /* 重新计算最高优先级 */
    g_rt_rq.highest_prio = rt_find_highest_prio(&g_rt_rq.active);
    
    kprintf("[RT] Dequeued '%s' (PID %u, prio=%d)\n",
            proc->name, proc->pid, prio);
}

/**
 * 选择下一个实时进程
 */
struct process *rt_pick_next_task(void)
{
    int prio;
    struct process *next;
    
    /* 如果没有实时进程，返回NULL */
    if (g_rt_rq.rt_nr_running == 0) {
        return NULL;
    }
    
    /* 找到最高优先级 */
    prio = g_rt_rq.highest_prio;
    
    if (prio >= MAX_RT_PRIO) {
        return NULL;
    }
    
    /* 从该优先级队列头部取出进程 */
    if (list_empty(&g_rt_rq.active.queue[prio])) {
        return NULL;
    }
    
    /* 获取第一个进程 */
    struct list_head *first_node = g_rt_rq.active.queue[prio].next;
    /* 由于我们使用的是process->next作为链表节点，这里需要特殊处理 */
    next = (struct process *)((char *)first_node - offsetof(struct process, next));
    
    return next;
}

/**
 * 实时调度器tick处理
 */
void rt_scheduler_tick(struct process *curr)
{
    if (!curr) {
        return;
    }
    
    /* 只有SCHED_RR需要处理时间片 */
    if (curr->policy != SCHED_RR) {
        return;
    }
    
    /* 递减时间片 */
    if (curr->time_slice_remaining > 0) {
        curr->time_slice_remaining--;
    }
    
    /* 时间片用完，将进程移到队列尾部 */
    if (curr->time_slice_remaining == 0) {
        int prio = curr->rt_priority;
        
        /* 从队列头移除 */
        struct list_head *node = (struct list_head *)&curr->next;
        list_del(node);
        
        /* 重新加入队列尾 */
        list_add_tail(node, &g_rt_rq.active.queue[prio]);
        
        /* 重置时间片 */
        curr->time_slice_remaining = RR_TIMESLICE;
        
        kprintf("[RT] RR timeslice expired for '%s', moved to tail\n",
                curr->name);
    }
}

/**
 * 检查当前进程是否应该被抢占
 */
int rt_check_preempt_curr(struct process *curr, struct process *wakeup)
{
    if (!curr || !wakeup) {
        return 0;
    }
    
    /* 如果唤醒进程的优先级更高，应该抢占 */
    if (wakeup->rt_priority < curr->rt_priority) {
        return 1;
    }
    
    return 0;
}

/**
 * 设置进程的实时优先级
 */
int rt_set_priority(struct process *proc, int rt_prio)
{
    if (!proc) {
        return -1;
    }
    
    if (!rt_prio_valid(rt_prio)) {
        return -1;
    }
    
    proc->rt_priority = rt_prio;
    
    kprintf("[RT] Set '%s' (PID %u) RT priority to %d\n",
            proc->name, proc->pid, rt_prio);
    
    return 0;
}

/**
 * 获取进程的实时优先级
 */
int rt_get_priority(struct process *proc)
{
    return proc ? proc->rt_priority : -1;
}

/**
 * 打印实时调度器统计信息
 */
void rt_print_stats(void)
{
    kprintf("\n=== Real-Time Scheduler Statistics ===\n");
    kprintf("RT processes:     %u\n", g_rt_rq.rt_nr_running);
    kprintf("Highest priority: %d\n", g_rt_rq.highest_prio);
    kprintf("RT execution time: %llu ns\n", g_rt_rq.rt_time);
    kprintf("RT throttled:     %llu times\n", g_rt_rq.rt_throttled);
    
    kprintf("\nActive queues:\n");
    for (int i = 0; i < MAX_RT_PRIO; i++) {
        if (test_bit(i, g_rt_rq.active.bitmap)) {
            int count = 0;
            /* 遍历队列计数 */
            struct list_head *pos;
            list_for_each(pos, &g_rt_rq.active.queue[i]) {
                count++;
            }
            kprintf("  Priority %d: %d process(es)\n", i, count);
        }
    }
    
    kprintf("=====================================\n\n");
}

