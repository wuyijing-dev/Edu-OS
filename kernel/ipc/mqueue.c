/**
 * mqueue.c - POSIX消息队列实现
 * 
 * Linux风格的消息队列系统
 */

#include <ipc/mqueue.h>
#include <mm/kmalloc.h>
#include <process/process.h>
#include <kernel.h>
#include <string.h>
#include <errno.h>
#include <fs/vfs.h>

/* 全局消息队列管理器 */
struct mqueue_manager g_mqueue_manager;

/**
 * 初始化消息队列系统
 */
void mqueue_init(void)
{
    kprintf("[MQueue] Initializing message queue system...\n");
    
    INIT_LIST_HEAD(&g_mqueue_manager.queues);
    g_mqueue_manager.queue_count = 0;
    g_mqueue_manager.total_messages = 0;
    g_mqueue_manager.total_bytes = 0;
    
    kprintf("[MQueue] Message queue system initialized\n");
    kprintf("[MQueue]   Max queues: %u\n", MQ_MAX_QUEUES);
    kprintf("[MQueue]   Max messages per queue: %u\n", MQ_MAX_MESSAGES);
    kprintf("[MQueue]   Max message size: %u bytes\n", MQ_MAX_MSG_SIZE);
}

/**
 * 查找消息队列
 */
struct mqueue *mqueue_find(const char *name)
{
    struct mqueue *mq;
    
    list_for_each_entry(mq, &g_mqueue_manager.queues, list) {
        if (strcmp(mq->name, name) == 0) {
            return mq;
        }
    }
    
    return NULL;
}

/**
 * 创建/打开消息队列
 */
struct mqueue *mq_open(const char *name, int oflag, mode_t mode, struct mq_attr *attr)
{
    if (!name || name[0] != '/') {
        return NULL;  /* 名称必须以'/'开头 */
    }
    
    /* 查找是否已存在 */
    struct mqueue *mq = mqueue_find(name);
    
    if (mq) {
        /* 队列已存在 */
        if (oflag & O_CREAT && oflag & O_EXCL) {
            return NULL;  /* 独占创建失败 */
        }
        mq->refcount++;
        return mq;
    }
    
    /* 创建新队列 */
    if (!(oflag & O_CREAT)) {
        return NULL;  /* 队列不存在且未指定创建 */
    }
    
    if (g_mqueue_manager.queue_count >= MQ_MAX_QUEUES) {
        kprintf("[MQueue] ERROR: Too many message queues\n");
        return NULL;
    }
    
    /* 分配消息队列结构 */
    mq = (struct mqueue *)kmalloc(sizeof(struct mqueue));
    if (!mq) {
        return NULL;
    }
    
    /* 初始化队列 */
    strncpy(mq->name, name, sizeof(mq->name) - 1);
    mq->name[sizeof(mq->name) - 1] = '\0';
    mq->flags = (oflag & O_NONBLOCK) ? MQ_FLAG_NONBLOCK : 0;
    mq->refcount = 1;
    mq->total_sent = 0;
    mq->total_recv = 0;
    
    /* 设置属性 */
    if (attr) {
        mq->attr = *attr;
        if (mq->attr.mq_maxmsg == 0 || mq->attr.mq_maxmsg > MQ_MAX_MESSAGES) {
            mq->attr.mq_maxmsg = MQ_MAX_MESSAGES;
        }
        if (mq->attr.mq_msgsize == 0 || mq->attr.mq_msgsize > MQ_MAX_MSG_SIZE) {
            mq->attr.mq_msgsize = MQ_MAX_MSG_SIZE;
        }
    } else {
        /* 默认属性 */
        mq->attr.mq_flags = mq->flags;
        mq->attr.mq_maxmsg = MQ_MAX_MESSAGES;
        mq->attr.mq_msgsize = MQ_MAX_MSG_SIZE;
        mq->attr.mq_curmsgs = 0;
    }
    
    /* 初始化链表 */
    INIT_LIST_HEAD(&mq->messages);
    INIT_LIST_HEAD(&mq->wait_send);
    INIT_LIST_HEAD(&mq->wait_recv);
    INIT_LIST_HEAD(&mq->list);
    
    /* 添加到全局队列列表 */
    list_add_tail(&mq->list, &g_mqueue_manager.queues);
    g_mqueue_manager.queue_count++;
    
