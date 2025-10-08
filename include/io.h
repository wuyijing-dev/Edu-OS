#ifndef IO_H
#define IO_H

#include "types.h"

/*
 * =============================================================================
 * x86 I/O端口操作函数
 * =============================================================================
 */

// 向端口写入一个字节
static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

// 从端口读取一个字节
static inline uint8_t inb(uint16_t port)
{
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// 向端口写入一个字（16位）
static inline void outw(uint16_t port, uint16_t value)
{
    __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
}

// 从端口读取一个字（16位）
static inline uint16_t inw(uint16_t port)
{
    uint16_t ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// 向端口写入一个双字（32位）
static inline void outl(uint16_t port, uint32_t value)
{
    __asm__ volatile ("outl %0, %1" : : "a"(value), "Nd"(port));
}

// 从端口读取一个双字（32位）
static inline uint32_t inl(uint16_t port)
{
    uint32_t ret;
    __asm__ volatile ("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// I/O延迟（向未使用的端口写入以等待）
static inline void io_wait(void)
{
    outb(0x80, 0);
}

#endif // IO_H

