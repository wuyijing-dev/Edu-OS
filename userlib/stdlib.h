/*
 * stdlib.h - 标准库函数
 */

#ifndef _STDLIB_H
#define _STDLIB_H

typedef unsigned int size_t;

/* 进程控制 */
void exit(int status);
void abort(void);

/* 内存分配 */
void *malloc(size_t size);
void free(void *ptr);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);

/* 字符串转换 */
int atoi(const char *nptr);
long atol(const char *nptr);

/* 其他工具 */
int abs(int n);
long labs(long n);

#endif /* _STDLIB_H */
