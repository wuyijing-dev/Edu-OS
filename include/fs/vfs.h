/*
 * vfs.h - 简化的虚拟文件系统（Linux风格）
 */

#ifndef VFS_H
#define VFS_H

#include <types.h>

/* 文件类型和模式（Linux风格） */
#define S_IFMT      0170000  /* 文件类型掩码 */
#define S_IFSOCK    0140000  /* socket */
#define S_IFLNK     0120000  /* 符号链接 */
#define S_IFREG     0100000  /* 普通文件 */
#define S_IFBLK     0060000  /* 块设备 */
#define S_IFDIR     0040000  /* 目录 */
#define S_IFCHR     0020000  /* 字符设备 */
#define S_IFIFO     0010000  /* FIFO */

/* 文件类型判断宏 */
#define S_ISDIR(m)  (((m) & S_IFMT) == S_IFDIR)
#define S_ISREG(m)  (((m) & S_IFMT) == S_IFREG)
#define S_ISLNK(m)  (((m) & S_IFMT) == S_IFLNK)
#define S_ISCHR(m)  (((m) & S_IFMT) == S_IFCHR)
#define S_ISBLK(m)  (((m) & S_IFMT) == S_IFBLK)
#define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)

/* 权限位 */
#define S_IRWXU     00700    /* 用户读写执行 */
#define S_IRUSR     00400    /* 用户读 */
#define S_IWUSR     00200    /* 用户写 */
#define S_IXUSR     00100    /* 用户执行 */

/* 打开标志 */
#define O_RDONLY    0
#define O_WRONLY    1
#define O_RDWR      2
#define O_CREAT     0x0040
#define O_EXCL      0x0080
#define O_NONBLOCK  0x0800
#define O_TRUNC     0x0200
#define O_APPEND    0x0400

/* 错误码 */
#define ENOENT      2    /* No such file or directory */
#define ESRCH       3    /* No such process */
#define EINTR       4    /* Interrupted system call */
#define EIO         5    /* I/O error */
#define EBADF       9    /* Bad file descriptor */
#define ECHILD      10   /* No child processes */
#define ENOMEM      12   /* Out of memory */
#define EFAULT      14   /* Bad address */
#define EEXIST      17   /* File exists */
#define ENOTDIR     20   /* Not a directory */
#define EISDIR      21   /* Is a directory */
#define EINVAL      22   /* Invalid argument */
#define ENOSPC      28   /* No space left on device */
#define ENAMETOOLONG 36  /* File name too long */
#define ENOSYS      38   /* Function not implemented */
#define ENOTEMPTY   39   /* Directory not empty */
#define EPIPE       32   /* Broken pipe */
#define EAGAIN      11   /* Try again */

/* 设备号操作宏 */
#define MINORBITS   20
#define MINORMASK   ((1U << MINORBITS) - 1)
#define MAJOR(dev)  ((unsigned int) ((dev) >> MINORBITS))
#define MINOR(dev)  ((unsigned int) ((dev) & MINORMASK))
#define MKDEV(ma,mi) (((ma) << MINORBITS) | (mi))

/* 最大值 */
#define VFS_MAX_NAME    256
#define VFS_MAX_OPEN_FILES 64

/* 前向声明 */
struct vfs_superblock;
struct vfs_inode;
struct vfs_dentry;
struct vfs_file;

/* ========== 操作表 ========== */

/* Superblock操作表 */
struct vfs_superblock_operations {
    struct vfs_inode* (*alloc_inode)(struct vfs_superblock *sb);
    void (*destroy_inode)(struct vfs_inode *inode);
    void (*put_super)(struct vfs_superblock *sb);
};

/* File操作表 */
/* 前向声明 */
struct wait_queue_head;

