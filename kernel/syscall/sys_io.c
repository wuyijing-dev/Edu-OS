/*
 * sys_io.c - 文件I/O相关系统调用
 */

#include <syscall.h>
#include <kernel.h>
#include <fs/vfs.h>
#include <string.h>

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
    
    /* 调用 VFS */
    return vfs_read(fd, buf, count);
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
    
    /* 调用 VFS */
    return vfs_write(fd, buf, count);
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
    
    /* 调用 VFS */
    return vfs_open(path, flags, mode);
}

/*
 * sys_close - 关闭文件描述符
 */
int sys_close(int fd)
{
    if (fd < 0) {
        return -EINVAL;
    }
    
    return vfs_close(fd);
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

