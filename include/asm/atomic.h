/*
 * atomic.h - 原子操作头文件
 * 参考Linux内核原子操作实现
 */

#ifndef _ASM_ATOMIC_H
#define _ASM_ATOMIC_H

#include <stdint.h>

/* 原子类型定义 */
typedef struct {
    volatile int counter;
} atomic_t;

/* 原子操作函数声明 */
static inline int atomic_read(const atomic_t *v);
static inline void atomic_set(atomic_t *v, int i);
static inline void atomic_add(int i, atomic_t *v);
static inline void atomic_sub(int i, atomic_t *v);
static inline void atomic_inc(atomic_t *v);
static inline void atomic_dec(atomic_t *v);
static inline int atomic_sub_and_test(int i, atomic_t *v);
static inline int atomic_inc_and_test(atomic_t *v);
static inline int atomic_dec_and_test(atomic_t *v);
static inline int atomic_add_negative(int i, atomic_t *v);
static inline int atomic_add_return(int i, atomic_t *v);
static inline int atomic_sub_return(int i, atomic_t *v);
static inline int atomic_inc_return(atomic_t *v);
static inline int atomic_dec_return(atomic_t *v);

/* 原子位操作 */
static inline void set_bit(int nr, volatile unsigned long *addr);
static inline void clear_bit(int nr, volatile unsigned long *addr);
static inline void change_bit(int nr, volatile unsigned long *addr);
static inline int test_and_set_bit(int nr, volatile unsigned long *addr);
static inline int test_and_clear_bit(int nr, volatile unsigned long *addr);
static inline int test_and_change_bit(int nr, volatile unsigned long *addr);
static inline int test_bit(int nr, volatile unsigned long *addr);

/* x86架构特定的原子操作实现 */
#ifdef __i386__

/* 原子读取 */
static inline int atomic_read(const atomic_t *v)
{
    return (*(volatile int *)&(v)->counter);
}

/* 原子设置 */
static inline void atomic_set(atomic_t *v, int i)
{
    v->counter = i;
}

/* 原子加法并返回旧值 */
static inline int atomic_add_return(int i, atomic_t *v)
{
    int __i;
    /* 使用x86的lock前缀确保原子性 */
    __asm__ __volatile__(
        "lock; xaddl %0, %1"
        : "=r"(__i), "+m"(v->counter)
        : "0"(i)
        : "memory");
    return __i + i;
}

/* 原子减法并返回旧值 */
static inline int atomic_sub_return(int i, atomic_t *v)
{
    return atomic_add_return(-i, v);
}

/* 原子加法 */
static inline void atomic_add(int i, atomic_t *v)
{
    atomic_add_return(i, v);
}

/* 原子减法 */
static inline void atomic_sub(int i, atomic_t *v)
{
    atomic_sub_return(i, v);
}

/* 原子递增 */
static inline void atomic_inc(atomic_t *v)
{
    atomic_add_return(1, v);
}

/* 原子递减 */
static inline void atomic_dec(atomic_t *v)
{
    atomic_sub_return(1, v);
}

/* 原子加法并测试是否为负 */
static inline int atomic_add_negative(int i, atomic_t *v)
{
    return atomic_add_return(i, v) < 0;
}

/* 原子减法并测试是否为0 */
static inline int atomic_sub_and_test(int i, atomic_t *v)
{
    return atomic_sub_return(i, v) == 0;
}

/* 原子递增并测试是否为0 */
static inline int atomic_inc_and_test(atomic_t *v)
{
    return atomic_inc_return(v) == 0;
}

/* 原子递减并测试是否为0 */
static inline int atomic_dec_and_test(atomic_t *v)
{
    return atomic_dec_return(v) == 0;
}

/* 原子递增并返回新值 */
static inline int atomic_inc_return(atomic_t *v)
{
    return atomic_add_return(1, v);
}

/* 原子递减并返回新值 */
static inline int atomic_dec_return(atomic_t *v)
{
    return atomic_sub_return(1, v);
}

/* 原子比较并交换 */
static inline int atomic_cmpxchg(atomic_t *v, int old, int new)
{
    int ret;
    __asm__ __volatile__(
        "lock; cmpxchgl %2, %1"
        : "=a"(ret), "+m"(v->counter)
        : "r"(new), "0"(old)
        : "memory");
    return ret;
}

/* 原子位操作实现 */
static inline void set_bit(int nr, volatile unsigned long *addr)
{
    __asm__ __volatile__(
        "lock; btsl %1, %0"
        : "+m"(*addr)
        : "Ir"(nr)
        : "memory");
}

