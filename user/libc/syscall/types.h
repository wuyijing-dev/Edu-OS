/*
 * types.h - 用户空间类型定义
 * 
 * 提供标准C类型（不依赖系统头文件）
 */

#ifndef _USERLIB_TYPES_H
#define _USERLIB_TYPES_H

/* 基础整数类型 */
typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;

typedef signed char        int8_t;
typedef signed short       int16_t;
typedef signed int         int32_t;
typedef signed long long   int64_t;

/* 大小和指针类型 */
typedef unsigned int       size_t;
typedef int                ssize_t;
typedef int                ptrdiff_t;
typedef int                off_t;     /* 文件偏移 */

/* 布尔类型 */
typedef int                bool;
#define true               1
#define false              0

/* NULL定义 */
#ifndef NULL
#define NULL ((void*)0)
#endif

/* 字符类型 */
typedef int                wchar_t;

#endif /* _USERLIB_TYPES_H */
