/*
 * time.h - POSIX时间接口
 */

#ifndef _TIME_H
#define _TIME_H

#include "../syscall/types.h"

typedef long time_t;
typedef long clock_t;

struct tm {
    int tm_sec;    /* 秒 (0-59) */
    int tm_min;    /* 分 (0-59) */
    int tm_hour;   /* 时 (0-23) */
    int tm_mday;   /* 日 (1-31) */
    int tm_mon;    /* 月 (0-11) */
    int tm_year;   /* 年（从1900开始） */
    int tm_wday;   /* 星期 (0-6) */
    int tm_yday;   /* 年内第几天 (0-365) */
    int tm_isdst;  /* 夏令时 */
};

/* 时间函数 */
time_t time(time_t *t);
struct tm *localtime(const time_t *timep);
struct tm *gmtime(const time_t *timep);
size_t strftime(char *s, size_t max, const char *format, const struct tm *tm);

/* 声明snprintf（time.c需要） */
extern int snprintf(char *str, size_t size, const char *format, ...);

#endif /* _TIME_H */


