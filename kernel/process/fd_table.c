/*
 * fd_table.c - Per-Process文件描述符表实现
 * 
 * Linux风格：每个进程独立的文件描述符表
 */

#include <process/process.h>
#include <mm/kmalloc.h>
#include <kernel.h>
#include <string.h>

/*
 * 创建文件描述符表
 */
struct file_descriptor_table *fd_table_create(void)
{
    struct file_descriptor_table *table = kmalloc(sizeof(struct file_descriptor_table));
    if (!table) {
        return NULL;
    }
    
    memset(table, 0, sizeof(struct file_descriptor_table));
    table->count = 0;
    
    return table;
}

/*
 * 销毁文件描述符表
 */
void fd_table_destroy(struct file_descriptor_table *table)
{
    if (!table) {
        return;
    }
    
    /* 关闭所有打开的文件 */
    for (int i = 0; i < MAX_FILES_PER_PROCESS; i++) {
        if (table->files[i]) {
            /* TODO: 调用vfs_close释放文件 */
            table->files[i] = NULL;
        }
    }
    
    kfree(table);
}

/*
 * 分配文件描述符
 */
int fd_table_alloc(struct file_descriptor_table *table, struct vfs_file *file)
{
    if (!table || !file) {
        return -1;
    }
    
    /* 查找空闲的fd */
    for (int i = 0; i < MAX_FILES_PER_PROCESS; i++) {
        if (!table->files[i]) {
            table->files[i] = file;
            table->count++;
            return i;
        }
    }
    
    return -1;  /* 文件描述符耗尽 */
}

/*
 * 释放文件描述符
 */
int fd_table_free(struct file_descriptor_table *table, int fd)
{
    if (!table || fd < 0 || fd >= MAX_FILES_PER_PROCESS) {
        return -1;
    }
    
    if (table->files[fd]) {
        table->files[fd] = NULL;
        table->count--;
        return 0;
    }
    
    return -1;
}

/*
 * 获取文件
 */
struct vfs_file *fd_table_get(struct file_descriptor_table *table, int fd)
{
    if (!table || fd < 0 || fd >= MAX_FILES_PER_PROCESS) {
        return NULL;
    }
    
    return table->files[fd];
}

/*
 * 复制文件描述符表（用于fork）
 */
struct file_descriptor_table *fd_table_copy(struct file_descriptor_table *src)
{
    if (!src) {
        return NULL;
    }
    
    struct file_descriptor_table *dst = fd_table_create();
    if (!dst) {
        return NULL;
    }
    
    /* 复制所有文件描述符（增加引用计数）*/
    for (int i = 0; i < MAX_FILES_PER_PROCESS; i++) {
        if (src->files[i]) {
            dst->files[i] = src->files[i];
            /* TODO: 增加文件引用计数 */
        }
    }
    
    dst->count = src->count;
    
    return dst;
}

