/**
 * cfs_sched.c - Linux风格的CFS调度器实现
 * 
 * Completely Fair Scheduler - 完全公平调度器
 * 核心思想：给每个进程公平的CPU时间，使用vruntime追踪
 */

#include <process/cfs_sched.h>
#include <process/process.h>
#include <rbtree.h>
#include <kernel.h>
#include <string.h>

/**
 * Nice值到权重的映射表（Linux内核标准）
 * nice=0的权重是1024
 * 每降低一个nice值，权重增加约1.25倍
 */
const int sched_prio_to_weight[40] = {
    /* -20 */ 88761, 71755, 56483, 46273, 36291,
    /* -15 */ 29154, 23254, 18705, 14949, 11916,
    /* -10 */  9548,  7620,  6100,  4904,  3906,
    /*  -5 */  3121,  2501,  1991,  1586,  1277,
    /*   0 */  1024,   820,   655,   526,   423,
    /*   5 */   335,   272,   215,   172,   137,
    /*  10 */   110,    87,    70,    56,    45,
    /*  15 */    36,    29,    23,    18,    15,
};

/**
 * 权重倒数的预计算值（用于优化除法）
 */
const uint32_t sched_prio_to_wmult[40] = {
    /* -20 */     48388,     59856,     76040,     92818,    118348,
    /* -15 */    147320,    184698,    229616,    287308,    360437,
    /* -10 */    449829,    563644,    704093,    875809,   1099582,
    /*  -5 */   1376151,   1717300,   2157191,   2708050,   3363326,
    /*   0 */   4194304,   5237765,   6557202,   8165337,  10153587,
    /*   5 */  12820798,  15790321,  19976592,  24970740,  31350126,
    /*  10 */  39045157,  49367440,  61356676,  76695844,  95443717,
    /*  15 */ 119304647, 148102320, 186737708, 238609294, 286331153,
};

/**
 * 全局CFS运行队列
 */
static struct cfs_rq g_cfs_rq;

/**
 * CFS调度器初始化
 */
void cfs_init(void)
{
    kprintf("[CFS] Initializing Completely Fair Scheduler...\n");
    
    memset(&g_cfs_rq, 0, sizeof(g_cfs_rq));
    
    g_cfs_rq.tasks_timeline = RB_ROOT;
    g_cfs_rq.rb_leftmost = NULL;
    g_cfs_rq.nr_running = 0;
    g_cfs_rq.min_vruntime = 0;
    g_cfs_rq.exec_clock = 0;
    g_cfs_rq.total_weight = 0;
    
    kprintf("[CFS] CFS scheduler initialized\n");
    kprintf("[CFS]   Target latency: %d ms\n", CFS_TARGET_LATENCY_NS / 1000000);
    kprintf("[CFS]   Min granularity: %d us\n", CFS_MIN_GRANULARITY_NS / 1000);
}

/**
 * 更新min_vruntime
 * min_vruntime是单调递增的，确保新进程不会获得不公平的优势
 */
static void update_min_vruntime(void)
{
    struct process *curr = NULL;
    uint64_t vruntime = g_cfs_rq.min_vruntime;
    
    /* 如果有最左节点，使用它的vruntime */
    if (g_cfs_rq.rb_leftmost) {
        struct rb_node *leftmost = g_cfs_rq.rb_leftmost;
        curr = rb_entry(leftmost, struct process, se.rb_node);
        
        if (curr) {
            vruntime = curr->se.vruntime;
        }
    }
    
    /* min_vruntime只增不减 */
    if (vruntime > g_cfs_rq.min_vruntime) {
        g_cfs_rq.min_vruntime = vruntime;
    }
}

/**
 * 将进程插入红黑树
 */
