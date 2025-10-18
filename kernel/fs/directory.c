/*
 * directory.c - 目录操作实现
 * 
 * 实现 opendir/readdir/closedir
 */

#include <fs/vfs.h>
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
    
    /* 获取文件对象 */
    extern struct vfs_file *vfs_state_open_files[64];
    /* TODO: 从 VFS 获取 file 对象 */
    
    /* 对于 FAT32，调用 fat32_readdir */
    struct fat32_fs_info *fs = fat32_get_fs();
    if (!fs) {
        return -EINVAL;
    }
    
    /* 使用 fd 作为 index（简化）*/
    char filename[256];
    void *fat_entry = kmalloc(32);  /* FAT 目录项 32字节 */
    
    int ret = fat32_readdir(fs, fs->root_cluster, fd, filename, fat_entry);
    if (ret <= 0) {
        kfree(fat_entry);
        return ret;
    }
    
    /* 填充 dirent */
    entry->d_ino = fd;
    entry->d_off = fd;
    entry->d_reclen = sizeof(struct dirent);
    entry->d_type = DT_REG;  /* 简化：默认为文件 */
    strncpy(entry->d_name, filename, sizeof(entry->d_name) - 1);
    
    kfree(fat_entry);
    return 1;
}

/*
 * sys_closedir - 关闭目录
 */
int sys_closedir(int fd)
{
    return vfs_close(fd);
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
    
    /* 调用 FAT32 mkdir */
    struct fat32_fs_info *fs = fat32_get_fs();
    if (!fs) {
        return -EINVAL;
    }
    
    return fat32_mkdir(fs, path);
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
    
    /* 调用 FAT32 rmdir */
    struct fat32_fs_info *fs = fat32_get_fs();
    if (!fs) {
        return -EINVAL;
    }
    
    /* TODO: 检查目录是否为空 */
    
    return fat32_rmdir(fs, path);
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
    
    /* 验证目录存在 */
    struct fat32_fs_info *fs = fat32_get_fs();
    if (!fs) {
        return -EINVAL;
    }
    
    struct fat32_dir_entry *entry = fat32_lookup(fs, path);
    if (!entry) {
        return -ENOENT;  /* 目录不存在 */
    }
    
    /* 检查是否是目录 */
    extern uint8_t FAT_ATTR_DIRECTORY;
    if (!(entry->attr & FAT_ATTR_DIRECTORY)) {
        kfree(entry);
        return -ENOTDIR;  /* 不是目录 */
    }
    
    kfree(entry);
    
    /* TODO: 更新进程 PCB 中的 cwd */
    /* 现在简化处理 */
    
    return 0;
}
