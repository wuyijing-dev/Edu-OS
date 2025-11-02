/**
 * select() 和 poll() 系统调用实现
 * Linux风格事件驱动I/O
 */

#include <sys/select.h>
#include <sys/poll.h>
#include <process/process.h>
#include <fs/vfs.h>
#include <mm/uaccess.h>
#include <kernel.h>
#include <wait_queue.h>
#include <string.h>

/* 本地错误码定义 */
#define EINVAL  22
#define EBADF   9
#define EFAULT  14
#define EINTR   4

/**
 * select() 系统调用
 */
int sys_select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
{
    extern struct process *process_get_current(void);
    struct process *proc = process_get_current();
    
    if (!proc || !proc->fd_table) {
        return -EBADF;
    }
    
    /* 参数验证 */
    if (nfds < 0 || nfds > FD_SETSIZE) {
        return -EINVAL;
    }
    
    /* 验证用户空间指针 */
    if (readfds && !is_user_buffer(readfds, sizeof(fd_set))) {
        return -EFAULT;
    }
    if (writefds && !is_user_buffer(writefds, sizeof(fd_set))) {
        return -EFAULT;
    }
    if (exceptfds && !is_user_buffer(exceptfds, sizeof(fd_set))) {
        return -EFAULT;
    }
    if (timeout && !is_user_buffer(timeout, sizeof(struct timeval))) {
        return -EFAULT;
    }
    
    /* 复制fd_set到内核空间 */
    fd_set kernel_readfds, kernel_writefds, kernel_exceptfds;
    fd_set result_readfds, result_writefds, result_exceptfds;
    
    if (readfds) {
        copy_from_user(&kernel_readfds, readfds, sizeof(fd_set));
    } else {
        FD_ZERO(&kernel_readfds);
    }
    
    if (writefds) {
        copy_from_user(&kernel_writefds, writefds, sizeof(fd_set));
    } else {
        FD_ZERO(&kernel_writefds);
    }
    
    if (exceptfds) {
        copy_from_user(&kernel_exceptfds, exceptfds, sizeof(fd_set));
    } else {
        FD_ZERO(&kernel_exceptfds);
    }
    
    FD_ZERO(&result_readfds);
    FD_ZERO(&result_writefds);
    FD_ZERO(&result_exceptfds);
    
    /* 创建等待队列 */
    struct wait_queue_head wait;
    init_waitqueue_head(&wait);
    
    int ready_count = 0;
    bool first_pass = true;
    
    /* 轮询文件描述符 */
    while (1) {
        ready_count = 0;
        
        for (int fd = 0; fd < nfds; fd++) {
            struct vfs_file *file = proc->fd_table->files[fd];
            
            if (!file) {
                continue;
            }
            
            unsigned int mask = 0;
            
            /* 调用文件的poll函数 */
            if (file->f_op && file->f_op->poll) {
                /* 只在第一次传递等待队列 */
                mask = file->f_op->poll(file, first_pass ? &wait : NULL);
            } else {
                /* 如果没有poll函数，假设总是就绪 */
                mask = POLLIN | POLLOUT;
            }
            
            /* 检查读就绪 */
            if (FD_ISSET(fd, &kernel_readfds)) {
                if (mask & (POLLIN | POLLHUP | POLLERR)) {
                    FD_SET(fd, &result_readfds);
                    ready_count++;
                }
            }
            
            /* 检查写就绪 */
            if (FD_ISSET(fd, &kernel_writefds)) {
                if (mask & (POLLOUT | POLLERR)) {
                    FD_SET(fd, &result_writefds);
                    ready_count++;
                }
            }
            
            /* 检查异常 */
            if (FD_ISSET(fd, &kernel_exceptfds)) {
                if (mask & POLLPRI) {
                    FD_SET(fd, &result_exceptfds);
                    ready_count++;
                }
            }
        }
        
        first_pass = false;
        
        /* 如果有就绪的文件描述符，返回 */
        if (ready_count > 0) {
            break;
        }
        
        /* 如果设置了超时且为0，立即返回 */
        if (timeout) {
            struct timeval tv;
            copy_from_user(&tv, timeout, sizeof(struct timeval));
            if (tv.tv_sec == 0 && tv.tv_usec == 0) {
                break;
            }
        }
        
        /* 阻塞等待事件（简化版：不实现真正的超时）*/
        proc->state = PROCESS_STATE_BLOCKED;
        extern void scheduler_schedule(void);
        scheduler_schedule();
        
        /* 被唤醒后继续检查 */
    }
    
    /* 复制结果回用户空间 */
    if (readfds) {
        copy_to_user(readfds, &result_readfds, sizeof(fd_set));
    }
    if (writefds) {
        copy_to_user(writefds, &result_writefds, sizeof(fd_set));
    }
    if (exceptfds) {
        copy_to_user(exceptfds, &result_exceptfds, sizeof(fd_set));
    }
    
    return ready_count;
}

