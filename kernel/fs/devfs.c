/*
 * devfs.c - 设备文件系统实现（Linux风格）
 */

#include <fs/vfs.h>
#include <fs/devfs.h>
#include <kernel.h>
#include <mm/kmalloc.h>
#include <string.h>

/* DevFS内部状态 */
static struct {
    struct vfs_superblock sb;
    struct vfs_inode *root_inode;
    struct vfs_dentry *dev_root;
    uint32_t next_ino;
    bool initialized;
} devfs_state;

/* 设备注册表 */
static struct devfs_device {
    char name[VFS_MAX_NAME];
    uint32_t major;
    uint32_t minor;
    struct vfs_inode *inode;
    struct devfs_device *next;
} *device_list = NULL;

/* ========== DevFS Inode操作 ========== */

/*
 * DevFS目录lookup
 */
static struct vfs_dentry *devfs_lookup(struct vfs_inode *dir, const char *name)
{
    if (!dir || !name) {
        return NULL;
    }
    
    /* 在设备列表中查找 */
    struct devfs_device *dev = device_list;
    while (dev) {
        if (strcmp(dev->name, name) == 0) {
            /* 找到了，创建dentry */
            struct vfs_dentry *dentry = vfs_alloc_dentry(name, dev->inode);
            return dentry;
        }
        dev = dev->next;
    }
    
    return NULL;
}

/* DevFS的inode操作 */
static struct vfs_inode_operations devfs_inode_ops = {
    .lookup = devfs_lookup,
};

/* ========== DevFS Superblock操作 ========== */

/* 暂时不需要 */
static struct vfs_superblock_operations devfs_sb_ops = {
    /* 空 */
};

/* ========== DevFS初始化 ========== */

/*
 * 初始化DevFS
 */
int devfs_init(void)
{
    kprintf("[DevFS] Initializing Device File System...\n");
    
    memset(&devfs_state, 0, sizeof(devfs_state));
    device_list = NULL;
    devfs_state.next_ino = 1;
    
    /* 创建根inode */
    devfs_state.root_inode = vfs_alloc_inode(NULL, devfs_state.next_ino++);
    if (!devfs_state.root_inode) {
        kprintf("[DevFS] Failed to allocate root inode\n");
        return -ENOMEM;
    }
    
    devfs_state.root_inode->mode = S_IFDIR | 0755;
    devfs_state.root_inode->i_op = &devfs_inode_ops;
    devfs_state.root_inode->f_op = NULL;  /* 目录没有文件操作 */
    
    /* 创建superblock */
    devfs_state.sb.s_op = &devfs_sb_ops;
    devfs_state.sb.root = devfs_state.root_inode;
    devfs_state.root_inode->sb = &devfs_state.sb;
    
    /* 注册到VFS */
    int ret = vfs_register_filesystem("devfs", &devfs_state.sb);
    if (ret < 0) {
        kprintf("[DevFS] Failed to register filesystem: %d\n", ret);
        vfs_free_inode(devfs_state.root_inode);
        return ret;
    }
    
    devfs_state.initialized = true;
    devfs_state.dev_root = vfs_get_root_dentry();
    
    kprintf("[DevFS] DevFS initialized and mounted at /\n");
    
    return 0;
}

/*
 * 注册设备
 */
int devfs_register_device(const char *name, uint32_t major, uint32_t minor,
                          struct vfs_file_operations *fops)
{
    if (!name || !fops || !devfs_state.initialized) {
        return -EINVAL;
    }
    
    /* 检查设备是否已存在 */
    struct devfs_device *dev = device_list;
    while (dev) {
        if (strcmp(dev->name, name) == 0) {
            return -EEXIST;
        }
        dev = dev->next;
    }
    
    /* 创建设备条目 */
    dev = (struct devfs_device*)kmalloc(sizeof(struct devfs_device));
    if (!dev) {
        return -ENOMEM;
    }
    
    strncpy(dev->name, name, VFS_MAX_NAME - 1);
    dev->name[VFS_MAX_NAME - 1] = '\0';
    dev->major = major;
    dev->minor = minor;
    
    /* 创建inode */
    dev->inode = vfs_alloc_inode(&devfs_state.sb, devfs_state.next_ino++);
    if (!dev->inode) {
        kfree(dev);
        return -ENOMEM;
    }
    
    dev->inode->mode = S_IFCHR | 0666;
    dev->inode->rdev = MKDEV(major, minor);
    dev->inode->f_op = fops;
    dev->inode->i_op = NULL;  /* 设备文件没有inode操作 */
    
    /* 添加到设备列表 */
    dev->next = device_list;
    device_list = dev;
    
    kprintf("[DevFS] Registered device: %s (major=%u, minor=%u)\n", name, major, minor);
    
    return 0;
}

/*
 * 卸载设备
 */
int devfs_unregister_device(const char *name)
{
    if (!name || !devfs_state.initialized) {
        return -EINVAL;
    }
    
    struct devfs_device **prev = &device_list;
    struct devfs_device *dev = device_list;
    
    while (dev) {
        if (strcmp(dev->name, name) == 0) {
            /* 找到了，移除 */
            *prev = dev->next;
            vfs_free_inode(dev->inode);
            kfree(dev);
            kprintf("[DevFS] Unregistered device: %s\n", name);
            return 0;
        }
        prev = &dev->next;
        dev = dev->next;
    }
    
    return -ENOENT;
}

