/* string.c - 字符串和内存操作函数 */

#include "string.h"
#include <mm/kmalloc.h>

void *memset(void *dest, int val, size_t count)
{
    unsigned char *d = (unsigned char *)dest;
    while (count--) *d++ = (unsigned char)val;
    return dest;
}

void *memcpy(void *dest, const void *src, size_t count)
{
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    while (count--) *d++ = *s++;
    return dest;
}

void *memmove(void *dest, const void *src, size_t count)
{
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    
    if (d < s) {
        while (count--) *d++ = *s++;
    } else {
        d += count;
        s += count;
        while (count--) *--d = *--s;
    }
    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n)
{
    const unsigned char *p1 = (const unsigned char *)s1;
    const unsigned char *p2 = (const unsigned char *)s2;
    
    while (n--) {
        if (*p1 != *p2) return *p1 - *p2;
        p1++;
        p2++;
    }
    return 0;
}

size_t strlen(const char *str)
{
    size_t len = 0;
    while (str[len]) len++;
    return len;
}

char *strcpy(char *dest, const char *src)
{
    char *d = dest;
    while ((*d++ = *src++))
        ;
    return dest;
}

char *strncpy(char *dest, const char *src, size_t n)
{
    size_t i;
    for (i = 0; i < n && src[i]; i++) {
        dest[i] = src[i];
    }
    for (; i < n; i++) {
        dest[i] = '\0';
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
    if (n == 0) return 0;
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

char *strcat(char *dest, const char *src)
{
    char *d = dest;
    while (*d) d++;
    while ((*d++ = *src++))
        ;
    return dest;
}

char *strncat(char *dest, const char *src, size_t n)
{
    char *d = dest;
    while (*d) d++;
    while (n-- && (*d++ = *src++))
        ;
    if (n == (size_t)-1) *d = '\0';
    return dest;
}

char *strchr(const char *s, int c)
{
    while (*s) {
        if (*s == (char)c) return (char *)s;
        s++;
    }
    if (c == '\0') return (char *)s;
    return NULL;
}

char *strrchr(const char *s, int c)
{
    const char *last = NULL;
    while (*s) {
        if (*s == (char)c) last = s;
        s++;
    }
    if (c == '\0') return (char *)s;
    return (char *)last;
}

/* FAT32 和 ProcFS 需要的额外函数 */

int strcasecmp(const char *s1, const char *s2)
{
    while (*s1 && *s2) {
        char c1 = (*s1 >= 'a' && *s1 <= 'z') ? (*s1 - 32) : *s1;
        char c2 = (*s2 >= 'a' && *s2 <= 'z') ? (*s2 - 32) : *s2;
        if (c1 != c2) {
            return c1 - c2;
        }
        s1++;
        s2++;
    }
    return *s1 - *s2;
}

char *strdup(const char *s)
{
    if (!s) return NULL;
    
    size_t len = strlen(s) + 1;
    char *new_str = (char*)kmalloc(len);
    if (!new_str) return NULL;
    
    memcpy(new_str, s, len);
    return new_str;
}

static char *strtok_saveptr = NULL;

char *strtok(char *str, const char *delim)
{
    if (str) {
        strtok_saveptr = str;
    }
    
    if (!strtok_saveptr) {
        return NULL;
    }
    
    /* 跳过前导分隔符 */
    while (*strtok_saveptr && strchr(delim, *strtok_saveptr)) {
        strtok_saveptr++;
    }
    
    if (*strtok_saveptr == '\0') {
        strtok_saveptr = NULL;
        return NULL;
    }
    
    /* 找到 token 的开始 */
    char *token_start = strtok_saveptr;
    
    /* 找到下一个分隔符 */
    while (*strtok_saveptr && !strchr(delim, *strtok_saveptr)) {
        strtok_saveptr++;
    }
    
    if (*strtok_saveptr) {
        *strtok_saveptr = '\0';
        strtok_saveptr++;
    } else {
        strtok_saveptr = NULL;
    }
    
    return token_start;
}

int toupper(int c)
{
    if (c >= 'a' && c <= 'z') {
        return c - 32;
    }
    return c;
}

int tolower(int c)
{
    if (c >= 'A' && c <= 'Z') {
        return c + 32;
    }
    return c;
}

int islower(int c)
{
    return (c >= 'a' && c <= 'z');
}

int isupper(int c)
{
    return (c >= 'A' && c <= 'Z');
}

int isalpha(int c)
{
    return islower(c) || isupper(c);
}

int isdigit(int c)
{
    return (c >= '0' && c <= '9');
}

int isalnum(int c)
{
    return isalpha(c) || isdigit(c);
}

int isspace(int c)
{
    return (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v');
}
