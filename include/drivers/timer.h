#ifndef _DRIVERS_TIMER_H
#define _DRIVERS_TIMER_H

#include <types.h>

/* PIT (Programmable Interval Timer) 8253/8254 */
#define PIT_FREQUENCY       1193182     // PIT输入频率(Hz)
#define PIT_CHANNEL0        0x40        // 通道0数据端口
#define PIT_CHANNEL1        0x41        // 通道1数据端口（DRAM刷新，已废弃）
#define PIT_CHANNEL2        0x42        // 通道2数据端口（PC扬声器）
#define PIT_COMMAND         0x43        // 命令端口

/* PIT命令字 */
#define PIT_CMD_BINARY      0x00        // 二进制模式
#define PIT_CMD_BCD         0x01        // BCD模式
#define PIT_CMD_MODE0       0x00        // 模式0：计数结束中断
#define PIT_CMD_MODE1       0x02        // 模式1：硬件可重触发单稳
#define PIT_CMD_MODE2       0x04        // 模式2：频率发生器
#define PIT_CMD_MODE3       0x06        // 模式3：方波发生器
#define PIT_CMD_MODE4       0x08        // 模式4：软件触发选通
#define PIT_CMD_MODE5       0x0A        // 模式5：硬件触发选通
#define PIT_CMD_LATCH       0x00        // 锁存计数值
#define PIT_CMD_LSB         0x10        // 只读/写低字节
#define PIT_CMD_MSB         0x20        // 只读/写高字节
#define PIT_CMD_BOTH        0x30        // 先读/写低字节，再高字节
#define PIT_CMD_CHANNEL0    0x00        // 选择通道0
#define PIT_CMD_CHANNEL1    0x40        // 选择通道1
#define PIT_CMD_CHANNEL2    0x80        // 选择通道2
#define PIT_CMD_READBACK    0xC0        // 读回命令

/* 默认定时器频率 */
#define TIMER_FREQUENCY_HZ  100         // 100Hz = 每秒100次中断 = 10ms一次

/*
 * 定时器初始化
 * 
 * @param frequency: 定时器频率(Hz)，典型值：100-1000
 */
void timer_init(uint32_t frequency);

/*
 * 获取系统运行时间
 * 
 * @return: 系统运行的tick数
 */
uint64_t timer_get_ticks(void);

/*
 * 获取系统运行秒数
 * 
 * @return: 系统运行的秒数
 */
uint64_t timer_get_uptime_seconds(void);

/*
 * 获取系统运行毫秒数
 * 
 * @return: 系统运行的毫秒数
 */
uint64_t timer_get_uptime_ms(void);

/*
 * 延迟指定tick数
 * 
 * @param ticks: 要延迟的tick数
 */
void timer_wait_ticks(uint32_t ticks);

/*
 * 延迟指定毫秒数
 * 
 * @param ms: 要延迟的毫秒数
 */
void timer_wait_ms(uint32_t ms);

/*
 * 获取定时器频率
 * 
 * @return: 定时器频率(Hz)
 */
uint32_t timer_get_frequency(void);

/*
 * 打印定时器信息
 */
void timer_print_info(void);

#endif /* _DRIVERS_TIMER_H */

