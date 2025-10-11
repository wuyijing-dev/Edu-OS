/* kernel.c - 内核核心函数 */

#include "kernel.h"
#include "vga.h"
#include "serial.h"
#include "string.h"

/* 输出模式：同时输出到 VGA 和串口 */
#define OUTPUT_TO_BOTH 1  /* 同时输出到 VGA 和串口（宿主机） */

/* 输出辅助函数 */
static inline void debug_putc(char c)
{
#if OUTPUT_TO_BOTH
    vga_putc(c);
    serial_putc(COM1, c);
#else
    vga_putc(c);
#endif
}

static inline void debug_puts(const char *s)
{
#if OUTPUT_TO_BOTH
    vga_puts(s);
    while (*s) {
        serial_putc(COM1, *s++);
    }
#else
    vga_puts(s);
#endif
}

/* 简化的数字转字符串函数 */
static void itoa_internal(long long num, char *str, int base, int is_signed)
{
    int i = 0;
    bool is_negative = false;
    
    if (num == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return;
    }
    
    if (is_signed && num < 0) {
        is_negative = true;
        num = -num;
    }
    
    while (num != 0) {
        int rem = num % base;
        str[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        num = num / base;
    }
    
    if (is_negative) {
        str[i++] = '-';
    }
    
    str[i] = '\0';
    
    /* 反转字符串 */
    int start = 0;
    int end = i - 1;
    while (start < end) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }
}

/* 简化的 kprintf 实现 */
void kprintf(const char *fmt, ...)
{
    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    kvprintf(fmt, args);
    __builtin_va_end(args);
}

void kvprintf(const char *fmt, __builtin_va_list args)
{
    char buffer[32];
    
    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            switch (*fmt) {
                case 's': {
                    const char *s = __builtin_va_arg(args, const char *);
                    if (s) {
                        debug_puts(s);
                    } else {
                        debug_puts("(null)");
                    }
                    break;
                }
                case 'd':
                case 'i': {
                    int num = __builtin_va_arg(args, int);
                    itoa_internal(num, buffer, 10, 1);
                    debug_puts(buffer);
                    break;
                }
                case 'u': {
                    unsigned int num = __builtin_va_arg(args, unsigned int);
                    itoa_internal(num, buffer, 10, 0);
                    debug_puts(buffer);
                    break;
                }
                case 'x': {
                    unsigned int num = __builtin_va_arg(args, unsigned int);
                    itoa_internal(num, buffer, 16, 0);
                    debug_puts(buffer);
                    break;
                }
                case 'X': {
                    unsigned int num = __builtin_va_arg(args, unsigned int);
                    itoa_internal(num, buffer, 16, 0);
                    /* 转换为大写 */
                    for (char *p = buffer; *p; p++) {
                        if (*p >= 'a' && *p <= 'f') {
                            *p = *p - 'a' + 'A';
                        }
                    }
                    debug_puts(buffer);
                    break;
                }
                case 'p': {
                    void *ptr = __builtin_va_arg(args, void *);
                    debug_puts("0x");
                    itoa_internal((unsigned long)ptr, buffer, 16, 0);
                    debug_puts(buffer);
                    break;
                }
                case 'c': {
                    char c = (char)__builtin_va_arg(args, int);
                    debug_putc(c);
                    break;
                }
                case '%': {
                    debug_putc('%');
                    break;
                }
                case '0': {
                    /* 处理 %08x 格式（简化版本） */
                    if (*(fmt+1) >= '1' && *(fmt+1) <= '9') {
                        int width = *(fmt+1) - '0';
                        fmt += 2;
                        if (*fmt == 'x' || *fmt == 'X') {
                            unsigned int num = __builtin_va_arg(args, unsigned int);
                            itoa_internal(num, buffer, 16, 0);
                            int len = strlen(buffer);
                            for (int i = 0; i < width - len; i++) {
                                debug_putc('0');
                            }
                            debug_puts(buffer);
                        }
                    }
                    break;
                }
                case 'l': {
                    /* 处理 %llu, %lld */
                    if (*(fmt+1) == 'l') {
                        fmt++;
                        if (*(fmt+1) == 'u') {
                            fmt++;
                            unsigned long long num = __builtin_va_arg(args, unsigned long long);
                            itoa_internal(num, buffer, 10, 0);
                            debug_puts(buffer);
                        } else if (*(fmt+1) == 'd') {
                            fmt++;
                            long long num = __builtin_va_arg(args, long long);
                            itoa_internal(num, buffer, 10, 1);
                            debug_puts(buffer);
                        }
                    }
                    break;
                }
                default:
                    debug_putc('%');
                    debug_putc(*fmt);
                    break;
            }
            fmt++;
        } else {
            debug_putc(*fmt++);
        }
    }
}

