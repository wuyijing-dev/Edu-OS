/*
 * dev_null.c - /dev/null设备实现
 */

#include <fs/vfs.h>
#include <fs/devfs.h>
#include <kernel.h>

/* /dev/null的文件操作 */

static int dev_null_open(struct vfs_inode *inode, struct vfs_file *file)
{
    (void)inode;
    (void)file;
    return 0;  /* 总是成功 */
}

static int dev_null_release(struct vfs_file *file)
{
    (void)file;
    return 0;
}

static int dev_null_read(struct vfs_file *file, char *buf, size_t count)
{
    (void)file;
    (void)buf;
    (void)count;
    return 0;  /* 总是返回EOF */
}

static int dev_null_write(struct vfs_file *file, const char *buf, size_t count)
{
    (void)file;
    (void)buf;
    return count;  /* 假装写入成功，实际丢弃所有数据 */
}

/* /dev/null的文件操作表 */
static struct vfs_file_operations dev_null_fops = {
    .open = dev_null_open,
    .release = dev_null_release,
    .read = dev_null_read,
    .write = dev_null_write,
};

/*
 * 初始化/dev/null
 */
int dev_null_init(void)
{
    int ret = devfs_register_device("null", DEV_NULL_MAJOR, 0, &dev_null_fops);
    if (ret < 0) {
        kprintf("[DEV] Failed to register /dev/null: %d\n", ret);
        return ret;
    }
    
    kprintf("[DEV] /dev/null registered\n");
    return 0;
}

