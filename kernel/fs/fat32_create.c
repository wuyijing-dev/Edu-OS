/*
 * fat32_create.c - FAT32 文件和目录创建
 */

#include <fs/fat32.h>
#include <kernel.h>
#include <string.h>
#include <mm/kmalloc.h>
#include <drivers/timer.h>

/* 外部函数 */
extern struct fat32_fs_info *fat32_get_fs(void);
extern void fat32_generate_short_name(const char *long_name, char *short_name_out, int tail_num);

/*
 * 获取当前 FAT 时间
 */
static uint16_t fat32_get_current_time(void)
{
    /* 简化：固定时间 12:00:00 */
    uint16_t hours = 12;
    uint16_t minutes = 0;
    uint16_t seconds = 0;
    
    return (hours << 11) | (minutes << 5) | (seconds / 2);
}

/*
 * 获取当前 FAT 日期
 */
static uint16_t fat32_get_current_date(void)
{
    /* 简化：固定日期 2024-10-11 */
    uint16_t year = 2024 - 1980;  /* FAT 从 1980 年开始 */
    uint16_t month = 10;
    uint16_t day = 11;
    
    return (year << 9) | (month << 5) | day;
}

/*
 * 在目录中找到空闲目录项位置
 * 
 * @return: 簇号和簇内偏移（通过指针返回）
 */
static int fat32_find_free_dir_entry(
    struct fat32_fs_info *fs,
    uint32_t dir_cluster,
    uint32_t *cluster_out,
    uint32_t *offset_out)
{
    uint32_t cluster = dir_cluster;
    uint8_t *cluster_buf = kmalloc(fs->cluster_size);
    
    if (!cluster_buf) {
        return -ENOMEM;
    }
    
    /* 遍历簇链 */
    while (cluster >= 2 && !fat32_is_eoc(cluster)) {
        /* 读取簇 */
        if (fat32_read_cluster(fs, cluster, cluster_buf) < 0) {
            kfree(cluster_buf);
            return -EIO;
        }
        
        /* 查找空闲项 */
        int entries_per_cluster = fs->cluster_size / sizeof(struct fat32_dir_entry);
        
        for (int i = 0; i < entries_per_cluster; i++) {
            struct fat32_dir_entry *entry = 
                (struct fat32_dir_entry*)(cluster_buf + i * sizeof(struct fat32_dir_entry));
            
            /* 空闲或已删除 */
            if (entry->name[0] == FAT_DIR_ENTRY_END || 
                entry->name[0] == FAT_DIR_ENTRY_FREE) {
                *cluster_out = cluster;
                *offset_out = i * sizeof(struct fat32_dir_entry);
                kfree(cluster_buf);
                return 0;
            }
        }
        
        /* 下一个簇 */
        cluster = fat32_read_fat(fs, cluster);
    }
    
    /* 没有空闲项，需要分配新簇 */
    uint32_t new_cluster = fat32_alloc_cluster(fs);
    if (new_cluster == 0) {
        kfree(cluster_buf);
        return -ENOSPC;
    }
    
    /* 链接到目录 */
    if (!fat32_is_eoc(cluster)) {
        /* 找到最后一个簇 */
        uint32_t last = dir_cluster;
        while (!fat32_is_eoc(fat32_read_fat(fs, last))) {
            last = fat32_read_fat(fs, last);
        }
        fat32_write_fat(fs, last, new_cluster);
    }
    
    /* 清空新簇 */
    memset(cluster_buf, 0, fs->cluster_size);
    fat32_write_cluster(fs, new_cluster, cluster_buf);
    
    *cluster_out = new_cluster;
    *offset_out = 0;
    
    kfree(cluster_buf);
    return 0;
}

/*
 * 写入目录项到磁盘
 */
static int fat32_write_dir_entry(
    struct fat32_fs_info *fs,
    uint32_t cluster,
    uint32_t offset,
    struct fat32_dir_entry *entry)
{
    uint8_t *cluster_buf = kmalloc(fs->cluster_size);
    if (!cluster_buf) {
        return -ENOMEM;
    }
    
    /* 读取簇 */
    if (fat32_read_cluster(fs, cluster, cluster_buf) < 0) {
        kfree(cluster_buf);
        return -EIO;
    }
    
