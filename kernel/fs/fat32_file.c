/*
 * fat32_file.c - FAT32 文件操作
 */

#include <fs/fat32.h>
#include <fs/vfs.h>
#include <kernel.h>
#include <string.h>
#include <mm/kmalloc.h>

/* 外部函数声明 */
extern struct fat32_fs_info *fat32_get_fs(void);

/* ========== VFS 文件操作 ========== */

/*
 * 打开文件
 */
static int fat32_file_open(struct vfs_inode *inode, struct vfs_file *file)
{
    (void)inode;
    file->pos = 0;
    return 0;
}

/*
 * 关闭文件
 */
static int fat32_file_release(struct vfs_file *file)
{
    /* 释放私有数据 */
    if (file->private_data) {
        kfree(file->private_data);
        file->private_data = NULL;
    }
    
    return 0;
}

/*
 * 读取文件
 */
static int fat32_file_read(struct vfs_file *file, char *buf, size_t count)
{
    struct fat32_dir_entry *entry = file->private_data;
    if (!entry) {
        return -EINVAL;
    }
    
    struct fat32_fs_info *fs = fat32_get_fs();
    if (!fs) {
        return -EINVAL;
    }
    
    /* 获取起始簇 */
    uint32_t cluster = fat32_get_first_cluster(entry);
    uint32_t offset = file->pos;
    uint32_t file_size = entry->file_size;
    
    /* 检查是否超出文件末尾 */
    if (offset >= file_size) {
        return 0;  // EOF
    }
    
    /* 调整读取大小 */
    if (offset + count > file_size) {
        count = file_size - offset;
    }
    
    /* 跳过开头的簇 */
    uint32_t cluster_offset = offset / fs->cluster_size;
    for (uint32_t i = 0; i < cluster_offset && !fat32_is_eoc(cluster); i++) {
        cluster = fat32_read_fat(fs, cluster);
    }
    
    /* 簇内偏移 */
    uint32_t offset_in_cluster = offset % fs->cluster_size;
    uint32_t bytes_read = 0;
    
    /* 分配簇缓冲区 */
    uint8_t *cluster_buf = kmalloc(fs->cluster_size);
    if (!cluster_buf) {
        return -ENOMEM;
    }
    
    /* 循环读取簇 */
    while (bytes_read < count && !fat32_is_eoc(cluster)) {
        /* 读取簇 */
        if (fat32_read_cluster(fs, cluster, cluster_buf) < 0) {
            kfree(cluster_buf);
            return -EIO;
        }
        
        /* 计算本次读取大小 */
        uint32_t to_read = fs->cluster_size - offset_in_cluster;
        if (to_read > count - bytes_read) {
            to_read = count - bytes_read;
        }
        
        /* 复制数据 */
        memcpy(buf + bytes_read, cluster_buf + offset_in_cluster, to_read);
        bytes_read += to_read;
        offset_in_cluster = 0;  // 后续簇从头开始
        
        /* 下一个簇 */
        cluster = fat32_read_fat(fs, cluster);
    }
    
    kfree(cluster_buf);
    
    /* 更新文件位置 */
    file->pos += bytes_read;
    
    return bytes_read;
}

/*
 * 写入文件
 */
static int fat32_file_write(struct vfs_file *file, const char *buf, size_t count)
{
    struct fat32_dir_entry *entry = file->private_data;
    if (!entry) {
        return -EINVAL;
    }
    
    struct fat32_fs_info *fs = fat32_get_fs();
    if (!fs) {
        return -EINVAL;
    }
    
    /* 获取起始簇 */
    uint32_t cluster = fat32_get_first_cluster(entry);
    uint32_t offset = file->pos;
    
    /* 如果是新文件，分配第一个簇 */
    if (cluster == 0) {
        cluster = fat32_alloc_cluster(fs);
        if (cluster == 0) {
            return -ENOSPC;  // 磁盘已满
        }
        
        fat32_set_first_cluster(entry, cluster);
    }
    
    /* 定位到 offset 所在的簇 */
    uint32_t cluster_offset = offset / fs->cluster_size;
    uint32_t prev_cluster = 0;
    
    for (uint32_t i = 0; i < cluster_offset && !fat32_is_eoc(cluster); i++) {
        prev_cluster = cluster;
        cluster = fat32_read_fat(fs, cluster);
    }
    
    /* 如果超出现有簇，分配新簇 */
    if (fat32_is_eoc(cluster) && cluster_offset > 0) {
        cluster = fat32_alloc_cluster(fs);
        if (cluster == 0) {
            return -ENOSPC;
        }
        
        /* 链接到前一个簇 */
        if (prev_cluster > 0) {
            fat32_write_fat(fs, prev_cluster, cluster);
        }
    }
    
    /* 簇内偏移 */
    uint32_t offset_in_cluster = offset % fs->cluster_size;
    uint32_t bytes_written = 0;
    
    /* 分配簇缓冲区 */
    uint8_t *cluster_buf = kmalloc(fs->cluster_size);
    if (!cluster_buf) {
        return -ENOMEM;
    }
    
    /* 循环写入簇 */
    while (bytes_written < count) {
        /* 读取现有簇内容（部分写入时需要） */
        if (offset_in_cluster > 0 || 
            (count - bytes_written) < (fs->cluster_size - offset_in_cluster)) {
            fat32_read_cluster(fs, cluster, cluster_buf);
        }
        
        /* 计算本次写入大小 */
        uint32_t to_write = fs->cluster_size - offset_in_cluster;
        if (to_write > count - bytes_written) {
            to_write = count - bytes_written;
        }
        
        /* 修改数据 */
        memcpy(cluster_buf + offset_in_cluster, buf + bytes_written, to_write);
        
        /* 写回簇 */
        if (fat32_write_cluster(fs, cluster, cluster_buf) < 0) {
            kfree(cluster_buf);
            return -EIO;
        }
        
        bytes_written += to_write;
        offset_in_cluster = 0;
        
        /* 需要新簇？ */
        if (bytes_written < count) {
            uint32_t next = fat32_read_fat(fs, cluster);
            
            if (fat32_is_eoc(next)) {
                /* 分配新簇 */
                next = fat32_alloc_cluster(fs);
                if (next == 0) {
                    break;  // 磁盘已满
                }
                
                /* 链接簇 */
                fat32_write_fat(fs, cluster, next);
            }
            
            cluster = next;
        }
    }
    
    kfree(cluster_buf);
    
    /* 更新文件大小 */
    if (file->pos + bytes_written > entry->file_size) {
        entry->file_size = file->pos + bytes_written;
        /* TODO: 更新磁盘上的目录项 */
    }
    
    /* 更新文件位置 */
    file->pos += bytes_written;
    
    return bytes_written;
}

/* ========== 文件创建和删除 ========== */

/*
 * 创建文件
 */
int fat32_create_file(
    struct fat32_fs_info *fs,
    const char *path,
    uint32_t mode)
{
    /* TODO: 实现文件创建 */
    (void)fs;
    (void)path;
    (void)mode;
    kprintf("[FAT32] create_file not yet implemented\n");
    return -ENOSYS;
}

/*
 * 删除文件
 */
int fat32_unlink(
    struct fat32_fs_info *fs,
    const char *path)
{
    /* TODO: 实现文件删除 */
    (void)fs;
    (void)path;
    kprintf("[FAT32] unlink not yet implemented\n");
    return -ENOSYS;
}

/* ========== VFS 操作表 ========== */

struct vfs_file_operations fat32_file_ops = {
    .open = fat32_file_open,
    .release = fat32_file_release,
    .read = fat32_file_read,
    .write = fat32_file_write,
};

