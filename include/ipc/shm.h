/**
 * POSIX共享内存对象
 * Linux风格实现
 */

#ifndef _IPC_SHM_H
#define _IPC_SHM_H

#include <types.h>
#include <fs/vfs.h>

/* 共享内存对象最大数量 */
#define MAX_SHM_OBJECTS 64

/* 共享内存对象名称最大长度 */
#define SHM_NAME_MAX 256

/**
 * 共享内存对象
 */
struct shm_object {
    char name[SHM_NAME_MAX];        /* 对象名称（/dev/shm/xxx）*/
    size_t size;                    /* 对象大小 */
    void *data;                     /* 数据指针（内核空间）*/
    uint32_t ref_count;             /* 引用计数 */
    mode_t mode;                    /* 访问权限 */
    bool unlinked;                  /* 是否已被unlink */
    uint32_t lock;                  /* 保护锁 */
};

/**
 * 共享内存管理器
 */
struct shm_manager {
    struct shm_object objects[MAX_SHM_OBJECTS];
    uint32_t lock;                  /* 保护整个管理器 */
};

/**
 * 全局共享内存管理器
 */
extern struct shm_manager *g_shm_manager;

/**
 * 初始化共享内存子系统
 */
int shm_init(void);

/**
 * 创建或打开共享内存对象
 */
struct shm_object *shm_create(const char *name, int oflag, mode_t mode);

/**
 * 查找共享内存对象
 */
struct shm_object *shm_find(const char *name);

/**
 * 增加共享内存对象引用计数
 */
void shm_get(struct shm_object *obj);

/**
 * 减少共享内存对象引用计数
 */
void shm_put(struct shm_object *obj);

/**
 * 删除共享内存对象
 */
int shm_unlink_object(const char *name);

/**
 * 设置共享内存对象大小
 */
int shm_truncate(struct shm_object *obj, size_t size);

#endif /* _IPC_SHM_H */