static void enqueue_entity(struct process *proc)
{
    struct rb_node **link = &g_cfs_rq.tasks_timeline.rb_node;
    struct rb_node *parent = NULL;
    struct process *entry;
    int leftmost = 1;
    
    /* 查找插入位置 */
    while (*link) {
        parent = *link;
        entry = rb_entry(parent, struct process, se.rb_node);
        
        /* 按vruntime排序 */
        if (proc->se.vruntime < entry->se.vruntime) {
            link = &parent->rb_left;
        } else {
            link = &parent->rb_right;
            leftmost = 0;
        }
    }
    
    /* 如果是最左节点，更新leftmost */
    if (leftmost) {
        g_cfs_rq.rb_leftmost = &proc->se.rb_node;
    }
    
    /* 插入红黑树 */
    rb_link_node(&proc->se.rb_node, parent, link);
    rb_insert_color(&proc->se.rb_node, &g_cfs_rq.tasks_timeline);
}

/**
 * 从红黑树中移除进程
 */
static void dequeue_entity(struct process *proc)
{
    /* 如果是最左节点，更新leftmost */
    if (g_cfs_rq.rb_leftmost == &proc->se.rb_node) {
        struct rb_node *next = rb_next(&proc->se.rb_node);
        g_cfs_rq.rb_leftmost = next;
    }
    
    /* 从红黑树中删除 */
    rb_erase(&proc->se.rb_node, &g_cfs_rq.tasks_timeline);
    /* 清除节点状态 */
    rb_clear_node(&proc->se.rb_node);
}

/**
 * 将进程加入CFS运行队列
 */
void cfs_enqueue_task(struct process *proc)
{
    if (!proc) {
        return;
    }
    
    /* 初始化调度实体（如果是新进程）*/
    if (proc->se.vruntime == 0) {
        /* 新进程的vruntime设置为当前min_vruntime */
        proc->se.vruntime = g_cfs_rq.min_vruntime;
    }
    
    /* 初始化权重 */
    if (proc->se.weight == 0) {
        proc->se.weight = nice_to_weight(proc->nice);
    }
    
    /* 插入红黑树 */
    enqueue_entity(proc);
    
    /* 更新统计 */
    g_cfs_rq.nr_running++;
    g_cfs_rq.total_weight += proc->se.weight;
    
    kprintf("[CFS] Enqueued '%s' (PID %u, vruntime=%llu, weight=%d, nice=%d)\n",
            proc->name, proc->pid, proc->se.vruntime, proc->se.weight, proc->nice);
}

/**
 * 从CFS运行队列中移除进程
 */
void cfs_dequeue_task(struct process *proc)
{
    if (!proc) {
        return;
    }
    
    /* 从红黑树中移除 */
    dequeue_entity(proc);
    
    /* 更新统计 */
    if (g_cfs_rq.nr_running > 0) {
        g_cfs_rq.nr_running--;
    }
    
    if (g_cfs_rq.total_weight >= proc->se.weight) {
        g_cfs_rq.total_weight -= proc->se.weight;
    }
    
    /* 更新min_vruntime */
    update_min_vruntime();
    
    kprintf("[CFS] Dequeued '%s' (PID %u, vruntime=%llu)\n",
            proc->name, proc->pid, proc->se.vruntime);
}

/**
 * 选择下一个要运行的进程
 * 返回vruntime最小的进程（红黑树最左节点）
 */
struct process *cfs_pick_next_task(void)
{
    struct rb_node *left = g_cfs_rq.rb_leftmost;
    
    if (!left) {
        return NULL;  /* 没有可运行的进程 */
    }
    
    struct process *next = rb_entry(left, struct process, se.rb_node);
    return next;
}

/**
 * 计算delta的公平化版本
 * delta_fair = delta_exec * NICE_0_LOAD / weight
 */
uint64_t cfs_calc_delta_fair(uint64_t delta, struct sched_entity *se)
{
    if (se->weight == 1024) {
        /* nice=0，直接返回 */
        return delta;
    }
    
    /* vruntime增量 = 实际时间 * 1024 / 权重 */
    return delta * 1024 / se->weight;
}

/**
 * 更新当前进程的虚拟运行时间
 */
