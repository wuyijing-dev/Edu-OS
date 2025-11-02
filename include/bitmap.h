/**
 * bitmap.h - Linux风格的位图操作
 * 
 * 用于高效的位操作，常用于资源分配跟踪
 */

#ifndef _BITMAP_H
#define _BITMAP_H

#include <types.h>
#include <string.h>

/**
 * 位图中一个字的位数
 */
#define BITS_PER_LONG (sizeof(unsigned long) * 8)

/**
 * 计算需要多少个long来存储n个位
 */
#define BITS_TO_LONGS(nr) \
    (((nr) + BITS_PER_LONG - 1) / BITS_PER_LONG)

/**
 * 定义位图
 */
#define DECLARE_BITMAP(name, bits) \
    unsigned long name[BITS_TO_LONGS(bits)]

/**
 * 设置位
 */
static inline void set_bit(int nr, volatile unsigned long *addr)
{
    unsigned long mask = 1UL << (nr % BITS_PER_LONG);
    unsigned long *p = ((unsigned long *)addr) + (nr / BITS_PER_LONG);
    *p |= mask;
}

/**
 * 清除位
 */
static inline void clear_bit(int nr, volatile unsigned long *addr)
{
    unsigned long mask = 1UL << (nr % BITS_PER_LONG);
    unsigned long *p = ((unsigned long *)addr) + (nr / BITS_PER_LONG);
    *p &= ~mask;
}

/**
 * 翻转位
 */
static inline void change_bit(int nr, volatile unsigned long *addr)
{
    unsigned long mask = 1UL << (nr % BITS_PER_LONG);
    unsigned long *p = ((unsigned long *)addr) + (nr / BITS_PER_LONG);
    *p ^= mask;
}

/**
 * 测试位
 */
static inline int test_bit(int nr, const volatile unsigned long *addr)
{
    return 1UL & (addr[nr / BITS_PER_LONG] >> (nr % BITS_PER_LONG));
}

/**
 * 测试并设置位
 */
static inline int test_and_set_bit(int nr, volatile unsigned long *addr)
{
    unsigned long mask = 1UL << (nr % BITS_PER_LONG);
    unsigned long *p = ((unsigned long *)addr) + (nr / BITS_PER_LONG);
    unsigned long old = *p;
    *p = old | mask;
    return (old & mask) != 0;
}

/**
 * 测试并清除位
 */
static inline int test_and_clear_bit(int nr, volatile unsigned long *addr)
{
    unsigned long mask = 1UL << (nr % BITS_PER_LONG);
    unsigned long *p = ((unsigned long *)addr) + (nr / BITS_PER_LONG);
    unsigned long old = *p;
    *p = old & ~mask;
    return (old & mask) != 0;
}

/**
 * 测试并翻转位
 */
static inline int test_and_change_bit(int nr, volatile unsigned long *addr)
{
    unsigned long mask = 1UL << (nr % BITS_PER_LONG);
    unsigned long *p = ((unsigned long *)addr) + (nr / BITS_PER_LONG);
    unsigned long old = *p;
    *p = old ^ mask;
    return (old & mask) != 0;
}

/**
 * 查找第一个为0的位
 */
static inline unsigned long find_first_zero_bit(const unsigned long *addr,
                                                unsigned long size)
{
    unsigned long idx;
    unsigned long result = 0;

    for (idx = 0; idx < size / BITS_PER_LONG; idx++) {
        if (addr[idx] != ~0UL) {
            for (unsigned long bit = 0; bit < BITS_PER_LONG; bit++) {
                if (!(addr[idx] & (1UL << bit)))
                    return idx * BITS_PER_LONG + bit;
            }
        }
    }

    /* 检查剩余的位 */
    for (result = idx * BITS_PER_LONG; result < size; result++) {
        if (!test_bit(result, addr))
            return result;
    }

    return size;
}

/**
 * 查找第一个为1的位
 */
static inline unsigned long find_first_bit(const unsigned long *addr,
                                           unsigned long size)
{
    unsigned long idx;

    for (idx = 0; idx < size / BITS_PER_LONG; idx++) {
        if (addr[idx] != 0) {
            for (unsigned long bit = 0; bit < BITS_PER_LONG; bit++) {
                if (addr[idx] & (1UL << bit))
                    return idx * BITS_PER_LONG + bit;
            }
        }
    }

    /* 检查剩余的位 */
    for (unsigned long result = idx * BITS_PER_LONG; result < size; result++) {
        if (test_bit(result, addr))
            return result;
    }

    return size;
}

/**
 * 查找下一个为0的位
 */
static inline unsigned long find_next_zero_bit(const unsigned long *addr,
                                               unsigned long size,
                                               unsigned long offset)
{
    if (offset >= size)
        return size;

    for (unsigned long bit = offset; bit < size; bit++) {
        if (!test_bit(bit, addr))
            return bit;
    }

    return size;
}

/**
 * 查找下一个为1的位
 */
static inline unsigned long find_next_bit(const unsigned long *addr,
                                          unsigned long size,
                                          unsigned long offset)
{
    if (offset >= size)
        return size;

    for (unsigned long bit = offset; bit < size; bit++) {
        if (test_bit(bit, addr))
            return bit;
    }

    return size;
}

/**
 * 位图清零
 */
static inline void bitmap_zero(unsigned long *dst, unsigned int nbits)
{
    unsigned int len = BITS_TO_LONGS(nbits) * sizeof(unsigned long);
    memset(dst, 0, len);
}

/**
 * 位图全部置1
 */
static inline void bitmap_fill(unsigned long *dst, unsigned int nbits)
{
    unsigned int len = BITS_TO_LONGS(nbits) * sizeof(unsigned long);
    memset(dst, 0xff, len);
}

/**
 * 位图复制
 */
static inline void bitmap_copy(unsigned long *dst, const unsigned long *src,
                               unsigned int nbits)
{
    unsigned int len = BITS_TO_LONGS(nbits) * sizeof(unsigned long);
    memcpy(dst, src, len);
}

/**
 * 遍历位图中所有为1的位
 */
#define for_each_set_bit(bit, addr, size) \
    for ((bit) = find_first_bit((addr), (size)); \
         (bit) < (size); \
         (bit) = find_next_bit((addr), (size), (bit) + 1))

/**
 * 遍历位图中所有为0的位
 */
#define for_each_clear_bit(bit, addr, size) \
    for ((bit) = find_first_zero_bit((addr), (size)); \
         (bit) < (size); \
         (bit) = find_next_zero_bit((addr), (size), (bit) + 1))

#endif /* _BITMAP_H */

