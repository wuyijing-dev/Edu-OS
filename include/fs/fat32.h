/*
 * fat32.h - FAT32 文件系统接口
 * 
 * 基于微软 FAT32 规范和 Linux FAT 驱动设计
 */

#ifndef FAT32_H
#define FAT32_H

#include <types.h>
#include <fs/vfs.h>

/* ========== 常量定义 ========== */

#define FAT32_SECTOR_SIZE      512    /* 扇区大小 */
#define FAT32_MAX_PATH         260    /* 最大路径长度 */
#define FAT32_MAX_NAME         255    /* 最大文件名长度 */

/* FAT 表项特殊值 */
#define FAT32_FREE             0x00000000  /* 空闲簇 */
#define FAT32_RESERVED         0x00000001  /* 保留 */
#define FAT32_BAD_CLUSTER      0x0FFFFFF7  /* 坏簇 */
#define FAT32_EOC_MIN          0x0FFFFFF8  /* 簇链结束（最小） */
#define FAT32_EOC_MAX          0x0FFFFFFF  /* 簇链结束（最大） */
#define FAT32_MASK             0x0FFFFFFF  /* FAT32 掩码（28位） */

/* 文件属性 */
#define FAT_ATTR_READ_ONLY     0x01   /* 只读 */
#define FAT_ATTR_HIDDEN        0x02   /* 隐藏 */
#define FAT_ATTR_SYSTEM        0x04   /* 系统 */
#define FAT_ATTR_VOLUME_ID     0x08   /* 卷标 */
#define FAT_ATTR_DIRECTORY     0x10   /* 目录 */
#define FAT_ATTR_ARCHIVE       0x20   /* 归档 */
#define FAT_ATTR_LONG_NAME     0x0F   /* 长文件名 */

/* 目录项特殊标记 */
#define FAT_DIR_ENTRY_FREE     0xE5   /* 已删除 */
#define FAT_DIR_ENTRY_END      0x00   /* 目录结束 */

/* ========== 数据结构 ========== */

/*
 * FAT32 引导扇区（扇区 0）
 */
struct fat32_boot_sector {
    /* 通用 DOS 引导记录 */
    uint8_t  jump[3];              // 0x00: 跳转指令
    char     oem_name[8];          // 0x03: OEM 名称
    
    /* BIOS 参数块（BPB） */
    uint16_t bytes_per_sector;     // 0x0B: 每扇区字节数
    uint8_t  sectors_per_cluster;  // 0x0D: 每簇扇区数
    uint16_t reserved_sectors;     // 0x0E: 保留扇区数
    uint8_t  num_fats;             // 0x10: FAT 表数量
    uint16_t root_entry_count;     // 0x11: 根目录项数
    uint16_t total_sectors_16;     // 0x13: 总扇区数（FAT32为0）
    uint8_t  media_type;           // 0x15: 媒体类型
    uint16_t fat_size_16;          // 0x16: FAT 大小（FAT32为0）
    uint16_t sectors_per_track;    // 0x18: 每磁道扇区数
    uint16_t num_heads;            // 0x1A: 磁头数
    uint32_t hidden_sectors;       // 0x1C: 隐藏扇区数
    uint32_t total_sectors_32;     // 0x20: 总扇区数
    
    /* FAT32 扩展 BPB */
    uint32_t fat_size_32;          // 0x24: FAT 大小
    uint16_t ext_flags;            // 0x28: 扩展标志
    uint16_t fs_version;           // 0x2A: 文件系统版本
    uint32_t root_cluster;         // 0x2C: 根目录起始簇号
    uint16_t fs_info;              // 0x30: FSInfo 扇区号
    uint16_t backup_boot_sector;   // 0x32: 备份引导扇区号
    uint8_t  reserved[12];         // 0x34: 保留
    uint8_t  drive_number;         // 0x40: 驱动器号
    uint8_t  reserved1;            // 0x41: 保留
    uint8_t  boot_signature;       // 0x42: 启动签名（0x29）
    uint32_t volume_id;            // 0x43: 卷序列号
    char     volume_label[11];     // 0x47: 卷标
    char     fs_type[8];           // 0x52: 文件系统类型
    uint8_t  boot_code[420];       // 0x5A: 引导代码
    uint16_t boot_signature_end;   // 0x1FE: 签名（0xAA55）
} __attribute__((packed));

