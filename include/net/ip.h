#ifndef NET_IP_H
#define NET_IP_H

#include <stdint.h>

/* 前向声明 */
struct sk_buff;
struct net_device;

/* IP协议号 */
#define IPPROTO_ICMP    1
#define IPPROTO_TCP     6
#define IPPROTO_UDP     17

/* IP头部 */
struct iphdr {
    uint8_t version_ihl;    /* 版本(4位) + 头部长度(4位) */
    uint8_t tos;            /* 服务类型 */
    uint16_t tot_len;       /* 总长度 */
    uint16_t id;            /* 标识 */
    uint16_t frag_off;      /* 标志(3位) + 片偏移(13位) */
    uint8_t ttl;            /* 生存时间 */
    uint8_t protocol;       /* 协议 */
    uint16_t check;         /* 校验和 */
    uint32_t saddr;         /* 源IP地址 */
    uint32_t daddr;         /* 目的IP地址 */
} __attribute__((packed));

/* ARP头部 */
struct arphdr {
    uint16_t ar_hrd;        /* 硬件类型 */
    uint16_t ar_pro;        /* 协议类型 */
    uint8_t ar_hln;         /* 硬件地址长度 */
    uint8_t ar_pln;         /* 协议地址长度 */
    uint16_t ar_op;         /* 操作码 */
} __attribute__((packed));

/* ARP操作码 */
#define ARPOP_REQUEST   1   /* ARP请求 */
#define ARPOP_REPLY     2   /* ARP应答 */

/* ICMP头部 */
struct icmphdr {
    uint8_t type;           /* 类型 */
    uint8_t code;           /* 代码 */
    uint16_t checksum;      /* 校验和 */
    union {
        struct {
            uint16_t id;
            uint16_t sequence;
        } echo;
        uint32_t gateway;
    } un;
} __attribute__((packed));

/* ICMP类型 */
#define ICMP_ECHOREPLY      0   /* Echo Reply */
#define ICMP_ECHO           8   /* Echo Request */

/* IP地址转换 */
#define IPADDR(a,b,c,d) ((uint32_t)(((a)<<24)|((b)<<16)|((c)<<8)|(d)))

/* 网络字节序转换 */
static inline uint16_t htons(uint16_t x) {
    return ((x & 0xFF) << 8) | ((x >> 8) & 0xFF);
}

static inline uint16_t ntohs(uint16_t x) {
    return htons(x);
}

static inline uint32_t htonl(uint32_t x) {
    return ((x & 0xFF) << 24) | ((x & 0xFF00) << 8) |
           ((x & 0xFF0000) >> 8) | ((x >> 24) & 0xFF);
}

static inline uint32_t ntohl(uint32_t x) {
    return htonl(x);
}

/* IP校验和 */
uint16_t ip_checksum(const void *data, uint32_t len);

/* IP数据包处理 */
int ip_rcv(struct sk_buff *skb);
int ip_send(struct sk_buff *skb, uint32_t daddr, uint8_t protocol);

/* ARP处理 */
int arp_rcv(void *skb);
int arp_resolve(struct net_device *dev, uint32_t ip, uint8_t *mac);

/* ICMP处理 */
int icmp_rcv(void *skb);
int icmp_send_echo_reply(void *skb);

#endif /* NET_IP_H */

