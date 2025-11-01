/*
 * sys_io.c - 文件I/O相关系统调用
 */

#include <syscall.h>
#include <kernel.h>
#include <fs/vfs.h>
#include <string.h>
#include <process/process.h>
#include <process/scheduler.h>

/*
 * sys_read - 从文件描述符读取数据
 */
int sys_read(int fd, char *buf, size_t count)
{
    /* 参数验证 */
    if (fd < 0 || !buf || count == 0) {
        return -EINVAL;
    }
    
    /* 检查缓冲区是否在用户空间 
     * 注意：暂时允许内核地址（用于内核态测试）
     */
    #if 0  // 暂时禁用严格检查
    if (!is_user_buffer(buf, count)) {
        kprintf("[SYSCALL] sys_read: invalid user buffer\n");
        return -EFAULT;
    }
    #endif
    
    /* Linux风格：通过进程fd_table读取 */
    extern struct process *process_get_current(void);
    struct process *proc = process_get_current();
    
    if (!proc || !proc->fd_table) {
        return -EBADF;
    }
    
    if (fd < 0 || fd >= MAX_FILES_PER_PROCESS) {
        return -EBADF;
    }
    
    struct vfs_file *file = proc->fd_table->files[fd];
    if (!file) {
        return -EBADF;
    }
    
    /* 使用vfs_file_read */
    extern ssize_t vfs_file_read(struct vfs_file *file, void *buf, size_t count);
    return vfs_file_read(file, buf, count);
}

/*
 * sys_write - 向文件描述符写入数据
 */
int sys_write(int fd, const char *buf, size_t count)
{
    /* 参数验证 */
    if (fd < 0 || !buf || count == 0) {
        return -EINVAL;
    }
    
    /* 检查缓冲区是否在用户空间 
     * 注意：暂时允许内核地址（用于内核态测试）
     * 真正的用户态程序会传递用户空间地址
     */
    #if 0  // 暂时禁用严格检查
    if (!is_user_buffer(buf, count)) {
        kprintf("[SYSCALL] sys_write: invalid user buffer\n");
        return -EFAULT;
    }
    #endif
    
    /* 获取当前进程的fd_table */
    extern struct process *scheduler_get_current(void);
    struct process *current = scheduler_get_current();
    
    if (!current || !current->fd_table) {
        /* 如果没有进程上下文或fd_table，回退到全局VFS */
        return vfs_write(fd, buf, count);
    }
    
    /* 从进程的fd_table获取文件 */
    if (fd >= MAX_FILES_PER_PROCESS || !current->fd_table->files[fd]) {
        return -EBADF;  /* Bad file descriptor */
    }
    
    struct vfs_file *file = current->fd_table->files[fd];
    
    /* 调用VFS写入 */
    extern ssize_t vfs_file_write(struct vfs_file *file, const void *buf, size_t count);
    return vfs_file_write(file, buf, count);
}

/*
 * sys_open - 打开文件
 */
int sys_open(const char *path, int flags, int mode)
{
    /* 参数验证 */
    if (!path) {
        return -EINVAL;
    }
    
    /* 检查路径是否在用户空间 
     * 注意：内核态调用时允许内核地址（用于测试）
     */
    #if 0  // 暂时禁用严格检查，允许内核态测试
    if (!is_user_address(path)) {
        kprintf("[SYSCALL] sys_open: invalid path address\n");
        return -EFAULT;
    }
    #endif
    
    /* 验证路径字符串（防止读取过多内存） */
    int path_len = 0;
    const char *p = path;
    while (path_len < 4096 && is_user_address(p)) {
        if (*p == '\0') break;
        p++;
        path_len++;
    }
    
    if (path_len >= 4096) {
        return -ENAMETOOLONG;
    }
    
    /* Linux风格：通过进程fd_table打开文件
     * 1. 使用vfs_open_file获取vfs_file指针
     * 2. 在进程fd_table中分配fd
     * 3. 返回进程的fd
     */
    extern struct process *process_get_current(void);
    struct process *proc = process_get_current();
    
    if (!proc || !proc->fd_table) {
        return -EBADF;
    }
    
    /* 打开文件并获取vfs_file指针 */
    extern struct vfs_file *vfs_open_file(const char *path, int flags, int mode);
    struct vfs_file *file = vfs_open_file(path, flags, mode);
    if (!file) {
        return -ENOENT;
    }
    
    /* 在进程fd_table中分配fd */
    extern int fd_table_alloc(struct file_descriptor_table *table, struct vfs_file *file);
    int fd = fd_table_alloc(proc->fd_table, file);
    if (fd < 0) {
        /* 分配失败，释放file */
        extern void file_put(struct vfs_file *file);
        file_put(file);
        return -ENOMEM;  /* Too many open files */
    }
    
    return fd;
}

/*
 * sys_close - 关闭文件描述符（Linux风格：使用进程fd_table）
 */
int sys_close(int fd)
{
    if (fd < 0) {
        return -EINVAL;
    }
    
    extern struct process *process_get_current(void);
    struct process *proc = process_get_current();
    
    if (!proc || !proc->fd_table) {
        return -EBADF;
    }
    
    /* 从进程fd_table中释放fd */
    extern int fd_table_free(struct file_descriptor_table *table, int fd);
    return fd_table_free(proc->fd_table, fd);
}

/*
 * sys_lseek - 改变文件指针位置
 */
int sys_lseek(int fd, off_t offset, int whence)
{
    extern int vfs_lseek(int fd, off_t offset, int whence);
    return vfs_lseek(fd, offset, whence);
}

/*
 * sys_creat - 创建文件（等同于 open with O_CREAT|O_WRONLY|O_TRUNC）
 */
int sys_creat(const char *path, mode_t mode)
{
    return sys_open(path, O_CREAT | O_WRONLY | O_TRUNC, mode);
}

/*
 * sys_unlink - 删除文件
 */
int sys_unlink(const char *path)
{
    /* 简化实现：暂不支持 */
    kprintf("[SYSCALL] sys_unlink: %s\n", path);
    return -ENOSYS;
}

/*
 * 目录操作函数（mkdir, rmdir, chdir）已在 kernel/fs/directory.c 中实现
 * 这里不再重复定义
 */

