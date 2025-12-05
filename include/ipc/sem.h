/*
 * sem.h - System V信号量头文件
 * 对标Linux include/linux/sem.h
 */

#ifndef _SEM_H
#define _SEM_H

#include <stdint.h>
#include <sys/types.h>
#include <ipc/ipc.h>

/* 信号量操作 */
struct sembuf {
    unsigned short sem_num;     /* 信号量编号 */
    short sem_op;               /* 操作值 */
    short sem_flg;              /* 操作标志 */
};

/* 信号量信息 */
struct seminfo {
    int semmap;     /* 信号量映射项数 */
    int semmni;     /* 最大信号量集数 */
    int semmns;     /* 最大信号量数 */
    int semmnu;     /* 最大撤销结构数 */
    int semmsl;     /* 每集最大信号量数 */
    int semopm;     /* 每次调用最大操作数 */
    int semume;     /* 每进程最大撤销数 */
    int semusz;     /* 撤销结构大小 */
    int semvmx;     /* 信号量最大值 */
    int semaem;     /* 退出时最大调整值 */
};

/* 信号量集描述符 */
struct semid_ds {
    struct ipc_perm sem_perm;   /* IPC权限 */
    time_t sem_otime;          /* 最后操作时间 */
    time_t sem_ctime;          /* 最后修改时间 */
    unsigned short sem_nsems; /* 信号量数量 */
};

/* 信号量操作标志 */
#define SEM_UNDO    0x1000      /* 自动撤销 */
#define SEM_A       0200        /* 修改权限 */
#define SEM_R       0400        /* 读权限 */

/* 信号量系统调用 */
int sys_semget(key_t key, int nsems, int semflg);
int sys_semop(int semid, struct sembuf *sops, unsigned nsops);
int sys_semctl(int semid, int semnum, int cmd, ...);

/* 内部函数 */
struct sem_array;
struct sem_queue;

int newary(key_t key, int nsems, int semflg);
struct sem_array *sem_lock(int semid);
void sem_unlock(struct sem_array *sma);
int try_atomic_semop(struct sem_array *sma, struct sembuf *sops, int nsops);
void do_smart_update(struct sem_array *sma, struct sem_queue *q, int undo, int max);

#endif /* _SEM_H */