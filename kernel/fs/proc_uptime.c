/*
 * proc_uptime.c - /proc/uptime 实现
 * 
 * 显示系统运行时间（Linux风格）
 */

#include <fs/procfs.h>
#include <kernel.h>
#include <drivers/timer.h>
#include <string.h>

/*
 * 读取系统运行时间
 * 
 * 格式：uptime idle_time
 *   - uptime: 系统运行时间（秒）
 *   - idle_time: CPU 空闲时间（秒）
 */
int proc_uptime_read(char *buf, size_t size, off_t *offset, void *data)
{
    (void)data;
    
    /* 获取系统 ticks */
    uint64_t ticks = timer_get_ticks();
    uint32_t freq = TIMER_FREQUENCY_HZ;
    
    /* 计算运行时间（秒） */
    double uptime = (double)ticks / freq;
    
    /* 计算空闲时间（简化：假设 50% 空闲） */
    /* TODO: 实现真实的 idle 时间统计 */
    double idle_time = uptime * 0.5;
    
    /* 生成内容 */
    char content[128];
    int len = snprintf(content, sizeof(content),
        "%.2f %.2f\n",
        uptime,
        idle_time
    );
    
    /* 处理 offset */
    return procfs_read_with_offset(content, len, buf, size, offset);
}