static inline void clear_bit(int nr, volatile unsigned long *addr)
{
    __asm__ __volatile__(
        "lock; btrl %1, %0"
        : "+m"(*addr)
        : "Ir"(nr)
        : "memory");
}

static inline void change_bit(int nr, volatile unsigned long *addr)
{
    __asm__ __volatile__(
        "lock; btcl %1, %0"
        : "+m"(*addr)
        : "Ir"(nr)
        : "memory");
}

static inline int test_and_set_bit(int nr, volatile unsigned long *addr)
{
    int oldbit;
    __asm__ __volatile__(
        "lock; btsl %2, %0; sbbl %1, %1"
        : "+m"(*addr), "=r"(oldbit)
        : "Ir"(nr)
        : "memory");
    return oldbit;
}

static inline int test_and_clear_bit(int nr, volatile unsigned long *addr)
{
    int oldbit;
    __asm__ __volatile__(
        "lock; btrl %2, %0; sbbl %1, %1"
        : "+m"(*addr), "=r"(oldbit)
        : "Ir"(nr)
        : "memory");
    return oldbit;
}

static inline int test_and_change_bit(int nr, volatile unsigned long *addr)
{
    int oldbit;
    __asm__ __volatile__(
        "lock; btcl %2, %0; sbbl %1, %1"
        : "+m"(*addr), "=r"(oldbit)
        : "Ir"(nr)
        : "memory");
    return oldbit;
}

static inline int test_bit(int nr, volatile unsigned long *addr)
{
    return 1UL & (addr[nr >> 5] >> (nr & 31));
}

#else /* 非x86架构的简化实现 */

/* 非x86架构使用简单的volatile操作（不是真正的原子操作） */
static inline int atomic_read(const atomic_t *v)
{
    return v->counter;
}

static inline void atomic_set(atomic_t *v, int i)
{
    v->counter = i;
}

static inline int atomic_add_return(int i, atomic_t *v)
{
    v->counter += i;
    return v->counter;
}

static inline int atomic_sub_return(int i, atomic_t *v)
{
    v->counter -= i;
    return v->counter;
}

static inline void atomic_add(int i, atomic_t *v)
{
    v->counter += i;
}

static inline void atomic_sub(int i, atomic_t *v)
{
    v->counter -= i;
}

static inline void atomic_inc(atomic_t *v)
{
    v->counter++;
}

static inline void atomic_dec(atomic_t *v)
{
    v->counter--;
}

static inline int atomic_add_negative(int i, atomic_t *v)
{
    return (v->counter += i) < 0;
}

static inline int atomic_sub_and_test(int i, atomic_t *v)
{
    return (v->counter -= i) == 0;
}

static inline int atomic_inc_and_test(atomic_t *v)
{
    return ++(v->counter) == 0;
}

static inline int atomic_dec_and_test(atomic_t *v)
{
    return --(v->counter) == 0;
}

static inline int atomic_inc_return(atomic_t *v)
{
    return ++(v->counter);
}

static inline int atomic_dec_return(atomic_t *v)
{
    return --(v->counter);
}

/* 非x86架构的位操作简化实现 */
static inline void set_bit(int nr, volatile unsigned long *addr)
{
    addr[nr >> 5] |= (1UL << (nr & 31));
}

static inline void clear_bit(int nr, volatile unsigned long *addr)
{
    addr[nr >> 5] &= ~(1UL << (nr & 31));
}

static inline void change_bit(int nr, volatile unsigned long *addr)
{
    addr[nr >> 5] ^= (1UL << (nr & 31));
}

static inline int test_and_set_bit(int nr, volatile unsigned long *addr)
{
    int oldbit = test_bit(nr, addr);
    set_bit(nr, addr);
    return oldbit;
}

static inline int test_and_clear_bit(int nr, volatile unsigned long *addr)
{
    int oldbit = test_bit(nr, addr);
    clear_bit(nr, addr);
    return oldbit;
}

static inline int test_and_change_bit(int nr, volatile unsigned long *addr)
{
    int oldbit = test_bit(nr, addr);
    change_bit(nr, addr);
    return oldbit;
}

static inline int test_bit(int nr, volatile unsigned long *addr)
{
    return 1UL & (addr[nr >> 5] >> (nr & 31));
}

#endif /* __i386__ */

#endif /* _ASM_ATOMIC_H */