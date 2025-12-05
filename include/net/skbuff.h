/*
 * skbuff.h - socket缓冲区头文件
 * 对标Linux include/linux/skbuff.h
 */

#ifndef _SKBUFF_H
#define _SKBUFF_H

#include <types.h>
#include <list.h>

/* 包含原子操作 - 必须在结构体定义之前 */
#include <asm/atomic.h>

/* 数据包类型 */
#define PACKET_HOST     0       /* 发给本机的包 */
#define PACKET_BROADCAST 1      /* 广播包 */
#define PACKET_MULTICAST 2      /* 多播包 */
#define PACKET_OTHERHOST 3      /* 发给其他主机的包 */
#define PACKET_OUTGOING 4       /* 发出的包 */

/* socket缓冲区标志 */
#define SKB_ALLOC_RX    0x01    /* 接收缓冲区 */
#define SKB_ALLOC_TX    0x02    /* 发送缓冲区 */
#define SKB_ALLOC_LOCAL 0x04    /* 本地分配 */

/* socket缓冲区结构 */
struct sk_buff {
    struct list_head list;      /* 链表节点 */
    
    /* 数据指针 */
    unsigned char *head;        /* 缓冲区起始 */
    unsigned char *data;        /* 数据起始 */
    unsigned char *tail;        /* 数据结束 */
    unsigned char *end;         /* 缓冲区结束 */
    
    /* 缓冲区大小 */
    unsigned int len;           /* 数据长度 */
    unsigned int data_len;      /* 数据部分长度 */
    unsigned int truesize;      /* 缓冲区总大小 */
    
    /* 协议相关信息 */
    uint32_t protocol;          /* 协议类型 */
    uint32_t saddr;             /* 源地址 */
    uint32_t daddr;             /* 目的地址 */
    uint16_t sport;             /* 源端口 */
    uint16_t dport;             /* 目的端口 */
    
    /* 网络设备 */
    struct net_device *dev;     /* 网络设备 */
    
    /* 传输层信息 */
    union {
        struct tcphdr *th;      /* TCP头 */
        struct udphdr *uh;      /* UDP头 */
        struct icmphdr *icmph;  /* ICMP头 */
        struct igmphdr *igmph;  /* IGMP头 */
        void *raw;              /* 原始数据 */
    } h;
    
    /* 网络层信息 */
    union {
        struct iphdr *iph;      /* IP头 */
        struct ipv6hdr *ipv6h;  /* IPv6头 */
        struct arphdr *arph;    /* ARP头 */
        void *raw;              /* 原始数据 */
    } nh;
    
    /* 链路层信息 */
    union {
        struct ethhdr *eth;     /* 以太网头 */
        void *raw;              /* 原始数据 */
    } mac;
    
    /* 私有数据 */
    void *cb[48];               /* 控制缓冲区 */
    
    /* 标志和状态 */
    uint32_t flags;             /* 标志 */
    uint32_t pkt_type;          /* 包类型 */
    uint32_t priority;        /* 优先级 */
    
    /* 时间戳 */
    uint32_t tstamp;            /* 时间戳 */
    
    /* 引用计数 */
    atomic_t users;             /* 用户数 */
    
    /* 校验和 */
    uint32_t csum;              /* 校验和 */
    uint32_t ip_summed;         /* IP校验和状态 */
    
    /* 队列映射 */
    uint16_t queue_mapping;     /* 队列映射 */
    
    /* 简化字段 - 兼容旧代码 */
    uint8_t *mac_header;        /* MAC头部指针 */
    uint8_t *network_header;    /* 网络层头部指针 */
    uint8_t *transport_header;/* 传输层头部指针 */
};

/* socket缓冲区操作函数 */
struct sk_buff *alloc_skb(unsigned int size, int priority);
struct sk_buff *dev_alloc_skb(unsigned int length);
void kfree_skb(struct sk_buff *skb);
void dev_kfree_skb(struct sk_buff *skb);
void consume_skb(struct sk_buff *skb);

/* socket缓冲区数据操作 */
unsigned char *skb_put(struct sk_buff *skb, unsigned int len);
unsigned char *skb_push(struct sk_buff *skb, unsigned int len);
unsigned char *skb_pull(struct sk_buff *skb, unsigned int len);
unsigned char *skb_headroom(const struct sk_buff *skb);
unsigned char *skb_tailroom(const struct sk_buff *skb);

/* socket缓冲区复制 */
struct sk_buff *skb_clone(struct sk_buff *skb, int priority);
struct sk_buff *skb_copy(const struct sk_buff *skb, int priority);
struct sk_buff *pskb_copy(struct sk_buff *skb, int headroom);

/* socket缓冲区重新分配 */
int skb_pad(struct sk_buff *skb, int pad);
void skb_reserve(struct sk_buff *skb, unsigned int len);
int skb_linearize(struct sk_buff *skb);

/* 前向声明 - 修复循环依赖 */
struct sk_buff;

/* 网络设备前向声明 */
struct net_device;

/* socket缓冲区队列头 */
struct sk_buff_head {
    struct list_head list;
    struct list_head *next;
    struct list_head *prev;
    unsigned int qlen;
};

/* socket缓冲区队列操作 */
static inline void skb_queue_head_init(struct sk_buff_head *list)
{
    list->next = &list->list;
    list->prev = &list->list;
    list->list.next = list->next;
    list->list.prev = list->prev;
    list->qlen = 0;
}

static inline void skb_queue_head(struct sk_buff_head *list, struct sk_buff *skb)
{
    list->list.next->prev = &skb->list;
    skb->list.next = list->next;
    skb->list.prev = &list->list;
    list->next = &skb->list;
    list->list.next = &skb->list;
    list->qlen++;
}

static inline void skb_queue_tail(struct sk_buff_head *list, struct sk_buff *skb)
{
    list->list.prev->next = &skb->list;
    skb->list.prev = list->prev;
    skb->list.next = &list->list;
    list->prev = &skb->list;
    list->list.prev = &skb->list;
    list->qlen++;
}

static inline struct sk_buff *skb_dequeue(struct sk_buff_head *list)
{
    struct sk_buff *skb;
    struct list_head *next;
    
    if (list->next == &list->list) {
        return NULL;
    }
    
    next = list->next;
    skb = list_entry(next, struct sk_buff, list);
    
    next->next->prev = &list->list;
    list->next = next->next;
    list->list.next = next->next;
    list->qlen--;
    
    return skb;
}

static inline struct sk_buff *skb_peek(struct sk_buff_head *list)
{
    if (list->next == &list->list) {
        return NULL;
    }
    
    return list_entry(list->next, struct sk_buff, list);
}

static inline int skb_queue_empty(const struct sk_buff_head *list)
{
    return list->next == &list->list;
}

static inline unsigned int skb_queue_len(const struct sk_buff_head *list)
{
    return list->qlen;
}

/* 简化sk_buff操作函数 - 兼容层 */
static inline struct sk_buff *alloc_skb_simple(unsigned int size) {
    return alloc_skb(size, 0);
}

/*
static inline void free_skb(struct sk_buff *skb)
{
    kfree_skb(skb);
}
*/
void free_skb(struct sk_buff *skb);

/* 原子操作 - 使用asm/atomic.h中的实现 */

#endif /* _SKBUFF_H */