/*
 * fcntl.h - POSIX 文件控制选项
 */

#ifndef _FCNTL_H
#define _FCNTL_H

/* open() 标志位 */
#define O_RDONLY    0x0000  /* 只读 */
#define O_WRONLY    0x0001  /* 只写 */
#define O_RDWR      0x0002  /* 读写 */
#define O_CREAT     0x0040  /* 创建文件 */
#define O_EXCL      0x0080  /* 与O_CREAT一起使用，文件存在则失败 */
#define O_TRUNC     0x0200  /* 截断文件 */
#define O_APPEND    0x0400  /* 追加模式 */
#define O_NONBLOCK  0x0800  /* 非阻塞模式 */
#define O_DIRECTORY 0x10000 /* 必须是目录 */

/* fcntl() 命令 */
#define F_DUPFD     0   /* 复制文件描述符 */
#define F_GETFD     1   /* 获取文件描述符标志 */
#define F_SETFD     2   /* 设置文件描述符标志 */
#define F_GETFL     3   /* 获取文件状态标志 */
#define F_SETFL     4   /* 设置文件状态标志 */

/* 文件描述符标志 */
#define FD_CLOEXEC  1   /* 执行时关闭 */

/* 函数声明 */
int open(const char *path, int flags, ...);
int fcntl(int fd, int cmd, ...);
int creat(const char *path, int mode);

#endif /* _FCNTL_H */

