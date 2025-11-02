/*
 * sys_time.c - time系统调用实现
 */

#include <syscall.h>
#include <types.h>
#include <sys/time.h>
#include <kernel.h>
#include <drivers/timer.h>
#include <fs/vfs.h>
#include <process/process.h>

/* errno错误码 */
#define EFAULT  14
#define EINVAL  22

/*
 * sys_time - 获取当前时间
 * @tloc: 可选的时间指针（如果非NULL，将时间写入此地址）
 * 
 * 返回: 自Unix纪元（1970-01-01 00:00:00 UTC）以来的秒数
 * 
 * 注意: 这是一个简化实现，返回系统启动后的秒数
 */
int sys_time(uint32_t *tloc)
{
    /* 获取系统启动后的秒数 */
    uint32_t seconds = (uint32_t)timer_get_uptime_seconds();
    
    /* 如果tloc非NULL，验证并写入 */
    if (tloc) {
        /* 验证用户空间地址 */
        if (!is_user_address(tloc) || !is_user_buffer(tloc, sizeof(uint32_t))) {
            return -EFAULT;
        }
        
        /* 写入时间值 */
        *tloc = seconds;
    }
    
    /* 返回时间值 */
    return (int)seconds;
}

/*
 * sys_gettimeofday - 获取当前时间（微秒精度）
 * 
 * @tv: timeval结构指针
 * @tz: timezone结构指针（通常为NULL）
 * 
 * 返回：0=成功，-1=失败
 */
int sys_gettimeofday(struct timeval *tv, struct timezone *tz)
{
    if (!tv) {
        extern int set_errno(int error_code);
        return set_errno(EFAULT);
    }
    
    /* 获取系统运行时间（ticks）*/
    extern uint64_t timer_get_ticks(void);
    uint64_t ticks = timer_get_ticks();
    
    /* 转换为秒和微秒（假设100Hz频率）*/
    tv->tv_sec = ticks / 100;
    tv->tv_usec = (ticks % 100) * 10000;  /* 10ms per tick */
    
    /* timezone通常不使用 */
    if (tz) {
        tz->tz_minuteswest = 0;
        tz->tz_dsttime = 0;
    }
    
    kprintf("[SYS_GETTIMEOFDAY] sec=%u, usec=%u\n", tv->tv_sec, tv->tv_usec);
    return 0;
}

/*
 * sys_nanosleep - 睡眠指定时间（纳秒精度）
 * 
 * @req: 请求睡眠时间
 * @rem: 剩余睡眠时间（如果被信号中断）
 * 
 * 返回：0=成功，-1=失败
 */
int sys_nanosleep(const struct timespec *req, struct timespec *rem)
{
    if (!req) {
        extern int set_errno(int error_code);
        return set_errno(EFAULT);
    }
    
    /* 参数验证 */
    if (req->tv_nsec < 0 || req->tv_nsec >= 1000000000) {
        extern int set_errno(int error_code);
        return set_errno(EINVAL);
    }
    
    /* 转换为ticks（假设100Hz频率，10ms per tick）*/
    uint64_t sleep_ms = req->tv_sec * 1000 + req->tv_nsec / 1000000;
    uint64_t sleep_ticks = (sleep_ms + 9) / 10;  /* 向上取整 */
    
    if (sleep_ticks == 0) {
        sleep_ticks = 1;  /* 至少睡眠1个tick */
    }
    
    kprintf("[SYS_NANOSLEEP] Sleeping for %llu ticks (%u.%09u sec)\n",
            sleep_ticks, req->tv_sec, req->tv_nsec);
    
    /* 简化实现：忙等待（TODO：实现真正的睡眠调度）*/
    extern uint64_t timer_get_ticks(void);
    uint64_t start_ticks = timer_get_ticks();
    uint64_t target_ticks = start_ticks + sleep_ticks;
    
    /* 切换到BLOCKED状态并等待 */
    extern struct process *process_get_current(void);
    struct process *current = process_get_current();
    if (current) {
        /* 简化版：自旋等待（生产环境应该让出CPU）*/
        while (timer_get_ticks() < target_ticks) {
            /* 检查是否有pending信号 */
            extern int signal_pending(struct process *proc);
            if (signal_pending(current)) {
                /* 被信号中断 */
                if (rem) {
                    uint64_t elapsed = timer_get_ticks() - start_ticks;
                    uint64_t remaining_ticks = (target_ticks > timer_get_ticks()) ? 
                                               (target_ticks - timer_get_ticks()) : 0;
                    uint64_t remaining_ms = remaining_ticks * 10;
                    rem->tv_sec = remaining_ms / 1000;
                    rem->tv_nsec = (remaining_ms % 1000) * 1000000;
                }
                extern int set_errno(int error_code);
                return set_errno(4);  /* EINTR */
            }
            /* 主动让出CPU */
            __asm__ volatile("hlt");
        }
    }
    
    if (rem) {
        rem->tv_sec = 0;
        rem->tv_nsec = 0;
    }
    
    return 0;
}

