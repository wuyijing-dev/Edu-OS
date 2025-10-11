/*
 * block.h - 块设备接口
 * 
 * 简化的块设备驱动抽象层
 */

#ifndef BLOCK_H
#define BLOCK_H

#include <types.h>

/* 块设备类型 */
enum block_device_type {
    BLOCK_DEV_IDE,      /* IDE/ATA 硬盘 */
    BLOCK_DEV_FLOPPY,   /* 软盘 */
    BLOCK_DEV_RAMDISK,  /* RAM 磁盘 */
};

/* 块设备结构 */
struct block_device {
    const char *name;                   /* 设备名（如 "hda", "hdb"） */
    enum block_device_type type;        /* 设备类型 */
    uint32_t sector_size;               /* 扇区大小（通常 512） */
    uint32_t total_sectors;             /* 总扇区数 */
    void *private_data;                 /* 驱动私有数据 */
    
    /* 操作函数 */
    int (*read_sectors)(struct block_device *bdev, uint32_t sector, uint32_t count, void *buf);
    int (*write_sectors)(struct block_device *bdev, uint32_t sector, uint32_t count, const void *buf);
};

/* ========== 块设备管理 ========== */

/*
 * 初始化块设备子系统
 */
void block_init(void);

/*
 * 注册块设备
 */
int block_register_device(struct block_device *bdev);

/*
 * 查找块设备
 */
struct block_device *block_get_device(const char *name);

/*
 * 读取扇区
 */
int block_read_sectors(struct block_device *bdev, uint32_t sector, uint32_t count, void *buf);

/*
 * 写入扇区
 */
int block_write_sectors(struct block_device *bdev, uint32_t sector, uint32_t count, const void *buf);

#endif // BLOCK_H