/*
 * FAT32 FSInfo 结构（扇区 1）
 */
struct fat32_fsinfo {
    uint32_t lead_signature;       // 0x00: 前导签名（0x41615252）
    uint8_t  reserved1[480];       // 0x04: 保留
    uint32_t struct_signature;     // 0x1E4: 结构签名（0x61417272）
    uint32_t free_count;           // 0x1E8: 空闲簇数
    uint32_t next_free;            // 0x1EC: 下一个空闲簇
    uint8_t  reserved2[12];        // 0x1F0: 保留
    uint32_t trail_signature;      // 0x1FC: 尾部签名（0xAA550000）
} __attribute__((packed));

/*
 * FAT32 目录项（32 字节）
 */
struct fat32_dir_entry {
    char     name[11];             // 0x00: 8.3 格式文件名
    uint8_t  attr;                 // 0x0B: 文件属性
    uint8_t  nt_reserved;          // 0x0C: 保留
    uint8_t  create_time_tenth;    // 0x0D: 创建时间（十分之一秒）
    uint16_t create_time;          // 0x0E: 创建时间
    uint16_t create_date;          // 0x10: 创建日期
    uint16_t last_access_date;     // 0x12: 最后访问日期
    uint16_t first_cluster_high;   // 0x14: 起始簇号（高16位）
    uint16_t write_time;           // 0x16: 修改时间
    uint16_t write_date;           // 0x18: 修改日期
    uint16_t first_cluster_low;    // 0x1A: 起始簇号（低16位）
    uint32_t file_size;            // 0x1C: 文件大小
} __attribute__((packed));

/*
 * 长文件名（LFN）目录项
 */
struct fat32_lfn_entry {
    uint8_t  order;                // 0x00: 序号
    uint16_t name1[5];             // 0x01: 名称第1-5字符
    uint8_t  attr;                 // 0x0B: 属性（固定0x0F）
    uint8_t  type;                 // 0x0C: 类型（固定0）
    uint8_t  checksum;             // 0x0D: 短文件名校验和
    uint16_t name2[6];             // 0x0E: 名称第6-11字符
    uint16_t first_cluster_low;    // 0x1A: 固定0
    uint16_t name3[2];             // 0x1C: 名称第12-13字符
} __attribute__((packed));

/*
 * FAT32 文件系统信息
 */
struct fat32_fs_info {
    struct vfs_superblock *sb;     /* VFS 超级块 */
    
    /* 从引导扇区提取的参数 */
    uint32_t bytes_per_sector;     /* 每扇区字节数 */
    uint32_t sectors_per_cluster;  /* 每簇扇区数 */
    uint32_t cluster_size;         /* 簇大小（字节） */
    uint32_t reserved_sectors;     /* 保留扇区数 */
    uint32_t num_fats;             /* FAT 表数量 */
    uint32_t fat_size;             /* FAT 大小（扇区） */
    uint32_t root_cluster;         /* 根目录起始簇号 */
    uint32_t total_sectors;        /* 总扇区数 */
    uint32_t total_clusters;       /* 总簇数 */
    
    /* 计算的偏移 */
    uint32_t fat_start_sector;     /* FAT 表起始扇区 */
    uint32_t data_start_sector;    /* 数据区起始扇区 */
    
    /* 块设备 */
    void *bdev;                    /* 块设备指针 */
    
    /* 缓存 */
    struct fat_cache *fat_cache;   /* FAT 表缓存 */
    struct cluster_cache *cluster_cache; /* 簇缓存 */
};

/*
 * FAT 表缓存项
 */
struct fat_cache_entry {
    uint32_t cluster;              /* 簇号 */
    uint32_t next_cluster;         /* 下一个簇号 */
    bool valid;                    /* 是否有效 */
};

/*
 * 簇缓存项
 */
