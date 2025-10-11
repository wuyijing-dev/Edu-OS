/*
 * devfs.h - 设备文件系统（Linux风格）
 */

#ifndef DEVFS_H
#define DEVFS_H

#include <fs/vfs.h>

/* 标准设备的主设备号（Linux兼容） */
#define DEV_NULL_MAJOR      1
#define DEV_ZERO_MAJOR      1
#define DEV_CONSOLE_MAJOR   5
#define DEV_TTY_MAJOR       4
#define DEV_RANDOM_MAJOR    1

/* DevFS初始化和管理 */
int devfs_init(void);
int devfs_register_device(const char *name, uint32_t major, uint32_t minor,
                          struct vfs_file_operations *fops);
int devfs_unregister_device(const char *name);

#endif // DEVFS_H