struct vfs_file_operations {
    int (*open)(struct vfs_inode *inode, struct vfs_file *file);
    int (*release)(struct vfs_file *file);
    int (*read)(struct vfs_file *file, char *buf, size_t count);
    int (*write)(struct vfs_file *file, const char *buf, size_t count);
    int (*ioctl)(struct vfs_file *file, uint32_t cmd, unsigned long arg);
    unsigned int (*poll)(struct vfs_file *file, struct wait_queue_head *wait);  /* Linux风格poll */
};

/* Inode操作表 */
struct vfs_inode_operations {
    struct vfs_dentry* (*lookup)(struct vfs_inode *dir, const char *name);
    int (*create)(struct vfs_inode *dir, const char *name, uint32_t mode);
    int (*mkdir)(struct vfs_inode *dir, const char *name, uint32_t mode);
};

/* ========== 核心数据结构 ========== */

/* Superblock */
struct vfs_superblock {
    const char *fstype;
    struct vfs_inode *root;
    struct vfs_superblock_operations *s_op;
    void *private_data;
};

/* Inode (Linux风格：添加引用计数) */
struct vfs_inode {
    uint32_t ino;
    uint32_t mode;
    uint32_t size;
    uint32_t rdev;  /* 设备号 */
    uint32_t ref_count;     /* Linux风格：引用计数 */
    struct vfs_superblock *sb;
    struct vfs_inode_operations *i_op;
    struct vfs_file_operations *f_op;
    void *private_data;
};

/* Dentry */
struct vfs_dentry {
    char name[VFS_MAX_NAME];
    struct vfs_inode *inode;
    struct vfs_dentry *parent;
    struct vfs_dentry *child;
    struct vfs_dentry *sibling;
};

/* File (Linux风格：添加引用计数) */
struct vfs_file {
    struct vfs_dentry *dentry;
    struct vfs_inode *inode;
    struct vfs_file_operations *f_op;
    uint32_t flags;
    uint32_t pos;
    uint32_t ref_count;     /* Linux风格：引用计数 */
    void *private_data;
};

/* ========== VFS接口 ========== */

/* lseek whence 参数 */
#define SEEK_SET    0    /* 从文件开头 */
#define SEEK_CUR    1    /* 从当前位置 */
#define SEEK_END    2    /* 从文件末尾 */

/* 系统调用级接口 */
void vfs_init(void);
int vfs_open(const char *path, int flags, int mode);
int vfs_close(int fd);
int vfs_read(int fd, char *buf, size_t count);
int vfs_write(int fd, const char *buf, size_t count);
off_t vfs_lseek(int fd, off_t offset, int whence);

/* 内部函数 */
struct vfs_dentry *vfs_lookup(const char *path);
int vfs_register_filesystem(const char *name, struct vfs_superblock *sb);
struct vfs_dentry *vfs_get_root_dentry(void);

/* 辅助函数 */
struct vfs_inode *vfs_alloc_inode(struct vfs_superblock *sb, uint32_t ino);
void vfs_free_inode(struct vfs_inode *inode);
struct vfs_dentry *vfs_alloc_dentry(const char *name, struct vfs_inode *inode);
void vfs_free_dentry(struct vfs_dentry *dentry);
int vfs_add_child_dentry(struct vfs_dentry *parent, struct vfs_dentry *child);

/* Linux风格：引用计数管理 */
struct vfs_inode *inode_get(struct vfs_inode *inode);   /* 增加inode引用 */
void inode_put(struct vfs_inode *inode);                /* 减少inode引用 */
struct vfs_file *file_get(struct vfs_file *file);       /* 增加file引用 */
void file_put(struct vfs_file *file);                   /* 减少file引用 */

/* Linux风格：进程独立的文件操作 */
struct vfs_file *vfs_open_file(const char *path, int flags, int mode);
ssize_t vfs_file_read(struct vfs_file *file, void *buf, size_t count);
ssize_t vfs_file_write(struct vfs_file *file, const void *buf, size_t count);
struct vfs_file *vfs_state_get_file(int fd);

#endif // VFS_H