struct cluster_cache_entry {
    uint32_t cluster;              /* 簇号 */
    uint8_t *data;                 /* 数据 */
    bool dirty;                    /* 是否脏 */
    bool valid;                    /* 是否有效 */
};

/* ========== 函数声明 ========== */

/*
 * 初始化 FAT32 驱动
 */
int fat32_init(void);

/*
 * 挂载 FAT32 文件系统
 * @param bdev: 块设备
 * @return: 0 成功，<0 失败
 */
int fat32_mount(void *bdev);

/*
 * 卸载 FAT32 文件系统
 */
int fat32_umount(struct fat32_fs_info *fs);

/*
 * 读取 FAT 表项
 * @param fs: 文件系统信息
 * @param cluster: 簇号
 * @return: 下一个簇号，或 FAT32_EOC/FAT32_FREE/FAT32_BAD
 */
uint32_t fat32_read_fat(struct fat32_fs_info *fs, uint32_t cluster);

/*
 * 写入 FAT 表项
 * @param fs: 文件系统信息
 * @param cluster: 簇号
 * @param value: 值
 * @return: 0 成功，<0 失败
 */
int fat32_write_fat(struct fat32_fs_info *fs, uint32_t cluster, uint32_t value);

/*
 * 分配空闲簇
 * @param fs: 文件系统信息
 * @return: 簇号，0 表示失败
 */
uint32_t fat32_alloc_cluster(struct fat32_fs_info *fs);

/*
 * 释放簇
 * @param fs: 文件系统信息
 * @param cluster: 簇号
 * @return: 0 成功，<0 失败
 */
int fat32_free_cluster(struct fat32_fs_info *fs, uint32_t cluster);

/*
 * 读取簇
 * @param fs: 文件系统信息
 * @param cluster: 簇号
 * @param buf: 缓冲区
 * @return: 0 成功，<0 失败
 */
int fat32_read_cluster(struct fat32_fs_info *fs, uint32_t cluster, void *buf);

/*
 * 写入簇
 * @param fs: 文件系统信息
 * @param cluster: 簇号
 * @param buf: 缓冲区
 * @return: 0 成功，<0 失败
 */
int fat32_write_cluster(struct fat32_fs_info *fs, uint32_t cluster, const void *buf);

/*
 * 获取起始簇号
 */
static inline uint32_t fat32_get_first_cluster(struct fat32_dir_entry *entry)
{
    return ((uint32_t)entry->first_cluster_high << 16) | entry->first_cluster_low;
}

/*
 * 设置起始簇号
 */
static inline void fat32_set_first_cluster(struct fat32_dir_entry *entry, uint32_t cluster)
{
    entry->first_cluster_high = (cluster >> 16) & 0xFFFF;
    entry->first_cluster_low = cluster & 0xFFFF;
}

/*
 * 判断是否是簇链结束
 */
static inline bool fat32_is_eoc(uint32_t cluster)
{
    return (cluster & FAT32_MASK) >= FAT32_EOC_MIN;
}

/*
 * 判断是否是坏簇
 */
static inline bool fat32_is_bad_cluster(uint32_t cluster)
{
    return (cluster & FAT32_MASK) == FAT32_BAD_CLUSTER;
}

/*
 * 判断是否是空闲簇
 */
static inline bool fat32_is_free_cluster(uint32_t cluster)
{
    return (cluster & FAT32_MASK) == FAT32_FREE;
}

/*
 * 获取全局 FAT32 文件系统
 */
struct fat32_fs_info *fat32_get_fs(void);

/*
 * 在目录中查找文件
 */
struct fat32_dir_entry *fat32_find_in_dir(
    struct fat32_fs_info *fs,
    uint32_t dir_cluster,
    const char *name);

/*
 * 查找文件（支持路径）
 */
struct fat32_dir_entry *fat32_lookup(
    struct fat32_fs_info *fs,
    const char *path);

/*
 * 列出目录内容
 */
int fat32_readdir(
    struct fat32_fs_info *fs,
    uint32_t dir_cluster,
    int index,
    char *name_out,
    struct fat32_dir_entry *entry_out);

/*
 * VFS 文件操作表
 */
extern struct vfs_file_operations fat32_file_ops;

#endif // FAT32_H

