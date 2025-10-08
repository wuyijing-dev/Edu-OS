/*
 * priority_sched.h - 优先级调度器
 * 
 * 实现32级优先级调度，支持：
 * - O(1)时间复杂度的进程选择
 * - 优先级老化防止饿死
 * - 位图加速的队列查找
 */

#ifndef PRIORITY_SCHED_H
#define PRIORITY_SCHED_H

#include <process/process.h>

#define MAX_PRIORITIES 32       /* 32个优先级级别（0最高，31最低） */

/* 优先级调度器初始化 */
void priority_scheduler_init(void);

/* 进程入队/出队 */
void priority_enqueue(struct process *proc);
void priority_dequeue(struct process *proc);

/* 选择下一个进程 */
struct process *priority_pick_next(void);

/* 优先级老化（防饿死） */
void priority_aging(void);

/* 调度主循环 */
void priority_schedule(void);

/* 统计信息 */
void priority_print_stats(void);
void priority_print_queues(void);

/* 辅助函数 */
int priority_get_queue_count(int priority);
bool priority_is_queue_empty(int priority);

#endif // PRIORITY_SCHED_H

