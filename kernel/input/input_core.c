/**
 * 输入设备核心实现
 * Linux风格的输入子系统
 */

#include <input/input_dev.h>
#include <mm/kmalloc.h>
#include <kernel.h>
#include <string.h>
#include <sys/time.h>
#include <sys/poll.h>
#include <mm/uaccess.h>
#include <wait_queue.h>

/* 全局输入管理器 */
struct input_manager *g_input_manager = NULL;

/* 简单的自旋锁实现 */
static inline void spin_lock(uint32_t *lock)
{
    while (__sync_lock_test_and_set(lock, 1)) {
        /* 忙等待 */
    }
}

static inline void spin_unlock(uint32_t *lock)
{
    __sync_lock_release(lock);
}

/**
 * 初始化输入子系统
 */
int input_subsystem_init(void)
{
    kprintf("[INPUT] Initializing input subsystem...\n");
    
    g_input_manager = (struct input_manager *)kmalloc(sizeof(struct input_manager));
    if (!g_input_manager) {
        kprintf("[INPUT] Failed to allocate input manager\n");
        return -1;
    }
    
    memset(g_input_manager, 0, sizeof(struct input_manager));
    g_input_manager->device_count = 0;
    g_input_manager->lock = 0;
    
    kprintf("[INPUT] Input subsystem initialized\n");
    return 0;
}

/**
 * 分配输入设备
 */
struct input_dev *input_allocate_device(void)
{
    struct input_dev *dev = (struct input_dev *)kmalloc(sizeof(struct input_dev));
    if (!dev) {
        return NULL;
    }
    
    memset(dev, 0, sizeof(struct input_dev));
    dev->event_head = 0;
    dev->event_tail = 0;
    dev->event_count = 0;
    dev->lock = 0;
    dev->opened = false;
    dev->grabbed = false;
    
    /* 分配等待队列 */
    dev->wait_queue = (struct wait_queue_head *)kmalloc(sizeof(struct wait_queue_head));
    if (dev->wait_queue) {
        init_waitqueue_head(dev->wait_queue);
    }
    
    return dev;
}

/**
 * 释放输入设备
 */
void input_free_device(struct input_dev *dev)
{
    if (dev) {
        if (dev->wait_queue) {
            kfree(dev->wait_queue);
        }
        kfree(dev);
    }
}

/**
 * 注册输入设备
 */
int input_register_device(struct input_dev *dev)
{
    if (!dev || !g_input_manager) {
        return -1;
    }
    
    spin_lock(&g_input_manager->lock);
    
    if (g_input_manager->device_count >= 16) {
        spin_unlock(&g_input_manager->lock);
        kprintf("[INPUT] Too many input devices\n");
        return -1;
    }
    
    /* 添加到设备列表 */
    uint32_t idx = g_input_manager->device_count;
    g_input_manager->devices[idx] = dev;
    g_input_manager->device_count++;
    
    spin_unlock(&g_input_manager->lock);
    
    kprintf("[INPUT] Registered device: %s (index=%u)\n", dev->name, idx);
    return idx;
}

/**
 * 注销输入设备
 */
void input_unregister_device(struct input_dev *dev)
{
    if (!dev || !g_input_manager) {
        return;
    }
    
    spin_lock(&g_input_manager->lock);
    
    /* 从设备列表中移除 */
    for (uint32_t i = 0; i < g_input_manager->device_count; i++) {
        if (g_input_manager->devices[i] == dev) {
            /* 将后面的设备前移 */
            for (uint32_t j = i; j < g_input_manager->device_count - 1; j++) {
                g_input_manager->devices[j] = g_input_manager->devices[j + 1];
            }
            g_input_manager->device_count--;
            break;
        }
    }
    
    spin_unlock(&g_input_manager->lock);
    
    kprintf("[INPUT] Unregistered device: %s\n", dev->name);
}

/**
 * 获取当前时间（微秒级）
 */
static void get_current_time(struct timeval *tv)
{
    extern uint32_t timer_get_ticks(void);
    uint32_t ticks = timer_get_ticks();
    
    /* 假设1 tick = 10ms */
    tv->tv_sec = ticks / 100;
    tv->tv_usec = (ticks % 100) * 10000;
}

/**
 * 报告输入事件
 */
void input_event(struct input_dev *dev, uint16_t type, uint16_t code, int32_t value)
{
    if (!dev) {
        return;
    }
    
    spin_lock(&dev->lock);
    
    /* 检查缓冲区是否已满 */
    if (dev->event_count >= INPUT_EVENT_BUFFER_SIZE) {
        spin_unlock(&dev->lock);
        kprintf("[INPUT] Event buffer full, dropping event\n");
        return;
    }
    
    /* 创建事件 */
    struct input_event *event = &dev->events[dev->event_head];
    get_current_time(&event->time);
    event->type = type;
    event->code = code;
    event->value = value;
    
    /* 更新环形缓冲区 */
    dev->event_head = (dev->event_head + 1) % INPUT_EVENT_BUFFER_SIZE;
    dev->event_count++;
    
    spin_unlock(&dev->lock);
    
    /* 唤醒等待的进程（select/poll支持）*/
    if (dev->wait_queue) {
        wake_up(dev->wait_queue);
    }
}

/**
 * VFS文件操作：打开设备
 */
int input_dev_open(struct vfs_file *file)
{
    if (!file || !file->private_data) {
        return -1;
    }
    
    struct input_dev *dev = (struct input_dev *)file->private_data;
    
    spin_lock(&dev->lock);
    dev->opened = true;
    spin_unlock(&dev->lock);
    
    kprintf("[INPUT] Device opened: %s\n", dev->name);
    return 0;
}

/**
 * VFS文件操作：关闭设备
 */
