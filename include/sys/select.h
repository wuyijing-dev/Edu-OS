/**
 * POSIX select() 系统调用
 * 参考：Linux include/uapi/linux/time.h 和 sys/select.h
 */

#ifndef _SYS_SELECT_H
#define _SYS_SELECT_H

#include <types.h>
#include <sys/time.h>

/* 文件描述符集合大小 */
#define FD_SETSIZE 1024

/* 文件描述符集合 */
typedef struct {
    uint32_t fds_bits[FD_SETSIZE / 32];
} fd_set;

/* 文件描述符集合操作宏 */
#define FD_ZERO(set) \
    do { \
        for (int __i = 0; __i < (FD_SETSIZE / 32); __i++) { \
            (set)->fds_bits[__i] = 0; \
        } \
    } while (0)

#define FD_SET(fd, set) \
    do { \
        if ((fd) >= 0 && (fd) < FD_SETSIZE) { \
            (set)->fds_bits[(fd) / 32] |= (1U << ((fd) % 32)); \
        } \
    } while (0)

#define FD_CLR(fd, set) \
    do { \
        if ((fd) >= 0 && (fd) < FD_SETSIZE) { \
            (set)->fds_bits[(fd) / 32] &= ~(1U << ((fd) % 32)); \
        } \
    } while (0)

#define FD_ISSET(fd, set) \
    (((fd) >= 0 && (fd) < FD_SETSIZE) ? \
     ((set)->fds_bits[(fd) / 32] & (1U << ((fd) % 32))) != 0 : 0)

/* select() 系统调用 */
#ifdef __KERNEL__
int sys_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
#else
int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
#endif

#endif /* _SYS_SELECT_H */

