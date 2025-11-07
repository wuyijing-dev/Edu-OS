/**
 * rt_sched.h - 实时调度器 (Real-Time Scheduler)
 * 
 * 支持SCHED_FIFO和SCHED_RR两种实时调度策略
 */

#ifndef _PROCESS_RT_SCHED_H
#define _PROCESS_RT_SCHED_H

#include <types.h>
#include <list.h>
#include <process/process.h>

/**
 * 实时优先级范围
 * Linux标准：0-99，数字越大优先级越高
 */
#define RT_PRIO_MIN     0
#define RT_PRIO_MAX     99
#define MAX_RT_PRIO     100

/**
 * 实时进程队列数组（每个优先级一个队列）
 */
struct rt_prio_array {
    struct list_head queue[MAX_RT_PRIO];  /* 100个优先级队列 */
    unsigned long bitmap[4];              /* 优先级位图（100/32=4）*/
    uint32_t nr_running;                  /* 运行中的实时进程数 */
};

/**
 * 实时运行队列
 */
struct rt_rq {
    struct rt_prio_array active;          /* 活动队列 */
    uint32_t rt_nr_running;               /* 实时进程总数 */
    int highest_prio;                     /* 最高优先级 */
    
    /* 统计信息 */
    uint64_t rt_time;                     /* 累计实时执行时间 */
    uint64_t rt_throttled;                /* 被限流的次数 */
};

/**
 * 实时调度器初始化
 */
void rt_sched_init(void);

/**
 * 将进程加入实时运行队列
 */
void rt_enqueue_task(struct process *proc);

/**
 * 从实时运行队列中移除进程
 */
void rt_dequeue_task(struct process *proc);

/**
 * 选择下一个实时进程
 * 返回最高优先级的进程
 */
struct process *rt_pick_next_task(void);

/**
 * 实时调度器tick处理
 * 处理SCHED_RR的时间片轮转
 */
void rt_scheduler_tick(struct process *curr);

/**
 * 检查当前进程是否应该被抢占
 */
int rt_check_preempt_curr(struct process *curr, struct process *wakeup);

/**
 * 设置进程的实时优先级
 */
int rt_set_priority(struct process *proc, int rt_prio);

/**
 * 获取进程的实时优先级
 */
int rt_get_priority(struct process *proc);

/**
 * 检查优先级是否有效
 */
static inline int rt_prio_valid(int prio)
{
    return prio >= RT_PRIO_MIN && prio <= RT_PRIO_MAX;
}

/**
 * 获取最高优先级
 */
int rt_find_highest_prio(struct rt_prio_array *array);

/**
 * 打印实时调度器统计信息
 */
void rt_print_stats(void);

/**
 * SCHED_RR的默认时间片
 */
#define RR_TIMESLICE    (100)  /* 100ms */

#endif /* _PROCESS_RT_SCHED_H */

