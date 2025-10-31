/*
 * scheduler.h - 进程调度器
 * 
 * 实现时间片轮转（Round Robin）调度算法
 */

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <process/process.h>

/* 调度器初始化 */
void scheduler_init(void);
void scheduler_enable(void);
void scheduler_enable_noschedule(void);
void scheduler_disable(void);

/* 进程队列操作 */
void scheduler_add_process(struct process *proc);
void scheduler_remove_process(struct process *proc);

/* 调度 */
void scheduler_schedule(void);
void scheduler_yield(void);
void scheduler_tick(void);

/* 获取当前进程 */
struct process *scheduler_get_current(void);

/* 上下文切换 */
void context_switch(struct process *prev, struct process *next);

/* 统计信息 */
void scheduler_print_stats(void);
void scheduler_print_ready_queue(void);

#endif // SCHEDULER_H