int input_dev_release(struct vfs_file *file)
{
    if (!file || !file->private_data) {
        return -1;
    }
    
    struct input_dev *dev = (struct input_dev *)file->private_data;
    
    spin_lock(&dev->lock);
    dev->opened = false;
    dev->grabbed = false;
    spin_unlock(&dev->lock);
    
    kprintf("[INPUT] Device closed: %s\n", dev->name);
    return 0;
}

/**
 * VFS文件操作：读取事件
 */
ssize_t input_dev_read(struct vfs_file *file, char *buf, size_t count, off_t *offset)
{
    if (!file || !file->private_data || !buf) {
        return -1;
    }
    
    struct input_dev *dev = (struct input_dev *)file->private_data;
    
    /* 检查缓冲区大小是否足够 */
    if (count < sizeof(struct input_event)) {
        return -1;  /* EINVAL */
    }
    
    /* 验证用户空间缓冲区 */
    if (!is_user_buffer(buf, count)) {
        return -1;  /* EFAULT */
    }
    
    /* 计算可以读取的事件数量 */
    size_t events_to_read = count / sizeof(struct input_event);
    size_t events_read = 0;
    
    spin_lock(&dev->lock);
    
    /* 读取事件 */
    while (events_read < events_to_read && dev->event_count > 0) {
        struct input_event *event = &dev->events[dev->event_tail];
        
        /* 复制到用户空间 */
        if (copy_to_user(buf + events_read * sizeof(struct input_event),
                        event, sizeof(struct input_event)) != 0) {
            spin_unlock(&dev->lock);
            return -1;  /* EFAULT */
        }
        
        /* 更新环形缓冲区 */
        dev->event_tail = (dev->event_tail + 1) % INPUT_EVENT_BUFFER_SIZE;
        dev->event_count--;
        events_read++;
    }
    
    spin_unlock(&dev->lock);
    
    return events_read * sizeof(struct input_event);
}

/**
 * VFS文件操作：ioctl控制
 */
int input_dev_ioctl(struct vfs_file *file, unsigned long request, unsigned long arg)
{
    if (!file || !file->private_data) {
        return -1;
    }
    
    struct input_dev *dev = (struct input_dev *)file->private_data;
    
    switch (request) {
    case EVIOCGVERSION: {
        /* 返回驱动版本（0x010001 = 1.0.1） */
        int version = 0x010001;
        if (copy_to_user((void *)arg, &version, sizeof(int)) != 0) {
            return -1;
        }
        return 0;
    }
    
    case EVIOCGID: {
        /* 返回设备ID */
        if (copy_to_user((void *)arg, &dev->id, sizeof(struct input_id)) != 0) {
            return -1;
        }
        return 0;
    }
    
    case EVIOCGRAB: {
        /* 独占设备 */
        int grab = 0;
        if (copy_from_user(&grab, (void *)arg, sizeof(int)) != 0) {
            return -1;
        }
        
        spin_lock(&dev->lock);
        dev->grabbed = (grab != 0);
        spin_unlock(&dev->lock);
        
        kprintf("[INPUT] Device %s: grabbed=%d\n", dev->name, dev->grabbed);
        return 0;
    }
    
    default:
        /* 检查是否为EVIOCGNAME */
        if ((_IOC_TYPE(request) == 'E') && (_IOC_NR(request) == 0x06)) {
            size_t len = _IOC_SIZE(request);
            size_t name_len = strlen(dev->name) + 1;
            if (name_len > len) {
                name_len = len;
            }
            if (copy_to_user((void *)arg, dev->name, name_len) != 0) {
                return -1;
            }
            return name_len;
        }
        
        /* 检查是否为EVIOCGBIT */
        if ((_IOC_TYPE(request) == 'E') && (_IOC_NR(request) >= 0x20 && _IOC_NR(request) < 0x20 + EV_MAX)) {
            uint16_t ev_type = _IOC_NR(request) - 0x20;
            size_t len = _IOC_SIZE(request);
            
            void *bits = NULL;
            size_t bits_size = 0;
            
            switch (ev_type) {
            case 0:  /* EV_SYN */
                bits = dev->evbit;
                bits_size = sizeof(dev->evbit);
                break;
            case EV_KEY:
                bits = dev->keybit;
                bits_size = sizeof(dev->keybit);
                break;
            case EV_REL:
                bits = dev->relbit;
                bits_size = sizeof(dev->relbit);
                break;
            case EV_ABS:
                bits = dev->absbit;
                bits_size = sizeof(dev->absbit);
                break;
            default:
                return -1;
            }
            
            if (len > bits_size) {
                len = bits_size;
            }
            
            if (copy_to_user((void *)arg, bits, len) != 0) {
                return -1;
            }
            return len;
        }
        
        kprintf("[INPUT] Unknown ioctl: 0x%lx\n", request);
        return -1;
    }
}

/**
 * VFS文件操作：poll检查
 */
unsigned int input_dev_poll(struct vfs_file *file, struct wait_queue_head *wait)
{
    if (!file || !file->private_data) {
        return POLLNVAL;
    }
    
    struct input_dev *dev = (struct input_dev *)file->private_data;
    unsigned int mask = 0;
    
    /* 将当前进程添加到等待队列 */
    if (wait && dev->wait_queue) {
        extern struct process *process_get_current(void);
        struct process *proc = process_get_current();
        
        if (proc) {
            struct wait_queue_entry entry;
            entry.process = proc;
            entry.next = NULL;
            add_wait_queue(dev->wait_queue, &entry);
        }
    }
    
    /* 检查是否有数据可读 */
    spin_lock(&dev->lock);
    if (dev->event_count > 0) {
        mask |= POLLIN | POLLRDNORM;  /* 有数据可读 */
    }
    spin_unlock(&dev->lock);
    
    return mask;
}

