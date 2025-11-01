/*
 * mouse.h - PS/2鼠标驱动接口
 */

#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>
#include <stdbool.h>

/* 鼠标按键位 */
#define MOUSE_LEFT_BUTTON   0x01
#define MOUSE_RIGHT_BUTTON  0x02
#define MOUSE_MIDDLE_BUTTON 0x04

/*
 * 初始化鼠标
 */
void mouse_init(void);

/*
 * 获取鼠标位置
 */
void mouse_get_position(int *x, int *y);

/*
 * 获取鼠标按键状态
 */
uint8_t mouse_get_buttons(void);

/*
 * 检查鼠标是否已初始化
 */
bool mouse_is_initialized(void);

#endif /* MOUSE_H */

