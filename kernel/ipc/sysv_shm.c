/*
 * sysv_shm.c - System V共享内存实现
 * 对标Linux ipc/shm.c
 */

#include <ipc/shm.h>
#include <ipc/ipc.h>
#include <mm/kmalloc.h>
#include <mm/mmap.h>
#include <string.h>
#include <errno.h>
#include <kernel.h>

/* System V共享内存最大数量 */
#define SHMMNI  4096      /* 最大共享内存段数 */
#define SHMMAX  0x2000000 /* 最大共享内存段大小 (32MB) */
#define SHMMIN  1         /* 最小共享内存段大小 */
#define SHMALL  2097152   /* 最大共享内存页数 */

/* System V共享内存内核结构 */
struct shmid_kernel {
    struct ipc_perm shm_perm;   /* IPC权限 */
    unsigned long shm_segsz;    /* 段大小 */
    void *shm_pages;           /* 页面指针数组 */
    unsigned long shm_npages;   /* 页面数量 */
    time_t shm_atime;          /* 最后连接时间 */
    time_t shm_dtime;          /* 最后分离时间 */
    time_t shm_ctime;          /* 最后修改时间 */
    pid_t shm_cpid;            /* 创建者PID */
    pid_t shm_lpid;            /* 最后操作者PID */
    unsigned long shm_nattch;  /* 当前连接数 */
    struct list_head shm_clist; /* 连接进程链表 */
};

/* 共享内存连接结构 */
struct shm_clist {
    struct list_head list;      /* 链表节点 */
    struct task_struct *task;   /* 连接进程 */
    void *shm_addr;            /* 连接地址 */
    int shm_flags;             /* 连接标志 */
};

/* 全局共享内存数组 */
static struct shmid_kernel *shm_segments[SHMMNI];
static unsigned long total_shm_pages = 0;

/* 获取共享内存段 */
static struct shmid_kernel *shm_lock(int shmid)
{
    int idx = ipc_id_to_idx(shmid);
    if (idx < 0 || idx >= SHMMNI) {
        return NULL;
    }
    
    struct shmid_kernel *shp = shm_segments[idx];
    if (!shp || shp->shm_perm.seq != (shmid >> 16)) {
        return NULL;
    }
    
    return shp;
}

/* 创建新的共享内存段 */
static int newseg(key_t key, size_t size, int shmflg)
{
    if (size < SHMMIN || size > SHMMAX) {
        return -EINVAL;
    }
    
    /* 计算需要的页面数 */
    unsigned long npages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    
    if (total_shm_pages + npages > SHMALL) {
        return -ENOSPC;
    }
    
    /* 查找空闲槽位 */
    int idx;
    for (idx = 0; idx < SHMMNI; idx++) {
        if (!shm_segments[idx]) {
            break;
        }
    }
    
    if (idx >= SHMMNI) {
        return -ENOSPC;
    }
    
    /* 分配共享内存结构 */
    struct shmid_kernel *shp = kmalloc(sizeof(struct shmid_kernel));
    if (!shp) {
        return -ENOMEM;
    }
    
    memset(shp, 0, sizeof(struct shmid_kernel));
    
    /* 初始化IPC权限 */
    shp->shm_perm.key = key;
    shp->shm_perm.uid = shp->shm_perm.cuid = 0; /* TODO: 当前用户ID */
    shp->shm_perm.gid = shp->shm_perm.cgid = 0; /* TODO: 当前组ID */
    shp->shm_perm.mode = (shmflg & IPC_PERM) | IPC_ALLOC;
    shp->shm_perm.seq = idx;
    
    /* 分配页面指针数组 */
    shp->shm_pages = kmalloc(sizeof(void *) * npages);
    if (!shp->shm_pages) {
        kfree(shp);
        return -ENOMEM;
    }
    
    /* 分配物理页面 */
    for (unsigned long i = 0; i < npages; i++) {
        shp->shm_pages[i] = alloc_page();
        if (!shp->shm_pages[i]) {
            /* 释放已分配的页面 */
            for (unsigned long j = 0; j < i; j++) {
                free_page(shp->shm_pages[j]);
            }
            kfree(shp->shm_pages);
            kfree(shp);
            return -ENOMEM;
        }
    }
    
    /* 初始化其他字段 */
    shp->shm_segsz = size;
    shp->shm_npages = npages;
    shp->shm_ctime = time(NULL);
    shp->shm_atime = shp->shm_dtime = 0;
    shp->shm_cpid = 0; /* TODO: 当前进程ID */
    shp->shm_lpid = 0;
    shp->shm_nattch = 0;
    INIT_LIST_HEAD(&shp->shm_clist);
    
    /* 添加到全局数组 */
    shm_segments[idx] = shp;
    total_shm_pages += npages;
    
    return ipc_idx_to_id(idx, shp->shm_perm.seq);
}

