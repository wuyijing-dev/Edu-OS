/*
 * mlfq_sched.h - 多级反馈队列调度器
 * 
 * 实现MLFQ调度算法，自适应进程行为
 */

#ifndef MLFQ_SCHED_H
#define MLFQ_SCHED_H

#include <process/process.h>

#define MLFQ_LEVELS 5           /* 5个队列级别 */
#define MLFQ_BASE_QUANTUM 100   /* 基础时间片：100 ticks (Linux风格：更长的时间片减少上下文切换开销) */
#define MLFQ_BOOST_INTERVAL 1000 /* 提升间隔：1000 ticks */

/* MLFQ调度器初始化 */
void mlfq_init(void);

/* 进程入队/出队 */
void mlfq_enqueue(struct process *proc);
void mlfq_dequeue(struct process *proc);

/* 选择下一个进程 */
struct process *mlfq_pick_next(void);

/* 定时器tick处理 */
void mlfq_tick(struct process *proc);

/* 进程主动让出CPU */
void mlfq_yield(struct process *proc);

/* 全局提升（防饿死） */
void mlfq_boost_all(void);

/* 调度主循环 */
void mlfq_schedule(void);

/* 统计信息 */
void mlfq_print_stats(void);
void mlfq_print_queues(void);

/* 辅助函数 */
int mlfq_get_level_count(int level);
uint32_t mlfq_get_level_quantum(int level);

#endif // MLFQ_SCHED_H

