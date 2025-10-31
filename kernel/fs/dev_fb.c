/*
 * dev_fb.c - Framebuffer 设备 (/dev/fb0)
 * 
 * 提供用户空间访问图形framebuffer的接口
 */

#include <fs/vfs.h>
#include <fs/devfs.h>
#include <drivers/bga.h>
#include <kernel.h>
#include <string.h>

/* ioctl 命令 */
#define FBIOGET_VSCREENINFO     0x4600
#define FBIOGET_FSCREENINFO     0x4601
#define FBIO_WAITFORVSYNC       0x4620

/* 屏幕信息结构（用户空间可见） */
struct fb_var_screeninfo {
    uint32_t xres;              /* 可见分辨率 */
    uint32_t yres;
    uint32_t xres_virtual;      /* 虚拟分辨率 */
    uint32_t yres_virtual;
    uint32_t xoffset;           /* 偏移 */
    uint32_t yoffset;
    uint32_t bits_per_pixel;    /* 色深 */
    uint32_t grayscale;         /* 0=彩色, 1=灰度 */
};

struct fb_fix_screeninfo {
    char id[16];                /* 标识字符串 */
    uint32_t smem_start;        /* framebuffer物理地址 */
    uint32_t smem_len;          /* framebuffer长度 */
    uint32_t line_length;       /* 每行字节数 */
};

/*
 * /dev/fb0 open
 */
static int dev_fb_open(struct vfs_inode *inode, struct vfs_file *file)
{
    (void)inode;
    (void)file;
    
    kprintf("[DEV] /dev/fb0 opened\n");
    return 0;
}

/*
 * /dev/fb0 release (close)
 */
static int dev_fb_release(struct vfs_file *file)
{
    (void)file;
    
    kprintf("[DEV] /dev/fb0 closed\n");
    return 0;
}

/*
 * /dev/fb0 ioctl
 */
static int dev_fb_ioctl(struct vfs_file *file, unsigned int cmd, unsigned long arg)
{
    (void)file;
    
    struct bga_mode_info *info = bga_get_mode_info();
    
    switch (cmd) {
    case FBIOGET_VSCREENINFO: {
        struct fb_var_screeninfo *var = (struct fb_var_screeninfo*)arg;
        var->xres = info->width;
        var->yres = info->height;
        var->xres_virtual = info->width;
        var->yres_virtual = info->height;
        var->xoffset = 0;
        var->yoffset = 0;
        var->bits_per_pixel = info->bpp;
        var->grayscale = 0;
        return 0;
    }
    
    case FBIOGET_FSCREENINFO: {
        struct fb_fix_screeninfo *fix = (struct fb_fix_screeninfo*)arg;
        strncpy(fix->id, "BGA", sizeof(fix->id));
        fix->smem_start = info->fb_physical;
        fix->smem_len = info->fb_size;
        fix->line_length = info->pitch;
        return 0;
    }
    
    case FBIO_WAITFORVSYNC:
        /* TODO: 实现垂直同步等待 */
        return 0;
    
    default:
        kprintf("[DEV] /dev/fb0 ioctl: unknown command 0x%x\n", cmd);
        return -1;
    }
}

/* /dev/fb0 操作表 */
static struct vfs_file_operations fb_ops = {
    .open = dev_fb_open,
    .release = dev_fb_release,
    .read = NULL,     /* framebuffer不支持read */
    .write = NULL,    /* framebuffer不支持write */
    .ioctl = dev_fb_ioctl,
};

/*
 * 注册 /dev/fb0 设备
 */
int dev_fb_init(void)
{
    kprintf("[DEV] Registering /dev/fb0...\n");
    
    /* 注册到devfs */
    if (devfs_register_device("fb0", 29, 0, &fb_ops) < 0) {
        kprintf("[DEV] Failed to register /dev/fb0\n");
        return -1;
    }
    
    kprintf("[DEV] /dev/fb0 registered successfully\n");
    return 0;
}
