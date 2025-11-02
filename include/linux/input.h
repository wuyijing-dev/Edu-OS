/**
 * Linux风格输入事件系统（evdev）
 * 参考：Linux kernel include/uapi/linux/input.h
 */

#ifndef _LINUX_INPUT_H
#define _LINUX_INPUT_H

#include <types.h>
#include <sys/time.h>
#include <sys/ioctl.h>

/**
 * 输入事件结构（Linux标准）
 */
struct input_event {
    struct timeval time;    /* 事件时间戳 */
    uint16_t type;          /* 事件类型 */
    uint16_t code;          /* 事件代码 */
    int32_t value;          /* 事件值 */
};

/**
 * 事件类型（type字段）
 */
#define EV_SYN          0x00    /* 同步事件（分隔事件包） */
#define EV_KEY          0x01    /* 按键事件（键盘、鼠标按钮） */
#define EV_REL          0x02    /* 相对坐标事件（鼠标移动） */
#define EV_ABS          0x03    /* 绝对坐标事件（触摸屏） */
#define EV_MSC          0x04    /* 杂项事件 */
#define EV_SW           0x05    /* 开关事件 */
#define EV_LED          0x11    /* LED事件 */
#define EV_SND          0x12    /* 声音事件 */
#define EV_REP          0x14    /* 重复事件 */
#define EV_FF           0x15    /* 力反馈事件 */
#define EV_PWR          0x16    /* 电源事件 */
#define EV_FF_STATUS    0x17    /* 力反馈状态 */
#define EV_MAX          0x1f
#define EV_CNT          (EV_MAX + 1)

/**
 * 同步事件代码（EV_SYN）
 */
#define SYN_REPORT      0       /* 事件包结束标记 */
#define SYN_CONFIG      1       /* 配置同步 */
#define SYN_MT_REPORT   2       /* 多点触摸报告 */
#define SYN_DROPPED     3       /* 事件丢失 */

/**
 * 按键事件代码（EV_KEY）- 键盘扫描码
 */
#define KEY_RESERVED    0
#define KEY_ESC         1
#define KEY_1           2
#define KEY_2           3
#define KEY_3           4
#define KEY_4           5
#define KEY_5           6
#define KEY_6           7
#define KEY_7           8
#define KEY_8           9
#define KEY_9           10
#define KEY_0           11
#define KEY_MINUS       12
#define KEY_EQUAL       13
#define KEY_BACKSPACE   14
#define KEY_TAB         15
#define KEY_Q           16
#define KEY_W           17
#define KEY_E           18
#define KEY_R           19
#define KEY_T           20
#define KEY_Y           21
#define KEY_U           22
#define KEY_I           23
#define KEY_O           24
#define KEY_P           25
#define KEY_LEFTBRACE   26
#define KEY_RIGHTBRACE  27
#define KEY_ENTER       28
#define KEY_LEFTCTRL    29
#define KEY_A           30
#define KEY_S           31
#define KEY_D           32
#define KEY_F           33
#define KEY_G           34
#define KEY_H           35
#define KEY_J           36
#define KEY_K           37
#define KEY_L           38
#define KEY_SEMICOLON   39
#define KEY_APOSTROPHE  40
#define KEY_GRAVE       41
#define KEY_LEFTSHIFT   42
#define KEY_BACKSLASH   43
#define KEY_Z           44
#define KEY_X           45
#define KEY_C           46
#define KEY_V           47
#define KEY_B           48
#define KEY_N           49
#define KEY_M           50
#define KEY_COMMA       51
#define KEY_DOT         52
#define KEY_SLASH       53
#define KEY_RIGHTSHIFT  54
#define KEY_KPASTERISK  55
#define KEY_LEFTALT     56
#define KEY_SPACE       57
#define KEY_CAPSLOCK    58
#define KEY_F1          59
#define KEY_F2          60
#define KEY_F3          61
#define KEY_F4          62
#define KEY_F5          63
#define KEY_F6          64
#define KEY_F7          65
#define KEY_F8          66
#define KEY_F9          67
#define KEY_F10         68
#define KEY_NUMLOCK     69
#define KEY_SCROLLLOCK  70
#define KEY_KP7         71
#define KEY_KP8         72
#define KEY_KP9         73
#define KEY_KPMINUS     74
#define KEY_KP4         75
#define KEY_KP5         76
#define KEY_KP6         77
#define KEY_KPPLUS      78
#define KEY_KP1         79
#define KEY_KP2         80
#define KEY_KP3         81
#define KEY_KP0         82
#define KEY_KPDOT       83
#define KEY_F11         87
#define KEY_F12         88
#define KEY_KPENTER     96
#define KEY_RIGHTCTRL   97
#define KEY_KPSLASH     98
#define KEY_SYSRQ       99
#define KEY_RIGHTALT    100
#define KEY_HOME        102
#define KEY_UP          103
#define KEY_PAGEUP      104
#define KEY_LEFT        105
#define KEY_RIGHT       106
#define KEY_END         107
#define KEY_DOWN        108
#define KEY_PAGEDOWN    109
#define KEY_INSERT      110
#define KEY_DELETE      111

