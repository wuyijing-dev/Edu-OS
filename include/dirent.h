/*
 * dirent.h - 目录操作定义
 */

#ifndef _DIRENT_H
#define _DIRENT_H

#include <types.h>

/* 文件类型 */
#define DT_UNKNOWN  0
#define DT_FIFO     1
#define DT_CHR      2
#define DT_DIR      4
#define DT_BLK      6
#define DT_REG      8
#define DT_LNK      10
#define DT_SOCK     12

/* 目录项结构 */
struct dirent {
    uint32_t d_ino;          /* Inode 号 */
    uint32_t d_off;          /* 偏移量 */
    uint16_t d_reclen;       /* 记录长度 */
    uint8_t  d_type;         /* 文件类型 */
    char     d_name[256];    /* 文件名 */
};

/* 系统调用 */
int sys_opendir(const char *path);
int sys_readdir(int fd, struct dirent *entry);
int sys_closedir(int fd);
int sys_mkdir(const char *path, mode_t mode);
int sys_rmdir(const char *path);
char *sys_getcwd(char *buf, size_t size);
int sys_chdir(const char *path);

#endif /* _DIRENT_H */
