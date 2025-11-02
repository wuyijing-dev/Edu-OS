/*
 * ioctl.h - POSIX ioctl定义
 * 
 * 符合POSIX标准的设备控制接口
 */

#ifndef _SYS_IOCTL_H
#define _SYS_IOCTL_H

#include <types.h>

/* ioctl命令编码宏（类似Linux）*/
#define _IOC_NRBITS   8
#define _IOC_TYPEBITS 8
#define _IOC_SIZEBITS 14
#define _IOC_DIRBITS  2

#define _IOC_NRSHIFT   0
#define _IOC_TYPESHIFT (_IOC_NRSHIFT + _IOC_NRBITS)
#define _IOC_SIZESHIFT (_IOC_TYPESHIFT + _IOC_TYPEBITS)
#define _IOC_DIRSHIFT  (_IOC_SIZESHIFT + _IOC_SIZEBITS)

/* ioctl方向 */
#define _IOC_NONE  0U
#define _IOC_WRITE 1U
#define _IOC_READ  2U

#define _IOC(dir,type,nr,size) \
    (((dir)  << _IOC_DIRSHIFT) | \
     ((type) << _IOC_TYPESHIFT) | \
     ((nr)   << _IOC_NRSHIFT) | \
     ((size) << _IOC_SIZESHIFT))

/* 便捷宏 */
#define _IO(type,nr)        _IOC(_IOC_NONE,(type),(nr),0)
#define _IOR(type,nr,size)  _IOC(_IOC_READ,(type),(nr),sizeof(size))
#define _IOW(type,nr,size)  _IOC(_IOC_WRITE,(type),(nr),sizeof(size))
#define _IOWR(type,nr,size) _IOC(_IOC_READ|_IOC_WRITE,(type),(nr),sizeof(size))

/* 解析ioctl命令 */
#define _IOC_DIR(nr)  (((nr) >> _IOC_DIRSHIFT) & ((1 << _IOC_DIRBITS)-1))
#define _IOC_TYPE(nr) (((nr) >> _IOC_TYPESHIFT) & ((1 << _IOC_TYPEBITS)-1))
#define _IOC_NR(nr)   (((nr) >> _IOC_NRSHIFT) & ((1 << _IOC_NRBITS)-1))
#define _IOC_SIZE(nr) (((nr) >> _IOC_SIZESHIFT) & ((1 << _IOC_SIZEBITS)-1))

/* 通用ioctl命令 */
#define FIONREAD  0x541B    /* 获取可读字节数 */
#define FIONBIO   0x5421    /* 设置非阻塞I/O */
#define FIOASYNC  0x5452    /* 设置异步I/O */

/* 终端ioctl命令（POSIX标准）*/
#define TCGETS    0x5401    /* 获取终端属性 */
#define TCSETS    0x5402    /* 设置终端属性 */
#define TCSETSW   0x5403    /* 设置终端属性（等待输出完成）*/
#define TCSETSF   0x5404    /* 设置终端属性（刷新）*/
#define TCGETA    0x5405
#define TCSETA    0x5406
#define TCSETAW   0x5407
#define TCSETAF   0x5408
#define TCSBRK    0x5409
#define TCXONC    0x540A
#define TCFLSH    0x540B

/* 窗口大小 */
struct winsize {
    unsigned short ws_row;      /* 行数 */
    unsigned short ws_col;      /* 列数 */
    unsigned short ws_xpixel;   /* 水平像素 */
    unsigned short ws_ypixel;   /* 垂直像素 */
};

#define TIOCGWINSZ  0x5413  /* 获取窗口大小 */
#define TIOCSWINSZ  0x5414  /* 设置窗口大小 */

/* 帧缓冲设备ioctl（自定义）*/
#define FBIOGET_VSCREENINFO  0x4600  /* 获取可变屏幕信息 */
#define FBIOPUT_VSCREENINFO  0x4601  /* 设置可变屏幕信息 */
#define FBIOGET_FSCREENINFO  0x4602  /* 获取固定屏幕信息 */
#define FBIOPAN_DISPLAY      0x4606  /* 平移显示 */

/* 帧缓冲信息结构 */
struct fb_var_screeninfo {
    uint32_t xres;           /* 可见分辨率 */
    uint32_t yres;
    uint32_t xres_virtual;   /* 虚拟分辨率 */
    uint32_t yres_virtual;
    uint32_t xoffset;        /* 从虚拟到可见的偏移 */
    uint32_t yoffset;
    uint32_t bits_per_pixel; /* 每像素位数 */
    uint32_t grayscale;      /* 0 = color, 1 = grayscale */
};

struct fb_fix_screeninfo {
    char id[16];             /* 标识字符串 */
    unsigned long smem_start;/* 帧缓冲内存起始地址 */
    uint32_t smem_len;       /* 帧缓冲内存长度 */
    uint32_t type;           /* FB类型 */
    uint32_t line_length;    /* 一行的字节数 */
};

/* 系统调用原型 */
#ifndef __KERNEL__
int ioctl(int fd, unsigned long request, ...);
#endif

#endif /* _SYS_IOCTL_H */

