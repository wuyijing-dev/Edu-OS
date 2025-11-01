/*
 * vfs_buffer.c - VFS缓冲区管理
 * 
 * Linux风格：预分配I/O缓冲区，避免在Page Fault处理中触发新的Page Fault
 */

#include <fs/vfs.h>
#include <mm/kmalloc.h>
#include <kernel.h>
#include <string.h>

/* VFS I/O缓冲区池 */
#define VFS_BUFFER_SIZE  (16 * 4096)  /* 64KB缓冲区 */
#define VFS_NUM_BUFFERS  4             /* 4个缓冲区 */

static struct {
    char *buffers[VFS_NUM_BUFFERS];
    bool in_use[VFS_NUM_BUFFERS];
    bool initialized;
} vfs_buffer_pool;

/*
 * 初始化VFS缓冲区池
 */
void vfs_buffer_init(void)
{
    if (vfs_buffer_pool.initialized) {
        return;
    }
    
    kprintf("[VFS_BUFFER] Initializing buffer pool...\n");
    
    memset(&vfs_buffer_pool, 0, sizeof(vfs_buffer_pool));
    
    /* 预分配所有缓冲区 */
    for (int i = 0; i < VFS_NUM_BUFFERS; i++) {
        vfs_buffer_pool.buffers[i] = kmalloc(VFS_BUFFER_SIZE);
        if (!vfs_buffer_pool.buffers[i]) {
            kprintf("[VFS_BUFFER] ERROR: Failed to allocate buffer %d\n", i);
            return;
        }
        vfs_buffer_pool.in_use[i] = false;
        
        /* 关键：预先访问所有页，确保kmalloc后的页面已映射 */
        for (uint32_t j = 0; j < VFS_BUFFER_SIZE; j += 4096) {
            volatile char *touch = &vfs_buffer_pool.buffers[i][j];
            *touch = 0;  /* 触发Page Fault，确保页面存在 */
        }
    }
    
    vfs_buffer_pool.initialized = true;
    kprintf("[VFS_BUFFER] Initialized: %d buffers x %u bytes\n", 
            VFS_NUM_BUFFERS, VFS_BUFFER_SIZE);
}

/*
 * 分配VFS缓冲区
 */
char *vfs_buffer_alloc(void)
{
    if (!vfs_buffer_pool.initialized) {
        vfs_buffer_init();
    }
    
    for (int i = 0; i < VFS_NUM_BUFFERS; i++) {
        if (!vfs_buffer_pool.in_use[i]) {
            vfs_buffer_pool.in_use[i] = true;
            return vfs_buffer_pool.buffers[i];
        }
    }
    
    /* 没有空闲缓冲区，使用kmalloc（可能有风险）*/
    return kmalloc(VFS_BUFFER_SIZE);
}

/*
 * 释放VFS缓冲区
 */
void vfs_buffer_free(char *buffer)
{
    if (!buffer) {
        return;
    }
    
    /* 检查是否是池中的缓冲区 */
    for (int i = 0; i < VFS_NUM_BUFFERS; i++) {
        if (vfs_buffer_pool.buffers[i] == buffer) {
            vfs_buffer_pool.in_use[i] = false;
            return;
        }
    }
    
    /* 不是池中的，释放 */
    kfree(buffer);
}


