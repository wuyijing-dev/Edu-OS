/*
 * ipc.h - System V IPC通用头文件
 * 对标Linux include/linux/ipc.h
 */

#ifndef _IPC_H
#define _IPC_H

#include <stdint.h>
#include <sys/types.h>

/* IPC key特殊值 */
#define IPC_PRIVATE ((key_t)0)      /* 私有key */
#define IPC_RMID    0               /* 删除资源 */
#define IPC_SET     1               /* 设置属性 */
#define IPC_STAT    2               /* 获取属性 */
#define IPC_INFO    3               /* 获取信息 */

/* IPC权限 */
#define IPC_CREAT   00001000        /* 如果不存在则创建 */
#define IPC_EXCL    00002000        /* 如果存在则失败 */
#define IPC_NOWAIT  00004000        /* 非阻塞操作 */

/* IPC权限掩码 */
#define IPC_PERM    0000777         /* 权限位掩码 */

/* IPC权限结构 */
struct ipc_perm {
    key_t key;                  /* IPC key */
    uint16_t uid;               /* 所有者用户ID */
    uint16_t gid;               /* 所有者组ID */
    uint16_t cuid;              /* 创建者用户ID */
    uint16_t cgid;              /* 创建者组ID */
    uint16_t mode;              /* 权限模式 */
    uint16_t seq;               /* 序列号 */
};

/* IPC命名空间 */
struct ipc_namespace {
    int sem_ctls[4];           /* 信号量控制参数 */
    int msg_ctl;               /* 消息队列控制参数 */
    int shm_ctl;               /* 共享内存控制参数 */
};

/* IPC操作结构 */
struct ipc_ops {
    int (*get)(int id);
    void (*lock)(int id);
    void (*unlock)(int id);
    int (*destroy)(int id);
};

/* IPC ID转换 */
static inline int ipc_id_to_idx(int id)
{
    return id & 0xFFFF;
}

static inline int ipc_idx_to_id(int idx, int seq)
{
    return (seq << 16) | idx;
}

/* IPC权限检查 */
int ipcperms(struct ipc_perm *ipcp, short flag);

#endif /* _IPC_H */