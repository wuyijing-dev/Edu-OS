/*
 * PIT (Programmable Interval Timer) 驱动
 * 
 * 提供系统定时器功能，用于进程调度、时间统计等
 */

#include <drivers/timer.h>
#include <arch/i386/irq.h>
#include <arch/i386/pic.h>
#include <io.h>
#include <kernel.h>

/* 全局变量 */
static volatile uint64_t system_ticks = 0;      // 系统运行tick数
static uint32_t timer_frequency = 0;            // 定时器频率
static bool timer_initialized = false;

/*
 * 定时器中断处理函数 (IRQ0)
 */
static void timer_handler(struct interrupt_frame *frame)
{
    (void)frame;  // 未使用
    
    system_ticks++;
    
    /* 每1000个tick输出一次，检查timer是否在运行 */
    if (system_ticks % 1000 == 0) {
        extern void serial_puts(uint16_t port, const char *str);
        serial_puts(0x3F8, "[TIMER_ALIVE]\n");
    }
    
    /* 调用调度器tick（如果调度器已初始化） */
    extern void scheduler_tick(void);
    scheduler_tick();
}

/*
 * 定时器初始化
 * 
 * 配置PIT以指定频率产生中断
 * 
 * 频率计算：
 *   divisor = PIT_FREQUENCY / frequency
 *   实际频率 = PIT_FREQUENCY / divisor
 * 
 * 例如：100Hz
 *   divisor = 1193182 / 100 = 11931.82 ≈ 11932
 *   实际频率 = 1193182 / 11932 ≈ 100.0015 Hz
 */
void timer_init(uint32_t frequency)
{
    if (timer_initialized) {
        kprintf("[TIMER] Warning: Timer already initialized\n");
        return;
    }
    
    /* 验证频率范围 */
    if (frequency < 18 || frequency > 1193182) {
        kprintf("[TIMER] Error: Invalid frequency %d Hz (valid: 18-1193182)\n", 
                frequency);
        frequency = TIMER_FREQUENCY_HZ;
    }
    
    timer_frequency = frequency;
    
    /* 计算分频值 */
    uint32_t divisor = PIT_FREQUENCY / frequency;
    if (divisor > 65535) {
        divisor = 65535;  // 16位最大值
    }
    
    /* 发送命令字：通道0，方波模式，二进制，先低后高 */
    uint8_t command = PIT_CMD_CHANNEL0 | PIT_CMD_MODE3 | PIT_CMD_BOTH | PIT_CMD_BINARY;
    outb(PIT_COMMAND, command);
    
    /* 发送分频值（低字节+高字节） */
    outb(PIT_CHANNEL0, divisor & 0xFF);
    outb(PIT_CHANNEL0, (divisor >> 8) & 0xFF);
    
    /* 注册IRQ0处理函数 */
    irq_install_handler(IRQ_TIMER, timer_handler);
    irq_enable(IRQ_TIMER);
    
    timer_initialized = true;
    
    /* 计算实际频率 */
    uint32_t actual_freq = PIT_FREQUENCY / divisor;
    
    kprintf("[TIMER] PIT Timer initialized\n");
    kprintf("[TIMER] Requested: %d Hz, Actual: %d Hz (divisor: %d)\n",
            frequency, actual_freq, divisor);
    kprintf("[TIMER] Tick interval: %d.%03d ms\n",
            1000 / actual_freq, (1000 * 1000 / actual_freq) % 1000);
}

/*
 * 获取系统运行tick数
 */
uint64_t timer_get_ticks(void)
{
    return system_ticks;
}

/*
 * 获取系统运行秒数
 */
uint64_t timer_get_uptime_seconds(void)
{
    if (timer_frequency == 0) {
        return 0;
    }
    return system_ticks / timer_frequency;
}

/*
 * 获取系统运行毫秒数
 */
uint64_t timer_get_uptime_ms(void)
{
    if (timer_frequency == 0) {
        return 0;
    }
    return (system_ticks * 1000) / timer_frequency;
}

/*
 * 延迟指定tick数（忙等待）
 * 
 * 注意：这是忙等待，会占用CPU
 * 生产环境应使用睡眠+调度器
 */
void timer_wait_ticks(uint32_t ticks)
{
    uint64_t end_ticks = system_ticks + ticks;
    while (system_ticks < end_ticks) {
        __asm__ volatile("hlt");  // 等待下一个中断
    }
}

/*
 * 延迟指定毫秒数
 */
void timer_wait_ms(uint32_t ms)
{
    if (timer_frequency == 0) {
        return;
    }
    
    uint32_t ticks = (ms * timer_frequency) / 1000;
    if (ticks == 0) {
        ticks = 1;
    }
    
    timer_wait_ticks(ticks);
}

/*
 * 获取定时器频率
 */
uint32_t timer_get_frequency(void)
{
    return timer_frequency;
}

/*
 * 打印定时器信息
 */
void timer_print_info(void)
{
    uint64_t uptime_s = timer_get_uptime_seconds();
    uint64_t uptime_ms = timer_get_uptime_ms();
    
    kprintf("\n=== Timer Information ===\n");
    kprintf("Frequency:    %d Hz\n", timer_frequency);
    kprintf("Total Ticks:  %llu\n", system_ticks);
    kprintf("Uptime:       %llu seconds (%llu ms)\n", uptime_s, uptime_ms);
    kprintf("Tick Rate:    %d.%03d ms per tick\n",
            1000 / timer_frequency,
            (1000 * 1000 / timer_frequency) % 1000);
    kprintf("\n");
}

