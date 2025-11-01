/*
 * pthread.h - POSIX线程支持
 */

#ifndef _PTHREAD_H
#define _PTHREAD_H

#include "../syscall/types.h"

/* 线程ID类型 */
typedef int pthread_t;

/* 线程属性 */
typedef struct {
    int detachstate;
    int schedpolicy;
    int schedpriority;
    size_t stacksize;
} pthread_attr_t;

/* 互斥锁 */
typedef struct {
    int locked;
    int owner;
} pthread_mutex_t;

/* 条件变量 */
typedef struct {
    int waiting_count;
} pthread_cond_t;

/* 线程分离状态 */
#define PTHREAD_CREATE_JOINABLE 0
#define PTHREAD_CREATE_DETACHED 1

/* 互斥锁初始化器 */
#define PTHREAD_MUTEX_INITIALIZER {0, 0}

/* 条件变量初始化器 */
#define PTHREAD_COND_INITIALIZER {0}

/* 线程函数 */
int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                   void *(*start_routine)(void*), void *arg);
int pthread_join(pthread_t thread, void **retval);
int pthread_detach(pthread_t thread);
void pthread_exit(void *retval);
pthread_t pthread_self(void);

/* 互斥锁函数 */
int pthread_mutex_init(pthread_mutex_t *mutex, const void *attr);
int pthread_mutex_destroy(pthread_mutex_t *mutex);
int pthread_mutex_lock(pthread_mutex_t *mutex);
int pthread_mutex_trylock(pthread_mutex_t *mutex);
int pthread_mutex_unlock(pthread_mutex_t *mutex);

/* 条件变量函数 */
int pthread_cond_init(pthread_cond_t *cond, const void *attr);
int pthread_cond_destroy(pthread_cond_t *cond);
int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);
int pthread_cond_signal(pthread_cond_t *cond);
int pthread_cond_broadcast(pthread_cond_t *cond);

#endif /* _PTHREAD_H */