    kprintf("[MQueue] Created queue: %s (maxmsg=%u, msgsize=%u)\n",
            mq->name, mq->attr.mq_maxmsg, mq->attr.mq_msgsize);
    
    return mq;
}

/**
 * 关闭消息队列
 */
int mq_close(struct mqueue *mq)
{
    if (!mq) {
        return -EINVAL;
    }
    
    if (mq->refcount > 0) {
        mq->refcount--;
    }
    
    /* 如果引用计数为0，可以被删除（但不立即删除）*/
    return 0;
}

/**
 * 删除消息队列
 */
int mq_unlink(const char *name)
{
    struct mqueue *mq = mqueue_find(name);
    if (!mq) {
        return -ENOENT;
    }
    
    /* 如果还有引用，标记为待删除 */
    if (mq->refcount > 0) {
        /* 简化实现：等待所有引用关闭后才删除 */
        return 0;
    }
    
    /* 释放所有消息 */
    struct mq_message *msg, *tmp;
    list_for_each_entry_safe(msg, tmp, &mq->messages, list) {
        list_del(&msg->list);
        kfree(msg);
    }
    
    /* 从全局列表中删除 */
    list_del(&mq->list);
    g_mqueue_manager.queue_count--;
    
    /* 释放队列结构 */
    kfree(mq);
    
    kprintf("[MQueue] Unlinked queue: %s\n", name);
    
    return 0;
}

/**
 * 发送消息（按优先级插入）
 */
int mq_send(struct mqueue *mq, const char *msg_ptr, size_t msg_len, uint32_t msg_prio)
{
    if (!mq || !msg_ptr) {
        return -EINVAL;
    }
    
    if (msg_len > mq->attr.mq_msgsize) {
        return -EMSGSIZE;
    }
    
    if (msg_prio >= MQ_PRIO_MAX) {
        return -EINVAL;
    }
    
    /* 检查队列是否已满 */
    if (mq->attr.mq_curmsgs >= mq->attr.mq_maxmsg) {
        if (mq->flags & MQ_FLAG_NONBLOCK) {
            return -EAGAIN;
        }
        /* TODO: 阻塞等待队列有空间 */
        return -EAGAIN;
    }
    
    /* 分配消息结构 */
    struct mq_message *msg = (struct mq_message *)kmalloc(
        sizeof(struct mq_message) + msg_len);
    if (!msg) {
        return -ENOMEM;
    }
    
    /* 初始化消息 */
    msg->priority = msg_prio;
    msg->size = msg_len;
    memcpy(msg->data, msg_ptr, msg_len);
    INIT_LIST_HEAD(&msg->list);
    
    /* 按优先级插入到队列（优先级高的在前）*/
    struct mq_message *pos;
    int inserted = 0;
    
    list_for_each_entry(pos, &mq->messages, list) {
        if (msg_prio > pos->priority) {
            list_add_tail(&msg->list, &pos->list);
            inserted = 1;
            break;
        }
    }
    
    if (!inserted) {
        list_add_tail(&msg->list, &mq->messages);
    }
    
    /* 更新统计 */
    mq->attr.mq_curmsgs++;
    mq->total_sent++;
    g_mqueue_manager.total_messages++;
    g_mqueue_manager.total_bytes += msg_len;
    
    /* TODO: 唤醒等待接收的进程 */
    
    return 0;
}

/**
 * 接收消息
 */
ssize_t mq_receive(struct mqueue *mq, char *msg_ptr, size_t msg_len, uint32_t *msg_prio)
{
    if (!mq || !msg_ptr) {
        return -EINVAL;
    }
    
    if (msg_len < mq->attr.mq_msgsize) {
        return -EMSGSIZE;
    }
    
    /* 检查队列是否为空 */
    if (mq->attr.mq_curmsgs == 0) {
        if (mq->flags & MQ_FLAG_NONBLOCK) {
            return -EAGAIN;
        }
        /* TODO: 阻塞等待消息到达 */
        return -EAGAIN;
    }
    
    /* 取出第一个消息（优先级最高）*/
    struct mq_message *msg = list_first_entry(&mq->messages, 
                                              struct mq_message, list);
    
    /* 复制消息数据 */
    size_t copy_len = (msg->size < msg_len) ? msg->size : msg_len;
    memcpy(msg_ptr, msg->data, copy_len);
    
    if (msg_prio) {
        *msg_prio = msg->priority;
    }
    
    ssize_t ret = msg->size;
    
    /* 从队列中删除消息 */
    list_del(&msg->list);
    kfree(msg);
    
    /* 更新统计 */
    mq->attr.mq_curmsgs--;
    mq->total_recv++;
    g_mqueue_manager.total_messages--;
    g_mqueue_manager.total_bytes -= copy_len;
    
    /* TODO: 唤醒等待发送的进程 */
    
    return ret;
}

