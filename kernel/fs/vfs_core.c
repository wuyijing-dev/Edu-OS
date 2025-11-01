/*
 * vfs_core.c - VFS核心实现（Linux风格）
 */

#include <fs/vfs.h>
#include <fs/fat32.h>
#include <kernel.h>
#include <mm/kmalloc.h>
#include <string.h>

/* VFS全局状态 */
static struct {
    struct vfs_dentry *root_dentry;
    struct vfs_superblock *root_sb;
    struct vfs_file *open_files[VFS_MAX_OPEN_FILES];
    uint32_t next_fd;
    bool initialized;
} vfs_state;

/* ========== 工具函数 ========== */

/*
 * 分配inode
 */
struct vfs_inode *vfs_alloc_inode(struct vfs_superblock *sb, uint32_t ino)
{
    struct vfs_inode *inode = (struct vfs_inode*)kmalloc(sizeof(struct vfs_inode));
    if (!inode) {
        return NULL;
    }
    
    memset(inode, 0, sizeof(struct vfs_inode));
    inode->ino = ino;
    inode->sb = sb;
    
    return inode;
}

/*
 * 释放inode
 */
void vfs_free_inode(struct vfs_inode *inode)
{
    if (inode) {
        kfree(inode);
    }
}

/*
 * 分配dentry
 */
struct vfs_dentry *vfs_alloc_dentry(const char *name, struct vfs_inode *inode)
{
    if (!name) {
        return NULL;
    }
    
    struct vfs_dentry *dentry = (struct vfs_dentry*)kmalloc(sizeof(struct vfs_dentry));
    if (!dentry) {
        return NULL;
    }
    
    memset(dentry, 0, sizeof(struct vfs_dentry));
    strncpy(dentry->name, name, VFS_MAX_NAME - 1);
    dentry->name[VFS_MAX_NAME - 1] = '\0';
    dentry->inode = inode;
    
    return dentry;
}

/*
 * 释放dentry
 */
void vfs_free_dentry(struct vfs_dentry *dentry)
{
    if (dentry) {
        kfree(dentry);
    }
}

/*
 * 添加子dentry
 */
int vfs_add_child_dentry(struct vfs_dentry *parent, struct vfs_dentry *child)
{
    if (!parent || !child) {
        return -EINVAL;
    }
    
    child->parent = parent;
    child->sibling = parent->child;
    parent->child = child;
    
    return 0;
}

/* ========== 路径解析 ========== */

/*
 * 在目录中查找子项
 */
static struct vfs_dentry *lookup_child(struct vfs_dentry *parent, const char *name)
{
    if (!parent || !name) {
        return NULL;
    }
    
    /* 特殊情况："." */
    if (strcmp(name, ".") == 0) {
        return parent;
    }
    
    /* 特殊情况：".." */
    if (strcmp(name, "..") == 0) {
        return parent->parent ? parent->parent : parent;
    }
    
    /* 遍历子项 */
    struct vfs_dentry *child = parent->child;
    while (child) {
        if (strcmp(child->name, name) == 0) {
            return child;
        }
        child = child->sibling;
    }
    
    /* 如果inode有lookup操作，调用它 */
    if (parent->inode && parent->inode->i_op && parent->inode->i_op->lookup) {
        return parent->inode->i_op->lookup(parent->inode, name);
    }
    
    return NULL;
}

/*
 * 路径解析
 */
struct vfs_dentry *vfs_lookup(const char *path)
{
    if (!path || !vfs_state.initialized || !vfs_state.root_dentry) {
        return NULL;
    }
    
    /* 空路径或根路径 */
    if (path[0] == '\0' || (path[0] == '/' && path[1] == '\0')) {
        return vfs_state.root_dentry;
    }
    
    /* 必须是绝对路径 */
    if (path[0] != '/') {
        return NULL;
    }
    
    /* 特殊处理：/proc 路径（临时解决方案） */
    if (strncmp(path, "/proc", 5) == 0) {
        /* 调用 procfs_lookup（需要外部声明） */
        extern struct vfs_dentry *procfs_lookup_path(const char *path);
        return procfs_lookup_path(path);
    }
    
    /* 从根目录开始（DevFS） */
    struct vfs_dentry *current = vfs_state.root_dentry;
    const char *p = path + 1;  /* 跳过开头的'/' */
    
    /* 逐个解析路径组件 */
    while (*p) {
        /* 跳过连续的'/' */
        while (*p == '/') {
            p++;
        }
        
        if (*p == '\0') {
            break;
        }
        
        /* 提取组件名 */
        char component[VFS_MAX_NAME];
        int i = 0;
        while (*p && *p != '/' && i < VFS_MAX_NAME - 1) {
            component[i++] = *p++;
        }
        component[i] = '\0';
        
        /* 查找子项 */
        current = lookup_child(current, component);
        if (!current) {
            /* DevFS 找不到，尝试 FAT32 */
            goto try_fat32;
        }
    }
    
    return current;

try_fat32:
    /* 尝试从 FAT32 查找 */
    extern struct fat32_fs_info *fat32_get_fs(void);
    extern struct fat32_dir_entry *fat32_lookup(struct fat32_fs_info *fs, const char *path);
    
