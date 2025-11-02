/*
 * stat.h - POSIX文件状态定义
 * 
 * 符合POSIX.1-2017标准
 */

#ifndef _SYS_STAT_H
#define _SYS_STAT_H

#include <types.h>

/* 文件类型和权限 */
#define S_IFMT      0170000   /* 文件类型掩码 */
#define S_IFSOCK    0140000   /* socket */
#define S_IFLNK     0120000   /* 符号链接 */
#define S_IFREG     0100000   /* 普通文件 */
#define S_IFBLK     0060000   /* 块设备 */
#define S_IFDIR     0040000   /* 目录 */
#define S_IFCHR     0020000   /* 字符设备 */
#define S_IFIFO     0010000   /* FIFO/管道 */

/* 文件权限 */
#define S_ISUID     04000     /* set-user-ID */
#define S_ISGID     02000     /* set-group-ID */
#define S_ISVTX     01000     /* sticky bit */

#define S_IRWXU     00700     /* 所有者读写执行 */
#define S_IRUSR     00400     /* 所有者读 */
#define S_IWUSR     00200     /* 所有者写 */
#define S_IXUSR     00100     /* 所有者执行 */

#define S_IRWXG     00070     /* 组读写执行 */
#define S_IRGRP     00040     /* 组读 */
#define S_IWGRP     00020     /* 组写 */
#define S_IXGRP     00010     /* 组执行 */

#define S_IRWXO     00007     /* 其他读写执行 */
#define S_IROTH     00004     /* 其他读 */
#define S_IWOTH     00002     /* 其他写 */
#define S_IXOTH     00001     /* 其他执行 */

/* 文件类型判断宏 */
#define S_ISREG(m)  (((m) & S_IFMT) == S_IFREG)   /* 普通文件 */
#define S_ISDIR(m)  (((m) & S_IFMT) == S_IFDIR)   /* 目录 */
#define S_ISCHR(m)  (((m) & S_IFMT) == S_IFCHR)   /* 字符设备 */
#define S_ISBLK(m)  (((m) & S_IFMT) == S_IFBLK)   /* 块设备 */
#define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)   /* FIFO */
#define S_ISLNK(m)  (((m) & S_IFMT) == S_IFLNK)   /* 符号链接 */
#define S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK)  /* socket */

/* stat结构（POSIX标准） */
struct stat {
    dev_t     st_dev;      /* 设备ID */
    ino_t     st_ino;      /* inode号 */
    mode_t    st_mode;     /* 文件类型和权限 */
    nlink_t   st_nlink;    /* 硬链接数 */
    uid_t     st_uid;      /* 所有者用户ID */
    gid_t     st_gid;      /* 所有者组ID */
    dev_t     st_rdev;     /* 设备ID（如果是设备文件）*/
    off_t     st_size;     /* 文件大小（字节）*/
    blksize_t st_blksize;  /* 块大小 */
    blkcnt_t  st_blocks;   /* 分配的块数 */
    time_t    st_atime;    /* 最后访问时间 */
    time_t    st_mtime;    /* 最后修改时间 */
    time_t    st_ctime;    /* 最后状态改变时间 */
};

/* 系统调用原型 */
#ifndef __KERNEL__
int stat(const char *path, struct stat *buf);
int fstat(int fd, struct stat *buf);
int lstat(const char *path, struct stat *buf);
int chmod(const char *path, mode_t mode);
int fchmod(int fd, mode_t mode);
int mkdir(const char *path, mode_t mode);
int mkfifo(const char *path, mode_t mode);
#endif

#endif /* _SYS_STAT_H */