/**
 * 获取队列属性
 */
int mq_getattr(struct mqueue *mq, struct mq_attr *attr)
{
    if (!mq || !attr) {
        return -EINVAL;
    }
    
    *attr = mq->attr;
    return 0;
}

/**
 * 设置队列属性
 */
int mq_setattr(struct mqueue *mq, const struct mq_attr *newattr, struct mq_attr *oldattr)
{
    if (!mq || !newattr) {
        return -EINVAL;
    }
    
    if (oldattr) {
        *oldattr = mq->attr;
    }
    
    /* 只能修改flags */
    mq->attr.mq_flags = newattr->mq_flags;
    mq->flags = (newattr->mq_flags & MQ_FLAG_NONBLOCK) ? MQ_FLAG_NONBLOCK : 0;
    
    return 0;
}

/**
 * 打印消息队列信息
 */
void mqueue_print_info(void)
{
    kprintf("\n=== Message Queue Statistics ===\n");
    kprintf("Total queues:   %u / %u\n", g_mqueue_manager.queue_count, MQ_MAX_QUEUES);
    kprintf("Total messages: %llu\n", g_mqueue_manager.total_messages);
    kprintf("Total bytes:    %llu\n", g_mqueue_manager.total_bytes);
    
    if (g_mqueue_manager.queue_count > 0) {
        kprintf("\nActive queues:\n");
        struct mqueue *mq;
        list_for_each_entry(mq, &g_mqueue_manager.queues, list) {
            kprintf("  %s: %u/%u messages (sent=%llu, recv=%llu)\n",
                    mq->name, mq->attr.mq_curmsgs, mq->attr.mq_maxmsg,
                    mq->total_sent, mq->total_recv);
        }
    }
    
    kprintf("================================\n\n");
}

/**
 * 系统调用：mq_open
 */
int sys_mq_open(const char *name, int oflag, mode_t mode, struct mq_attr *attr)
{
    /* TODO: 用户空间指针验证 */
    struct mqueue *mq = mq_open(name, oflag, mode, attr);
    if (!mq) {
        return -1;
    }
    
    /* TODO: 分配文件描述符并返回 */
    return (int)(uintptr_t)mq;  /* 简化：直接返回指针 */
}

/**
 * 系统调用：mq_close
 */
int sys_mq_close(int mqdes)
{
    struct mqueue *mq = (struct mqueue *)(uintptr_t)mqdes;
    return mq_close(mq);
}

/**
 * 系统调用：mq_unlink
 */
int sys_mq_unlink(const char *name)
{
    /* TODO: 用户空间指针验证 */
    return mq_unlink(name);
}

/**
 * 系统调用：mq_send
 */
int sys_mq_send(int mqdes, const char *msg_ptr, size_t msg_len, uint32_t msg_prio)
{
    /* TODO: 用户空间指针验证 */
    struct mqueue *mq = (struct mqueue *)(uintptr_t)mqdes;
    return mq_send(mq, msg_ptr, msg_len, msg_prio);
}

/**
 * 系统调用：mq_receive
 */
ssize_t sys_mq_receive(int mqdes, char *msg_ptr, size_t msg_len, uint32_t *msg_prio)
{
    /* TODO: 用户空间指针验证 */
    struct mqueue *mq = (struct mqueue *)(uintptr_t)mqdes;
    return mq_receive(mq, msg_ptr, msg_len, msg_prio);
}

/**
 * 系统调用：mq_getattr
 */
int sys_mq_getattr(int mqdes, struct mq_attr *attr)
{
    /* TODO: 用户空间指针验证 */
    struct mqueue *mq = (struct mqueue *)(uintptr_t)mqdes;
    return mq_getattr(mq, attr);
}

/**
 * 系统调用：mq_setattr
 */
int sys_mq_setattr(int mqdes, const struct mq_attr *newattr, struct mq_attr *oldattr)
{
    /* TODO: 用户空间指针验证 */
    struct mqueue *mq = (struct mqueue *)(uintptr_t)mqdes;
    return mq_setattr(mq, newattr, oldattr);
}

