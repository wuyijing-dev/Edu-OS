/*
 * procfs.c - 进程文件系统核心实现
 *
 * 基于 Linux procfs 设计：
 *   - 伪文件系统（不需要磁盘）
 *   - 动态内容生成
 *   - 实时反映内核状态
 */

#include <fs/procfs.h>
#include <fs/vfs.h>
#include <fs/devfs.h>
#include <kernel.h>
#include <string.h>
#include <mm/kmalloc.h>
#include <process/process.h>

/* ========== 全局变量 ========== */

static struct procfs_sb_info g_procfs_sb_info;
static struct vfs_superblock *g_procfs_sb = NULL;
static struct vfs_inode *g_procfs_root = NULL;
static bool g_procfs_initialized = false;

/* ========== 静态函数声明 ========== */

static struct vfs_inode *procfs_alloc_inode(struct vfs_superblock *sb, uint32_t mode);
static int procfs_file_open(struct vfs_inode *inode, struct vfs_file *file);
static int procfs_file_release(struct vfs_file *file);
static int procfs_file_read(struct vfs_file *file, char *buf, size_t count);
static int procfs_file_write(struct vfs_file *file, const char *buf, size_t count);
static struct vfs_dentry *procfs_lookup(struct vfs_inode *dir, const char *name);
static int procfs_root_readdir(struct vfs_file *file, void *dirent, void *data);

/* ========== 操作表 ========== */

/* Proc 文件操作表 */
static struct vfs_file_operations procfs_file_ops = {
    .open = procfs_file_open,
    .release = procfs_file_release,
    .read = procfs_file_read,
    .write = procfs_file_write,
};

/* Proc 目录 inode 操作表 */
static struct vfs_inode_operations procfs_dir_iops = {
    .lookup = procfs_lookup,
};

/* ========== 辅助函数 ========== */

/*
 * 处理带 offset 的读取
 */
int procfs_read_with_offset(
    const char *content,
    size_t content_len,
    char *buf,
    size_t size,
    off_t *offset)
{
    /* 已经读到末尾 */
    if (*offset >= (off_t)content_len) {
        return 0;  // EOF
    }
    
    /* 计算本次要读取的字节数 */
    size_t remaining = content_len - *offset;
    size_t to_read = (remaining > size) ? size : remaining;
    
    /* 复制数据 */
    memcpy(buf, content + *offset, to_read);
    *offset += to_read;
    
    return to_read;
}

/*
 * 查找进程
 */
struct process *procfs_find_process(pid_t pid)
{
    return process_find_by_pid(pid);
}

/* ========== Inode 管理 ========== */

/*
 * 分配 Proc Inode
 */
static struct vfs_inode *procfs_alloc_inode(struct vfs_superblock *sb, uint32_t mode)
{
    struct vfs_inode *inode = vfs_alloc_inode(sb, g_procfs_sb_info.next_ino++);
    if (!inode) {
        return NULL;
    }
    
    inode->mode = mode;
    inode->size = 0;  // Proc 文件大小为 0（动态生成）
    inode->rdev = 0;
    inode->sb = sb;
    
    /* 分配私有数据 */
    struct proc_inode_data *data = kmalloc(sizeof(struct proc_inode_data));
    if (!data) {
        vfs_free_inode(inode);
        return NULL;
    }
    
    memset(data, 0, sizeof(struct proc_inode_data));
    inode->private_data = data;
    
    /* 根据类型设置操作表 */
    if (S_ISDIR(mode)) {
        inode->f_op = &procfs_file_ops;
        inode->i_op = &procfs_dir_iops;
        data->type = PROC_ENTRY_DIR;
    } else if (S_ISREG(mode)) {
        inode->f_op = &procfs_file_ops;
        inode->i_op = NULL;
        data->type = PROC_ENTRY_FILE;
    }
    
    return inode;
}

/* ========== 文件操作 ========== */

/*
 * open 操作
 */
static int procfs_file_open(struct vfs_inode *inode, struct vfs_file *file)
{
    (void)inode;
    file->pos = 0;
    return 0;
}

/*
 * release 操作
 */
static int procfs_file_release(struct vfs_file *file)
{
    (void)file;
    return 0;
}

