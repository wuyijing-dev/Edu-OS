#ifndef _DRIVERS_KEYBOARD_H
#define _DRIVERS_KEYBOARD_H

#include <types.h>

/* PS/2键盘端口 */
#define KEYBOARD_DATA_PORT      0x60    // 数据端口
#define KEYBOARD_STATUS_PORT    0x64    // 状态端口
#define KEYBOARD_COMMAND_PORT   0x64    // 命令端口

/* 键盘状态寄存器位 */
#define KEYBOARD_STATUS_OUTPUT_FULL     0x01    // 输出缓冲区满
#define KEYBOARD_STATUS_INPUT_FULL      0x02    // 输入缓冲区满
#define KEYBOARD_STATUS_SYSTEM_FLAG     0x04    // 系统标志
#define KEYBOARD_STATUS_COMMAND_DATA    0x08    // 命令/数据标志
#define KEYBOARD_STATUS_KEYBOARD_LOCKED 0x10    // 键盘锁定
#define KEYBOARD_STATUS_AUX_OUTPUT      0x20    // 辅助设备输出
#define KEYBOARD_STATUS_TIMEOUT         0x40    // 超时错误
#define KEYBOARD_STATUS_PARITY_ERROR    0x80    // 奇偶校验错误

/* 特殊键扫描码 */
#define KEY_ESCAPE          0x01
#define KEY_BACKSPACE       0x0E
#define KEY_TAB             0x0F
#define KEY_ENTER           0x1C
#define KEY_LEFT_CTRL       0x1D
#define KEY_LEFT_SHIFT      0x2A
#define KEY_RIGHT_SHIFT     0x36
#define KEY_LEFT_ALT        0x38
#define KEY_CAPS_LOCK       0x3A
#define KEY_F1              0x3B
#define KEY_F2              0x3C
#define KEY_F3              0x3D
#define KEY_F4              0x3E
#define KEY_F5              0x3F
#define KEY_F6              0x40
#define KEY_F7              0x41
#define KEY_F8              0x42
#define KEY_F9              0x43
#define KEY_F10             0x44
#define KEY_NUM_LOCK        0x45
#define KEY_SCROLL_LOCK     0x46
#define KEY_F11             0x57
#define KEY_F12             0x58

/* 扩展扫描码前缀 */
#define KEY_EXTENDED        0xE0

/* 按键释放标志 */
#define KEY_RELEASE_FLAG    0x80

/* 按键状态标志 */
#define KBD_FLAG_SHIFT      0x01
#define KBD_FLAG_CTRL       0x02
#define KBD_FLAG_ALT        0x04
#define KBD_FLAG_CAPS       0x08
#define KBD_FLAG_NUM        0x10
#define KBD_FLAG_SCROLL     0x20

/* 键盘缓冲区大小 */
#define KEYBOARD_BUFFER_SIZE 256

/*
 * 键盘初始化
 */
void keyboard_init(void);

/*
 * 读取按键（阻塞）
 * 
 * @return: ASCII字符，如果是特殊键返回0
 */
char keyboard_getchar(void);

/*
 * 检查是否有按键（非阻塞）
 * 
 * @return: true=有按键，false=无按键
 */
bool keyboard_haskey(void);

/*
 * 获取键盘状态标志
 * 
 * @return: 组合的键盘状态标志（SHIFT/CTRL/ALT等）
 */
uint8_t keyboard_get_flags(void);

/*
 * 设置LED状态
 * 
 * @param caps: Caps Lock LED
 * @param num: Num Lock LED
 * @param scroll: Scroll Lock LED
 */
void keyboard_set_leds(bool caps, bool num, bool scroll);

/*
 * 清空键盘缓冲区
 */
void keyboard_flush(void);

/*
 * 打印键盘信息
 */
void keyboard_print_info(void);

#endif /* _DRIVERS_KEYBOARD_H */

