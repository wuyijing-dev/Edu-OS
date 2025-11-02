/*
 * sys_stat.c - 文件状态查询系统调用
 * 
 * 实现stat/fstat等POSIX标准调用
 */

#include <process/process.h>
#include <fs/vfs.h>
#include <sys/stat.h>
#include <kernel.h>
#include <string.h>

/* errno错误码 */
#define EBADF   9
#define EINVAL  22
#define ENOENT  2
#define EFAULT  14

/*
 * vfs_inode_to_stat - 将VFS inode转换为stat结构
 */
static void vfs_inode_to_stat(struct vfs_inode *inode, struct stat *st)
{
    memset(st, 0, sizeof(struct stat));
    
    if (!inode) return;
    
    st->st_ino = inode->ino;
    st->st_mode = inode->mode;
    st->st_nlink = 1;  /* 简化：硬链接数固定为1 */
    st->st_uid = 0;    /* 简化：所有者为root */
    st->st_gid = 0;    /* 简化：组为root */
    st->st_size = inode->size;
    st->st_atime = 0;  /* 简化：暂不跟踪访问时间 */
    st->st_mtime = 0;  /* 简化：暂不跟踪修改时间 */
    st->st_ctime = 0;  /* 简化：暂不跟踪创建时间 */
    st->st_rdev = inode->rdev;
    st->st_dev = 0;    /* 简化：设备ID为0 */
    
    /* 计算块数（假设块大小为512字节）*/
    st->st_blksize = 512;
    st->st_blocks = (inode->size + 511) / 512;
}

/*
 * sys_stat - 获取文件状态
 * 
 * @path: 文件路径
 * @buf: stat结构指针
 * 
 * 返回：0=成功，-1=失败
 */
int sys_stat(const char *path, struct stat *buf)
{
    if (!path || !buf) {
        extern int set_errno(int error_code);
        return set_errno(EFAULT);
    }
    
    /* 直接查找dentry（避免打开文件）*/
    extern struct vfs_dentry *vfs_lookup(const char *path);
    struct vfs_dentry *dentry = vfs_lookup(path);
    if (!dentry || !dentry->inode) {
        extern int set_errno(int error_code);
        return set_errno(ENOENT);
    }
    
    /* 转换为stat结构 */
    vfs_inode_to_stat(dentry->inode, buf);
    
    kprintf("[SYS_STAT] path=%s, size=%u, mode=0x%x\n", 
            path, buf->st_size, buf->st_mode);
    
    return 0;
}

/*
 * sys_fstat - 通过文件描述符获取文件状态
 * 
 * @fd: 文件描述符
 * @buf: stat结构指针
 * 
 * 返回：0=成功，-1=失败
 */
int sys_fstat(int fd, struct stat *buf)
{
    extern struct process *process_get_current(void);
    struct process *current = process_get_current();
    
    if (!current || !current->fd_table || !buf) {
        extern int set_errno(int error_code);
        return set_errno(EFAULT);
    }
    
    /* 检查fd有效性 */
    if (fd < 0 || fd >= MAX_FILES_PER_PROCESS || !current->fd_table->files[fd]) {
        extern int set_errno(int error_code);
        return set_errno(EBADF);
    }
    
    struct vfs_file *file = current->fd_table->files[fd];
    if (!file->inode) {
        extern int set_errno(int error_code);
        return set_errno(EBADF);
    }
    
    /* 转换为stat结构 */
    vfs_inode_to_stat(file->inode, buf);
    
    kprintf("[SYS_FSTAT] fd=%d, size=%u, mode=0x%x\n", 
            fd, buf->st_size, buf->st_mode);
    
    return 0;
}

/*
 * sys_lstat - 获取符号链接本身的状态
 * 
 * 注意：EduOS暂不支持符号链接，lstat等同于stat
 */
int sys_lstat(const char *path, struct stat *buf)
{
    return sys_stat(path, buf);
}

