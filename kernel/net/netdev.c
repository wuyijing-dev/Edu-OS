/*
 * 网络设备抽象层 - 类似Linux net/core/dev.c
 */

#include <net/netdev.h>
#include <net/skbuff.h>
#include <net/ethernet.h>
#include <mm/kmalloc.h>
#include <string.h>

/* 全局网络设备链表 */
static struct net_device *dev_base = NULL;

/* sk_buff内存池 */
#define SKB_POOL_SIZE 256
static struct sk_buff skb_pool[SKB_POOL_SIZE];
static uint32_t skb_pool_bitmap[SKB_POOL_SIZE / 32];

/*
 * 分配sk_buff
 */
struct sk_buff *alloc_skb(unsigned int size, int priority)
{
    (void)priority; /* 暂未使用优先级 */
    struct sk_buff *skb = NULL;
    
    /* 从内存池分配 */
    for (uint32_t i = 0; i < SKB_POOL_SIZE; i++) {
        uint32_t idx = i / 32;
        uint32_t bit = i % 32;
        if (!(skb_pool_bitmap[idx] & (1 << bit))) {
            skb_pool_bitmap[idx] |= (1 << bit);
            skb = &skb_pool[i];
            break;
        }
    }
    
    if (!skb) {
        return NULL;
    }
    
    /* 分配数据缓冲区 */
    skb->head = kmalloc(size);
    if (!skb->head) {
        uint32_t i = skb - skb_pool;
        skb_pool_bitmap[i / 32] &= ~(1 << (i % 32));
        return NULL;
    }
    
    skb->data = skb->head;
    skb->tail = skb->head;
    skb->end = skb->head + size;
    skb->len = 0;
    skb->data_len = size;
    /* skb->next removed, use list_head */
    INIT_LIST_HEAD(&skb->list);
    skb->dev = NULL;
    skb->protocol = 0;
    
    return skb;
}

void kfree_skb(struct sk_buff *skb)
{
    if (!skb) {
        return;
    }
    
    if (skb->head) {
        kfree(skb->head);
    }
    
    uint32_t i = skb - skb_pool;
    if (i < SKB_POOL_SIZE) {
        skb_pool_bitmap[i / 32] &= ~(1 << (i % 32));
    }
}

void free_skb(struct sk_buff *skb)
{
    kfree_skb(skb);
}



/*
 * 在skb头部预留空间
 */
void skb_reserve(struct sk_buff *skb, uint32_t len)
{
    skb->data += len;
    skb->tail += len;
}

/*
 * 在skb尾部添加数据
 */
uint8_t *skb_put(struct sk_buff *skb, uint32_t len)
{
    uint8_t *tmp = skb->tail;
    skb->tail += len;
    skb->len += len;
    return tmp;
}

/*
 * 在skb头部添加数据
 */
uint8_t *skb_push(struct sk_buff *skb, uint32_t len)
{
    skb->data -= len;
    skb->len += len;
    return skb->data;
}

/*
 * 从skb头部移除数据
 */
uint8_t *skb_pull(struct sk_buff *skb, uint32_t len)
{
    skb->data += len;
    skb->len -= len;
    return skb->data;
}

/*
 * 注册网络设备
 */
int register_netdev(struct net_device *dev)
{
    if (!dev) {
        return -1;
    }
    
    /* 添加到全局链表 */
    dev->next = dev_base;
    dev_base = dev;
    
    /* 设置广播地址 */
    for (int i = 0; i < ETH_ALEN; i++) {
        dev->broadcast[i] = 0xFF;
    }
    
    /* 设置MTU */
    if (dev->mtu == 0) {
        dev->mtu = ETH_MTU;
    }
    
    return 0;
}

/*
 * 注销网络设备
 */
void unregister_netdev(struct net_device *dev)
{
    if (!dev) {
        return;
    }
    
    /* 从链表中移除 */
    struct net_device **p = &dev_base;
    while (*p) {
        if (*p == dev) {
            *p = dev->next;
            break;
        }
        p = &(*p)->next;
    }
}

/*
 * 根据名称查找网络设备
 */
struct net_device *dev_get_by_name(const char *name)
{
    struct net_device *dev = dev_base;
    while (dev) {
        if (strcmp(dev->name, name) == 0) {
            return dev;
        }
        dev = dev->next;
    }
    return NULL;
}

/*
 * 接收数据包 (从驱动调用)
 */
int netif_rx(struct sk_buff *skb)
{
    if (!skb || !skb->dev) {
        free_skb(skb);
        return -1;
    }
    
    struct net_device *dev = skb->dev;
    
    /* 添加到接收队列 */
    skb_queue_tail(&dev->rx_queue, skb);
    
    /* 更新统计信息 */
    dev->stats.rx_packets++;
    dev->stats.rx_bytes += skb->len;
    
    /* 通知上层协议 */
    /* TODO: 唤醒网络处理线程 */
    
    return 0;
}

/*
 * 发送数据包
 */
int dev_queue_xmit(struct sk_buff *skb)
{
    if (!skb || !skb->dev) {
        free_skb(skb);
        return -1;
    }
    
    struct net_device *dev = skb->dev;
    
    if (!dev->netdev_ops || !dev->netdev_ops->ndo_start_xmit) {
        free_skb(skb);
        return -1;
    }
    
    /* 调用驱动发送函数 */
    int ret = dev->netdev_ops->ndo_start_xmit(skb, dev);
    
    if (ret == 0) {
        dev->stats.tx_packets++;
        dev->stats.tx_bytes += skb->len;
    } else {
        dev->stats.tx_errors++;
    }
    
    return ret;
}

/*
 * 网络设备初始化
 */
void netdev_init(void)
{
    /* 初始化sk_buff内存池 */
    for (uint32_t i = 0; i < SKB_POOL_SIZE / 32; i++) {
        skb_pool_bitmap[i] = 0;
    }
}

