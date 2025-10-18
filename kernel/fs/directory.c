/*
 * directory.c - 目录操作实现
 * 
 * 实现 opendir/readdir/closedir
 */

#include <fs/vfs.h>
#include <fs/fat32.h>
#include <process/process.h>
#include <kernel.h>
#include <mm/kmalloc.h>
#include <string.h>

/* 目录流结构 */
struct dir_stream {
    int fd;              /* 关联的文件描述符 */
    uint32_t position;   /* 当前读取位置 */
    void *private_data;  /* 文件系统私有数据 */
};

/* 目录项结构（用户空间可见） */
struct dirent {
    uint32_t d_ino;          /* Inode 号 */
    uint32_t d_off;          /* 偏移量 */
    uint16_t d_reclen;       /* 记录长度 */
    uint8_t  d_type;         /* 文件类型 */
    char     d_name[256];    /* 文件名 */
};

/* 文件类型常量 */
#define DT_UNKNOWN  0
#define DT_REG      8   /* 普通文件 */
#define DT_DIR      4   /* 目录 */
#define DT_CHR      2   /* 字符设备 */
#define DT_BLK      6   /* 块设备 */

/*
 * sys_opendir - 打开目录
 */
int sys_opendir(const char *path)
{
    if (!path) {
        return -EINVAL;
    }
    
    kprintf("[DIR] Opening directory: %s\n", path);
    
    /* 使用普通文件open，但检查是否是目录 */
    int fd = vfs_open(path, O_RDONLY, 0);
    if (fd < 0) {
        return fd;
    }
    
    /* TODO: 验证是目录 */
    
    return fd;
}

/*
 * sys_readdir - 读取目录项
 */
int sys_readdir(int fd, struct dirent *entry)
{
    if (fd < 0 || !entry) {
        return -EINVAL;
    }
    
    /* 简化实现：暂时返回空目录 */
    /* TODO: 实现完整的目录读取功能 */
    (void)fd;
    (void)entry;
    
    return 0;  /* 表示目录已读完 */
}

/*
 * sys_closedir - 关闭目录
 */
int sys_closedir(int fd)
{
    return vfs_close(fd);
}

/* VFS 包装函数（供内核代码使用）*/
int vfs_opendir(const char *path)
{
    return sys_opendir(path);
}

int vfs_readdir(int fd, void *entry)
{
    return sys_readdir(fd, (struct dirent*)entry);
}

int vfs_closedir(int fd)
{
    return sys_closedir(fd);
}

/*
 * sys_mkdir - 创建目录
 */
int sys_mkdir(const char *path, mode_t mode)
{
    (void)mode;
    
    if (!path) {
        return -EINVAL;
    }
    
    kprintf("[DIR] Creating directory: %s\n", path);
    
    /* TODO: 实现FAT32 mkdir */
    return -ENOSYS;
}

/*
 * sys_rmdir - 删除目录
 */
int sys_rmdir(const char *path)
{
    if (!path) {
        return -EINVAL;
    }
    
    kprintf("[DIR] Removing directory: %s\n", path);
    
    /* TODO: 实现FAT32 rmdir */
    return -ENOSYS;
}

/*
 * sys_getcwd - 获取当前工作目录
 */
char *sys_getcwd(char *buf, size_t size)
{
    if (!buf || size == 0) {
        return NULL;
    }
    
    /* 从进程 PCB 中获取 cwd */
    struct process *proc = process_get_current();
    if (!proc) {
        strncpy(buf, "/", size);
        return buf;
    }
    
    /* TODO: 在 PCB 中添加 cwd 字段 */
    /* 现在默认返回根目录 */
    strncpy(buf, "/", size);
    
    return buf;
}

/*
 * sys_chdir - 改变当前工作目录
 */
int sys_chdir(const char *path)
{
    if (!path) {
        return -EINVAL;
    }
    
    kprintf("[DIR] Changing directory to: %s\n", path);
    
    /* TODO: 验证目录存在并更新PCB */
    return -ENOSYS;
}
