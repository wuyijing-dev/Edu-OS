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
    
    /* Linux风格：从用户空间复制数据到内核空间
     * 关键修复：buf是用户空间地址，需要通过用户页表访问
     * 
     * 方法1（临时）：通过Page Fault让内核映射用户页
     * 方法2（正确）：切换CR3到用户页表，读取数据，切回内核CR3
     */
    
    /* 简化实现：直接访问（依赖内核态仍有用户CR3）*/
    /* Linux风格：我们在异常/系统调用时不切换CR3，所以可以直接访问 */
    for (size_t i = 0; i < count; i++) {
        /* 串口和VGA双输出 */
        extern void serial_putc(uint16_t port, char c);
        char ch = buf[i];
        vga_putc(ch);
        serial_putc(0x3F8, ch);  /* 同时输出到串口 */
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

