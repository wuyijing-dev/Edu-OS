#ifndef NET_NETDEV_H
#define NET_NETDEV_H

#include <stdint.h>
#include <stdbool.h>
#include <net/skbuff.h>

/* 网络设备状态 */
#define NETDEV_UP       0x0001  /* 设备已启动 */
#define NETDEV_RUNNING  0x0002  /* 设备正在运行 */
#define NETDEV_PROMISC  0x0004  /* 混杂模式 */

/* MAC地址长度 */
#define ETH_ALEN        6

/* 最大传输单元 */
#define ETH_MTU         1500
#define ETH_FRAME_LEN   1514    /* 最大以太网帧长度 */

/* 网络数据包结构 - 使用标准skbuff.h中的定义 */
/* struct sk_buff 定义在 skbuff.h 中 */

struct net_device;

/* 网络设备操作接口 (类似Linux net_device_ops) */
struct net_device_ops {
    int (*ndo_open)(struct net_device *dev);
    int (*ndo_stop)(struct net_device *dev);
    int (*ndo_start_xmit)(struct sk_buff *skb, struct net_device *dev);
    void (*ndo_set_rx_mode)(struct net_device *dev);
    int (*ndo_set_mac_address)(struct net_device *dev, void *addr);
};

/* 网络设备统计信息 */
struct net_device_stats {
    uint32_t rx_packets;        /* 接收包数 */
    uint32_t tx_packets;        /* 发送包数 */
    uint32_t rx_bytes;          /* 接收字节数 */
    uint32_t tx_bytes;          /* 发送字节数 */
    uint32_t rx_errors;         /* 接收错误数 */
    uint32_t tx_errors;         /* 发送错误数 */
    uint32_t rx_dropped;        /* 接收丢包数 */
    uint32_t tx_dropped;        /* 发送丢包数 */
};

/* 网络设备结构 (类似Linux net_device) */
struct net_device {
    char name[16];              /* 设备名称 (如 eth0) */
    uint32_t flags;             /* 设备标志 */
    uint32_t mtu;               /* 最大传输单元 */
    uint8_t dev_addr[ETH_ALEN]; /* MAC地址 */
    uint8_t broadcast[ETH_ALEN];/* 广播地址 */
    
    /* IP配置 */
    uint32_t ip_addr;           /* IP地址 */
    uint32_t netmask;           /* 子网掩码 */
    uint32_t gateway;           /* 网关 */
    
    /* 设备操作 */
    const struct net_device_ops *netdev_ops;
    
    /* 统计信息 */
    struct net_device_stats stats;
    
    /* 接收队列 */
    struct sk_buff_head rx_queue;
    uint32_t rx_queue_len;
    
    /* 私有数据 */
    void *priv;
    
    /* 链表 */
    struct net_device *next;
};

/* sk_buff操作函数 - 使用标准skbuff.h中的定义 */
/* 这些函数声明在skbuff.h中，这里不需要重复声明 */
#if 0
struct sk_buff *alloc_skb(uint32_t size);
struct sk_buff *alloc_skb_simple(uint32_t size);
void kfree_skb(struct sk_buff *skb);
void free_skb(struct sk_buff *skb);
void skb_reserve(struct sk_buff *skb, uint32_t len);
uint8_t *skb_put(struct sk_buff *skb, uint32_t len);
uint8_t *skb_push(struct sk_buff *skb, uint32_t len);
uint8_t *skb_pull(struct sk_buff *skb, uint32_t len);
#endif

/* 网络设备注册/注销 */
int register_netdev(struct net_device *dev);
void unregister_netdev(struct net_device *dev);

/* 网络设备查找 */
struct net_device *dev_get_by_name(const char *name);

/* 数据包接收 */
int netif_rx(struct sk_buff *skb);

/* 数据包发送 */
int dev_queue_xmit(struct sk_buff *skb);

/* 网络设备初始化 */
void netdev_init(void);

#endif /* NET_NETDEV_H */

