/*
 * sys_time.c - time系统调用实现
 */

#include <syscall.h>
#include <types.h>
#include <kernel.h>
#include <drivers/timer.h>
#include <fs/vfs.h>

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

