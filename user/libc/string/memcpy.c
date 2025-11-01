/*
 * memcpy.c - POSIX string.h: memcpy, memmove, memset
 */

#include "string.h"

#ifndef NULL
#define NULL ((void*)0)
#endif

void *memcpy(void *dest, const void *src, size_t n)
{
    char *d = dest;
    const char *s = src;
    
    while (n--) {
        *d++ = *s++;
    }
    
    return dest;
}

void *memmove(void *dest, const void *src, size_t n)
{
    char *d = dest;
    const char *s = src;
    
    if (d < s) {
        /* 正向复制 */
        while (n--) {
            *d++ = *s++;
        }
    } else {
        /* 反向复制（处理重叠） */
        d += n;
        s += n;
        while (n--) {
            *--d = *--s;
        }
    }
    
    return dest;
}

void *memset(void *s, int c, size_t n)
{
    unsigned char *p = s;
    
    while (n--) {
        *p++ = (unsigned char)c;
    }
    
    return s;
}

int memcmp(const void *s1, const void *s2, size_t n)
{
    const unsigned char *p1 = s1;
    const unsigned char *p2 = s2;
    
    while (n--) {
        if (*p1 != *p2) {
            return *p1 - *p2;
        }
        p1++;
        p2++;
    }
    
    return 0;
}

void *memchr(const void *s, int c, size_t n)
{
    const unsigned char *p = s;
    
    while (n--) {
        if (*p == (unsigned char)c) {
            return (void *)p;
        }
        p++;
    }
    
    return NULL;
}

