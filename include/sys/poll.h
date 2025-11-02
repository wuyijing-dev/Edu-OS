/**
 * POSIX poll() 系统调用
 * 参考：Linux include/uapi/asm-generic/poll.h
 */

#ifndef _SYS_POLL_H
#define _SYS_POLL_H

#include <types.h>

/* poll事件标志 */
#define POLLIN      0x0001  /* 有数据可读 */
#define POLLPRI     0x0002  /* 有紧急数据可读 */
#define POLLOUT     0x0004  /* 写数据不会阻塞 */
#define POLLERR     0x0008  /* 错误条件 */
#define POLLHUP     0x0010  /* 挂断 */
#define POLLNVAL    0x0020  /* 无效的文件描述符 */
#define POLLRDNORM  0x0040  /* 普通数据可读 */
#define POLLRDBAND  0x0080  /* 优先级带数据可读 */
#define POLLWRNORM  0x0100  /* 普通数据可写 */
#define POLLWRBAND  0x0200  /* 优先级带数据可写 */

/* poll文件描述符结构 */
struct pollfd {
    int fd;         /* 文件描述符 */
    short events;   /* 请求的事件 */
    short revents;  /* 返回的事件 */
};

/* poll() 系统调用 */
#ifdef __KERNEL__
int sys_poll(struct pollfd *fds, unsigned int nfds, int timeout);
#else
int poll(struct pollfd *fds, unsigned int nfds, int timeout);
#endif

#endif /* _SYS_POLL_H */

