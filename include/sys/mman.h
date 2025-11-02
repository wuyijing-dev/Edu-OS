/**
 * POSIX内存管理接口
 * 包括mmap和共享内存
 */

#ifndef _SYS_MMAN_H
#define _SYS_MMAN_H

#include <types.h>

/* mmap保护标志 */
#define PROT_NONE   0x0     /* 页不可访问 */
#define PROT_READ   0x1     /* 页可读 */
#define PROT_WRITE  0x2     /* 页可写 */
#define PROT_EXEC   0x4     /* 页可执行 */

/* mmap标志 */
#define MAP_SHARED      0x01    /* 共享映射 */
#define MAP_PRIVATE     0x02    /* 私有映射（COW）*/
#define MAP_FIXED       0x10    /* 固定地址映射 */
#define MAP_ANONYMOUS   0x20    /* 匿名映射（不关联文件）*/
#define MAP_ANON        MAP_ANONYMOUS

/* mmap失败返回值 */
#define MAP_FAILED  ((void *) -1)

/* msync标志 */
#define MS_ASYNC        1   /* 异步同步 */
#define MS_SYNC         2   /* 同步同步 */
#define MS_INVALIDATE   4   /* 使缓存失效 */

/* madvise建议 */
#define MADV_NORMAL     0   /* 无特殊处理 */
#define MADV_RANDOM     1   /* 随机访问 */
#define MADV_SEQUENTIAL 2   /* 顺序访问 */
#define MADV_WILLNEED   3   /* 将要访问 */
#define MADV_DONTNEED   4   /* 不再访问 */

/* shm_open标志（使用open()的标志）- 避免与vfs.h冲突 */
#ifndef O_RDONLY
#define O_RDONLY    0x0000
#endif
#ifndef O_WRONLY
#define O_WRONLY    0x0001
#endif
#ifndef O_RDWR
#define O_RDWR      0x0002
#endif
#ifndef O_CREAT
#define O_CREAT     0x0040
#endif
#ifndef O_EXCL
#define O_EXCL      0x0080
#endif
#ifndef O_TRUNC
#define O_TRUNC     0x0200
#endif

/* 系统调用 */
#ifdef __KERNEL__
void *sys_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
int sys_munmap(void *addr, size_t length);
int sys_msync(void *addr, size_t length, int flags);
int sys_mprotect(void *addr, size_t length, int prot);
int sys_shm_open(const char *name, int oflag, mode_t mode);
int sys_shm_unlink(const char *name);
#else
void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
int munmap(void *addr, size_t length);
int msync(void *addr, size_t length, int flags);
int mprotect(void *addr, size_t length, int prot);
int shm_open(const char *name, int oflag, mode_t mode);
int shm_unlink(const char *name);
#endif

#endif /* _SYS_MMAN_H */
