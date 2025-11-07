/**
 * cfs_sched.h - Linux风格的CFS (Completely Fair Scheduler)
 * 
 * 基于红黑树和虚拟运行时间(vruntime)的公平调度器
 */

#ifndef _PROCESS_CFS_SCHED_H
#define _PROCESS_CFS_SCHED_H

#include <types.h>
#include <rbtree.h>
#include <process/process.h>

/**
 * CFS配置参数
 */
#define CFS_TARGET_LATENCY_NS   (6 * 1000000)    /* 目标延迟：6ms */
#define CFS_MIN_GRANULARITY_NS  (750000)         /* 最小粒度：0.75ms */
#define CFS_WAKEUP_GRANULARITY_NS (1000000)      /* 唤醒粒度：1ms */

/**
 * Nice值到权重的转换表
 * Linux内核使用1024作为nice=0的基准权重
 */
extern const int sched_prio_to_weight[40];
extern const uint32_t sched_prio_to_wmult[40];

/**
 * CFS运行队列
 * 使用红黑树管理可运行进程，按vruntime排序
 */
struct cfs_rq {
    struct rb_root tasks_timeline;      /* 红黑树根节点 */
    struct rb_node *rb_leftmost;        /* 最左节点（vruntime最小） */
    
    uint32_t nr_running;                /* 运行队列中的进程数 */
    uint64_t min_vruntime;              /* 队列中最小的vruntime */
    
    /* 统计信息 */
    uint64_t exec_clock;                /* 累计执行时间 */
    uint64_t total_weight;              /* 总权重 */
};

/**
 * CFS调度器初始化
 */
void cfs_init(void);

/**
 * 将进程加入CFS运行队列
 */
void cfs_enqueue_task(struct process *proc);

/**
 * 从CFS运行队列中移除进程
 */
void cfs_dequeue_task(struct process *proc);

/**
 * 选择下一个要运行的进程
 * 返回vruntime最小的进程
 */
struct process *cfs_pick_next_task(void);

/**
 * 更新进程的虚拟运行时间
 * 在定时器tick时调用
 */
void cfs_update_curr(struct process *curr);

/**
 * 检查当前进程是否应该被抢占
 */
int cfs_check_preempt_curr(struct process *curr, struct process *wakeup);

/**
 * 进程唤醒时调用
 */
void cfs_task_waking(struct process *proc);

/**
 * 计算进程的时间片
 * 基于权重和系统负载动态计算
 */
uint64_t cfs_calc_delta_fair(uint64_t delta, struct sched_entity *se);

/**
 * Nice值相关操作
 */
void cfs_set_task_nice(struct process *proc, int nice);
int cfs_get_task_nice(struct process *proc);
void cfs_reweight_task(struct process *proc, int prio);

/**
 * 打印CFS统计信息
 */
void cfs_print_stats(void);

/**
 * 工具函数：计算vruntime
 */
static inline uint64_t cfs_calc_vruntime(uint64_t delta_exec, int weight)
{
    /* vruntime = real_time * NICE_0_LOAD / weight */
    /* 权重越大，vruntime增长越慢，获得更多CPU时间 */
    return delta_exec * 1024 / weight;
}

/**
 * 工具函数：nice值到权重的转换
 */
static inline int nice_to_weight(int nice)
{
    /* nice: -20到+19，映射到数组索引0-39 */
    int idx = nice + 20;
    if (idx < 0) idx = 0;
    if (idx > 39) idx = 39;
    return sched_prio_to_weight[idx];
}

/**
 * 工具函数：优先级到nice值
 */
static inline int prio_to_nice(int prio)
{
    /* 静态优先级100-139 -> nice -20到+19 */
    return prio - 120;
}

/**
 * 工具函数：nice值到优先级
 */
static inline int nice_to_prio(int nice)
{
    /* nice -20到+19 -> 静态优先级100-139 */
    return nice + 120;
}

#endif /* _PROCESS_CFS_SCHED_H */

