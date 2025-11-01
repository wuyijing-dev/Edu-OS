/*
 * strdup.c - POSIX字符串复制函数
 */

#ifndef NULL
#define NULL ((void*)0)
#endif

typedef unsigned int size_t;

extern void *malloc(size_t size);
extern size_t strlen(const char *s);
extern char *strcpy(char *dest, const char *src);

/*
 * strdup - 复制字符串（分配新内存）
 */
char *strdup(const char *s)
{
    if (!s) return NULL;
    
    size_t len = strlen(s) + 1;
    char *new_str = (char*)malloc(len);
    
    if (!new_str) return NULL;
    
    return strcpy(new_str, s);
}

/*
 * strndup - 复制最多n个字符的字符串
 */
char *strndup(const char *s, size_t n)
{
    if (!s) return NULL;
    
    size_t len = strlen(s);
    if (len > n) len = n;
    
    char *new_str = (char*)malloc(len + 1);
    if (!new_str) return NULL;
    
    for (size_t i = 0; i < len; i++) {
        new_str[i] = s[i];
    }
    new_str[len] = '\0';
    
    return new_str;
}

