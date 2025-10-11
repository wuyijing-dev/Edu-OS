/*
 * proc_version.c - /proc/version 实现
 * 
 * 显示内核版本信息（Linux风格）
 */

#include <fs/procfs.h>
#include <kernel.h>
#include <string.h>

/* 内核版本信息 */
#define EDUOS_VERSION_MAJOR 0
#define EDUOS_VERSION_MINOR 9
#define EDUOS_VERSION_PATCH 0
#define EDUOS_VERSION_NAME "Quantum"

/*
 * 读取内核版本
 */
int proc_version_read(char *buf, size_t size, off_t *offset, void *data)
{
    (void)data;
    
    /* 生成版本信息（类似 Linux） */
    char content[512];
    int len = snprintf(content, sizeof(content),
        "EduOS version %d.%d.%d-%s (%s@%s) "
        "(gcc version " __VERSION__ ") "
        "%s %s\n",
        EDUOS_VERSION_MAJOR,
        EDUOS_VERSION_MINOR,
        EDUOS_VERSION_PATCH,
        EDUOS_VERSION_NAME,
        "eduos",          // 编译用户
        "eduos-builder",  // 编译主机
        __DATE__,
        __TIME__
    );
    
    /* 处理 offset */
    return procfs_read_with_offset(content, len, buf, size, offset);
}