    /* 修改目录项 */
    memcpy(cluster_buf + offset, entry, sizeof(struct fat32_dir_entry));
    
    /* 写回 */
    if (fat32_write_cluster(fs, cluster, cluster_buf) < 0) {
        kfree(cluster_buf);
        return -EIO;
    }
    
    kfree(cluster_buf);
    return 0;
}

/*
 * 创建文件
 */
int fat32_create_file(
    struct fat32_fs_info *fs,
    const char *path,
    uint32_t mode)
{
    (void)mode;
    
    if (!fs || !path) {
        return -EINVAL;
    }
    
    kprintf("[FAT32] Creating file: %s\n", path);
    
    /* 提取目录和文件名 */
    const char *filename = strrchr(path, '/');
    uint32_t dir_cluster = fs->root_cluster;
    
    if (filename) {
        filename++;  /* 跳过 '/' */
    } else {
        filename = path;
    }
    
    /* 生成短文件名 */
    char short_name[11];
    fat32_generate_short_name(filename, short_name, 0);
    
    /* 查找空闲目录项 */
    uint32_t entry_cluster, entry_offset;
    int ret = fat32_find_free_dir_entry(fs, dir_cluster, &entry_cluster, &entry_offset);
    if (ret < 0) {
        return ret;
    }
    
    /* 创建目录项 */
    struct fat32_dir_entry new_entry;
    memset(&new_entry, 0, sizeof(new_entry));
    
    memcpy(new_entry.name, short_name, 11);
    new_entry.attr = FAT_ATTR_ARCHIVE;
    new_entry.create_time = fat32_get_current_time();
    new_entry.create_date = fat32_get_current_date();
    new_entry.write_time = new_entry.create_time;
    new_entry.write_date = new_entry.create_date;
    new_entry.first_cluster_high = 0;
    new_entry.first_cluster_low = 0;
    new_entry.file_size = 0;
    
    /* 写入目录项 */
    ret = fat32_write_dir_entry(fs, entry_cluster, entry_offset, &new_entry);
    if (ret < 0) {
        return ret;
    }
    
    kprintf("[FAT32] File created: %s\n", filename);
    return 0;
}

/*
 * 删除文件
 */
int fat32_unlink(
    struct fat32_fs_info *fs,
    const char *path)
{
    if (!fs || !path) {
        return -EINVAL;
    }
    
    kprintf("[FAT32] Deleting file: %s\n", path);
    
    /* 查找文件 */
    struct fat32_dir_entry *entry = fat32_lookup(fs, path);
    if (!entry) {
        return -ENOENT;
    }
    
    /* 不能删除目录 */
    if (entry->attr & FAT_ATTR_DIRECTORY) {
        kfree(entry);
        return -EISDIR;
    }
    
    /* 获取起始簇 */
    uint32_t cluster = fat32_get_first_cluster(entry);
    
    /* 释放簇链 */
    if (cluster >= 2) {
        fat32_free_cluster(fs, cluster);
    }
    
    /* 标记目录项为已删除 */
    entry->name[0] = FAT_DIR_ENTRY_FREE;
    
    /* TODO: 写回目录项到磁盘 */
    
    kfree(entry);
    
    kprintf("[FAT32] File deleted: %s\n", path);
    return 0;
}

/*
 * 创建目录
 */
