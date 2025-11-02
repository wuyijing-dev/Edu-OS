/*
 * mouse.c - PS/2鼠标驱动
 * 
 * 支持标准PS/2鼠标（3键+滚轮）
 * Linux风格：同时支持传统接口和evdev事件接口
 */

#include <drivers/mouse.h>
#include <arch/i386/irq.h>
#include <kernel.h>
#include <io.h>
#include <input/input_dev.h>
#include <linux/input.h>
#include <string.h>

/* PS/2控制器端口 */
#define PS2_DATA    0x60
#define PS2_STATUS  0x64
#define PS2_COMMAND 0x64

/* 鼠标数据 */
static struct {
    int x, y;           /* 坐标 */
    uint8_t buttons;    /* 按键状态 */
    uint8_t prev_buttons; /* 上一次按键状态 */
    int8_t dx, dy;      /* 移动增量 */
    uint8_t cycle;      /* 数据包周期 */
    uint8_t packet[4];  /* 数据包缓冲 */
    bool initialized;
    struct input_dev *input_dev;  /* Linux风格输入设备 */
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
        
        uint8_t new_buttons = flags & 0x07;  /* 左中右键 */
        
        /* Linux风格：报告evdev事件 */
        if (mouse_state.input_dev) {
            /* 报告相对移动 */
            if (mouse_state.dx != 0) {
                input_report_rel(mouse_state.input_dev, REL_X, mouse_state.dx);
            }
            if (mouse_state.dy != 0) {
                input_report_rel(mouse_state.input_dev, REL_Y, mouse_state.dy);
            }
            
            /* 报告按键变化 */
            if ((new_buttons & MOUSE_LEFT_BUTTON) != (mouse_state.prev_buttons & MOUSE_LEFT_BUTTON)) {
                input_report_key(mouse_state.input_dev, BTN_LEFT,
                                (new_buttons & MOUSE_LEFT_BUTTON) ? KEY_PRESS : KEY_RELEASE);
            }
            if ((new_buttons & MOUSE_RIGHT_BUTTON) != (mouse_state.prev_buttons & MOUSE_RIGHT_BUTTON)) {
                input_report_key(mouse_state.input_dev, BTN_RIGHT,
                                (new_buttons & MOUSE_RIGHT_BUTTON) ? KEY_PRESS : KEY_RELEASE);
            }
            if ((new_buttons & MOUSE_MIDDLE_BUTTON) != (mouse_state.prev_buttons & MOUSE_MIDDLE_BUTTON)) {
                input_report_key(mouse_state.input_dev, BTN_MIDDLE,
                                (new_buttons & MOUSE_MIDDLE_BUTTON) ? KEY_PRESS : KEY_RELEASE);
            }
            
            /* 同步事件 */
            input_sync(mouse_state.input_dev);
        }
        
        mouse_state.prev_buttons = new_buttons;
        mouse_state.buttons = new_buttons;
        
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
    mouse_state.prev_buttons = 0;
    mouse_state.cycle = 0;
    mouse_state.input_dev = NULL;
    
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
    
    /* Linux风格：创建输入设备 */
    mouse_state.input_dev = input_allocate_device();
    if (mouse_state.input_dev) {
        /* 设置设备信息 */
        strcpy(mouse_state.input_dev->name, "PS/2 Generic Mouse");
        mouse_state.input_dev->id.bustype = BUS_I8042;
        mouse_state.input_dev->id.vendor = 0x0002;
        mouse_state.input_dev->id.product = 0x0001;
        mouse_state.input_dev->id.version = 0x0100;
        
        /* 设置设备能力：支持相对坐标和按键 */
        input_set_capability(mouse_state.input_dev, EV_REL, REL_X);
        input_set_capability(mouse_state.input_dev, EV_REL, REL_Y);
        input_set_capability(mouse_state.input_dev, EV_KEY, BTN_LEFT);
        input_set_capability(mouse_state.input_dev, EV_KEY, BTN_RIGHT);
        input_set_capability(mouse_state.input_dev, EV_KEY, BTN_MIDDLE);
        
        /* 注册输入设备 */
        int dev_idx = input_register_device(mouse_state.input_dev);
        if (dev_idx >= 0) {
            kprintf("[MOUSE] Registered as /dev/input/event%d\n", dev_idx);
        }
    }
    
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

