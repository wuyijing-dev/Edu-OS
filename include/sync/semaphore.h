/*
 * semaphore.h - 信号量
 * 
 * 基于 Linux 内核信号量实现
 */

#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include <types.h>
#include <process/process.h>

/* 信号量结构 */
struct semaphore {
    volatile int32_t count;         /* 信号量计数 */
    struct process *wait_list;      /* 等待队列 */
    const char *name;               /* 名称（调试用） */
};

/* ========== 信号量操作 ========== */

/*
 * 初始化信号量
 * 
 * @param count: 初始计数（通常为 1 表示互斥，>1 表示资源数量）
 */
void sem_init(struct semaphore *sem, int32_t count, const char *name);

/*
 * P 操作（等待，down）
 * 
 * 计数减 1，如果 < 0 则阻塞
 */
void sem_wait(struct semaphore *sem);

/*
 * V 操作（信号，up）
 * 
 * 计数加 1，如果有等待者则唤醒一个
 */
void sem_post(struct semaphore *sem);

/*
 * 尝试 P 操作（非阻塞）
 * 
 * @return: 0=成功，-1=失败
 */
int sem_trywait(struct semaphore *sem);

/*
 * 获取信号量值
 */
int32_t sem_getvalue(struct semaphore *sem);

/* ========== 静态初始化宏 ========== */

#define SEMAPHORE_INITIALIZER(semname, val) \
    { .count = val, .wait_list = NULL, .name = #semname }

#define DEFINE_SEMAPHORE(name, val) \
    struct semaphore name = SEMAPHORE_INITIALIZER(name, val)

#endif // SEMAPHORE_H

