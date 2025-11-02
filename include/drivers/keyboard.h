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

/* PS/2扫描码（用于内部处理，避免与Linux keycode冲突）*/
#define PS2_ESCAPE          0x01
#define PS2_BACKSPACE       0x0E
#define PS2_TAB             0x0F
#define PS2_ENTER           0x1C
#define PS2_LEFT_CTRL       0x1D
#define PS2_LEFT_SHIFT      0x2A
#define PS2_RIGHT_SHIFT     0x36
#define PS2_LEFT_ALT        0x38
#define PS2_CAPS_LOCK       0x3A
#define PS2_F1              0x3B
#define PS2_F2              0x3C
#define PS2_F3              0x3D
#define PS2_F4              0x3E
#define PS2_F5              0x3F
#define PS2_F6              0x40
#define PS2_F7              0x41
#define PS2_F8              0x42
#define PS2_F9              0x43
#define PS2_F10             0x44
#define PS2_NUM_LOCK        0x45
#define PS2_SCROLL_LOCK     0x46
#define PS2_F11             0x57
#define PS2_F12             0x58

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