/**
 * poll() 系统调用
 */
int sys_poll(struct pollfd *fds, unsigned int nfds, int timeout)
{
    extern struct process *process_get_current(void);
    struct process *proc = process_get_current();
    
    if (!proc || !proc->fd_table) {
        return -EBADF;
    }
    
    /* 参数验证 */
    if (!fds || !is_user_buffer(fds, sizeof(struct pollfd) * nfds)) {
        return -EFAULT;
    }
    
    /* 复制pollfd数组到内核空间 */
    struct pollfd *kernel_fds = (struct pollfd *)kmalloc(sizeof(struct pollfd) * nfds);
    if (!kernel_fds) {
        return -ENOMEM;
    }
    
    copy_from_user(kernel_fds, fds, sizeof(struct pollfd) * nfds);
    
    /* 创建等待队列 */
    struct wait_queue_head wait;
    init_waitqueue_head(&wait);
    
    int ready_count = 0;
    bool first_pass = true;
    
    /* 轮询文件描述符 */
    while (1) {
        ready_count = 0;
        
        for (unsigned int i = 0; i < nfds; i++) {
            int fd = kernel_fds[i].fd;
            kernel_fds[i].revents = 0;
            
            if (fd < 0) {
                continue;
            }
            
            if (fd >= MAX_FILES_PER_PROCESS) {
                kernel_fds[i].revents = POLLNVAL;
                ready_count++;
                continue;
            }
            
            struct vfs_file *file = proc->fd_table->files[fd];
            
            if (!file) {
                kernel_fds[i].revents = POLLNVAL;
                ready_count++;
                continue;
            }
            
            unsigned int mask = 0;
            
            /* 调用文件的poll函数 */
            if (file->f_op && file->f_op->poll) {
                mask = file->f_op->poll(file, first_pass ? &wait : NULL);
            } else {
                /* 如果没有poll函数，假设总是就绪 */
                mask = POLLIN | POLLOUT;
            }
            
            /* 设置返回事件 */
            kernel_fds[i].revents = mask & (kernel_fds[i].events | POLLERR | POLLHUP);
            
            if (kernel_fds[i].revents) {
                ready_count++;
            }
        }
        
        first_pass = false;
        
        /* 如果有就绪的文件描述符，返回 */
        if (ready_count > 0) {
            break;
        }
        
        /* 如果超时为0，立即返回 */
        if (timeout == 0) {
            break;
        }
        
        /* 阻塞等待事件（简化版：不实现真正的超时）*/
        proc->state = PROCESS_STATE_BLOCKED;
        extern void scheduler_schedule(void);
        scheduler_schedule();
        
        /* 被唤醒后继续检查 */
    }
    
    /* 复制结果回用户空间 */
    copy_to_user(fds, kernel_fds, sizeof(struct pollfd) * nfds);
    
    kfree(kernel_fds);
    
    return ready_count;
}

