/*
 * procfs.h - 进程文件系统（ProcFS）接口
 * 
 * Linux 风格的 /proc 文件系统实现
 * 特点：
 *   - 伪文件系统（不占用磁盘）
 *   - 动态内容生成（每次读取时实时生成）
 *   - 导出内核和进程信息
 */

#ifndef PROCFS_H
#define PROCFS_H

#include <types.h>
#include <fs/vfs.h>

/* ========== 常量定义 ========== */

#define PROC_NAME_MAX   256     /* 最大文件名长度 */
#define PROC_BUFFER_SIZE 4096   /* 单次读取最大缓冲区 */

/* ========== 数据结构 ========== */

/* Proc 条目类型 */
enum proc_entry_type {
    PROC_ENTRY_DIR,             /* 目录 */
    PROC_ENTRY_FILE,            /* 普通文件 */
    PROC_ENTRY_SYMLINK,         /* 符号链接 */
};

/* Proc 文件读取函数 */
typedef int (*proc_read_func_t)(char *buf, size_t size, off_t *offset, void *data);

/* Proc 文件写入函数（可选） */
typedef int (*proc_write_func_t)(const char *buf, size_t size, off_t *offset, void *data);

/* Proc 目录遍历函数 */
typedef int (*proc_readdir_func_t)(struct vfs_file *file, void *dirent, void *data);

/* Proc Inode 私有数据 */
struct proc_inode_data {
    enum proc_entry_type type;  /* 条目类型 */
    pid_t pid;                  /* 关联的进程 PID（0表示全局） */
    
    /* 动态内容生成函数 */
    proc_read_func_t read_func;
    proc_write_func_t write_func;
    proc_readdir_func_t readdir_func;
    
    void *private_data;         /* 私有数据指针 */
};

/* Proc 文件定义（用于静态定义文件） */
struct proc_file_def {
    const char *name;           /* 文件名 */
    uint32_t mode;              /* 权限（如 0444 表示只读） */
    proc_read_func_t read_func; /* 读取函数 */
    proc_write_func_t write_func; /* 写入函数（可为 NULL） */
};

/* Proc 超级块信息 */
struct procfs_sb_info {
    struct vfs_superblock *sb;  /* VFS 超级块 */
    uint32_t next_ino;          /* 下一个 inode 号 */
};

/* ========== ProcFS 核心接口 ========== */

/*
 * 初始化 ProcFS
 */
int procfs_init(void);

/*
 * 挂载 ProcFS 到指定路径
 */
int procfs_mount(const char *path);

/*
 * 创建 Proc 文件
 * @param parent: 父目录 inode
 * @param name: 文件名
 * @param mode: 权限
 * @param read_func: 读取函数
 * @param write_func: 写入函数（可为 NULL）
 * @return: 新创建的 inode，失败返回 NULL
 */
struct vfs_inode *procfs_create_file(
    struct vfs_inode *parent,
    const char *name,
    uint32_t mode,
    proc_read_func_t read_func,
    proc_write_func_t write_func
);

/*
 * 创建 Proc 目录
 * @param parent: 父目录 inode
 * @param name: 目录名
 * @param readdir_func: readdir 函数
 * @return: 新创建的 inode，失败返回 NULL
 */
struct vfs_inode *procfs_create_dir(
    struct vfs_inode *parent,
    const char *name,
    proc_readdir_func_t readdir_func
);

/* ========== 全局 Proc 文件接口 ========== */

/*
 * /proc/cpuinfo - CPU 信息
 */
int proc_cpuinfo_read(char *buf, size_t size, off_t *offset, void *data);

/*
 * /proc/meminfo - 内存信息
 */
int proc_meminfo_read(char *buf, size_t size, off_t *offset, void *data);

/*
 * /proc/uptime - 系统运行时间
 */
int proc_uptime_read(char *buf, size_t size, off_t *offset, void *data);

/*
 * /proc/version - 内核版本
 */
int proc_version_read(char *buf, size_t size, off_t *offset, void *data);

/* ========== 进程 Proc 文件接口 ========== */

/*
 * /proc/[pid]/status - 进程状态
 */
int proc_pid_status_read(char *buf, size_t size, off_t *offset, void *data);

/*
 * /proc/[pid]/cmdline - 命令行
 */
int proc_pid_cmdline_read(char *buf, size_t size, off_t *offset, void *data);

/*
 * /proc/[pid]/stat - 统计信息
 */
int proc_pid_stat_read(char *buf, size_t size, off_t *offset, void *data);

/* ========== 辅助函数 ========== */

/*
 * 处理带 offset 的读取
 * @param content: 完整内容
 * @param content_len: 内容长度
 * @param buf: 输出缓冲区
 * @param size: 缓冲区大小
 * @param offset: 当前偏移（会被更新）
 * @return: 实际读取的字节数，0 表示 EOF
 */
int procfs_read_with_offset(
    const char *content,
    size_t content_len,
    char *buf,
    size_t size,
    off_t *offset
);

/*
 * 查找进程
 * @param pid: 进程 PID
 * @return: 进程指针，未找到返回 NULL
 */
struct process *procfs_find_process(pid_t pid);

#endif // PROCFS_H

