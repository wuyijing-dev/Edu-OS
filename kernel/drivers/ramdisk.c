/*
 * ramdisk.c - RAM Disk 驱动（内存模拟磁盘）
 * 
 * 用于在没有真实磁盘时测试文件系统
 * 这是 Linux 也使用的技术（initrd）
 */

#include <drivers/block.h>
#include <fs/vfs.h>
#include <kernel.h>
#include <string.h>
#include <mm/kmalloc.h>

/* RAM Disk 私有数据 */
struct ramdisk_data {
    uint8_t *data;              /* 数据指针 */
    uint32_t size;              /* 大小（字节） */
};

/*
 * RAM Disk 读取扇区
 */
static int ramdisk_read_sectors(struct block_device *bdev, uint32_t sector, uint32_t count, void *buf)
{
    struct ramdisk_data *rd = bdev->private_data;
    
    uint32_t offset = sector * bdev->sector_size;
    uint32_t size = count * bdev->sector_size;
    
    if (offset + size > rd->size) {
        return -EINVAL;
    }
    
    memcpy(buf, rd->data + offset, size);
    return 0;
}

/*
 * RAM Disk 写入扇区
 */
static int ramdisk_write_sectors(struct block_device *bdev, uint32_t sector, uint32_t count, const void *buf)
{
    struct ramdisk_data *rd = bdev->private_data;
    
    uint32_t offset = sector * bdev->sector_size;
    uint32_t size = count * bdev->sector_size;
    
    if (offset + size > rd->size) {
        return -EINVAL;
    }
    
    memcpy(rd->data + offset, buf, size);
    return 0;
}

/*
 * 创建 RAM Disk
 * 
 * @param name: 设备名
 * @param size_mb: 大小（MB）
 * @return: 块设备指针，失败返回 NULL
 */
struct block_device *ramdisk_create(const char *name, uint32_t size_mb)
{
    kprintf("[RAMDisk] Creating RAM disk: %s (%u MB)...\n", name, size_mb);
    
    /* 分配内存 */
    uint32_t size = size_mb * 1024 * 1024;
    uint8_t *data = kmalloc(size);
    if (!data) {
        kprintf("[RAMDisk] Failed to allocate %u MB\n", size_mb);
        return NULL;
    }
    
    /* 清零 */
    memset(data, 0, size);
    
    /* 创建私有数据 */
    struct ramdisk_data *rd = kmalloc(sizeof(struct ramdisk_data));
    if (!rd) {
        kfree(data);
        return NULL;
    }
    
    rd->data = data;
    rd->size = size;
    
    /* 创建块设备 */
    struct block_device *bdev = kmalloc(sizeof(struct block_device));
    if (!bdev) {
        kfree(rd);
        kfree(data);
        return NULL;
    }
    
    bdev->name = name;
    bdev->type = BLOCK_DEV_RAMDISK;
    bdev->sector_size = 512;
    bdev->total_sectors = size / 512;
    bdev->private_data = rd;
    bdev->read_sectors = ramdisk_read_sectors;
    bdev->write_sectors = ramdisk_write_sectors;
    
    /* 注册 */
    if (block_register_device(bdev) < 0) {
        kfree(bdev);
        kfree(rd);
        kfree(data);
        return NULL;
    }
    
    kprintf("[RAMDisk] Created RAM disk: %s (%u sectors)\n", name, bdev->total_sectors);
    
    return bdev;
}

