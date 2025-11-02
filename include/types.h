#ifndef _TYPES_H
#define _TYPES_H

/* 基本整数类型 */
typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;

typedef signed char        int8_t;
typedef signed short       int16_t;
typedef signed int         int32_t;
typedef signed long long   int64_t;

/* 大小和指针类型 */
typedef uint32_t size_t;
typedef int32_t  ssize_t;
typedef uint32_t uintptr_t;
typedef int32_t  intptr_t;

/* 文件系统相关类型 */
typedef int32_t  off_t;      /* 文件偏移量 */
typedef uint32_t mode_t;     /* 文件权限模式 */
typedef uint32_t pid_t;      /* 进程 ID */
typedef uint32_t uid_t;      /* 用户 ID */
typedef uint32_t gid_t;      /* 组 ID */
typedef uint32_t dev_t;      /* 设备号 */
typedef uint32_t ino_t;      /* inode号 */
typedef uint32_t nlink_t;    /* 硬链接数 */
typedef int32_t  blksize_t;  /* 块大小 */
typedef int32_t  blkcnt_t;   /* 块数量 */
typedef uint32_t time_t;     /* 时间（秒）*/
typedef int32_t  suseconds_t;/* 微秒 */
typedef int32_t  clockid_t;  /* 时钟ID */

/* 布尔类型 */
typedef int bool;
#define true  1
#define false 0

/* NULL指针 */
#ifndef NULL
#define NULL ((void*)0)
#endif

/* offsetof宏 - 获取结构体成员的偏移量 */
#ifndef offsetof
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif

#endif /* _TYPES_H */

