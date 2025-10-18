/*
 * stdlib.c - 标准库函数实现
 */

#include "stdlib.h"
#include "string.h"
#include "unistd.h"

/* 进程退出 */
void exit(int status)
{
    _exit(status);
}

/* 异常终止 */
void abort(void)
{
    _exit(1);
}

/* ========== 简单的内存分配器 ========== */

#define HEAP_SIZE (64 * 1024)  /* 64KB 堆（减小以避免ELF过大） */

static char heap[HEAP_SIZE];
static size_t heap_ptr = 0;

/* 简单的 bump allocator */
void *malloc(size_t size)
{
    if (size == 0) {
        return 0;
    }
    
    /* 对齐到8字节 */
    size = (size + 7) & ~7;
    
    if (heap_ptr + size > HEAP_SIZE) {
        return 0;  /* 内存不足 */
    }
    
    void *ptr = &heap[heap_ptr];
    heap_ptr += size;
    
    return ptr;
}

/* 简化版：不实际释放 */
void free(void *ptr)
{
    (void)ptr;
    /* 简单实现：不回收内存 */
    /* 实际生产环境应该实现 free list */
}

/* 分配并清零 */
void *calloc(size_t nmemb, size_t size)
{
    size_t total = nmemb * size;
    void *ptr = malloc(total);
    
    if (ptr) {
        memset(ptr, 0, total);
    }
    
    return ptr;
}

/* 重新分配 */
void *realloc(void *ptr, size_t size)
{
    if (!ptr) {
        return malloc(size);
    }
    
    if (size == 0) {
        free(ptr);
        return 0;
    }
    
    /* 简化版：分配新内存并复制 */
    void *new_ptr = malloc(size);
    if (new_ptr) {
        /* 注意：这里无法知道原来的大小，假设复制 size 字节 */
        memcpy(new_ptr, ptr, size);
    }
    
    return new_ptr;
}

/* ========== 字符串转换 ========== */

/* 字符串转整数 */
int atoi(const char *nptr)
{
    int result = 0;
    int sign = 1;
    
    /* 跳过空白 */
    while (*nptr == ' ' || *nptr == '\t' || *nptr == '\n') {
        nptr++;
    }
    
    /* 处理符号 */
    if (*nptr == '-') {
        sign = -1;
        nptr++;
    } else if (*nptr == '+') {
        nptr++;
    }
    
    /* 转换数字 */
    while (*nptr >= '0' && *nptr <= '9') {
        result = result * 10 + (*nptr - '0');
        nptr++;
    }
    
    return sign * result;
}

/* 字符串转长整数 */
long atol(const char *nptr)
{
    /* 简化版：与 atoi 相同 */
    return (long)atoi(nptr);
}

/* ========== 数学函数 ========== */

/* 绝对值 */
int abs(int n)
{
    return n < 0 ? -n : n;
}

/* 长整数绝对值 */
long labs(long n)
{
    return n < 0 ? -n : n;
}
