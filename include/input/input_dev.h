/**
 * 输入设备核心结构
 * Linux风格的输入子系统
 */

#ifndef _INPUT_DEV_H
#define _INPUT_DEV_H

#include <types.h>
#include <linux/input.h>
#include <fs/vfs.h>

/* 输入事件环形缓冲区大小 */
#define INPUT_EVENT_BUFFER_SIZE 64

/**
 * 输入设备结构（Linux风格）
 */
struct input_dev {
    char name[64];                      /* 设备名称 */
    struct input_id id;                 /* 设备ID */
    
    /* 事件环形缓冲区 */
    struct input_event events[INPUT_EVENT_BUFFER_SIZE];
    uint32_t event_head;                /* 写入位置 */
    uint32_t event_tail;                /* 读取位置 */
    uint32_t event_count;               /* 当前事件数量 */
    
    /* 同步原语（简化版：使用自旋锁） */
    uint32_t lock;                      /* 保护事件缓冲区 */
    
    /* 设备状态 */
    bool opened;                        /* 是否已打开 */
    bool grabbed;                       /* 是否被独占 */
    
    /* 设备能力位图 */
    uint32_t evbit[EV_CNT / 32 + 1];    /* 支持的事件类型 */
    uint32_t keybit[KEY_MAX / 32 + 1];  /* 支持的按键 */
    uint32_t relbit[REL_CNT / 32 + 1];  /* 支持的相对坐标轴 */
    uint32_t absbit[ABS_CNT / 32 + 1];  /* 支持的绝对坐标轴 */
    
    /* 设备私有数据 */
    void *private;                      /* 驱动私有数据 */
    
    /* VFS文件操作 */
    struct vfs_file_operations *fops;   /* 文件操作表 */
};

/* 按键代码最大值 */
#define KEY_MAX         0x2ff

/**
 * 输入设备管理器
 */
struct input_manager {
    struct input_dev *devices[16];      /* 最多16个输入设备 */
    uint32_t device_count;              /* 当前设备数量 */
    uint32_t lock;                      /* 保护设备列表 */
};

/**
 * 全局输入管理器
 */
extern struct input_manager *g_input_manager;

/**
 * 输入设备核心API
 */

/* 初始化输入子系统 */
int input_subsystem_init(void);

/* 分配输入设备 */
struct input_dev *input_allocate_device(void);

/* 释放输入设备 */
void input_free_device(struct input_dev *dev);

/* 注册输入设备 */
int input_register_device(struct input_dev *dev);

/* 注销输入设备 */
void input_unregister_device(struct input_dev *dev);

/* 报告输入事件 */
void input_event(struct input_dev *dev, uint16_t type, uint16_t code, int32_t value);

/* 报告按键事件 */
static inline void input_report_key(struct input_dev *dev, uint16_t code, int32_t value)
{
    input_event(dev, EV_KEY, code, value);
}

/* 报告相对坐标事件 */
static inline void input_report_rel(struct input_dev *dev, uint16_t code, int32_t value)
{
    input_event(dev, EV_REL, code, value);
}

/* 报告绝对坐标事件 */
static inline void input_report_abs(struct input_dev *dev, uint16_t code, int32_t value)
{
    input_event(dev, EV_ABS, code, value);
}

/* 同步事件（标记事件包结束） */
static inline void input_sync(struct input_dev *dev)
{
    input_event(dev, EV_SYN, SYN_REPORT, 0);
}

/* 设置设备能力位 */
static inline void input_set_capability(struct input_dev *dev, uint16_t type, uint16_t code)
{
    switch (type) {
    case EV_KEY:
        if (code < KEY_MAX) {
            dev->keybit[code / 32] |= (1 << (code % 32));
        }
        break;
    case EV_REL:
        if (code < REL_CNT) {
            dev->relbit[code / 32] |= (1 << (code % 32));
        }
        break;
    case EV_ABS:
        if (code < ABS_CNT) {
            dev->absbit[code / 32] |= (1 << (code % 32));
        }
        break;
    }
    dev->evbit[type / 32] |= (1 << (type % 32));
}

/* VFS文件操作（由驱动实现） */
int input_dev_open(struct vfs_file *file);
int input_dev_release(struct vfs_file *file);
ssize_t input_dev_read(struct vfs_file *file, char *buf, size_t count, off_t *offset);
int input_dev_ioctl(struct vfs_file *file, unsigned long request, unsigned long arg);

#endif /* _INPUT_DEV_H */

