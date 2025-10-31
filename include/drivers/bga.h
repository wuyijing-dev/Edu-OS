/*
 * bga.h - Bochs Graphics Adapter 驱动
 * 
 * 支持任意分辨率的图形显示
 */

#ifndef _DRIVERS_BGA_H
#define _DRIVERS_BGA_H

#include <types.h>

/* BGA I/O 端口 */
#define VBE_DISPI_IOPORT_INDEX          0x01CE
#define VBE_DISPI_IOPORT_DATA           0x01CF

/* BGA 寄存器索引 */
#define VBE_DISPI_INDEX_ID              0x0
#define VBE_DISPI_INDEX_XRES            0x1
#define VBE_DISPI_INDEX_YRES            0x2
#define VBE_DISPI_INDEX_BPP             0x3
#define VBE_DISPI_INDEX_ENABLE          0x4
#define VBE_DISPI_INDEX_BANK            0x5
#define VBE_DISPI_INDEX_VIRT_WIDTH      0x6
#define VBE_DISPI_INDEX_VIRT_HEIGHT     0x7
#define VBE_DISPI_INDEX_X_OFFSET        0x8
#define VBE_DISPI_INDEX_Y_OFFSET        0x9

/* BGA 标识版本 */
#define VBE_DISPI_ID0                   0xB0C0
#define VBE_DISPI_ID1                   0xB0C1
#define VBE_DISPI_ID2                   0xB0C2
#define VBE_DISPI_ID3                   0xB0C3
#define VBE_DISPI_ID4                   0xB0C4
#define VBE_DISPI_ID5                   0xB0C5

/* BGA 使能标志 */
#define VBE_DISPI_DISABLED              0x00
#define VBE_DISPI_ENABLED               0x01
#define VBE_DISPI_LFB_ENABLED           0x40  /* Linear Frame Buffer */
#define VBE_DISPI_NOCLEARMEM            0x80

/* 最大分辨率 */
#define VBE_DISPI_MAX_XRES              1920
#define VBE_DISPI_MAX_YRES              1080
#define VBE_DISPI_MAX_BPP               32

/* BGA 显示模式信息 */
struct bga_mode_info {
    uint16_t width;          /* 宽度（像素） */
    uint16_t height;         /* 高度（像素） */
    uint16_t bpp;            /* 色深（位/像素） */
    uint32_t pitch;          /* 每行字节数 */
    uint32_t fb_size;        /* framebuffer总大小（字节） */
    uint32_t fb_physical;    /* framebuffer物理地址 */
    uint32_t fb_virtual;     /* framebuffer虚拟地址 */
};

/* 像素格式（RGB） */
struct bga_pixel {
    uint8_t blue;
    uint8_t green;
    uint8_t red;
    uint8_t reserved;
} __attribute__((packed));

/* 颜色常量（32位ARGB格式） */
#define COLOR_BLACK         0xFF000000
#define COLOR_WHITE         0xFFFFFFFF
#define COLOR_RED           0xFFFF0000
#define COLOR_GREEN         0xFF00FF00
#define COLOR_BLUE          0xFF0000FF
#define COLOR_YELLOW        0xFFFFFF00
#define COLOR_CYAN          0xFF00FFFF
#define COLOR_MAGENTA       0xFFFF00FF
#define COLOR_GRAY          0xFF808080
#define COLOR_LIGHT_GRAY    0xFFC0C0C0
#define COLOR_DARK_GRAY     0xFF404040

/* BGA 驱动接口 */
int bga_init(void);
int bga_set_mode(uint16_t width, uint16_t height, uint16_t bpp);
struct bga_mode_info *bga_get_mode_info(void);

/* 基础绘图函数 */
void bga_put_pixel(uint32_t x, uint32_t y, uint32_t color);
uint32_t bga_get_pixel(uint32_t x, uint32_t y);
void bga_fill_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color);
void bga_draw_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color);
void bga_clear_screen(uint32_t color);

/* Framebuffer访问 */
uint32_t *bga_get_framebuffer(void);

#endif /* _DRIVERS_BGA_H */
