/**
 * time.h - 时间相关定义
 */

#ifndef _TIME_H
#define _TIME_H

#include <types.h>

/**
 * 时间结构
 */
struct timespec {
    time_t tv_sec;      /* 秒 */
    long tv_nsec;       /* 纳秒 */
};

struct timeval {
    time_t tv_sec;      /* 秒 */
    long tv_usec;       /* 微秒 */
};

/**
 * 时钟类型
 */
#define CLOCK_REALTIME      0
#define CLOCK_MONOTONIC     1
#define CLOCK_PROCESS_CPUTIME_ID  2
#define CLOCK_THREAD_CPUTIME_ID   3

#endif /* _TIME_H */


