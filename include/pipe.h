/*
 * pipe.h - POSIX管道定义
 * 
 * 符合POSIX.1-2017标准
 */

#ifndef _PIPE_H
#define _PIPE_H

#include <types.h>

/* 管道缓冲区大小（Linux: 64KB，简化版：4KB） */
#define PIPE_BUF    4096

/* 管道结构 */
struct pipe {
    char buffer[PIPE_BUF];      /* 环形缓冲区 */
    uint32_t read_pos;          /* 读位置 */
    uint32_t write_pos;         /* 写位置 */
    uint32_t size;              /* 当前数据量 */
    
    uint32_t readers;           /* 读端引用计数 */
    uint32_t writers;           /* 写端引用计数 */
    
    /* 同步原语（简化版，暂不实现） */
    // struct wait_queue read_wait;
    // struct wait_queue write_wait;
    // spinlock_t lock;
};

/* 管道操作函数 */
struct pipe *pipe_create(void);
void pipe_destroy(struct pipe *pipe);
ssize_t pipe_read(struct pipe *pipe, char *buf, size_t count);
ssize_t pipe_write(struct pipe *pipe, const char *buf, size_t count);
void pipe_close_read(struct pipe *pipe);
void pipe_close_write(struct pipe *pipe);

/* 系统调用 */
int sys_pipe(int pipefd[2]);

#endif /* _PIPE_H */

