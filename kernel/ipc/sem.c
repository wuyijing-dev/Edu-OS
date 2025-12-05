/*
 * sem.c - System V信号量实现
 * 对标Linux ipc/sem.c
 */

#include <ipc/sem.h>
#include <ipc/ipc.h>
#include <mm/kmalloc.h>
#include <string.h>
#include <errno.h>
#include <kernel.h>

/* 信号量数组最大数量 */
#define SEMMNI  128
#define SEMMSL  250
#define SEMMNS  (SEMMNI * SEMMSL)
#define SEMOPM  32
#define SEMVMX  32767
#define SEMAEM  16384

/* 信号量结构 */
struct sem {
    int semval;                 /* 信号量值 */
    int sempid;                 /* 最后操作PID */
};

/* 信号量集数组 */
struct sem_array {
    struct ipc_perm sem_perm;   /* IPC权限 */
    time_t sem_otime;          /* 最后操作时间 */
    time_t sem_ctime;          /* 最后修改时间 */
    struct sem *sem_base;      /* 信号量数组 */
    int sem_nsems;             /* 信号量数量 */
    struct sem_array *next;    /* 链表指针 */
    struct sem_queue *pending; /* 等待队列 */
    struct sem_queue **pending_last;
};

/* 信号量操作队列 */
struct sem_queue {
    struct sem_queue *next;      /* 链表指针 */
    struct sem_queue **prev;     /* 前驱指针 */
    struct task_struct *sleeper; /* 等待任务 */
    struct sem_undo *undo;       /* 撤销结构 */
    int pid;                     /* 进程ID */
    int status;                  /* 状态 */
    struct sembuf *sops;         /* 操作数组 */
    int nsops;                   /* 操作数量 */
    int alter;                   /* 是否修改信号量 */
};

/* 信号量撤销结构 */
struct sem_undo {
    struct sem_undo *proc_next;  /* 进程撤销链表 */
    struct sem_undo *id_next;    /* ID撤销链表 */
    int semid;                   /* 信号量集ID */
    short *semadj;               /* 调整值数组 */
};

/* 全局信号量数组 */
static struct sem_array *sem_arrays[SEMMNI];
static int sem_ctls[4] = {SEMMSL, SEMMNS, SEMOPM, SEMMNI};
static int used_sems = 0;

/* 信号量权限检查 */
static int sem_check_permissions(struct ipc_perm *perm, int flag)
{
    return ipcperms(perm, flag);
}

/* 查找信号量集 */
static struct sem_array *sem_lock(int semid)
{
    int idx = ipc_id_to_idx(semid);
    if (idx < 0 || idx >= SEMMNI) {
        return NULL;
    }
    
    struct sem_array *sma = sem_arrays[idx];
    if (!sma || sma->sem_perm.seq != (semid >> 16)) {
        return NULL;
    }
    
    return sma;
}

/* 释放信号量集锁 */
static void sem_unlock(struct sem_array *sma)
{
    /* 简单实现，实际应该使用自旋锁 */
}

/* 创建新信号量集 */
static int newary(key_t key, int nsems, int semflg)
{
    if (nsems < 0 || nsems > SEMMSL) {
        return -EINVAL;
    }
    
    if (used_sems + nsems > SEMMNS) {
        return -ENOSPC;
    }
    
    /* 查找空闲槽位 */
    int idx;
    for (idx = 0; idx < SEMMNI; idx++) {
        if (!sem_arrays[idx]) {
            break;
        }
    }
    
    if (idx >= SEMMNI) {
        return -ENOSPC;
    }
    
    /* 分配信号量集 */
    struct sem_array *sma = kmalloc(sizeof(struct sem_array));
    if (!sma) {
        return -ENOMEM;
    }
    
    memset(sma, 0, sizeof(struct sem_array));
    
    /* 初始化权限 */
    sma->sem_perm.key = key;
    sma->sem_perm.uid = sma->sem_perm.cuid = 0; /* TODO: 当前用户ID */
    sma->sem_perm.gid = sma->sem_perm.cgid = 0; /* TODO: 当前组ID */
    sma->sem_perm.mode = (semflg & IPC_PERM) | IPC_ALLOC;
    sma->sem_perm.seq = idx;
    
    /* 分配信号量数组 */
    sma->sem_base = kmalloc(sizeof(struct sem) * nsems);
    if (!sma->sem_base) {
        kfree(sma);
        return -ENOMEM;
    }
    
    memset(sma->sem_base, 0, sizeof(struct sem) * nsems);
    sma->sem_nsems = nsems;
    
    /* 初始化时间戳 */
    sma->sem_ctime = time(NULL);
    sma->sem_otime = 0;
    
    /* 添加到全局数组 */
    sem_arrays[idx] = sma;
    used_sems += nsems;
    
    return ipc_idx_to_id(idx, sma->sem_perm.seq);
}

