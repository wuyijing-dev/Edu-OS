#ifndef KERNEL_H
#define KERNEL_H

#include "types.h"

/*
 * =============================================================================
 * 内核版本信息
 * =============================================================================
 */

#define KERNEL_NAME     "EduOS"
#define KERNEL_VERSION  "0.1.0"
#define KERNEL_AUTHOR   "EduOS Team"

/*
 * =============================================================================
 * 内核魔数
 * =============================================================================
 */

#define KERNEL_MAGIC    0xDEADBEEF

/*
 * =============================================================================
 * 调试宏
 * =============================================================================
 */

#ifdef DEBUG
    #define KDEBUG(fmt, ...) printk("[DEBUG] " fmt, ##__VA_ARGS__)
#else
    #define KDEBUG(fmt, ...)
#endif

/*
 * =============================================================================
 * 断言宏
 * =============================================================================
 */

#define ASSERT(condition) \
    do { \
        if (!(condition)) { \
            printk("ASSERT FAILED: %s:%d: %s\n", \
                    __FILE__, __LINE__, #condition); \
            while(1) { __asm__ volatile("cli; hlt"); } \
        } \
    } while(0)

/*
 * =============================================================================
 * 核心函数声明
 * =============================================================================
 */

// 内核主函数（支持Multiboot2启动信息）
struct multiboot_info;  /* Forward declaration */
void kernel_main(uint32_t magic, struct multiboot_info *mbi);

// 内核恐慌
void panic(const char *msg) __attribute__((noreturn));

// 格式化输出（类似printf）
void kprintf(const char *fmt, ...);
void kvprintf(const char *fmt, __builtin_va_list args);
int snprintf(char *buf, size_t size, const char *fmt, ...);
int ksnprintf(char *buf, size_t size, const char *fmt, ...);

#endif // KERNEL_H
