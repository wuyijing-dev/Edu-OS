/*
 * stdio.c - 标准输入输出实现
 */

#include "stdio.h"
#include "string.h"
#include "unistd.h"

/* 输出单个字符 */
int putchar(int c)
{
    char ch = (char)c;
    return write(STDOUT_FILENO, &ch, 1);
}

/* 输入单个字符 */
int getchar(void)
{
    char c;
    if (read(STDIN_FILENO, &c, 1) == 1) {
        return (unsigned char)c;
    }
    return -1;
}

/* 输出字符串 */
int puts(const char *s)
{
    int len = strlen(s);
    write(STDOUT_FILENO, s, len);
    return putchar('\n');
}

/* 输入字符串 */
char *gets(char *s)
{
    int i = 0;
    int c;
    
    while ((c = getchar()) != '\n' && c != -1) {
        s[i++] = c;
    }
    s[i] = '\0';
    return s;
}

/* 文件输出字符 */
int fputc(int c, int fd)
{
    char ch = (char)c;
    return write(fd, &ch, 1);
}

/* 文件输入字符 */
int fgetc(int fd)
{
    char c;
    if (read(fd, &c, 1) == 1) {
        return (unsigned char)c;
    }
    return -1;
}

/* 文件输出字符串 */
int fputs(const char *s, int fd)
{
    return write(fd, s, strlen(s));
}

/* 数字转字符串（内部辅助函数）*/
static int num_to_str(char *buf, int num, int base, int uppercase)
{
    const char *digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    int i = 0;
    int is_negative = 0;
    
    if (num == 0) {
        buf[i++] = '0';
        buf[i] = '\0';
        return i;
    }
    
    if (num < 0 && base == 10) {
        is_negative = 1;
        num = -num;
    }
    
    /* 转换数字 */
    char tmp[32];
    int j = 0;
    while (num > 0) {
        tmp[j++] = digits[num % base];
        num /= base;
    }
    
    /* 添加负号 */
    if (is_negative) {
        buf[i++] = '-';
    }
    
    /* 反转 */
    while (j > 0) {
        buf[i++] = tmp[--j];
    }
    
    buf[i] = '\0';
    return i;
}

/* 格式化输出到字符串 */
int vsprintf(char *str, const char *fmt, int *args)
{
    int count = 0;
    int arg_index = 0;
    
    while (*fmt) {
        if (*fmt == '%' && *(fmt + 1)) {
            fmt++;
            switch (*fmt) {
                case 's': {  /* 字符串 */
                    char *s = (char*)args[arg_index++];
                    if (s) {
                        while (*s) {
                            str[count++] = *s++;
                        }
                    } else {
                        char *null_str = "(null)";
                        while (*null_str) {
                            str[count++] = *null_str++;
                        }
                    }
                    break;
                }
                case 'd':  /* 十进制整数 */
                case 'i': {
                    char buf[32];
                    num_to_str(buf, args[arg_index++], 10, 0);
                    char *p = buf;
                    while (*p) {
                        str[count++] = *p++;
                    }
                    break;
                }
                case 'u': {  /* 无符号十进制 */
                    char buf[32];
                    num_to_str(buf, args[arg_index++], 10, 0);
                    char *p = buf;
                    while (*p) {
                        str[count++] = *p++;
                    }
                    break;
                }
                case 'x': {  /* 十六进制（小写） */
                    char buf[32];
                    num_to_str(buf, args[arg_index++], 16, 0);
                    char *p = buf;
                    while (*p) {
                        str[count++] = *p++;
                    }
                    break;
                }
                case 'X': {  /* 十六进制（大写） */
                    char buf[32];
                    num_to_str(buf, args[arg_index++], 16, 1);
                    char *p = buf;
                    while (*p) {
                        str[count++] = *p++;
                    }
                    break;
                }
                case 'c': {  /* 字符 */
                    str[count++] = (char)args[arg_index++];
                    break;
                }
                case 'p': {  /* 指针 */
                    str[count++] = '0';
                    str[count++] = 'x';
                    char buf[32];
                    num_to_str(buf, args[arg_index++], 16, 0);
                    char *p = buf;
                    while (*p) {
                        str[count++] = *p++;
                    }
                    break;
                }
                case '%': {
                    str[count++] = '%';
                    break;
                }
                default:
                    str[count++] = '%';
                    str[count++] = *fmt;
                    break;
            }
        } else {
            str[count++] = *fmt;
        }
        fmt++;
    }
    
    str[count] = '\0';
    return count;
}

/* sprintf - 格式化到字符串 */
int sprintf(char *str, const char *fmt, ...)
{
    int *args = (int*)(&fmt + 1);
    return vsprintf(str, fmt, args);
}

/* printf - 格式化输出到标准输出 */
int printf(const char *fmt, ...)
{
    char buf[1024];
    int *args = (int*)(&fmt + 1);
    int len = vsprintf(buf, fmt, args);
    write(STDOUT_FILENO, buf, len);
    return len;
}

/* fprintf - 格式化输出到文件 */
int fprintf(int fd, const char *fmt, ...)
{
    char buf[1024];
    int *args = (int*)(&fmt + 1);
    int len = vsprintf(buf, fmt, args);
    write(fd, buf, len);
    return len;
}
