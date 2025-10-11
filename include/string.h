#ifndef STRING_H
#define STRING_H

#include "types.h"

/*
 * =============================================================================
 * 字符串和内存操作函数
 * =============================================================================
 */

// 内存操作
void *memset(void *dest, int val, size_t count);
void *memcpy(void *dest, const void *src, size_t count);
void *memmove(void *dest, const void *src, size_t count);
int memcmp(const void *s1, const void *s2, size_t n);

// 字符串操作
size_t strlen(const char *str);
char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, size_t n);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
char *strcat(char *dest, const char *src);
char *strncat(char *dest, const char *src, size_t n);
char *strchr(const char *s, int c);
char *strrchr(const char *s, int c);

// FAT32 和 ProcFS 需要的额外函数
int strcasecmp(const char *s1, const char *s2);
char *strdup(const char *s);
char *strtok(char *str, const char *delim);

// 字符分类函数
int toupper(int c);
int tolower(int c);
int islower(int c);
int isupper(int c);
int isalpha(int c);
int isdigit(int c);
int isalnum(int c);
int isspace(int c);

#endif // STRING_H
