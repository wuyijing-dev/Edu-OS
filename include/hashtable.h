/**
 * hashtable.h - Linux风格的哈希表
 * 
 * 基于链表实现的哈希表，使用链地址法解决冲突
 */

#ifndef _HASHTABLE_H
#define _HASHTABLE_H

#include <list.h>
#include <types.h>

/**
 * 哈希表节点（嵌入到数据结构中）
 */
struct hlist_node {
    struct hlist_node *next;
    struct hlist_node **pprev;  /* 指向前一个节点的next指针 */
};

/**
 * 哈希表头
 */
struct hlist_head {
    struct hlist_node *first;
};

/**
 * 初始化哈希表头
 */
#define HLIST_HEAD_INIT { .first = NULL }

/**
 * 初始化哈希表节点
 */
#define INIT_HLIST_NODE(ptr) ((ptr)->next = NULL, (ptr)->pprev = NULL)

/**
 * 检查哈希表头是否为空
 */
static inline int hlist_empty(const struct hlist_head *h)
{
    return !h->first;
}

/**
 * 检查节点是否在哈希表中
 */
static inline int hlist_unhashed(const struct hlist_node *h)
{
    return !h->pprev;
}

/**
 * 删除哈希表节点
 */
static inline void hlist_del(struct hlist_node *n)
{
    struct hlist_node *next = n->next;
    struct hlist_node **pprev = n->pprev;
    
    *pprev = next;
    if (next)
        next->pprev = pprev;
    
    n->next = NULL;
    n->pprev = NULL;
}

/**
 * 在哈希表头部添加节点
 */
static inline void hlist_add_head(struct hlist_node *n, struct hlist_head *h)
{
    struct hlist_node *first = h->first;
    n->next = first;
    if (first)
        first->pprev = &n->next;
    h->first = n;
    n->pprev = &h->first;
}

/**
 * 在指定节点前插入
 */
static inline void hlist_add_before(struct hlist_node *n,
                                    struct hlist_node *next)
{
    n->pprev = next->pprev;
    n->next = next;
    next->pprev = &n->next;
    *(n->pprev) = n;
}

/**
 * 在指定节点后插入
 */
static inline void hlist_add_after(struct hlist_node *n,
                                   struct hlist_node *next)
{
    next->next = n->next;
    n->next = next;
    next->pprev = &n->next;
    
    if (next->next)
        next->next->pprev = &next->next;
}

/**
 * 获取包含哈希表节点的结构体指针
 */
#define hlist_entry(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

/**
 * 遍历哈希表
 */
#define hlist_for_each(pos, head) \
    for (pos = (head)->first; pos; pos = pos->next)

/**
 * 安全遍历哈希表
 */
#define hlist_for_each_safe(pos, n, head) \
    for (pos = (head)->first; pos && ({ n = pos->next; 1; }); \
         pos = n)

/**
 * 遍历哈希表中的每个元素
 */
#define hlist_for_each_entry(pos, head, member) \
    for (pos = hlist_entry((head)->first, typeof(*(pos)), member); \
         pos; \
         pos = hlist_entry((pos)->member.next, typeof(*(pos)), member))

/**
 * 安全遍历哈希表中的每个元素
 */
#define hlist_for_each_entry_safe(pos, n, head, member) \
    for (pos = hlist_entry((head)->first, typeof(*pos), member); \
         pos && ({ n = pos->member.next; 1; }); \
         pos = hlist_entry(n, typeof(*pos), member))

/**
 * 定义哈希表（指定桶数量）
 */
#define DEFINE_HASHTABLE(name, bits) \
    struct hlist_head name[1 << (bits)]

/**
 * 哈希表大小
 */
#define HASH_SIZE(name) (sizeof(name) / sizeof((name)[0]))

/**
 * 哈希表位数
 */
#define HASH_BITS(name) ilog2(HASH_SIZE(name))

/**
 * 初始化哈希表
 */
#define hash_init(hashtable) \
    do { \
        uint32_t __i; \
        for (__i = 0; __i < HASH_SIZE(hashtable); __i++) \
            (hashtable)[__i].first = NULL; \
    } while (0)

/**
 * 简单的哈希函数（乘法哈希）
 */
static inline uint32_t hash_32(uint32_t val, unsigned int bits)
{
    /* 黄金比例：2^32 / φ */
    uint32_t hash = val * 0x9e3779b9;
    return hash >> (32 - bits);
}

/**
 * 指针哈希
 */
static inline uint32_t hash_ptr(const void *ptr, unsigned int bits)
{
    return hash_32((uint32_t)(uintptr_t)ptr, bits);
}

/**
 * 添加到哈希表
 */
#define hash_add(hashtable, node, key) \
    hlist_add_head(node, &hashtable[hash_32(key, HASH_BITS(hashtable))])

/**
 * 从哈希表删除
 */
#define hash_del(node) hlist_del(node)

/**
 * 在哈希表中查找
 */
#define hash_for_each_possible(name, obj, member, key) \
    hlist_for_each_entry(obj, &name[hash_32(key, HASH_BITS(name))], member)

/**
 * 遍历整个哈希表
 */
#define hash_for_each(name, bkt, obj, member) \
    for ((bkt) = 0; (bkt) < HASH_SIZE(name); (bkt)++) \
        hlist_for_each_entry(obj, &name[bkt], member)

/**
 * 安全遍历整个哈希表
 */
#define hash_for_each_safe(name, bkt, tmp, obj, member) \
    for ((bkt) = 0; (bkt) < HASH_SIZE(name); (bkt)++) \
        hlist_for_each_entry_safe(obj, tmp, &name[bkt], member)

/**
 * 计算以2为底的对数（用于哈希表大小计算）
 */
static inline unsigned int ilog2(unsigned int n)
{
    unsigned int log = 0;
    while (n >>= 1)
        log++;
    return log;
}

#endif /* _HASHTABLE_H */

