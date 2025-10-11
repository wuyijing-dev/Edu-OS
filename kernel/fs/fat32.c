/*
 * fat32.c - FAT32 文件系统核心实现
 *
 * 基于 Linux FAT 驱动和微软 FAT32 规范
 */

#include <fs/fat32.h>
#include <fs/vfs.h>
#include <kernel.h>
#include <string.h>
#include <mm/kmalloc.h>

/* ========== 全局变量 ========== */

static struct fat32_fs_info *g_fat32_fs = NULL;
static bool g_fat32_initialized = false;

/* ========== 块设备模拟接口 ========== */

/*
 * 读取扇区（临时实现，后续需要真实的块设备驱动）
 */
static int read_sectors(void *bdev, uint32_t sector, uint32_t count, void *buf)
{
    (void)bdev;
    (void)sector;
    (void)count;
    (void)buf;
    
    /* TODO: 实现真实的块设备读取 */
    kprintf("[FAT32] read_sectors: sector=%u, count=%u\n", sector, count);
    return 0;
}

/*
 * 写入扇区
 */
static int write_sectors(void *bdev, uint32_t sector, uint32_t count, const void *buf)
{
    (void)bdev;
    (void)sector;
    (void)count;
    (void)buf;
    
    /* TODO: 实现真实的块设备写入 */
    kprintf("[FAT32] write_sectors: sector=%u, count=%u\n", sector, count);
    return 0;
}

/* ========== 簇和扇区转换 ========== */

/*
 * 簇号转扇区号
 */
static uint32_t cluster_to_sector(struct fat32_fs_info *fs, uint32_t cluster)
{
    /* 簇 2 是第一个数据簇，对应 data_start_sector */
    return fs->data_start_sector + (cluster - 2) * fs->sectors_per_cluster;
}

/* ========== FAT 表操作 ========== */

/*
 * 读取 FAT 表项
 */
uint32_t fat32_read_fat(struct fat32_fs_info *fs, uint32_t cluster)
{
    if (cluster < 2 || cluster >= fs->total_clusters) {
        return FAT32_BAD_CLUSTER;
    }
    
    /* 计算 FAT 表项的位置 */
    uint32_t fat_offset = cluster * 4;  /* 每个表项 4 字节 */
    uint32_t fat_sector = fs->fat_start_sector + (fat_offset / fs->bytes_per_sector);
    uint32_t entry_offset = fat_offset % fs->bytes_per_sector;
    
    /* 读取扇区 */
    uint8_t sector_buf[FAT32_SECTOR_SIZE];
    if (read_sectors(fs->bdev, fat_sector, 1, sector_buf) < 0) {
        return FAT32_BAD_CLUSTER;
    }
    
    /* 提取表项（只用低 28 位） */
    uint32_t entry = *(uint32_t*)(sector_buf + entry_offset);
    return entry & FAT32_MASK;
}

/*
 * 写入 FAT 表项
 */
int fat32_write_fat(struct fat32_fs_info *fs, uint32_t cluster, uint32_t value)
{
    if (cluster < 2 || cluster >= fs->total_clusters) {
        return -EINVAL;
    }
    
    /* 计算 FAT 表项的位置 */
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = fs->fat_start_sector + (fat_offset / fs->bytes_per_sector);
    uint32_t entry_offset = fat_offset % fs->bytes_per_sector;
    
    /* 读取扇区 */
    uint8_t sector_buf[FAT32_SECTOR_SIZE];
    if (read_sectors(fs->bdev, fat_sector, 1, sector_buf) < 0) {
        return -EIO;
    }
    
    /* 修改表项（保留高 4 位） */
    uint32_t *entry_ptr = (uint32_t*)(sector_buf + entry_offset);
    *entry_ptr = (*entry_ptr & 0xF0000000) | (value & FAT32_MASK);
    
    /* 写回扇区 */
    if (write_sectors(fs->bdev, fat_sector, 1, sector_buf) < 0) {
        return -EIO;
    }
    
    /* 同时更新第二个 FAT 表（如果存在） */
    if (fs->num_fats > 1) {
        uint32_t fat2_sector = fat_sector + fs->fat_size;
        write_sectors(fs->bdev, fat2_sector, 1, sector_buf);
    }
    
    return 0;
}

/*
 * 分配空闲簇
 */
