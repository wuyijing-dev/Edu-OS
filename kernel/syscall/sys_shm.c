/**
 * POSIX共享内存系统调用
 * shm_open() 和 shm_unlink()
 */

#include <sys/mman.h>
#include <ipc/shm.h>
#include <process/process.h>
#include <mm/uaccess.h>
#include <fs/vfs.h>  /* 包含O_CREAT等标志 */
#include <kernel.h>
#include <string.h>

/* 本地错误码定义 */
#define EINVAL  22
#define ENOENT  2
#define EEXIST  17
#define ENOMEM  12
#define EBADF   9
#define EFAULT  14
#define ENAMETOOLONG 36

/**
 * ftruncate() 系统调用
 * 设置文件或共享内存对象大小
 */
int sys_ftruncate(int fd, off_t length)
{
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
    
    /* 检查是否为共享内存对象 */
    if (file->private_data) {
        struct shm_object *obj = (struct shm_object *)file->private_data;
        
        if (length < 0) {
            return -EINVAL;
        }
        
        int ret = shm_truncate(obj, (size_t)length);
        if (ret < 0) {
            return -ENOMEM;
        }
        
        return 0;
    }
    
    /* 普通文件的ftruncate（暂不支持）*/
    return -EINVAL;
}

/**
 * shm_open() 系统调用
 * 创建或打开POSIX共享内存对象
 */
int sys_shm_open(const char *name, int oflag, mode_t mode)
{
    extern struct process *process_get_current(void);
    struct process *proc = process_get_current();
    
    if (!proc || !proc->fd_table) {
        return -EBADF;
    }
    
    /* 验证用户空间指针 */
    if (!name || !is_user_buffer(name, SHM_NAME_MAX)) {
        return -EFAULT;
    }
    
    /* 复制名称到内核空间 */
    char kernel_name[SHM_NAME_MAX];
    copy_from_user(kernel_name, name, SHM_NAME_MAX);
    kernel_name[SHM_NAME_MAX - 1] = '\0';
    
    /* 验证名称格式（必须以/开头）*/
    if (kernel_name[0] != '/') {
        return -EINVAL;
    }
    
    /* 检查名称长度 */
    size_t name_len = strlen(kernel_name);
    if (name_len >= SHM_NAME_MAX) {
        return -ENAMETOOLONG;
    }
    
    /* 创建或打开共享内存对象 */
    struct shm_object *obj = shm_create(kernel_name, oflag, mode);
    if (!obj) {
        if ((oflag & O_CREAT) && (oflag & O_EXCL)) {
            return -EEXIST;
        }
        return -ENOENT;
    }
    
    /* 创建VFS文件对象 */
    extern struct vfs_file *kmalloc(size_t size);
    struct vfs_file *file = (struct vfs_file *)kmalloc(sizeof(struct vfs_file));
    if (!file) {
        shm_put(obj);
        return -ENOMEM;
    }
    
    /* 设置文件属性 */
    memset(file, 0, sizeof(struct vfs_file));
    file->dentry = NULL;
    file->inode = NULL;
    file->f_op = NULL;  /* 共享内存对象不需要文件操作 */
    file->flags = oflag;
    file->pos = 0;
    file->ref_count = 1;
    file->private_data = obj;  /* 关联共享内存对象 */
    
    /* 分配文件描述符 */
    extern int fd_table_alloc(struct file_descriptor_table *table, struct vfs_file *file);
    int fd = fd_table_alloc(proc->fd_table, file);
    if (fd < 0) {
        extern void free_file(struct vfs_file *file);
        free_file(file);
        shm_put(obj);
        return -ENOMEM;
    }
    
    kprintf("[SHM] Process %d opened shared memory: %s (fd=%d)\n", 
            proc->pid, kernel_name, fd);
    
    return fd;
}

/**
 * shm_unlink() 系统调用
 * 删除POSIX共享内存对象
 */
int sys_shm_unlink(const char *name)
{
    /* 验证用户空间指针 */
    if (!name || !is_user_buffer(name, SHM_NAME_MAX)) {
        return -EFAULT;
    }
    
    /* 复制名称到内核空间 */
    char kernel_name[SHM_NAME_MAX];
    copy_from_user(kernel_name, name, SHM_NAME_MAX);
    kernel_name[SHM_NAME_MAX - 1] = '\0';
    
    /* 验证名称格式 */
    if (kernel_name[0] != '/') {
        return -EINVAL;
    }
    
    /* 删除共享内存对象 */
    int ret = shm_unlink_object(kernel_name);
    if (ret < 0) {
        return -ENOENT;
    }
    
    kprintf("[SHM] Unlinked shared memory: %s\n", kernel_name);
    
    return 0;
}