/*
 * read 操作（动态生成内容）
 */
static int procfs_file_read(struct vfs_file *file, char *buf, size_t count)
{
    if (!file || !file->inode || !buf) {
        return -EINVAL;
    }
    
    struct proc_inode_data *data = file->inode->private_data;
    if (!data || !data->read_func) {
        return -EINVAL;
    }
    
    /* 调用读取函数（动态生成内容） */
    off_t offset = file->pos;
    int ret = data->read_func(buf, count, &offset, data->private_data);
    
    if (ret > 0) {
        file->pos = offset;
    }
    
    return ret;
}

/*
 * write 操作（部分文件支持）
 */
static int procfs_file_write(struct vfs_file *file, const char *buf, size_t count)
{
    if (!file || !file->inode || !buf) {
        return -EINVAL;
    }
    
    struct proc_inode_data *data = file->inode->private_data;
    if (!data || !data->write_func) {
        return -EINVAL;  // 大部分 proc 文件只读
    }
    
    /* 调用写入函数 */
    off_t offset = file->pos;
    int ret = data->write_func(buf, count, &offset, data->private_data);
    
    if (ret > 0) {
        file->pos = offset;
    }
    
    return ret;
}

/* ========== 目录操作 ========== */

/*
 * lookup 操作
 */
static struct vfs_dentry *procfs_lookup(struct vfs_inode *dir, const char *name)
{
    if (!dir || !name) {
        return NULL;
    }
    
    /* 检查是否是全局文件 */
    static struct proc_file_def global_files[] = {
        { "cpuinfo",  S_IFREG | 0444, proc_cpuinfo_read,  NULL },
        { "meminfo",  S_IFREG | 0444, proc_meminfo_read,  NULL },
        { "uptime",   S_IFREG | 0444, proc_uptime_read,   NULL },
        { "version",  S_IFREG | 0444, proc_version_read,  NULL },
        { NULL,       0,              NULL,               NULL }
    };
    
    for (int i = 0; global_files[i].name; i++) {
        if (strcmp(name, global_files[i].name) == 0) {
            /* 创建文件 inode */
            struct vfs_inode *inode = procfs_alloc_inode(dir->sb, global_files[i].mode);
            if (!inode) {
                return NULL;
            }
            
            struct proc_inode_data *data = inode->private_data;
            data->read_func = global_files[i].read_func;
            data->write_func = global_files[i].write_func;
            data->pid = 0;  // 全局文件
            
            /* 创建 dentry */
            struct vfs_dentry *dentry = vfs_alloc_dentry(name, inode);
            return dentry;
        }
    }
    
    /* 检查是否是 PID 目录 */
    pid_t pid = 0;
    for (const char *p = name; *p; p++) {
        if (*p >= '0' && *p <= '9') {
            pid = pid * 10 + (*p - '0');
        } else {
            /* 不是数字，不是 PID */
            return NULL;
        }
    }
    
    /* 查找进程 */
    struct process *proc = procfs_find_process(pid);
    if (!proc) {
        return NULL;  // 进程不存在
    }
    
    /* 创建进程目录 inode */
    struct vfs_inode *inode = procfs_alloc_inode(dir->sb, S_IFDIR | 0555);
    if (!inode) {
        return NULL;
    }
    
    struct proc_inode_data *data = inode->private_data;
    data->pid = pid;
    data->readdir_func = NULL;  // TODO: 实现进程目录 readdir
    
    /* 创建 dentry */
    struct vfs_dentry *dentry = vfs_alloc_dentry(name, inode);
    return dentry;
}

/*
 * root 目录 readdir
 */
static int procfs_root_readdir(struct vfs_file *file, void *dirent, void *data)
{
    /* TODO: 实现完整的 readdir */
    (void)file;
    (void)dirent;
    (void)data;
    return 0;
}

/* ========== 公共接口 ========== */

/*
 * 创建 Proc 文件
 */
