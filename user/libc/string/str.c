/*
 * str.c - POSIX string.h: strlen, strcpy, strcmp等
 */

#include "string.h"

#ifndef NULL
#define NULL ((void*)0)
#endif

size_t strlen(const char *s)
{
    size_t len = 0;
    while (*s++) {
        len++;
    }
    return len;
}

char *strcpy(char *dest, const char *src)
{
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

char *strncpy(char *dest, const char *src, size_t n)
{
    size_t i;
    
    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    
    for (; i < n; i++) {
        dest[i] = '\0';
    }
    
    return dest;
}

char *strcat(char *dest, const char *src)
{
    char *d = dest;
    
    while (*d) d++;
    while ((*d++ = *src++));
    
    return dest;
}

char *strncat(char *dest, const char *src, size_t n)
{
    char *d = dest;
    
    while (*d) d++;
    
    while (n-- && (*d++ = *src++));
    
    if (n == 0) {
        *d = '\0';
    }
    
    return dest;
}

int strcmp(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

int strncmp(const char *s1, const char *s2, size_t n)
{
    while (n && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    
    if (n == 0) {
        return 0;
    }
    
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

char *strchr(const char *s, int c)
{
    while (*s) {
        if (*s == (char)c) {
            return (char *)s;
        }
        s++;
    }
    
    return (c == '\0') ? (char *)s : NULL;
}

char *strrchr(const char *s, int c)
{
    const char *last = NULL;
    
    while (*s) {
        if (*s == (char)c) {
            last = s;
        }
        s++;
    }
    
    if (c == '\0') {
        return (char *)s;
    }
    
    return (char *)last;
}

char *strstr(const char *haystack, const char *needle)
{
    if (*needle == '\0') {
        return (char *)haystack;
    }
    
    while (*haystack) {
        const char *h = haystack;
        const char *n = needle;
        
        while (*h && *n && (*h == *n)) {
            h++;
            n++;
        }
        
        if (*n == '\0') {
            return (char *)haystack;
        }
        
        haystack++;
    }
    
    return NULL;
}

char *strtok(char *str, const char *delim)
{
    static char *saved = NULL;
    
    if (str) {
        saved = str;
    }
    
    if (!saved) {
        return NULL;
    }
    
    /* 跳过前导分隔符 */
    while (*saved && strchr(delim, *saved)) {
        saved++;
    }
    
    if (*saved == '\0') {
        saved = NULL;
        return NULL;
    }
    
    char *token = saved;
    
    /* 查找下一个分隔符 */
    while (*saved && !strchr(delim, *saved)) {
        saved++;
    }
    
    if (*saved) {
        *saved = '\0';
        saved++;
    } else {
        saved = NULL;
    }
    
    return token;
}

