/*
 * dev_zero.c - /dev/zero设备实现
 */

#include <fs/vfs.h>
#include <fs/devfs.h>
#include <kernel.h>
#include <string.h>

/* /dev/zero的文件操作 */

static int dev_zero_open(struct vfs_inode *inode, struct vfs_file *file)
{
    (void)inode;
    (void)file;
    return 0;
}

static int dev_zero_release(struct vfs_file *file)
{
    (void)file;
    return 0;
}

static int dev_zero_read(struct vfs_file *file, char *buf, size_t count)
{
    (void)file;
    
    if (!buf || count == 0) {
        return 0;
    }
    
    /* 用零填充缓冲区 */
    memset(buf, 0, count);
    
    return count;
}

static int dev_zero_write(struct vfs_file *file, const char *buf, size_t count)
{
    (void)file;
    (void)buf;
    return count;  /* 假装写入成功，实际丢弃所有数据 */
}

/* /dev/zero的文件操作表 */
static struct vfs_file_operations dev_zero_fops = {
    .open = dev_zero_open,
    .release = dev_zero_release,
    .read = dev_zero_read,
    .write = dev_zero_write,
};

/*
 * 初始化/dev/zero
 */
int dev_zero_init(void)
{
    int ret = devfs_register_device("zero", DEV_ZERO_MAJOR, 0, &dev_zero_fops);
    if (ret < 0) {
        kprintf("[DEV] Failed to register /dev/zero: %d\n", ret);
        return ret;
    }
    
    kprintf("[DEV] /dev/zero registered\n");
    return 0;
}

