/*
 * fat32_vfs.c - FAT32 与 VFS 的集成
 */

#include <fs/fat32.h>
#include <fs/vfs.h>
#include <kernel.h>
#include <string.h>
#include <mm/kmalloc.h>

/* 外部函数 */
extern struct fat32_fs_info *fat32_get_fs(void);
extern struct fat32_dir_entry *fat32_lookup(struct fat32_fs_info *fs, const char *path);
extern int fat32_create_file(struct fat32_fs_info *fs, const char *path, uint32_t mode);
extern int fat32_mkdir(struct fat32_fs_info *fs, const char *path);

/* ========== VFS Inode 操作 ========== */

/*
 * FAT32 lookup
 */
static struct vfs_dentry *fat32_vfs_lookup(struct vfs_inode *dir, const char *name)
{
    struct fat32_fs_info *fs = fat32_get_fs();
    if (!fs) {
        return NULL;
    }
    
    /* 构造完整路径 */
    char path[FAT32_MAX_PATH];
    snprintf(path, sizeof(path), "/%s", name);
    
    /* 查找文件 */
    struct fat32_dir_entry *entry = fat32_lookup(fs, path);
    if (!entry) {
        return NULL;
    }
    
    /* 创建 VFS inode */
    uint32_t mode = S_IFREG | 0644;
    if (entry->attr & FAT_ATTR_DIRECTORY) {
        mode = S_IFDIR | 0755;
    }
    
    struct vfs_inode *inode = vfs_alloc_inode(dir->sb, 0);
    if (!inode) {
        kfree(entry);
        return NULL;
    }
    
    inode->mode = mode;
    inode->size = entry->file_size;
    inode->private_data = entry;  /* 保存 FAT32 目录项 */
    
    /* 创建 dentry */
    struct vfs_dentry *dentry = vfs_alloc_dentry(name, inode);
    return dentry;
}

/*
 * FAT32 create
 */
static int fat32_vfs_create(struct vfs_inode *dir, const char *name, uint32_t mode)
{
    struct fat32_fs_info *fs = fat32_get_fs();
    if (!fs) {
        return -EINVAL;
    }
    
    /* 构造路径 */
    char path[FAT32_MAX_PATH];
    snprintf(path, sizeof(path), "/%s", name);
    
    return fat32_create_file(fs, path, mode);
}

/*
 * FAT32 mkdir
 */
static int fat32_vfs_mkdir(struct vfs_inode *dir, const char *name, uint32_t mode)
{
    struct fat32_fs_info *fs = fat32_get_fs();
    if (!fs) {
        return -EINVAL;
    }
    
    /* 构造路径 */
    char path[FAT32_MAX_PATH];
    snprintf(path, sizeof(path), "/%s", name);
    
    return fat32_mkdir(fs, path);
}

/* VFS Inode 操作表 */
static struct vfs_inode_operations fat32_inode_ops = {
    .lookup = fat32_vfs_lookup,
    .create = fat32_vfs_create,
    .mkdir = fat32_vfs_mkdir,
};

/* ========== 导出操作表 ========== */

struct vfs_inode_operations *fat32_get_inode_ops(void)
{
    return &fat32_inode_ops;
}

