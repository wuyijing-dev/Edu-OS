/*
 * time.h - POSIX时间定义
 * 
 * 符合POSIX.1-2017标准
 */

#ifndef _SYS_TIME_H
#define _SYS_TIME_H

#include <types.h>

/* timeval结构 */
struct timeval {
    time_t      tv_sec;     /* 秒 */
    suseconds_t tv_usec;    /* 微秒 */
};

/* timespec结构 */
struct timespec {
    time_t  tv_sec;         /* 秒 */
    long    tv_nsec;        /* 纳秒 */
};

/* timezone结构 */
struct timezone {
    int tz_minuteswest;     /* 格林威治以西的分钟数 */
    int tz_dsttime;         /* DST校正类型 */
};

/* 系统调用原型 */
#ifndef __KERNEL__
int gettimeofday(struct timeval *tv, struct timezone *tz);
int settimeofday(const struct timeval *tv, const struct timezone *tz);
int nanosleep(const struct timespec *req, struct timespec *rem);
int clock_gettime(clockid_t clk_id, struct timespec *tp);
int clock_settime(clockid_t clk_id, const struct timespec *tp);
#endif

/* 时钟类型 */
#define CLOCK_REALTIME           0
#define CLOCK_MONOTONIC          1
#define CLOCK_PROCESS_CPUTIME_ID 2
#define CLOCK_THREAD_CPUTIME_ID  3

#endif /* _SYS_TIME_H */

