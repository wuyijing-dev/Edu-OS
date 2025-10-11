/*
 * ramdisk.h - RAM Disk 驱动接口
 */

#ifndef RAMDISK_H
#define RAMDISK_H

#include <types.h>
#include <drivers/block.h>

/*
 * 创建 RAM Disk
 */
struct block_device *ramdisk_create(const char *name, uint32_t size_mb);

#endif // RAMDISK_H

