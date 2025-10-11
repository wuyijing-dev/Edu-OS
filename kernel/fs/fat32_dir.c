/*
 * fat32_dir.c - FAT32 目录操作
 */

#include <fs/fat32.h>
#include <kernel.h>
#include <string.h>
#include <mm/kmalloc.h>

/* 外部函数声明 */
extern struct fat32_fs_info *fat32_get_fs(void);

/* ========== 辅助函数 ========== */

/*
 * 格式化 8.3 文件名
 * "HELLO   TXT" → "HELLO.TXT"
 */
static void format_83_name(const char *fat_name, char *output)
{
    int j = 0;
    
    /* 处理主文件名（8 字符） */
    for (int i = 0; i < 8 && fat_name[i] != ' '; i++) {
        output[j++] = fat_name[i];
    }
    
    /* 处理扩展名（3 字符） */
    if (fat_name[8] != ' ') {
        output[j++] = '.';
        for (int i = 8; i < 11 && fat_name[i] != ' '; i++) {
            output[j++] = fat_name[i];
        }
    }
    
    output[j] = '\0';
}

/*
 * 转换为 8.3 格式
 * "hello.txt" → "HELLO   TXT"
 */
static void to_83_name(const char *name, char *fat_name)
{
    memset(fat_name, ' ', 11);
    
    const char *dot = strchr(name, '.');
    int name_len = dot ? (dot - name) : strlen(name);
    
    /* 主文件名（最多 8 字符） */
    int i;
    for (i = 0; i < 8 && i < name_len; i++) {
        fat_name[i] = toupper(name[i]);
    }
    
    /* 扩展名（最多 3 字符） */
    if (dot) {
        dot++;
        for (i = 0; i < 3 && dot[i]; i++) {
            fat_name[8 + i] = toupper(dot[i]);
        }
    }
}

/*
 * 比较文件名
 */
static bool name_match(const char *fat_name, const char *name)
{
    char formatted[13];
    format_83_name(fat_name, formatted);
    
    /* 不区分大小写比较 */
    return strcasecmp(formatted, name) == 0;
}

/* ========== 目录遍历 ========== */

/*
 * 在目录中查找文件
 */
struct fat32_dir_entry *fat32_find_in_dir(
    struct fat32_fs_info *fs,
    uint32_t dir_cluster,
    const char *name)
{
    uint32_t cluster = dir_cluster;
    
    /* 分配簇缓冲区 */
    uint8_t *cluster_buf = kmalloc(fs->cluster_size);
    if (!cluster_buf) {
        return NULL;
    }
    
    /* 遍历簇链 */
    while (cluster >= 2 && !fat32_is_eoc(cluster)) {
        /* 读取簇 */
        if (fat32_read_cluster(fs, cluster, cluster_buf) < 0) {
            kfree(cluster_buf);
            return NULL;
        }
        
        /* 遍历目录项 */
        int entries_per_cluster = fs->cluster_size / sizeof(struct fat32_dir_entry);
        
        for (int i = 0; i < entries_per_cluster; i++) {
            struct fat32_dir_entry *entry = 
                (struct fat32_dir_entry*)(cluster_buf + i * sizeof(struct fat32_dir_entry));
            
            /* 目录结束 */
            if (entry->name[0] == FAT_DIR_ENTRY_END) {
                kfree(cluster_buf);
                return NULL;
            }
            
            /* 跳过已删除项 */
            if (entry->name[0] == FAT_DIR_ENTRY_FREE) {
                continue;
            }
            
            /* 跳过长文件名项 */
            if (entry->attr == FAT_ATTR_LONG_NAME) {
                continue;
            }
            
            /* 跳过卷标 */
            if (entry->attr & FAT_ATTR_VOLUME_ID) {
                continue;
            }
            
            /* 比较文件名 */
            if (name_match(entry->name, name)) {
                /* 找到了，复制目录项 */
                struct fat32_dir_entry *result = kmalloc(sizeof(struct fat32_dir_entry));
                if (result) {
                    memcpy(result, entry, sizeof(struct fat32_dir_entry));
                }
                kfree(cluster_buf);
                return result;
            }
        }
        
        /* 下一个簇 */
        cluster = fat32_read_fat(fs, cluster);
    }
    
    kfree(cluster_buf);
    return NULL;
}

/*
 * 列出目录内容
 */
