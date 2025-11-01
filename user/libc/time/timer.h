/*
 * timer.h - POSIX定时器支持
 */

#ifndef _TIMER_H
#define _TIMER_H

#include "../syscall/types.h"
#include "time.h"

/* 定时器ID类型 */
typedef int timer_t;

/* 定时器规格 */
struct itimerspec {
    struct timespec it_interval;  /* 定时器间隔 */
    struct timespec it_value;     /* 初始过期时间 */
};

/* 定时器值（兼容setitimer）*/
struct itimerval {
    struct timeval it_interval;   /* 定时器间隔 */
    struct timeval it_value;      /* 当前值 */
};

/* 定时器类型 */
#define ITIMER_REAL    0  /* 实时定时器 */
#define ITIMER_VIRTUAL 1  /* 虚拟定时器（进程CPU时间）*/
#define ITIMER_PROF    2  /* 性能定时器（进程+系统CPU时间）*/

/* 定时器函数 */
int timer_create(int clockid, void *sevp, timer_t *timerid);
int timer_delete(timer_t timerid);
int timer_settime(timer_t timerid, int flags,
                  const struct itimerspec *new_value,
                  struct itimerspec *old_value);
int timer_gettime(timer_t timerid, struct itimerspec *curr_value);

/* 简化的定时器接口 */
int setitimer(int which, const struct itimerval *new_value,
              struct itimerval *old_value);
int getitimer(int which, struct itimerval *curr_value);

/* 睡眠函数 */
unsigned int sleep(unsigned int seconds);
int usleep(unsigned int usec);
int nanosleep(const struct timespec *req, struct timespec *rem);

#endif /* _TIMER_H */