    struct fat32_fs_info *fat32 = fat32_get_fs();
    if (fat32) {
        struct fat32_dir_entry *fat_entry = fat32_lookup(fat32, path);
        if (fat_entry) {
            /* 找到了！创建临时 dentry */
            extern struct vfs_dentry *vfs_alloc_dentry(const char *name, struct vfs_inode *inode);
            extern struct vfs_inode *vfs_alloc_inode(struct vfs_superblock *sb, uint32_t ino);
            
            /* 提取文件名 */
            const char *name = strrchr(path, '/');
            name = name ? name + 1 : path;
            
            /* 创建临时 inode */
            struct vfs_inode *inode = vfs_alloc_inode(NULL, 0);
            if (inode) {
                inode->size = fat_entry->file_size;
                inode->private_data = fat_entry;
                
                /* 设置 FAT32 文件操作 */
                extern struct vfs_file_operations fat32_file_ops;
                inode->f_op = &fat32_file_ops;
                
                return vfs_alloc_dentry(name, inode);
            }
            
            kfree(fat_entry);
        }
    }
    
    return NULL;
}

/* ========== 文件描述符管理 ========== */

/*
 * 分配文件描述符
 */
static int alloc_fd(void)
{
    for (uint32_t i = 0; i < VFS_MAX_OPEN_FILES; i++) {
        if (!vfs_state.open_files[i]) {
            return i;
        }
    }
    return -1;
}

/*
 * 分配file对象
 */
static struct vfs_file *alloc_file(void)
{
    struct vfs_file *file = (struct vfs_file*)kmalloc(sizeof(struct vfs_file));
    if (!file) {
        return NULL;
    }
    
    memset(file, 0, sizeof(struct vfs_file));
    return file;
}

/*
 * 释放file对象
 */
static void free_file(struct vfs_file *file)
{
    if (file) {
        kfree(file);
    }
}

/* ========== VFS系统调用实现 ========== */

/*
 * VFS初始化
 */
void vfs_init(void)
{
    kprintf("[VFS] Initializing Virtual File System...\n");
    
    memset(&vfs_state, 0, sizeof(vfs_state));
    vfs_state.next_fd = 0;
    vfs_state.initialized = false;
    
    /* 注意：root_dentry和root_sb由具体文件系统（如devfs）设置 */
    
    kprintf("[VFS] VFS core initialized\n");
}

/*
 * 注册文件系统并设置为根
 */
int vfs_register_filesystem(const char *name, struct vfs_superblock *sb)
{
    if (!name || !sb || !sb->root) {
        return -EINVAL;
    }
    
    /* 创建根dentry */
    struct vfs_dentry *root_dentry = vfs_alloc_dentry("/", sb->root);
    if (!root_dentry) {
        return -ENOMEM;
    }
    
    vfs_state.root_dentry = root_dentry;
    vfs_state.root_sb = sb;
    vfs_state.initialized = true;
    
    kprintf("[VFS] Registered filesystem: %s\n", name);
    
    return 0;
}

/*
 * open系统调用
 */
int vfs_open(const char *path, int flags, int mode)
{
    (void)mode;  /* 暂不使用 */
    
    if (!path || !vfs_state.initialized) {
        return -EINVAL;
    }
    
    /* 路径解析 */
    struct vfs_dentry *dentry = vfs_lookup(path);
    if (!dentry) {
        return -ENOENT;
    }
    
    if (!dentry->inode) {
        return -ENOENT;
    }
    
    /* 分配file对象 */
    struct vfs_file *file = alloc_file();
    if (!file) {
        return -ENOMEM;
    }
    
    file->dentry = dentry;
    file->inode = dentry->inode;
    file->f_op = dentry->inode->f_op;
    file->flags = flags;
    file->pos = 0;
    file->private_data = dentry->inode->private_data;  // ← 传递 FAT32 目录项！
    
    /* 调用文件系统的open */
    if (file->f_op && file->f_op->open) {
        int ret = file->f_op->open(file->inode, file);
        if (ret < 0) {
            free_file(file);
            return ret;
        }
    }
    
    /* 分配文件描述符 */
    int fd = alloc_fd();
    if (fd < 0) {
        if (file->f_op && file->f_op->release) {
            file->f_op->release(file);
        }
        free_file(file);
        return -ENOMEM;
    }
    
    vfs_state.open_files[fd] = file;
    
    return fd;
}

/*
 * close系统调用
 */
int vfs_close(int fd)
{
    if (fd < 0 || fd >= VFS_MAX_OPEN_FILES) {
        return -EBADF;
    }
    
    struct vfs_file *file = vfs_state.open_files[fd];
    if (!file) {
        return -EBADF;
    }
    
    /* 调用文件系统的release */
    if (file->f_op && file->f_op->release) {
        file->f_op->release(file);
    }
    
    /* 释放file对象 */
    free_file(file);
    vfs_state.open_files[fd] = NULL;
    
    return 0;
}

/*
 * read系统调用
 */