/* 尝试原子信号量操作 */
int try_atomic_semop(struct sem_array *sma, struct sembuf *sops, int nsops)
{
    int result = 0;
    
    /* 首先检查所有操作是否可执行 */
    for (int i = 0; i < nsops; i++) {
        struct sembuf *sop = &sops[i];
        
        if (sop->sem_num >= sma->sem_nsems) {
            return -EFBIG;
        }
        
        struct sem *curr = &sma->sem_base[sop->sem_num];
        
        if (sop->sem_op > 0) {
            /* 增加操作 */
            if (curr->semval + sop->sem_op > SEMVMX) {
                return -ERANGE;
            }
        } else if (sop->sem_op < 0) {
            /* 减少操作 */
            if (curr->semval < -sop->sem_op) {
                return -EAGAIN;  /* 需要等待 */
            }
        } else {
            /* 等待操作 */
            if (curr->semval != 0) {
                return -EAGAIN;  /* 需要等待 */
            }
        }
    }
    
    /* 执行所有操作 */
    for (int i = 0; i < nsops; i++) {
        struct sembuf *sop = &sops[i];
        struct sem *curr = &sma->sem_base[sop->sem_num];
        
        if (sop->sem_op != 0) {
            curr->semval += sop->sem_op;
            curr->sempid = 0; /* TODO: 当前进程ID */
        }
    }
    
    return result;
}

/* System V信号量获取 */
int sys_semget(key_t key, int nsems, int semflg)
{
    if (key == IPC_PRIVATE) {
        /* 私有信号量集 */
        return newary(key, nsems, semflg);
    }
    
    /* 查找已存在的信号量集 */
    for (int i = 0; i < SEMMNI; i++) {
        struct sem_array *sma = sem_arrays[i];
        if (sma && sma->sem_perm.key == key) {
            if ((semflg & IPC_CREAT) && (semflg & IPC_EXCL)) {
                return -EEXIST;
            }
            
            if (nsems > 0 && sma->sem_nsems != nsems) {
                return -EINVAL;
            }
            
            if (sem_check_permissions(&sma->sem_perm, semflg & IPC_PERM)) {
                return ipc_idx_to_id(i, sma->sem_perm.seq);
            } else {
                return -EACCES;
            }
        }
    }
    
    /* 创建新信号量集 */
    if (semflg & IPC_CREAT) {
        return newary(key, nsems, semflg);
    }
    
    return -ENOENT;
}

/* System V信号量操作 */
int sys_semop(int semid, struct sembuf *sops, unsigned nsops)
{
    if (!sops || nsops > SEMOPM) {
        return -EINVAL;
    }
    
    struct sem_array *sma = sem_lock(semid);
    if (!sma) {
        return -EINVAL;
    }
    
    /* 检查权限 */
    int alter = 0;
    for (unsigned i = 0; i < nsops; i++) {
        if (sops[i].sem_op != 0) {
            alter = 1;
            break;
        }
    }
    
    if (alter && sem_check_permissions(&sma->sem_perm, SEM_A)) {
        sem_unlock(sma);
        return -EACCES;
    } else if (!alter && sem_check_permissions(&sma->sem_perm, SEM_R)) {
        sem_unlock(sma);
        return -EACCES;
    }
    
    /* 尝试原子操作 */
    int result = try_atomic_semop(sma, sops, nsops);
    
    if (result == 0) {
        /* 更新操作时间 */
        sma->sem_otime = time(NULL);
    }
    
    sem_unlock(sma);
    return result;
}

/* System V信号量控制 */
int sys_semctl(int semid, int semnum, int cmd, ...)
{
    struct sem_array *sma = sem_lock(semid);
    if (!sma) {
        return -EINVAL;
    }
    
    int result = 0;
    
    switch (cmd) {
    case IPC_RMID:
        /* 删除信号量集 */
        if (!sem_check_permissions(&sma->sem_perm, IPC_M)) {
            result = -EACCES;
            break;
        }
        
        /* 释放资源 */
        used_sems -= sma->sem_nsems;
        kfree(sma->sem_base);
        kfree(sma);
        sem_arrays[ipc_id_to_idx(semid)] = NULL;
        break;
        
    case IPC_SET:
        /* 设置属性 */
        if (!sem_check_permissions(&sma->sem_perm, IPC_M)) {
            result = -EACCES;
            break;
        }
        /* TODO: 实现属性设置 */
        sma->sem_ctime = time(NULL);
        break;
        
    case IPC_STAT:
        /* 获取属性 */
        if (!sem_check_permissions(&sma->sem_perm, SEM_R)) {
            result = -EACCES;
            break;
        }
        /* TODO: 复制属性到用户空间 */
        break;
        
    case GETVAL:
        /* 获取信号量值 */
        if (semnum < 0 || semnum >= sma->sem_nsems) {
            result = -EINVAL;
            break;
        }
        if (!sem_check_permissions(&sma->sem_perm, SEM_R)) {
            result = -EACCES;
            break;
        }
        result = sma->sem_base[semnum].semval;
        break;
        
    case SETVAL:
        /* 设置信号量值 */
        if (semnum < 0 || semnum >= sma->sem_nsems) {
            result = -EINVAL;
            break;
        }
        if (!sem_check_permissions(&sma->sem_perm, SEM_A)) {
            result = -EACCES;
            break;
        }
        /* TODO: 从参数获取值 */
        sma->sem_base[semnum].semval = 1; /* 示例值 */
        sma->sem_ctime = time(NULL);
        break;
        
    default:
        result = -EINVAL;
        break;
    }
    
    sem_unlock(sma);
    return result;
}