uint32_t fat32_alloc_cluster(struct fat32_fs_info *fs)
{
    /* 从簇 2 开始扫描 */
    for (uint32_t cluster = 2; cluster < fs->total_clusters; cluster++) {
        uint32_t entry = fat32_read_fat(fs, cluster);
        
        if (fat32_is_free_cluster(entry)) {
            /* 标记为已使用（EOF） */
            fat32_write_fat(fs, cluster, FAT32_EOC_MAX);
            return cluster;
        }
    }
    
    /* 没有空闲簇 */
    return 0;
}

/*
 * 释放簇链
 */
int fat32_free_cluster(struct fat32_fs_info *fs, uint32_t cluster)
{
    while (cluster >= 2 && cluster < fs->total_clusters) {
        /* 读取下一个簇号 */
        uint32_t next = fat32_read_fat(fs, cluster);
        
        /* 释放当前簇 */
        fat32_write_fat(fs, cluster, FAT32_FREE);
        
        /* 如果是簇链结束，退出 */
        if (fat32_is_eoc(next) || fat32_is_bad_cluster(next)) {
            break;
        }
        
        cluster = next;
    }
    
    return 0;
}

/* ========== 簇读写 ========== */

/*
 * 读取簇
 */
int fat32_read_cluster(struct fat32_fs_info *fs, uint32_t cluster, void *buf)
{
    if (cluster < 2 || cluster >= fs->total_clusters) {
        return -EINVAL;
    }
    
    uint32_t sector = cluster_to_sector(fs, cluster);
    return read_sectors(fs->bdev, sector, fs->sectors_per_cluster, buf);
}

/*
 * 写入簇
 */
int fat32_write_cluster(struct fat32_fs_info *fs, uint32_t cluster, const void *buf)
{
    if (cluster < 2 || cluster >= fs->total_clusters) {
        return -EINVAL;
    }
    
    uint32_t sector = cluster_to_sector(fs, cluster);
    return write_sectors(fs->bdev, sector, fs->sectors_per_cluster, buf);
}

/* ========== 文件系统挂载 ========== */

/*
 * 验证引导扇区
 */
static bool validate_boot_sector(struct fat32_boot_sector *bs)
{
    /* 检查签名 */
    if (bs->boot_signature_end != 0xAA55) {
        kprintf("[FAT32] Invalid boot signature: 0x%04X\n", bs->boot_signature_end);
        return false;
    }
    
    /* 检查文件系统类型 */
    if (strncmp(bs->fs_type, "FAT32", 5) != 0) {
        kprintf("[FAT32] Not a FAT32 filesystem: %.8s\n", bs->fs_type);
        return false;
    }
    
    /* 检查扇区大小 */
    if (bs->bytes_per_sector != 512 && bs->bytes_per_sector != 1024 &&
        bs->bytes_per_sector != 2048 && bs->bytes_per_sector != 4096) {
        kprintf("[FAT32] Invalid bytes_per_sector: %u\n", bs->bytes_per_sector);
        return false;
    }
    
    /* 检查每簇扇区数（必须是 2 的幂） */
    uint8_t spc = bs->sectors_per_cluster;
    if (spc == 0 || (spc & (spc - 1)) != 0) {
        kprintf("[FAT32] Invalid sectors_per_cluster: %u\n", spc);
        return false;
    }
    
    /* 检查 FAT 表数量 */
    if (bs->num_fats == 0) {
        kprintf("[FAT32] Invalid num_fats: %u\n", bs->num_fats);
        return false;
    }
    
    /* FAT32 特有检查 */
    if (bs->root_entry_count != 0) {
        kprintf("[FAT32] root_entry_count should be 0 for FAT32\n");
        return false;
    }
    
    if (bs->total_sectors_16 != 0) {
        kprintf("[FAT32] total_sectors_16 should be 0 for FAT32\n");
        return false;
    }
    
    if (bs->fat_size_16 != 0) {
        kprintf("[FAT32] fat_size_16 should be 0 for FAT32\n");
        return false;
    }
    
    return true;
}

/*
 * 挂载 FAT32 文件系统
 */
