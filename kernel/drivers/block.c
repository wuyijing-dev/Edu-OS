/*
 * block.c - 块设备管理
 */

#include <drivers/block.h>
#include <fs/vfs.h>
#include <kernel.h>
#include <string.h>
#include <mm/kmalloc.h>

/* ========== 全局变量 ========== */

#define MAX_BLOCK_DEVICES 8

static struct block_device *block_devices[MAX_BLOCK_DEVICES];
static int num_block_devices = 0;

/* ========== 块设备管理 ========== */

/*
 * 初始化块设备子系统
 */
void block_init(void)
{
    kprintf("[BLOCK] Initializing block device subsystem...\n");
    
    for (int i = 0; i < MAX_BLOCK_DEVICES; i++) {
        block_devices[i] = NULL;
    }
    num_block_devices = 0;
    
    kprintf("[BLOCK] Block device subsystem initialized\n");
}

/*
 * 注册块设备
 */
int block_register_device(struct block_device *bdev)
{
    if (!bdev || !bdev->name) {
        return -EINVAL;
    }
    
    if (num_block_devices >= MAX_BLOCK_DEVICES) {
        kprintf("[BLOCK] Too many block devices\n");
        return -ENOMEM;
    }
    
    /* 检查重复 */
    for (int i = 0; i < num_block_devices; i++) {
        if (strcmp(block_devices[i]->name, bdev->name) == 0) {
            kprintf("[BLOCK] Device %s already registered\n", bdev->name);
            return -EEXIST;
        }
    }
    
    block_devices[num_block_devices++] = bdev;
    
    kprintf("[BLOCK] Registered device: %s (%u sectors, %u bytes/sector)\n",
            bdev->name, bdev->total_sectors, bdev->sector_size);
    
    return 0;
}

/*
 * 查找块设备
 */
struct block_device *block_get_device(const char *name)
{
    if (!name) {
        return NULL;
    }
    
    for (int i = 0; i < num_block_devices; i++) {
        if (strcmp(block_devices[i]->name, name) == 0) {
            return block_devices[i];
        }
    }
    
    return NULL;
}

/*
 * 读取扇区
 */
int block_read_sectors(struct block_device *bdev, uint32_t sector, uint32_t count, void *buf)
{
    if (!bdev || !buf) {
        return -EINVAL;
    }
    
    if (!bdev->read_sectors) {
        return -ENOSYS;
    }
    
    if (sector + count > bdev->total_sectors) {
        return -EINVAL;
    }
    
    return bdev->read_sectors(bdev, sector, count, buf);
}

/*
 * 写入扇区
 */
int block_write_sectors(struct block_device *bdev, uint32_t sector, uint32_t count, const void *buf)
{
    if (!bdev || !buf) {
        return -EINVAL;
    }
    
    if (!bdev->write_sectors) {
        return -ENOSYS;
    }
    
    if (sector + count > bdev->total_sectors) {
        return -EINVAL;
    }
    
    return bdev->write_sectors(bdev, sector, count, buf);
}

