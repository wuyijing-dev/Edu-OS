/*
 * dev_console.c - /dev/console设备实现
 */

#include <fs/vfs.h>
#include <fs/devfs.h>
#include <kernel.h>
#include <vga.h>

/* /dev/console的文件操作 */

static int dev_console_open(struct vfs_inode *inode, struct vfs_file *file)
{
    (void)inode;
    (void)file;
    return 0;
}

static int dev_console_release(struct vfs_file *file)
{
    (void)file;
    return 0;
}

static int dev_console_read(struct vfs_file *file, char *buf, size_t count)
{
    (void)file;
    (void)buf;
    (void)count;
    /* TODO: 实现从键盘读取 */
    return 0;
}

static int dev_console_write(struct vfs_file *file, const char *buf, size_t count)
{
    (void)file;
    
    if (!buf || count == 0) {
        return 0;
    }
    
    /* 输出到VGA控制台 */
    for (size_t i = 0; i < count; i++) {
        vga_putc(buf[i]);
    }
    
    return count;
}

/* /dev/console的文件操作表 */
static struct vfs_file_operations dev_console_fops = {
    .open = dev_console_open,
    .release = dev_console_release,
    .read = dev_console_read,
    .write = dev_console_write,
};

/*
 * 初始化/dev/console
 */
int dev_console_init(void)
{
    int ret = devfs_register_device("console", DEV_CONSOLE_MAJOR, 0, &dev_console_fops);
    if (ret < 0) {
        kprintf("[DEV] Failed to register /dev/console: %d\n", ret);
        return ret;
    }
    
    kprintf("[DEV] /dev/console registered\n");
    return 0;
}

