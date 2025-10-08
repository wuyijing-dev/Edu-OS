#ifndef VGA_H
#define VGA_H

#include "types.h"

/*
 * =============================================================================
 * VGA文本模式驱动
 * =============================================================================
 */

// VGA文本模式参数
#define VGA_WIDTH   80
#define VGA_HEIGHT  25
#define VGA_MEMORY  0xB8000

// VGA颜色代码
enum vga_color {
    VGA_COLOR_BLACK         = 0,
    VGA_COLOR_BLUE          = 1,
    VGA_COLOR_GREEN         = 2,
    VGA_COLOR_CYAN          = 3,
    VGA_COLOR_RED           = 4,
    VGA_COLOR_MAGENTA       = 5,
    VGA_COLOR_BROWN         = 6,
    VGA_COLOR_LIGHT_GREY    = 7,
    VGA_COLOR_DARK_GREY     = 8,
    VGA_COLOR_LIGHT_BLUE    = 9,
    VGA_COLOR_LIGHT_GREEN   = 10,
    VGA_COLOR_LIGHT_CYAN    = 11,
    VGA_COLOR_LIGHT_RED     = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_LIGHT_BROWN   = 14,  // 黄色
    VGA_COLOR_WHITE         = 15,
};

// VGA CRT控制器端口
#define VGA_CTRL_REGISTER   0x3D4
#define VGA_DATA_REGISTER   0x3D5

// CRT寄存器索引
#define VGA_CURSOR_HIGH     0x0E
#define VGA_CURSOR_LOW      0x0F

/*
 * =============================================================================
 * 内联辅助函数
 * =============================================================================
 */

// 生成VGA颜色属性
static inline uint8_t vga_make_color(enum vga_color fg, enum vga_color bg) {
    return fg | (bg << 4);
}

// 生成VGA显存条目（字符+属性）
static inline uint16_t vga_make_entry(unsigned char c, uint8_t color) {
    return (uint16_t) c | ((uint16_t) color << 8);
}

/*
 * =============================================================================
 * 函数声明
 * =============================================================================
 */

// 初始化VGA
void vga_init(void);

// 清屏
void vga_clear(void);

// 设置前景色和背景色
void vga_set_color(enum vga_color fg, enum vga_color bg);

// 获取当前颜色
uint8_t vga_get_color(void);

// 输出单个字符
void vga_putc(char c);
void vga_putchar(char c);  // 别名

// 输出字符串
void vga_puts(const char *str);
void vga_write_string(const char *str);  // 别名

// 在指定位置输出字符
void vga_putc_at(char c, uint8_t color, uint8_t x, uint8_t y);

// 设置光标位置
void vga_set_cursor(uint8_t x, uint8_t y);

// 获取光标位置
void vga_get_cursor(uint8_t *x, uint8_t *y);

// 启用/禁用光标
void vga_enable_cursor(uint8_t cursor_start, uint8_t cursor_end);
void vga_disable_cursor(void);

// 滚屏
void vga_scroll(void);

#endif // VGA_H
