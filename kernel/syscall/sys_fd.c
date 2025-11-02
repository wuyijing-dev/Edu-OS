/*
 * sys_fd.c - 文件描述符操作系统调用
 * 
 * 包括 dup, dup2, fcntl 等POSIX标准调用
 */

#include <process/process.h>
#include <fs/vfs.h>
#include <kernel.h>

/* 外部函数声明 */
extern int fd_table_alloc(struct file_descriptor_table *table, struct vfs_file *file);
extern void fd_table_free(struct file_descriptor_table *table, int fd);
extern struct vfs_file *file_get(struct vfs_file *file);
extern void file_put(struct vfs_file *file);

/* errno错误码 */
#define EBADF   9
#define EINVAL  22
#define EMFILE  24

/* 最大文件描述符数（与process.h中定义一致） */
#define MAX_FDS MAX_FILES_PER_PROCESS

/*
 * sys_dup - 复制文件描述符
 * 
 * @oldfd: 要复制的文件描述符
 * 
 * 返回：新的文件描述符，-1表示失败
 * 
 * POSIX标准：dup()返回最小的未使用文件描述符
 */
int sys_dup(int oldfd)
{
    extern struct process *process_get_current(void);
    struct process *current = process_get_current();
    
    if (!current || !current->fd_table) {
        extern int set_errno(int error_code);
        return set_errno(EBADF);
    }
    
    /* 检查oldfd有效性 */
    if (oldfd < 0 || oldfd >= MAX_FDS || !current->fd_table->files[oldfd]) {
        extern int set_errno(int error_code);
        return set_errno(EBADF);
    }
    
    /* 获取文件对象 */
    struct vfs_file *file = current->fd_table->files[oldfd];
    
    /* 增加引用计数 */
    extern struct vfs_file *file_get(struct vfs_file *file);
    file_get(file);
    
    /* 分配新的文件描述符 */
    int newfd = fd_table_alloc(current->fd_table, file);
    if (newfd < 0) {
        extern void file_put(struct vfs_file *file);
        file_put(file);
        extern int set_errno(int error_code);
        return set_errno(EMFILE);
    }
    
    kprintf("[SYS_DUP] Duplicated fd %d to fd %d\n", oldfd, newfd);
    return newfd;
}

/*
 * sys_dup2 - 复制文件描述符到指定编号
 * 
 * @oldfd: 要复制的文件描述符
 * @newfd: 目标文件描述符编号
 * 
 * 返回：新的文件描述符（即newfd），-1表示失败
 * 
 * POSIX标准：
 * - 如果newfd已打开，先关闭它
 * - 如果oldfd==newfd，直接返回newfd
 */
int sys_dup2(int oldfd, int newfd)
{
    extern struct process *process_get_current(void);
    struct process *current = process_get_current();
    
    if (!current || !current->fd_table) {
        extern int set_errno(int error_code);
        return set_errno(EBADF);
    }
    
    /* 检查oldfd有效性 */
    if (oldfd < 0 || oldfd >= MAX_FDS || !current->fd_table->files[oldfd]) {
        extern int set_errno(int error_code);
        return set_errno(EBADF);
    }
    
    /* 检查newfd有效性 */
    if (newfd < 0 || newfd >= MAX_FDS) {
        extern int set_errno(int error_code);
        return set_errno(EBADF);
    }
    
    /* 特殊情况：oldfd == newfd */
    if (oldfd == newfd) {
        return newfd;
    }
    
    /* 如果newfd已打开，先关闭它 */
    if (current->fd_table->files[newfd]) {
        fd_table_free(current->fd_table, newfd);
    }
    
    /* 获取文件对象并增加引用计数 */
    struct vfs_file *file = current->fd_table->files[oldfd];
    extern struct vfs_file *file_get(struct vfs_file *file);
    file_get(file);
    
    /* 设置新的文件描述符 */
    current->fd_table->files[newfd] = file;
    
    kprintf("[SYS_DUP2] Duplicated fd %d to fd %d\n", oldfd, newfd);
    return newfd;
}

/*
 * sys_fcntl - 文件控制操作
 * 
 * @fd: 文件描述符
 * @cmd: 命令
 * @arg: 参数（可选）
 * 
 * 返回：取决于cmd，-1表示失败
 */
int sys_fcntl(int fd, int cmd, unsigned long arg)
{
    extern struct process *process_get_current(void);
    struct process *current = process_get_current();
    
    if (!current || !current->fd_table) {
        extern int set_errno(int error_code);
        return set_errno(EBADF);
    }
    
    /* 检查fd有效性 */
    if (fd < 0 || fd >= MAX_FDS || !current->fd_table->files[fd]) {
        extern int set_errno(int error_code);
        return set_errno(EBADF);
    }
    
    struct vfs_file *file = current->fd_table->files[fd];
    
    /* fcntl命令 */
    #define F_DUPFD     0   /* 复制文件描述符 */
    #define F_GETFD     1   /* 获取close-on-exec标志 */
    #define F_SETFD     2   /* 设置close-on-exec标志 */
    #define F_GETFL     3   /* 获取文件状态标志 */
    #define F_SETFL     4   /* 设置文件状态标志 */
    
    switch (cmd) {
        case F_DUPFD:
            /* 复制文件描述符（从arg开始查找） */
            {
                int newfd;
                for (newfd = (int)arg; newfd < MAX_FDS; newfd++) {
                    if (!current->fd_table->files[newfd]) {
                        extern struct vfs_file *file_get(struct vfs_file *file);
                        file_get(file);
                        current->fd_table->files[newfd] = file;
                        kprintf("[SYS_FCNTL] F_DUPFD: fd %d duplicated to fd %d\n", 
                                fd, newfd);
                        return newfd;
                    }
                }
                extern int set_errno(int error_code);
                return set_errno(EMFILE);
            }
            
        case F_GETFD:
            /* 获取close-on-exec标志（暂不支持，返回0） */
            kprintf("[SYS_FCNTL] F_GETFD: fd %d\n", fd);
            return 0;
            
        case F_SETFD:
            /* 设置close-on-exec标志（暂不支持） */
            kprintf("[SYS_FCNTL] F_SETFD: fd %d, arg=%lu\n", fd, arg);
            return 0;
            
        case F_GETFL:
            /* 获取文件状态标志 */
            kprintf("[SYS_FCNTL] F_GETFL: fd %d, flags=0x%x\n", fd, file->flags);
            return file->flags;
            
        case F_SETFL:
            /* 设置文件状态标志（只允许设置某些标志）*/
            /* O_APPEND, O_NONBLOCK, O_ASYNC等 */
            #define O_NONBLOCK  04000
            /* O_APPEND已在vfs.h中定义 */
            
            /* 只允许修改这些标志 */
            file->flags = (file->flags & ~(O_APPEND | O_NONBLOCK)) | 
                         (arg & (O_APPEND | O_NONBLOCK));
            kprintf("[SYS_FCNTL] F_SETFL: fd %d, new_flags=0x%x\n", 
                    fd, file->flags);
            return 0;
            
        default:
            kprintf("[SYS_FCNTL] Unknown command: %d\n", cmd);
            extern int set_errno(int error_code);
            return set_errno(EINVAL);
    }
}