int fat32_mkdir(
    struct fat32_fs_info *fs,
    const char *path)
{
    if (!fs || !path) {
        return -EINVAL;
    }
    
    kprintf("[FAT32] Creating directory: %s\n", path);
    
    /* 提取父目录和目录名 */
    const char *dirname = strrchr(path, '/');
    uint32_t parent_cluster = fs->root_cluster;
    
    if (dirname) {
        dirname++;
    } else {
        dirname = path;
    }
    
    /* 分配簇给新目录 */
    uint32_t new_cluster = fat32_alloc_cluster(fs);
    if (new_cluster == 0) {
        return -ENOSPC;
    }
    
    /* 生成短文件名 */
    char short_name[11];
    fat32_generate_short_name(dirname, short_name, 0);
    
    /* 查找空闲目录项 */
    uint32_t entry_cluster, entry_offset;
    int ret = fat32_find_free_dir_entry(fs, parent_cluster, &entry_cluster, &entry_offset);
    if (ret < 0) {
        fat32_free_cluster(fs, new_cluster);
        return ret;
    }
    
    /* 创建目录项 */
    struct fat32_dir_entry new_entry;
    memset(&new_entry, 0, sizeof(new_entry));
    
    memcpy(new_entry.name, short_name, 11);
    new_entry.attr = FAT_ATTR_DIRECTORY;
    new_entry.create_time = fat32_get_current_time();
    new_entry.create_date = fat32_get_current_date();
    new_entry.write_time = new_entry.create_time;
    new_entry.write_date = new_entry.create_date;
    fat32_set_first_cluster(&new_entry, new_cluster);
    new_entry.file_size = 0;
    
    /* 写入目录项 */
    ret = fat32_write_dir_entry(fs, entry_cluster, entry_offset, &new_entry);
    if (ret < 0) {
        fat32_free_cluster(fs, new_cluster);
        return ret;
    }
    
    /* 初始化新目录（创建 . 和 .. 项） */
    uint8_t *dir_buf = kmalloc(fs->cluster_size);
    if (dir_buf) {
        memset(dir_buf, 0, fs->cluster_size);
        
        struct fat32_dir_entry *entries = (struct fat32_dir_entry*)dir_buf;
        
        /* . 项（当前目录） */
        memcpy(entries[0].name, ".          ", 11);
        entries[0].attr = FAT_ATTR_DIRECTORY;
        fat32_set_first_cluster(&entries[0], new_cluster);
        
        /* .. 项（父目录） */
        memcpy(entries[1].name, "..         ", 11);
        entries[1].attr = FAT_ATTR_DIRECTORY;
        fat32_set_first_cluster(&entries[1], parent_cluster);
        
        /* 写入 */
        fat32_write_cluster(fs, new_cluster, dir_buf);
        kfree(dir_buf);
    }
    
    kprintf("[FAT32] Directory created: %s\n", dirname);
    return 0;
}

/*
 * 删除目录
 */
int fat32_rmdir(
    struct fat32_fs_info *fs,
    const char *path)
{
    if (!fs || !path) {
        return -EINVAL;
    }
    
    kprintf("[FAT32] Removing directory: %s\n", path);
    
    /* 查找目录 */
    struct fat32_dir_entry *entry = fat32_lookup(fs, path);
    if (!entry) {
        return -ENOENT;
    }
    
    /* 必须是目录 */
    if (!(entry->attr & FAT_ATTR_DIRECTORY)) {
        kfree(entry);
        return -ENOTDIR;
    }
    
    /* 检查是否为空（只包含 . 和 ..） */
    uint32_t cluster = fat32_get_first_cluster(entry);
    uint8_t *cluster_buf = kmalloc(fs->cluster_size);
    
    if (!cluster_buf) {
        kfree(entry);
        return -ENOMEM;
    }
    
    if (fat32_read_cluster(fs, cluster, cluster_buf) < 0) {
        kfree(cluster_buf);
        kfree(entry);
        return -EIO;
    }
    
    /* 检查是否只有 . 和 .. */
    int entries_per_cluster = fs->cluster_size / sizeof(struct fat32_dir_entry);
    for (int i = 2; i < entries_per_cluster; i++) {
        struct fat32_dir_entry *e = (struct fat32_dir_entry*)(cluster_buf + i * sizeof(struct fat32_dir_entry));
        
        if (e->name[0] != FAT_DIR_ENTRY_END && e->name[0] != FAT_DIR_ENTRY_FREE) {
            /* 目录非空 */
            kfree(cluster_buf);
            kfree(entry);
            return -ENOTEMPTY;
        }
    }
    
    kfree(cluster_buf);
    
    /* 释放簇链 */
    fat32_free_cluster(fs, cluster);
    
    /* 标记目录项为已删除 */
    /* TODO: 写回到磁盘 */
    
    kfree(entry);
    
    kprintf("[FAT32] Directory removed: %s\n", path);
    return 0;
}

/*
 * 更新目录项（修改文件大小、时间等）
 */
int fat32_update_dir_entry(
    struct fat32_fs_info *fs,
    const char *path,
    struct fat32_dir_entry *new_entry)
{
    /* TODO: 实现完整的目录项更新 */
    (void)fs;
    (void)path;
    (void)new_entry;
    return 0;
}

