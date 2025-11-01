/*
 * time.c - POSIX时间函数（基础版本）
 */

#include "time.h"
#include "../syscall/unistd.h"

/* 外部syscall包装 */
extern int _time_syscall(unsigned int *tloc);

/* 系统启动以来的秒数 */
time_t time(time_t *t)
{
    unsigned int temp;
    int ret = _time_syscall(t ? &temp : NULL);
    
    if (t) {
        *t = (time_t)temp;
    }
    
    return (time_t)ret;
}

/* 简化的localtime */
struct tm *localtime(const time_t *timep)
{
    static struct tm tm_result;
    
    if (!timep) return NULL;
    
    /* 简化：返回固定时间 */
    tm_result.tm_sec = 0;
    tm_result.tm_min = 0;
    tm_result.tm_hour = 12;
    tm_result.tm_mday = 1;
    tm_result.tm_mon = 10;  /* 11月 */
    tm_result.tm_year = 125; /* 2025 */
    tm_result.tm_wday = 5;
    tm_result.tm_yday = 305;
    tm_result.tm_isdst = 0;
    
    return &tm_result;
}

/* 格式化时间字符串 */
size_t strftime(char *s, size_t max, const char *format, const struct tm *tm)
{
    /* 简化实现：只支持基本格式 */
    if (!s || !format || !tm || max == 0) return 0;
    
    /* 暂时返回固定格式 */
    return snprintf(s, max, "%02d:%02d:%02d", 
                   tm->tm_hour, tm->tm_min, tm->tm_sec);
}