int vfs_read(int fd, char *buf, size_t count)
{
    if (fd < 0 || fd >= VFS_MAX_OPEN_FILES || !buf) {
        return -EINVAL;
    }
    
    struct vfs_file *file = vfs_state.open_files[fd];
    if (!file) {
        return -EBADF;
    }
    
    /* 检查权限 - 提取访问模式位（低2位）*/
    int access = file->flags & 3;  // 提取 O_RDONLY/O_WRONLY/O_RDWR
    if (access != O_RDONLY && access != O_RDWR) {
        return -EBADF;
    }
    
    /* 调用文件系统的read */
    if (!file->f_op || !file->f_op->read) {
        return -EINVAL;
    }
    
    return file->f_op->read(file, buf, count);
}

/*
 * write系统调用
 */
/*
 * vfs_state_get_file - 从全局VFS状态获取文件指针（用于进程fd_table初始化）
 */
struct vfs_file *vfs_state_get_file(int fd)
{
    if (fd < 0 || fd >= VFS_MAX_OPEN_FILES) {
        return NULL;
    }
    return vfs_state.open_files[fd];
}

/*
 * vfs_open_file - 直接打开文件并返回vfs_file指针（用于进程fd_table）
 */
struct vfs_file *vfs_open_file(const char *path, int flags, int mode)
{
    /* 使用全局vfs_open打开文件，然后复制文件结构 */
    int global_fd = vfs_open(path, flags, mode);
    if (global_fd < 0) {
        return NULL;
    }
    
    /* 获取全局文件结构 */
    struct vfs_file *global_file = vfs_state_get_file(global_fd);
    if (!global_file) {
        vfs_close(global_fd);
        return NULL;
    }
    
    /* 分配新的文件结构（每个进程独立） */
    struct vfs_file *file = kmalloc(sizeof(struct vfs_file));
    if (!file) {
        vfs_close(global_fd);
        return NULL;
    }
    
    /* 复制文件结构 */
    memcpy(file, global_file, sizeof(struct vfs_file));
    
    /* 重要：不要关闭全局fd！
     * 因为我们复制的vfs_file中的inode指针指向全局文件的inode
     * 如果关闭全局fd，inode可能被释放，导致后续读取失败
     * 
     * TODO: 实现inode引用计数机制
     * 目前的workaround：保持全局fd打开，让VFS管理其生命周期
     */
    /* vfs_close(global_fd); - 不要关闭！ */
    
    return file;
}

/*
 * vfs_file_read - 直接通过vfs_file指针读取（用于进程fd_table）
 */
ssize_t vfs_file_read(struct vfs_file *file, void *buf, size_t count)
{
    if (!file || !buf) {
        return -EINVAL;
    }
    
    /* 检查权限 */
    int access = file->flags & 3;
    if (access != O_RDONLY && access != O_RDWR) {
        return -EBADF;
    }
    
    /* 调用文件系统的read */
    if (!file->f_op || !file->f_op->read) {
        return -EINVAL;
    }
    
    return file->f_op->read(file, buf, count);
}

/*
 * vfs_file_write - 直接通过vfs_file指针写入（用于进程fd_table）
 */
ssize_t vfs_file_write(struct vfs_file *file, const void *buf, size_t count)
{
    if (!file || !buf) {
        return -EINVAL;
    }
    
    /* 检查权限 - 提取访问模式位（低2位）*/
    int access = file->flags & 3;  // 提取 O_RDONLY/O_WRONLY/O_RDWR
    if (access != O_WRONLY && access != O_RDWR) {
        return -EBADF;
    }
    
    /* 调用文件系统的write */
    if (!file->f_op || !file->f_op->write) {
        return -EINVAL;
    }
    
    return file->f_op->write(file, buf, count);
}

int vfs_write(int fd, const char *buf, size_t count)
{
    if (fd < 0 || fd >= VFS_MAX_OPEN_FILES || !buf) {
        return -EINVAL;
    }
    
    struct vfs_file *file = vfs_state.open_files[fd];
    if (!file) {
        return -EBADF;
    }
    
    return vfs_file_write(file, buf, count);
}

/*
 * lseek系统调用 - 改变文件读写位置
 */
off_t vfs_lseek(int fd, off_t offset, int whence)
{
    if (fd < 0 || fd >= VFS_MAX_OPEN_FILES) {
        return -EINVAL;
    }
    
    struct vfs_file *file = vfs_state.open_files[fd];
    if (!file) {
        return -EBADF;
    }
    
    off_t new_pos;
    
    switch (whence) {
        case SEEK_SET:
            new_pos = offset;
            break;
        case SEEK_CUR:
            new_pos = file->pos + offset;
            break;
        case SEEK_END:
            if (!file->inode) {
                return -EINVAL;
            }
            new_pos = file->inode->size + offset;
            break;
        default:
            return -EINVAL;
    }
    
    /* 不允许负数位置 */
    if (new_pos < 0) {
        return -EINVAL;
    }
    
    file->pos = new_pos;
    return new_pos;
}

/*
 * 获取根dentry（用于devfs等挂载）
 */
struct vfs_dentry *vfs_get_root_dentry(void)
{
    return vfs_state.root_dentry;
}