int fat32_mount(void *bdev)
{
    kprintf("[FAT32] Mounting FAT32 filesystem...\n");
    
    /* 分配文件系统结构 */
    struct fat32_fs_info *fs = kmalloc(sizeof(struct fat32_fs_info));
    if (!fs) {
        kprintf("[FAT32] Failed to allocate fs_info\n");
        return -ENOMEM;
    }
    
    memset(fs, 0, sizeof(struct fat32_fs_info));
    fs->bdev = bdev;
    
    /* 读取引导扇区 */
    struct fat32_boot_sector bs;
    if (read_sectors(bdev, 0, 1, &bs) < 0) {
        kprintf("[FAT32] Failed to read boot sector\n");
        kfree(fs);
        return -EIO;
    }
    
    /* 验证引导扇区 */
    if (!validate_boot_sector(&bs)) {
        kfree(fs);
        return -EINVAL;
    }
    
    /* 提取参数 */
    fs->bytes_per_sector = bs.bytes_per_sector;
    fs->sectors_per_cluster = bs.sectors_per_cluster;
    fs->cluster_size = bs.bytes_per_sector * bs.sectors_per_cluster;
    fs->reserved_sectors = bs.reserved_sectors;
    fs->num_fats = bs.num_fats;
    fs->fat_size = bs.fat_size_32;
    fs->root_cluster = bs.root_cluster;
    fs->total_sectors = bs.total_sectors_32;
    
    /* 计算偏移 */
    fs->fat_start_sector = bs.reserved_sectors;
    fs->data_start_sector = bs.reserved_sectors + (bs.num_fats * bs.fat_size_32);
    
    /* 计算总簇数 */
    uint32_t data_sectors = fs->total_sectors - fs->data_start_sector;
    fs->total_clusters = (data_sectors / fs->sectors_per_cluster) + 2;
    
    /* 打印信息 */
    kprintf("[FAT32] Volume Label: %.11s\n", bs.volume_label);
    kprintf("[FAT32] Bytes per Sector: %u\n", fs->bytes_per_sector);
    kprintf("[FAT32] Sectors per Cluster: %u\n", fs->sectors_per_cluster);
    kprintf("[FAT32] Cluster Size: %u bytes\n", fs->cluster_size);
    kprintf("[FAT32] FAT Size: %u sectors\n", fs->fat_size);
    kprintf("[FAT32] Root Cluster: %u\n", fs->root_cluster);
    kprintf("[FAT32] Total Sectors: %u\n", fs->total_sectors);
    kprintf("[FAT32] Total Clusters: %u\n", fs->total_clusters);
    kprintf("[FAT32] FAT Start Sector: %u\n", fs->fat_start_sector);
    kprintf("[FAT32] Data Start Sector: %u\n", fs->data_start_sector);
    
    /* 保存全局指针 */
    g_fat32_fs = fs;
    
    kprintf("[FAT32] FAT32 filesystem mounted successfully\n");
    
    return 0;
}

/*
 * 卸载 FAT32 文件系统
 */
int fat32_umount(struct fat32_fs_info *fs)
{
    if (!fs) {
        return -EINVAL;
    }
    
    /* 释放缓存 */
    if (fs->fat_cache) {
        kfree(fs->fat_cache);
    }
    
    if (fs->cluster_cache) {
        kfree(fs->cluster_cache);
    }
    
    /* 释放文件系统结构 */
    kfree(fs);
    
    if (g_fat32_fs == fs) {
        g_fat32_fs = NULL;
    }
    
    kprintf("[FAT32] Filesystem unmounted\n");
    
    return 0;
}

/*
 * 初始化 FAT32 驱动
 */
int fat32_init(void)
{
    if (g_fat32_initialized) {
        kprintf("[FAT32] Already initialized\n");
        return 0;
    }
    
    kprintf("[FAT32] Initializing FAT32 filesystem driver...\n");
    
    /* TODO: 注册到 VFS */
    
    g_fat32_initialized = true;
    
    kprintf("[FAT32] FAT32 driver initialized\n");
    
    return 0;
}

/*
 * 获取全局 FAT32 文件系统
 */
struct fat32_fs_info *fat32_get_fs(void)
{
    return g_fat32_fs;
}

/* ========== 外部接口声明 ========== */

/* 供其他模块调用 */
extern struct fat32_dir_entry *fat32_find_in_dir(
    struct fat32_fs_info *fs,
    uint32_t dir_cluster,
    const char *name);

extern int fat32_readdir(
    struct fat32_fs_info *fs,
    uint32_t dir_cluster,
    int index,
    char *name_out,
    struct fat32_dir_entry *entry_out);

extern struct fat32_dir_entry *fat32_lookup(
    struct fat32_fs_info *fs,
    const char *path);

