/*
 * semaphore.c - 信号量实现
 */

#include <sync/semaphore.h>
#include <process/process.h>
#include <kernel.h>

/*
 * 初始化信号量
 */
void sem_init(struct semaphore *sem, int32_t count, const char *name)
{
    if (!sem) return;
    
    sem->count = count;
    sem->wait_list = NULL;
    sem->name = name ? name : "unnamed";
}

/*
 * P 操作（等待，减少计数）
 */
void sem_wait(struct semaphore *sem)
{
    if (!sem) return;
    
    /* 禁用中断（原子操作） */
    asm volatile("cli");
    
    sem->count--;
    
    if (sem->count < 0) {
        /* 需要阻塞 */
        struct process *current = process_get_current();
        
        if (current) {
            /* 加入等待队列 */
            current->next_waiting = sem->wait_list;
            sem->wait_list = current;
            
            /* 设置为阻塞状态 */
            current->state = PROCESS_STATE_BLOCKED;
            
            /* 重新调度 */
            asm volatile("sti");
            /* TODO: 调用 schedule() 切换到其他进程 */
            
            /* 当被唤醒时会从这里继续 */
        } else {
            /* 内核线程，简单忙等 */
            sem->count++;  // 恢复
            asm volatile("sti");
            
            /* 忙等待 */
            while (sem->count <= 0) {
                asm volatile("pause");
            }
            
            sem_wait(sem);  // 重试
        }
    } else {
        /* 成功获取 */
        asm volatile("sti");
    }
}

/*
 * V 操作（信号，增加计数）
 */
void sem_post(struct semaphore *sem)
{
    if (!sem) return;
    
    /* 禁用中断 */
    asm volatile("cli");
    
    sem->count++;
    
    if (sem->count <= 0 && sem->wait_list) {
        /* 有等待的进程，唤醒一个 */
        struct process *wake = sem->wait_list;
        sem->wait_list = wake->next_waiting;
        wake->next_waiting = NULL;
        
        /* 设置为就绪状态 */
        wake->state = PROCESS_STATE_READY;
        
        /* TODO: 加入就绪队列 */
    }
    
    /* 重新启用中断 */
    asm volatile("sti");
}

/*
 * 尝试 P 操作（非阻塞）
 */
int sem_trywait(struct semaphore *sem)
{
    if (!sem) return -1;
    
    asm volatile("cli");
    
    if (sem->count > 0) {
        sem->count--;
        asm volatile("sti");
        return 0;
    }
    
    asm volatile("sti");
    return -1;
}

/*
 * 获取信号量值
 */
int32_t sem_getvalue(struct semaphore *sem)
{
    if (!sem) return -1;
    return sem->count;
}