int fat32_readdir(
    struct fat32_fs_info *fs,
    uint32_t dir_cluster,
    int index,
    char *name_out,
    struct fat32_dir_entry *entry_out)
{
    uint32_t cluster = dir_cluster;
    int current_index = 0;
    
    /* 分配簇缓冲区 */
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
        
        /* 遍历目录项 */
        int entries_per_cluster = fs->cluster_size / sizeof(struct fat32_dir_entry);
        
        for (int i = 0; i < entries_per_cluster; i++) {
            struct fat32_dir_entry *entry = 
                (struct fat32_dir_entry*)(cluster_buf + i * sizeof(struct fat32_dir_entry));
            
            /* 目录结束 */
            if (entry->name[0] == FAT_DIR_ENTRY_END) {
                kfree(cluster_buf);
                return 0;  // 没有更多条目
            }
            
            /* 跳过已删除项 */
            if (entry->name[0] == FAT_DIR_ENTRY_FREE) {
                continue;
            }
            
            /* 跳过长文件名项 */
            if (entry->attr == FAT_ATTR_LONG_NAME) {
                continue;
            }
            
            /* 跳过卷标 */
            if (entry->attr & FAT_ATTR_VOLUME_ID) {
                continue;
            }
            
            /* 是否到达目标索引？ */
            if (current_index == index) {
                /* 格式化文件名 */
                format_83_name(entry->name, name_out);
                
                /* 复制目录项 */
                if (entry_out) {
                    memcpy(entry_out, entry, sizeof(struct fat32_dir_entry));
                }
                
                kfree(cluster_buf);
                return 1;  // 成功
            }
            
            current_index++;
        }
        
        /* 下一个簇 */
        cluster = fat32_read_fat(fs, cluster);
    }
    
    kfree(cluster_buf);
    return 0;  // 没有更多条目
}

/* ========== 路径解析 ========== */

/*
 * 查找文件（支持路径）
 */
struct fat32_dir_entry *fat32_lookup(
    struct fat32_fs_info *fs,
    const char *path)
{
    /* 从根目录开始 */
    uint32_t cluster = fs->root_cluster;
    
    /* 跳过开头的 '/' */
    if (path[0] == '/') {
        path++;
    }
    
    /* 如果是根目录 */
    if (path[0] == '\0') {
        /* 创建根目录项 */
        struct fat32_dir_entry *root = kmalloc(sizeof(struct fat32_dir_entry));
        if (!root) return NULL;
        
        memset(root, 0, sizeof(struct fat32_dir_entry));
        strcpy(root->name, "/          ");
        root->attr = FAT_ATTR_DIRECTORY;
        fat32_set_first_cluster(root, fs->root_cluster);
        
        return root;
    }
    
    /* 分割路径 */
    char *path_copy = strdup(path);
    if (!path_copy) return NULL;
    
    char *token = strtok(path_copy, "/");
    struct fat32_dir_entry *entry = NULL;
    
    while (token) {
        /* 在当前目录中查找 */
        entry = fat32_find_in_dir(fs, cluster, token);
        
        if (!entry) {
            kfree(path_copy);
            return NULL;  // 未找到
        }
        
        /* 获取下一个路径部分 */
        token = strtok(NULL, "/");
        
        /* 如果还有路径，必须是目录 */
        if (token) {
            if (!(entry->attr & FAT_ATTR_DIRECTORY)) {
                kfree(entry);
                kfree(path_copy);
                return NULL;  // 不是目录
            }
            
            /* 进入子目录 */
            cluster = fat32_get_first_cluster(entry);
            kfree(entry);  // 释放中间目录项
        }
    }
    
    kfree(path_copy);
    return entry;
}

/* ========== 目录创建和删除 ========== */

/*
 * 创建目录
 */
int fat32_mkdir(
    struct fat32_fs_info *fs,
    const char *path)
{
    /* TODO: 实现目录创建 */
    (void)fs;
    (void)path;
    kprintf("[FAT32] mkdir not yet implemented\n");
    return -ENOSYS;
}

/*
 * 删除目录
 */
int fat32_rmdir(
    struct fat32_fs_info *fs,
    const char *path)
{
    /* TODO: 实现目录删除 */
    (void)fs;
    (void)path;
    kprintf("[FAT32] rmdir not yet implemented\n");
    return -ENOSYS;
}