void cfs_update_curr(struct process *curr)
{
    if (!curr) {
        return;
    }
    
    /* 简化实现：每次tick增加1ms的vruntime */
    uint64_t delta_exec = 1000000;  /* 1ms in nanoseconds */
    
    /* 更新累计执行时间 */
    curr->se.sum_exec_runtime += delta_exec;
    g_cfs_rq.exec_clock += delta_exec;
    
    /* 计算vruntime增量（考虑权重）*/
    uint64_t delta_fair = cfs_calc_delta_fair(delta_exec, &curr->se);
    curr->se.vruntime += delta_fair;
    
    /* 更新min_vruntime */
    update_min_vruntime();
}

/**
 * 检查当前进程是否应该被抢占
 * 比较当前进程和唤醒进程的vruntime差距
 */
int cfs_check_preempt_curr(struct process *curr, struct process *wakeup)
{
    if (!curr || !wakeup) {
        return 0;
    }
    
    /* 如果唤醒进程的vruntime明显小于当前进程，应该抢占 */
    uint64_t gran = CFS_WAKEUP_GRANULARITY_NS;
    
    if (wakeup->se.vruntime + gran < curr->se.vruntime) {
        return 1;  /* 应该抢占 */
    }
    
    return 0;
}

/**
 * 进程唤醒时调用
 */
void cfs_task_waking(struct process *proc)
{
    if (!proc) {
        return;
    }
    
    /* 如果进程睡眠时间过长，调整其vruntime */
    /* 避免进程因为长时间睡眠而获得不公平的优势 */
    if (proc->se.vruntime < g_cfs_rq.min_vruntime) {
        proc->se.vruntime = g_cfs_rq.min_vruntime;
    }
}

/**
 * 设置进程的nice值
 */
void cfs_set_task_nice(struct process *proc, int nice)
{
    if (!proc) {
        return;
    }
    
    /* 限制nice值范围 */
    if (nice < -20) nice = -20;
    if (nice > 19) nice = 19;
    
    proc->nice = nice;
    proc->static_priority = nice_to_prio(nice);
    
    /* 重新计算权重 */
    cfs_reweight_task(proc, proc->static_priority);
    
    kprintf("[CFS] Set '%s' nice=%d (weight=%d)\n",
            proc->name, nice, proc->se.weight);
}

/**
 * 获取进程的nice值
 */
int cfs_get_task_nice(struct process *proc)
{
    return proc ? proc->nice : 0;
}

/**
 * 重新计算进程权重
 */
void cfs_reweight_task(struct process *proc, int prio)
{
    if (!proc) {
        return;
    }
    
    int old_weight = proc->se.weight;
    int new_weight = nice_to_weight(prio_to_nice(prio));
    
    proc->se.weight = new_weight;
    
    /* 如果进程在运行队列中，更新总权重 */
    /* 检查节点是否在树中（未被清除）*/
    if (!rb_empty_node(&proc->se.rb_node)) {
        g_cfs_rq.total_weight = g_cfs_rq.total_weight - old_weight + new_weight;
    }
}

/**
 * 打印CFS统计信息
 */
void cfs_print_stats(void)
{
    kprintf("\n=== CFS Scheduler Statistics ===\n");
    kprintf("Running tasks:    %u\n", g_cfs_rq.nr_running);
    kprintf("Min vruntime:     %llu ns\n", g_cfs_rq.min_vruntime);
    kprintf("Exec clock:       %llu ns\n", g_cfs_rq.exec_clock);
    kprintf("Total weight:     %llu\n", g_cfs_rq.total_weight);
    
    if (g_cfs_rq.rb_leftmost) {
        struct process *leftmost = rb_entry(g_cfs_rq.rb_leftmost, 
                                           struct process, se.rb_node);
        kprintf("Next task:        %s (PID %u, vruntime=%llu)\n",
                leftmost->name, leftmost->pid, leftmost->se.vruntime);
    }
    
    kprintf("================================\n\n");
}

