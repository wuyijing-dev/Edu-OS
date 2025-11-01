/*
 * mouse.c - PS/2鼠标驱动
 * 
 * 支持标准PS/2鼠标（3键+滚轮）
 */

#include <drivers/mouse.h>
#include <arch/i386/irq.h>
#include <kernel.h>
#include <io.h>

/* PS/2控制器端口 */
#define PS2_DATA    0x60
#define PS2_STATUS  0x64
#define PS2_COMMAND 0x64

/* 鼠标数据 */
static struct {
    int x, y;           /* 坐标 */
    uint8_t buttons;    /* 按键状态 */
    int8_t dx, dy;      /* 移动增量 */
    uint8_t cycle;      /* 数据包周期 */
    uint8_t packet[4];  /* 数据包缓冲 */
    bool initialized;
} mouse_state;

/*
 * 等待鼠标可写
 */
static void mouse_wait_write(void)
{
    uint32_t timeout = 100000;
    while (timeout--) {
        if ((inb(PS2_STATUS) & 0x02) == 0) {
            return;
        }
    }
}

/*
 * 等待鼠标可读
 */
static void mouse_wait_read(void)
{
    uint32_t timeout = 100000;
    while (timeout--) {
        if (inb(PS2_STATUS) & 0x01) {
            return;
        }
    }
}

/*
 * 发送命令到鼠标
 */
static void mouse_write(uint8_t data)
{
    mouse_wait_write();
    outb(PS2_COMMAND, 0xD4);  /* 写鼠标命令 */
    mouse_wait_write();
    outb(PS2_DATA, data);
}

/*
 * 从鼠标读取数据
 */
static uint8_t mouse_read(void)
{
    mouse_wait_read();
    return inb(PS2_DATA);
}

/*
 * 鼠标中断处理器
 */
static void mouse_irq_handler(struct interrupt_frame *frame)
{
    (void)frame;
    
    uint8_t status = inb(PS2_STATUS);
    if (!(status & 0x20)) {
        return;  /* 不是鼠标数据 */
    }
    
    uint8_t data = inb(PS2_DATA);
    
    /* 组装数据包（3字节或4字节）*/
    mouse_state.packet[mouse_state.cycle++] = data;
    
    if (mouse_state.cycle == 3) {
        mouse_state.cycle = 0;
        
        /* 解析数据包 */
        uint8_t flags = mouse_state.packet[0];
        mouse_state.dx = (int8_t)mouse_state.packet[1];
        mouse_state.dy = -(int8_t)mouse_state.packet[2];  /* Y轴反转 */
        
        mouse_state.buttons = flags & 0x07;  /* 左中右键 */
        
        /* 更新坐标 */
        mouse_state.x += mouse_state.dx;
        mouse_state.y += mouse_state.dy;
        
        /* 边界检查 */
        if (mouse_state.x < 0) mouse_state.x = 0;
        if (mouse_state.y < 0) mouse_state.y = 0;
        if (mouse_state.x > 1024) mouse_state.x = 1024;
        if (mouse_state.y > 768) mouse_state.y = 768;
    }
}

/*
 * 初始化鼠标
 */
void mouse_init(void)
{
    kprintf("[MOUSE] Initializing PS/2 mouse...\n");
    
    mouse_state.x = 512;
    mouse_state.y = 384;
    mouse_state.buttons = 0;
    mouse_state.cycle = 0;
    
    /* 启用辅助设备（鼠标） */
    mouse_wait_write();
    outb(PS2_COMMAND, 0xA8);
    
    /* 获取压缩状态字节 */
    mouse_wait_write();
    outb(PS2_COMMAND, 0x20);
    mouse_wait_read();
    uint8_t status = inb(PS2_DATA) | 0x02;
    
    /* 设置压缩状态字节 */
    mouse_wait_write();
    outb(PS2_COMMAND, 0x60);
    mouse_wait_write();
    outb(PS2_DATA, status);
    
    /* 使用默认设置 */
    mouse_write(0xF6);
    mouse_read();  /* ACK */
    
    /* 启用数据报告 */
    mouse_write(0xF4);
    mouse_read();  /* ACK */
    
    /* 注册IRQ12处理器 */
    irq_install_handler(12, mouse_irq_handler);
    irq_enable(12);
    
    mouse_state.initialized = true;
    
    kprintf("[MOUSE] PS/2 mouse initialized\n");
    kprintf("[MOUSE] Initial position: (%d, %d)\n", mouse_state.x, mouse_state.y);
}

/*
 * 获取鼠标位置
 */
void mouse_get_position(int *x, int *y)
{
    if (x) *x = mouse_state.x;
    if (y) *y = mouse_state.y;
}

/*
 * 获取鼠标按键状态
 */
uint8_t mouse_get_buttons(void)
{
    return mouse_state.buttons;
}

/*
 * 检查鼠标是否已初始化
 */
bool mouse_is_initialized(void)
{
    return mouse_state.initialized;
}

