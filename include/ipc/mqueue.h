/**
 * mqueue.h - POSIX消息队列（Message Queue）
 * 
 * 用于进程间异步通信，支持优先级
 */

#ifndef _IPC_MQUEUE_H
#define _IPC_MQUEUE_H

#include <types.h>
#include <list.h>
#include <fs/vfs.h>
#include <time.h>

/* 前置声明 */
struct sigevent;

/**
 * 消息队列配置
 */
#define MQ_MAX_QUEUES       64      /* 系统最大消息队列数 */
#define MQ_MAX_MESSAGES     32      /* 每个队列最大消息数 */
#define MQ_MAX_MSG_SIZE     8192    /* 最大消息大小（字节）*/
#define MQ_PRIO_MAX         32768   /* 最大优先级值 */

/**
 * 消息队列标志
 */
#define MQ_FLAG_NONBLOCK    0x01    /* 非阻塞模式 */
#define MQ_FLAG_CLOEXEC     0x02    /* exec时关闭 */

/**
 * 消息结构
 */
struct mq_message {
    struct list_head list;      /* 链表节点 */
    uint32_t priority;          /* 消息优先级（0-32767）*/
    size_t size;                /* 消息大小 */
    char data[0];               /* 消息数据（柔性数组）*/
};

/**
 * 消息队列属性
 */
struct mq_attr {
    uint32_t mq_flags;          /* 标志：0或MQ_FLAG_NONBLOCK */
    uint32_t mq_maxmsg;         /* 队列最大消息数 */
    uint32_t mq_msgsize;        /* 最大消息大小 */
    uint32_t mq_curmsgs;        /* 当前消息数 */
};

/**
 * 消息队列描述符
 */
struct mqueue {
    char name[64];              /* 队列名称 */
    uint32_t flags;             /* MQ_FLAG_* 标志 */
    struct mq_attr attr;        /* 队列属性 */
    
    /* 消息链表（按优先级排序）*/
    struct list_head messages;
    
    /* 等待队列 */
    struct list_head wait_send;     /* 等待发送的进程 */
    struct list_head wait_recv;     /* 等待接收的进程 */
    
    /* 引用计数 */
    uint32_t refcount;
    
    /* 统计信息 */
    uint64_t total_sent;        /* 总发送消息数 */
    uint64_t total_recv;        /* 总接收消息数 */
    
    /* 链表节点 */
    struct list_head list;
};

/**
 * 消息队列管理器
 */
struct mqueue_manager {
    struct list_head queues;    /* 所有消息队列链表 */
    uint32_t queue_count;       /* 当前队列数 */
    
    /* 统计信息 */
    uint64_t total_messages;    /* 系统总消息数 */
    uint64_t total_bytes;       /* 系统总消息字节数 */
};

/**
 * 全局消息队列管理器
 */
extern struct mqueue_manager g_mqueue_manager;

/**
 * 消息队列操作函数
 */

/* 初始化消息队列系统 */
void mqueue_init(void);

/* 创建/打开消息队列 */
struct mqueue *mq_open(const char *name, int oflag, mode_t mode, struct mq_attr *attr);

/* 关闭消息队列 */
int mq_close(struct mqueue *mq);

/* 删除消息队列 */
int mq_unlink(const char *name);

/* 发送消息 */
int mq_send(struct mqueue *mq, const char *msg_ptr, size_t msg_len, uint32_t msg_prio);

/* 接收消息 */
ssize_t mq_receive(struct mqueue *mq, char *msg_ptr, size_t msg_len, uint32_t *msg_prio);

/* 定时发送消息 */
int mq_timedsend(struct mqueue *mq, const char *msg_ptr, size_t msg_len, 
                 uint32_t msg_prio, const struct timespec *abs_timeout);

/* 定时接收消息 */
ssize_t mq_timedreceive(struct mqueue *mq, char *msg_ptr, size_t msg_len, 
                        uint32_t *msg_prio, const struct timespec *abs_timeout);

/* 获取/设置队列属性 */
int mq_getattr(struct mqueue *mq, struct mq_attr *attr);
int mq_setattr(struct mqueue *mq, const struct mq_attr *newattr, struct mq_attr *oldattr);

/* 通知（简化版）*/
int mq_notify(struct mqueue *mq, const struct sigevent *notification);

/* 查找消息队列 */
struct mqueue *mqueue_find(const char *name);

/* 打印消息队列信息（调试用）*/
void mqueue_print_info(void);

/**
 * 系统调用接口
 */
int sys_mq_open(const char *name, int oflag, mode_t mode, struct mq_attr *attr);
int sys_mq_close(int mqdes);
int sys_mq_unlink(const char *name);
int sys_mq_send(int mqdes, const char *msg_ptr, size_t msg_len, uint32_t msg_prio);
ssize_t sys_mq_receive(int mqdes, char *msg_ptr, size_t msg_len, uint32_t *msg_prio);
int sys_mq_getattr(int mqdes, struct mq_attr *attr);
int sys_mq_setattr(int mqdes, const struct mq_attr *newattr, struct mq_attr *oldattr);

#endif /* _IPC_MQUEUE_H */

