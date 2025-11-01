/*
 * 以太网帧处理 - 类似Linux net/ethernet/eth.c
 */

#include <net/ethernet.h>
#include <net/netdev.h>
#include <net/ip.h>
#include <string.h>

/*
 * 解析以太网帧类型
 */
int eth_type_trans(struct sk_buff *skb)
{
    struct ethhdr *eth = (struct ethhdr *)skb->data;
    
    /* 保存协议类型 */
    skb->protocol = ntohs(eth->h_proto);
    
    /* 移除以太网头部 */
    skb_pull(skb, sizeof(struct ethhdr));
    
    /* 根据协议类型分发 */
    switch (skb->protocol) {
    case ETH_P_IP:
        return ip_rcv(skb);
    case ETH_P_ARP:
        return arp_rcv(skb);
    default:
        free_skb(skb);
        return -1;
    }
}

/*
 * 构造以太网帧头部
 */
int eth_header(struct sk_buff *skb, struct net_device *dev,
               uint16_t type, const void *daddr, const void *saddr, uint32_t len)
{
    struct ethhdr *eth = (struct ethhdr *)skb_push(skb, sizeof(struct ethhdr));
    
    /* 设置协议类型 */
    eth->h_proto = htons(type);
    
    /* 设置源MAC地址 */
    if (saddr) {
        memcpy(eth->h_source, saddr, ETH_ALEN);
    } else {
        memcpy(eth->h_source, dev->dev_addr, ETH_ALEN);
    }
    
    /* 设置目的MAC地址 */
    if (daddr) {
        memcpy(eth->h_dest, daddr, ETH_ALEN);
        return ETH_ALEN;
    }
    
    return -ETH_ALEN;
}