#define KEY_MAX         0x2ff  /* 最大按键代码 */

/* 鼠标按钮 */
#define BTN_MISC        0x100
#define BTN_0           0x100
#define BTN_1           0x101
#define BTN_2           0x102
#define BTN_3           0x103
#define BTN_4           0x104
#define BTN_5           0x105
#define BTN_6           0x106
#define BTN_7           0x107
#define BTN_8           0x108
#define BTN_9           0x109

#define BTN_MOUSE       0x110
#define BTN_LEFT        0x110
#define BTN_RIGHT       0x111
#define BTN_MIDDLE      0x112
#define BTN_SIDE        0x113
#define BTN_EXTRA       0x114
#define BTN_FORWARD     0x115
#define BTN_BACK        0x116
#define BTN_TASK        0x117

/**
 * 相对坐标事件代码（EV_REL）
 */
#define REL_X           0x00    /* 鼠标X轴相对移动 */
#define REL_Y           0x01    /* 鼠标Y轴相对移动 */
#define REL_Z           0x02    /* 鼠标滚轮 */
#define REL_RX          0x03
#define REL_RY          0x04
#define REL_RZ          0x05
#define REL_HWHEEL      0x06    /* 水平滚轮 */
#define REL_DIAL        0x07
#define REL_WHEEL       0x08    /* 垂直滚轮 */
#define REL_MISC        0x09
#define REL_MAX         0x0f
#define REL_CNT         (REL_MAX + 1)

/**
 * 绝对坐标事件代码（EV_ABS）
 */
#define ABS_X           0x00
#define ABS_Y           0x01
#define ABS_Z           0x02
#define ABS_RX          0x03
#define ABS_RY          0x04
#define ABS_RZ          0x05
#define ABS_THROTTLE    0x06
#define ABS_RUDDER      0x07
#define ABS_WHEEL       0x08
#define ABS_GAS         0x09
#define ABS_BRAKE       0x0a
#define ABS_MAX         0x3f
#define ABS_CNT         (ABS_MAX + 1)

/**
 * 按键/按钮状态值
 */
#define KEY_RELEASE     0       /* 按键释放 */
#define KEY_PRESS       1       /* 按键按下 */
#define KEY_REPEAT      2       /* 按键重复 */

/**
 * ioctl命令（用于查询设备信息）
 */
#define EVIOCGVERSION   _IOR('E', 0x01, int)                    /* 获取驱动版本 */
#define EVIOCGID        _IOR('E', 0x02, struct input_id)        /* 获取设备ID */
#define EVIOCGNAME(len) _IOC(_IOC_READ, 'E', 0x06, len)         /* 获取设备名称 */
#define EVIOCGBIT(ev,len) _IOC(_IOC_READ, 'E', 0x20 + (ev), len) /* 获取事件位图 */
#define EVIOCGRAB       _IOW('E', 0x90, int)                    /* 独占设备 */

/**
 * 设备ID结构
 */
struct input_id {
    uint16_t bustype;   /* 总线类型 */
    uint16_t vendor;    /* 厂商ID */
    uint16_t product;   /* 产品ID */
    uint16_t version;   /* 版本号 */
};

/* 总线类型 */
#define BUS_PCI         0x01
#define BUS_ISAPNP      0x02
#define BUS_USB         0x03
#define BUS_HIL         0x04
#define BUS_BLUETOOTH   0x05
#define BUS_VIRTUAL     0x06
#define BUS_ISA         0x10
#define BUS_I8042       0x11    /* PS/2 */
#define BUS_XTKBD       0x12
#define BUS_RS232       0x13
#define BUS_GAMEPORT    0x14
#define BUS_PARPORT     0x15
#define BUS_AMIGA       0x16
#define BUS_ADB         0x17
#define BUS_I2C         0x18
#define BUS_HOST        0x19
#define BUS_GSC         0x1A
#define BUS_ATARI       0x1B
#define BUS_SPI         0x1C

#endif /* _LINUX_INPUT_H */