/* System V共享内存获取 */
int sys_shmget(key_t key, size_t size, int shmflg)
{
    if (key == IPC_PRIVATE) {
        /* 私有共享内存 */
        return newseg(key, size, shmflg);
    }
    
    /* 查找已存在的共享内存 */
    for (int i = 0; i < SHMMNI; i++) {
        struct shmid_kernel *shp = shm_segments[i];
        if (shp && shp->shm_perm.key == key) {
            if ((shmflg & IPC_CREAT) && (shmflg & IPC_EXCL)) {
                return -EEXIST;
            }
            
            if (size > 0 && shp->shm_segsz < size) {
                return -EINVAL;
            }
            
            /* TODO: 权限检查 */
            return ipc_idx_to_id(i, shp->shm_perm.seq);
        }
    }
    
    /* 创建新共享内存 */
    if (shmflg & IPC_CREAT) {
        return newseg(key, size, shmflg);
    }
    
    return -ENOENT;
}

/* System V共享内存连接 */
void *sys_shmat(int shmid, const void *shmaddr, int shmflg)
{
    struct shmid_kernel *shp = shm_lock(shmid);
    if (!shp) {
        return (void *)-EINVAL;
    }
    
    /* TODO: 权限检查 */
    
    /* 分配虚拟地址空间 */
    void *addr;
    if (shmaddr) {
        addr = (void *)shmaddr;
    } else {
        /* 自动分配地址 */
        addr = find_unmapped_area(shp->shm_segsz);
    }
    
    if (!addr) {
        return (void *)-ENOMEM;
    }
    
    /* 映射页面到进程地址空间 */
    for (unsigned long i = 0; i < shp->shm_npages; i++) {
        void *page_addr = (void *)((uintptr_t)addr + i * PAGE_SIZE);
        map_page_to_user(current->mm, page_addr, shp->shm_pages[i], 
                        (shmflg & SHM_RDONLY) ? PAGE_READONLY : PAGE_READWRITE);
    }
    
    /* 更新连接信息 */
    shp->shm_nattch++;
    shp->shm_atime = time(NULL);
    shp->shm_lpid = 0; /* TODO: 当前进程ID */
    
    return addr;
}

/* System V共享内存分离 */
int sys_shmdt(const void *shmaddr)
{
    if (!shmaddr) {
        return -EINVAL;
    }
    
    /* 查找连接的共享内存 */
    for (int i = 0; i < SHMMNI; i++) {
        struct shmid_kernel *shp = shm_segments[i];
        if (!shp) continue;
        
        /* TODO: 检查地址是否属于此共享内存段 */
        /* 简化实现：假设地址匹配 */
        
        /* 解除映射 */
        unmap_user_pages(current->mm, (void *)shmaddr, shp->shm_segsz);
        
        /* 更新连接信息 */
        shp->shm_nattch--;
        shp->shm_dtime = time(NULL);
        
        /* 如果没有连接且已标记删除，释放资源 */
        if (shp->shm_nattch == 0 && (shp->shm_perm.mode & IPC_RMID)) {
            /* 释放页面 */
            for (unsigned long j = 0; j < shp->shm_npages; j++) {
                free_page(shp->shm_pages[j]);
            }
            total_shm_pages -= shp->shm_npages;
            kfree(shp->shm_pages);
            kfree(shp);
            shm_segments[i] = NULL;
        }
        
        return 0;
    }
    
    return -EINVAL;
}

/* System V共享内存控制 */
int sys_shmctl(int shmid, int cmd, struct shmid_ds *buf)
{
    struct shmid_kernel *shp = shm_lock(shmid);
    if (!shp) {
        return -EINVAL;
    }
    
    int result = 0;
    
    switch (cmd) {
    case IPC_RMID:
        /* 标记删除 */
        if (0 /* TODO: 权限检查 */) {
            result = -EACCES;
            break;
        }
        shp->shm_perm.mode |= IPC_RMID;
        
        /* 如果没有连接，立即删除 */
        if (shp->shm_nattch == 0) {
            for (unsigned long i = 0; i < shp->shm_npages; i++) {
                free_page(shp->shm_pages[i]);
            }
            total_shm_pages -= shp->shm_npages;
            kfree(shp->shm_pages);
            kfree(shp);
            shm_segments[ipc_id_to_idx(shmid)] = NULL;
        }
        break;
        
    case IPC_SET:
        /* 设置属性 */
        if (0 /* TODO: 权限检查 */) {
            result = -EACCES;
            break;
        }
        /* TODO: 从用户空间复制属性 */
        shp->shm_ctime = time(NULL);
        break;
        
    case IPC_STAT:
        /* 获取属性 */
        if (0 /* TODO: 权限检查 */) {
            result = -EACCES;
            break;
        }
        /* TODO: 复制属性到用户空间 */
        break;
        
    case SHM_LOCK:
        /* 锁定共享内存 */
        /* TODO: 实现内存锁定 */
        break;
        
    case SHM_UNLOCK:
        /* 解锁共享内存 */
        /* TODO: 实现内存解锁 */
        break;
        
    default:
        result = -EINVAL;
        break;
    }
    
    return result;
}