/*
 * dev_mouse.c - /dev/mouse设备实现
 * 
 * 提供用户空间访问鼠标的接口
 */

#include <fs/vfs.h>
#include <fs/devfs.h>
#include <drivers/mouse.h>
#include <kernel.h>

/* 鼠标数据包结构（用户空间可见）*/
struct mouse_packet {
    int16_t x, y;       /* 位置 */
    int16_t dx, dy;     /* 增量 */
    uint8_t buttons;    /* 按键状态 */
    uint8_t reserved;
} __attribute__((packed));

/*
 * /dev/mouse open
 */
static int dev_mouse_open(struct vfs_inode *inode, struct vfs_file *file)
{
    (void)inode;
    (void)file;
    
    /* 检查鼠标是否已初始化 */
    if (!mouse_is_initialized()) {
        kprintf("[DEV] /dev/mouse: Mouse not initialized\n");
        return -1;
    }
    
    kprintf("[DEV] /dev/mouse opened\n");
    return 0;
}

/*
 * /dev/mouse release (close)
 */
static int dev_mouse_release(struct vfs_file *file)
{
    (void)file;
    kprintf("[DEV] /dev/mouse closed\n");
    return 0;
}

/*
 * /dev/mouse read - 读取鼠标状态
 */
static int dev_mouse_read(struct vfs_file *file, char *buf, size_t count)
{
    (void)file;
    
    if (!buf || count < sizeof(struct mouse_packet)) {
        return -22;  /* EINVAL */
    }
    
    /* 获取鼠标状态 */
    struct mouse_packet packet;
    
    mouse_get_position(&packet.x, &packet.y);
    packet.dx = 0;  /* TODO: 实现增量跟踪 */
    packet.dy = 0;
    packet.buttons = mouse_get_buttons();
    packet.reserved = 0;
    
    /* 复制到用户缓冲区 */
    struct mouse_packet *user_packet = (struct mouse_packet*)buf;
    user_packet->x = packet.x;
    user_packet->y = packet.y;
    user_packet->dx = packet.dx;
    user_packet->dy = packet.dy;
    user_packet->buttons = packet.buttons;
    
    return sizeof(struct mouse_packet);
}

/* /dev/mouse操作表 */
static struct vfs_file_operations mouse_ops = {
    .open = dev_mouse_open,
    .release = dev_mouse_release,
    .read = dev_mouse_read,
    .write = NULL,  /* 鼠标不支持写入 */
};

/*
 * 注册/dev/mouse设备
 */
int dev_mouse_init(void)
{
    kprintf("[DEV] Registering /dev/mouse...\n");
    
    /* 注册到devfs */
    if (devfs_register_device("mouse", 13, 32, &mouse_ops) < 0) {
        kprintf("[DEV] Failed to register /dev/mouse\n");
        return -1;
    }
    
    kprintf("[DEV] /dev/mouse registered successfully\n");
    return 0;
}


