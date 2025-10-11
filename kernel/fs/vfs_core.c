/*
 * vfs_core.c - VFS核心实现（Linux风格）
 */

#include <fs/vfs.h>
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
    
    /* 从根目录开始 */
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
            return NULL;
        }
    }
    
    return current;
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
int vfs_write(int fd, const char *buf, size_t count)
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
    if (access != O_WRONLY && access != O_RDWR) {
        return -EBADF;
    }
    
    /* 调用文件系统的write */
    if (!file->f_op || !file->f_op->write) {
        return -EINVAL;
    }
    
    return file->f_op->write(file, buf, count);
}

/*
 * 获取根dentry（用于devfs等挂载）
 */
struct vfs_dentry *vfs_get_root_dentry(void)
{
    return vfs_state.root_dentry;
}

