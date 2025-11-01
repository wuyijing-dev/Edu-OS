#ifndef NET_ETHERNET_H
#define NET_ETHERNET_H

#include <stdint.h>

/* 前向声明 */
struct sk_buff;
struct net_device;

/* 以太网协议类型 */
#define ETH_P_IP        0x0800  /* IPv4 */
#define ETH_P_ARP       0x0806  /* ARP */
#define ETH_P_IPV6      0x86DD  /* IPv6 */

/* 以太网帧头部 */
struct ethhdr {
    uint8_t h_dest[6];      /* 目的MAC地址 */
    uint8_t h_source[6];    /* 源MAC地址 */
    uint16_t h_proto;       /* 协议类型 */
} __attribute__((packed));

/* 以太网帧处理 */
int eth_type_trans(struct sk_buff *skb);
int eth_header(struct sk_buff *skb, struct net_device *dev,
               uint16_t type, const void *daddr, const void *saddr, uint32_t len);

#endif /* NET_ETHERNET_H */