struct vfs_inode *procfs_create_file(
    struct vfs_inode *parent,
    const char *name,
    uint32_t mode,
    proc_read_func_t read_func,
    proc_write_func_t write_func)
{
    if (!parent || !name || !read_func) {
        return NULL;
    }
    
    /* 创建 inode */
    struct vfs_inode *inode = procfs_alloc_inode(parent->sb, S_IFREG | mode);
    if (!inode) {
        return NULL;
    }
    
    struct proc_inode_data *data = inode->private_data;
    data->read_func = read_func;
    data->write_func = write_func;
    
    /* 添加到父目录 */
    struct vfs_dentry *dentry = vfs_alloc_dentry(name, inode);
    if (!dentry) {
        vfs_free_inode(inode);
        return NULL;
    }
    
    /* 获取根 dentry 并添加子项 */
    struct vfs_dentry *root_dentry = vfs_get_root_dentry();
    if (root_dentry) {
        vfs_add_child_dentry(root_dentry, dentry);
    }
    
    return inode;
}

/*
 * 创建 Proc 目录
 */
struct vfs_inode *procfs_create_dir(
    struct vfs_inode *parent,
    const char *name,
    proc_readdir_func_t readdir_func)
{
    if (!parent || !name) {
        return NULL;
    }
    
    /* 创建 inode */
    struct vfs_inode *inode = procfs_alloc_inode(parent->sb, S_IFDIR | 0555);
    if (!inode) {
        return NULL;
    }
    
    struct proc_inode_data *data = inode->private_data;
    data->readdir_func = readdir_func;
    
    /* 添加到父目录 */
    struct vfs_dentry *dentry = vfs_alloc_dentry(name, inode);
    if (!dentry) {
        vfs_free_inode(inode);
        return NULL;
    }
    
    /* 获取根 dentry 并添加子项 */
    struct vfs_dentry *root_dentry = vfs_get_root_dentry();
    if (root_dentry) {
        vfs_add_child_dentry(root_dentry, dentry);
    }
    
    return inode;
}

/*
 * 初始化 ProcFS
 */
int procfs_init(void)
{
    if (g_procfs_initialized) {
        kprintf("[ProcFS] Already initialized\n");
        return 0;
    }
    
    kprintf("[ProcFS] Initializing Process Filesystem...\n");
    
    /* 创建超级块 */
    g_procfs_sb = kmalloc(sizeof(struct vfs_superblock));
    if (!g_procfs_sb) {
        kprintf("[ProcFS] Failed to allocate superblock\n");
        return -ENOMEM;
    }
    
    memset(g_procfs_sb, 0, sizeof(struct vfs_superblock));
    g_procfs_sb->fstype = "proc";
    g_procfs_sb->s_op = NULL;  // ProcFS 不需要超级块操作
    
    /* 初始化 sb_info */
    g_procfs_sb_info.sb = g_procfs_sb;
    g_procfs_sb_info.next_ino = 1;
    g_procfs_sb->private_data = &g_procfs_sb_info;
    
    /* 创建根 inode */
    g_procfs_root = procfs_alloc_inode(g_procfs_sb, S_IFDIR | 0555);
    if (!g_procfs_root) {
        kprintf("[ProcFS] Failed to create root inode\n");
        kfree(g_procfs_sb);
        return -ENOMEM;
    }
    
    struct proc_inode_data *root_data = g_procfs_root->private_data;
    root_data->readdir_func = procfs_root_readdir;
    
    g_procfs_sb->root = g_procfs_root;
    
    /* 注册到 VFS */
    int ret = vfs_register_filesystem("proc", g_procfs_sb);
    if (ret < 0) {
        kprintf("[ProcFS] Failed to register filesystem: %d\n", ret);
        vfs_free_inode(g_procfs_root);
        kfree(g_procfs_sb);
        return ret;
    }
    
    g_procfs_initialized = true;
    kprintf("[ProcFS] ProcFS initialized successfully\n");
    
    return 0;
}

/*
 * 挂载 ProcFS
 */
int procfs_mount(const char *path)
{
    if (!g_procfs_initialized) {
        kprintf("[ProcFS] Not initialized, call procfs_init() first\n");
        return -EINVAL;
    }
    
    if (!path) {
        path = "/proc";
    }
    
    kprintf("[ProcFS] Mounting procfs at %s\n", path);
    
    /* TODO: 实现 VFS mount 接口 */
    /* vfs_mount(path, "proc", 0, NULL); */
    
    return 0;
}

