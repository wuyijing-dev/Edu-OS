/**
 * POSIX共享内存实现
 * Linux风格
 */

#include <ipc/shm.h>
#include <mm/kmalloc.h>
#include <kernel.h>
#include <string.h>
#include <fs/vfs.h>  /* 包含O_CREAT等标志 */

/* 全局共享内存管理器 */
struct shm_manager *g_shm_manager = NULL;

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
 * 初始化共享内存子系统
 */
int shm_init(void)
{
    kprintf("[SHM] Initializing POSIX shared memory subsystem...\n");
    
    g_shm_manager = (struct shm_manager *)kmalloc(sizeof(struct shm_manager));
    if (!g_shm_manager) {
        kprintf("[SHM] Failed to allocate shared memory manager\n");
        return -1;
    }
    
    memset(g_shm_manager, 0, sizeof(struct shm_manager));
    g_shm_manager->lock = 0;
    
    kprintf("[SHM] Shared memory subsystem initialized\n");
    return 0;
}

/**
 * 创建或打开共享内存对象
 */
struct shm_object *shm_create(const char *name, int oflag, mode_t mode)
{
    if (!g_shm_manager || !name) {
        return NULL;
    }
    
    spin_lock(&g_shm_manager->lock);
    
    /* 查找是否已存在 */
    struct shm_object *obj = NULL;
    for (int i = 0; i < MAX_SHM_OBJECTS; i++) {
        if (g_shm_manager->objects[i].name[0] != '\0' &&
            strcmp(g_shm_manager->objects[i].name, name) == 0) {
            obj = &g_shm_manager->objects[i];
            break;
        }
    }
    
    /* 如果存在且要求独占创建，失败 */
    if (obj && (oflag & O_CREAT) && (oflag & O_EXCL)) {
        spin_unlock(&g_shm_manager->lock);
        kprintf("[SHM] Object already exists: %s\n", name);
        return NULL;
    }
    
    /* 如果不存在且没有O_CREAT标志，失败 */
    if (!obj && !(oflag & O_CREAT)) {
        spin_unlock(&g_shm_manager->lock);
        kprintf("[SHM] Object not found: %s\n", name);
        return NULL;
    }
    
    /* 创建新对象 */
    if (!obj) {
        /* 查找空闲槽位 */
        for (int i = 0; i < MAX_SHM_OBJECTS; i++) {
            if (g_shm_manager->objects[i].name[0] == '\0') {
                obj = &g_shm_manager->objects[i];
                break;
            }
        }
        
        if (!obj) {
            spin_unlock(&g_shm_manager->lock);
            kprintf("[SHM] Too many shared memory objects\n");
            return NULL;
        }
        
        /* 初始化对象 */
        strncpy(obj->name, name, SHM_NAME_MAX - 1);
        obj->name[SHM_NAME_MAX - 1] = '\0';
        obj->size = 0;
        obj->data = NULL;
        obj->ref_count = 0;
        obj->mode = mode;
        obj->unlinked = false;
        obj->lock = 0;
        
        kprintf("[SHM] Created shared memory object: %s\n", name);
    }
    
    /* 增加引用计数 */
    obj->ref_count++;
    
    spin_unlock(&g_shm_manager->lock);
    
    return obj;
}

/**
 * 查找共享内存对象
 */
struct shm_object *shm_find(const char *name)
{
    if (!g_shm_manager || !name) {
        return NULL;
    }
    
    spin_lock(&g_shm_manager->lock);
    
    struct shm_object *obj = NULL;
    for (int i = 0; i < MAX_SHM_OBJECTS; i++) {
        if (g_shm_manager->objects[i].name[0] != '\0' &&
            strcmp(g_shm_manager->objects[i].name, name) == 0) {
            obj = &g_shm_manager->objects[i];
            break;
        }
    }
    
    spin_unlock(&g_shm_manager->lock);
    
    return obj;
}

/**
 * 增加共享内存对象引用计数
 */
void shm_get(struct shm_object *obj)
{
    if (!obj) {
        return;
    }
    
    spin_lock(&obj->lock);
    obj->ref_count++;
    spin_unlock(&obj->lock);
}

/**
 * 减少共享内存对象引用计数
 */
void shm_put(struct shm_object *obj)
{
    if (!obj) {
        return;
    }
    
    spin_lock(&obj->lock);
    
    if (obj->ref_count > 0) {
        obj->ref_count--;
    }
    
    /* 如果引用计数为0且已被unlink，释放资源 */
    if (obj->ref_count == 0 && obj->unlinked) {
        if (obj->data) {
            kfree(obj->data);
            obj->data = NULL;
        }
        
        kprintf("[SHM] Freed shared memory object: %s\n", obj->name);
        
        /* 清空对象 */
        memset(obj, 0, sizeof(struct shm_object));
    }
    
    spin_unlock(&obj->lock);
}

/**
 * 删除共享内存对象
 */
int shm_unlink_object(const char *name)
{
    if (!g_shm_manager || !name) {
        return -1;
    }
    
    spin_lock(&g_shm_manager->lock);
    
    struct shm_object *obj = NULL;
    for (int i = 0; i < MAX_SHM_OBJECTS; i++) {
        if (g_shm_manager->objects[i].name[0] != '\0' &&
            strcmp(g_shm_manager->objects[i].name, name) == 0) {
            obj = &g_shm_manager->objects[i];
            break;
        }
    }
    
    if (!obj) {
        spin_unlock(&g_shm_manager->lock);
        return -1;  /* ENOENT */
    }
    
    /* 标记为已unlink */
    obj->unlinked = true;
    
    spin_unlock(&g_shm_manager->lock);
    
    kprintf("[SHM] Unlinked shared memory object: %s\n", name);
    
    /* 如果引用计数为0，立即释放 */
    shm_put(obj);
    
    return 0;
}

/**
 * 设置共享内存对象大小
 */
int shm_truncate(struct shm_object *obj, size_t size)
{
    if (!obj) {
        return -1;
    }
    
    spin_lock(&obj->lock);
    
    /* 如果已有数据且大小不同，重新分配 */
    if (obj->data && obj->size != size) {
        kfree(obj->data);
        obj->data = NULL;
        obj->size = 0;
    }
    
    /* 分配新内存 */
    if (!obj->data && size > 0) {
        obj->data = kmalloc(size);
        if (!obj->data) {
            spin_unlock(&obj->lock);
            kprintf("[SHM] Failed to allocate %zu bytes for %s\n", size, obj->name);
            return -1;
        }
        
        /* 清零 */
        memset(obj->data, 0, size);
        obj->size = size;
        
        kprintf("[SHM] Truncated %s to %zu bytes\n", obj->name, size);
    }
    
    spin_unlock(&obj->lock);
    
    return 0;
}

