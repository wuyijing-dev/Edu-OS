#ifndef SERIAL_H
#define SERIAL_H

#include "types.h"

/*
 * =============================================================================
 * 串行端口（UART）驱动
 * =============================================================================
 */

// COM端口基地址
#define COM1    0x3F8
#define COM2    0x2F8
#define COM3    0x3E8
#define COM4    0x2E8

// 串口寄存器偏移
#define SERIAL_DATA         0  // 数据寄存器（DLAB=0）
#define SERIAL_DIV_LOW      0  // 波特率除数低字节（DLAB=1）
#define SERIAL_IER          1  // 中断使能寄存器（DLAB=0）
#define SERIAL_DIV_HIGH     1  // 波特率除数高字节（DLAB=1）
#define SERIAL_FIFO_CTRL    2  // FIFO控制寄存器
#define SERIAL_LINE_CTRL    3  // 线路控制寄存器（LCR）
#define SERIAL_MODEM_CTRL   4  // 调制解调器控制寄存器
#define SERIAL_LINE_STATUS  5  // 线路状态寄存器（LSR）
#define SERIAL_MODEM_STATUS 6  // 调制解调器状态寄存器
#define SERIAL_SCRATCH      7  // Scratch寄存器

// LCR - 线路控制寄存器位
#define SERIAL_LCR_DLAB     0x80  // 除数锁存访问位
#define SERIAL_LCR_8BITS    0x03  // 8位数据位
#define SERIAL_LCR_1STOP    0x00  // 1位停止位
#define SERIAL_LCR_NO_PARITY 0x00 // 无校验

// LSR - 线路状态寄存器位
#define SERIAL_LSR_DATA_READY    0x01  // 数据就绪
#define SERIAL_LSR_THR_EMPTY     0x20  // 发送保持寄存器空

// 波特率（115200 / divisor）
#define SERIAL_BAUD_115200  1
#define SERIAL_BAUD_57600   2
#define SERIAL_BAUD_38400   3
#define SERIAL_BAUD_19200   6
#define SERIAL_BAUD_9600    12

/*
 * =============================================================================
 * 函数声明
 * =============================================================================
 */

// 初始化串口
void serial_init(uint16_t port);

// 发送单个字符
void serial_putc(uint16_t port, char c);

// 发送字符串
void serial_puts(uint16_t port, const char *str);

// 读取单个字符（如果可用）
char serial_getc(uint16_t port);

// 检查是否有数据可读
int serial_received(uint16_t port);

// 检查是否可以发送
int serial_transmit_empty(uint16_t port);

// 默认串口（COM1）的便捷函数
static inline void serial_write(char c) {
    serial_putc(COM1, c);
}

static inline void serial_print(const char *str) {
    serial_puts(COM1, str);
}

#endif // SERIAL_H