void panic(const char *msg)
{
    __asm__ volatile ("cli");
    
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_RED);
    vga_clear();
    
    vga_write_string("\n\n");
    vga_write_string("================================================================================\n");
    vga_write_string("                              KERNEL PANIC                                      \n");
    vga_write_string("================================================================================\n");
    vga_write_string("\nA fatal error has occurred and the system cannot continue.\n\n");
    vga_write_string("Error: ");
    vga_write_string(msg);
    vga_write_string("\n\nSystem halted.\n\n");
    vga_write_string("================================================================================\n");
    
    while (1) {
        __asm__ volatile ("hlt");
    }
}

/*
 * 格式化输出到缓冲区（类似snprintf）
 */
int snprintf(char *buf, size_t size, const char *fmt, ...)
{
    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    
    if (!buf || size == 0 || !fmt) {
        __builtin_va_end(args);
        return 0;
    }
    
    char *p = buf;
    size_t written = 0;
    char temp_buffer[32];
    
    while (*fmt && written < size - 1) {
        if (*fmt == '%') {
            fmt++;
            switch (*fmt) {
                case 's': {
                    const char *s = __builtin_va_arg(args, const char *);
                    if (s) {
                        while (*s && written < size - 1) {
                            *p++ = *s++;
                            written++;
                        }
                    }
                    break;
                }
                case 'd':
                case 'i': {
                    int num = __builtin_va_arg(args, int);
                    itoa_internal(num, temp_buffer, 10, 1);
                    const char *s = temp_buffer;
                    while (*s && written < size - 1) {
                        *p++ = *s++;
                        written++;
                    }
                    break;
                }
                case 'u': {
                    unsigned int num = __builtin_va_arg(args, unsigned int);
                    itoa_internal(num, temp_buffer, 10, 0);
                    const char *s = temp_buffer;
                    while (*s && written < size - 1) {
                        *p++ = *s++;
                        written++;
                    }
                    break;
                }
                case 'x': {
                    unsigned int num = __builtin_va_arg(args, unsigned int);
                    itoa_internal(num, temp_buffer, 16, 0);
                    const char *s = temp_buffer;
                    while (*s && written < size - 1) {
                        *p++ = *s++;
                        written++;
                    }
                    break;
                }
                case 'c': {
                    char c = (char)__builtin_va_arg(args, int);
                    if (written < size - 1) {
                        *p++ = c;
                        written++;
                    }
                    break;
                }
                case '%': {
                    if (written < size - 1) {
                        *p++ = '%';
                        written++;
                    }
                    break;
                }
                default:
                    break;
            }
            fmt++;
        } else {
            *p++ = *fmt++;
            written++;
        }
    }
    
    *p = '\0';
    __builtin_va_end(args);
    return written;
}

int ksnprintf(char *buf, size_t size, const char *fmt, ...)
{
    if (!buf || size == 0 || !fmt) {
        return 0;
    }
    
    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    
    char buffer[32];
    size_t pos = 0;
    
    while (*fmt && pos < size - 1) {
        if (*fmt != '%') {
            buf[pos++] = *fmt++;
            continue;
        }
        
        fmt++;  // 跳过'%'
        
        switch (*fmt) {
            case 's': {
                const char *str = __builtin_va_arg(args, const char*);
                if (str) {
                    while (*str && pos < size - 1) {
                        buf[pos++] = *str++;
                    }
                }
                break;
            }
            case 'd': {
                int num = __builtin_va_arg(args, int);
                itoa_internal(num, buffer, 10, 1);
                for (int i = 0; buffer[i] && pos < size - 1; i++) {
                    buf[pos++] = buffer[i];
                }
                break;
            }
            case 'u': {
                unsigned int num = __builtin_va_arg(args, unsigned int);
                itoa_internal(num, buffer, 10, 0);
                for (int i = 0; buffer[i] && pos < size - 1; i++) {
                    buf[pos++] = buffer[i];
                }
                break;
            }
            case 'x': {
                unsigned int num = __builtin_va_arg(args, unsigned int);
                itoa_internal(num, buffer, 16, 0);
                for (int i = 0; buffer[i] && pos < size - 1; i++) {
                    buf[pos++] = buffer[i];
                }
                break;
            }
            case 'c': {
                char c = (char)__builtin_va_arg(args, int);
                if (pos < size - 1) {
                    buf[pos++] = c;
                }
                break;
            }
            case '%': {
                if (pos < size - 1) {
                    buf[pos++] = '%';
                }
                break;
            }
            default:
                if (pos < size - 2) {
                    buf[pos++] = '%';
                    buf[pos++] = *fmt;
                }
                break;
        }
        
        fmt++;
    }
    
    buf[pos] = '\0';
    
    __builtin_va_end(args);
    
    return pos;
}
