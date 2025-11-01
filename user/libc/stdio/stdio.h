/*
 * stdio.h - 标准输入输出
 */

#ifndef _STDIO_H
#define _STDIO_H

typedef unsigned int size_t;

/* 标准流 */
#define stdin   0
#define stdout  1
#define stderr  2

/* 基本I/O */
int putchar(int c);
int getchar(void);
int puts(const char *s);
char *gets(char *s);

/* 格式化输出 */
int printf(const char *fmt, ...);
int fprintf(int fd, const char *fmt, ...);
int sprintf(char *str, const char *fmt, ...);

/* 文件操作（简化版）*/
int fputc(int c, int fd);
int fgetc(int fd);
int fputs(const char *s, int fd);

#endif /* _STDIO_H */
