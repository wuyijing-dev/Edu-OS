/*
 * IP/ARP/ICMP协议实现 - 类似Linux net/ipv4/
 */

#include <net/skbuff.h>
#include <net/ip.h>
#include <net/ethernet.h>
#include <net/netdev.h>
#include <string.h>

/* ARP缓存表 */
#define ARP_CACHE_SIZE 32

struct arp_entry {
    uint32_t ip;
    uint8_t mac[ETH_ALEN];
    uint32_t timestamp;
    bool valid;
};

static struct arp_entry arp_cache[ARP_CACHE_SIZE];

/* 网络统计计数器（全局，供其他模块访问）*/
volatile uint32_t icmp_echo_request_count = 0;
volatile uint32_t icmp_echo_reply_count = 0;
volatile uint32_t ip_packet_count = 0;
volatile uint32_t eth_packet_count = 0;

/* 获取网络统计信息 */
void net_get_stats(uint32_t *icmp_req, uint32_t *icmp_reply, uint32_t *ip_pkts, uint32_t *eth_pkts)
{
    if (icmp_req) *icmp_req = icmp_echo_request_count;
    if (icmp_reply) *icmp_reply = icmp_echo_reply_count;
    if (ip_pkts) *ip_pkts = ip_packet_count;
    if (eth_pkts) *eth_pkts = eth_packet_count;
}

/*
 * 计算IP校验和
 */
uint16_t ip_checksum(const void *data, uint32_t len)
{
    const uint16_t *p = data;
    uint32_t sum = 0;
    
    while (len > 1) {
        sum += *p++;
        len -= 2;
    }
    
    if (len) {
        sum += *(uint8_t *)p;
    }
    
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    return ~sum;
}

/*
 * IP数据包接收
 */
int ip_rcv(struct sk_buff *skb)
{
    struct iphdr *iph = (struct iphdr *)skb->data;
    
    ip_packet_count++;  /* 统计IP包 */
    
    /* 验证版本 */
    if ((iph->version_ihl >> 4) != 4) {
        free_skb(skb);
        return -1;
    }
    
    /* 验证校验和 */
    uint16_t check = iph->check;
    iph->check = 0;
    if (ip_checksum(iph, (iph->version_ihl & 0x0F) * 4) != check) {
        free_skb(skb);
        return -1;
    }
    iph->check = check;
    
    /* 设置协议头指针 */
    skb->network_header = skb->data;
    skb->nh.raw = (void *)iph;
    
    /* 根据协议分发 */
    switch (iph->protocol) {
        case IPPROTO_ICMP:
            return icmp_rcv(skb);
        case IPPROTO_TCP:
            // return tcp_rcv(skb);
            free_skb(skb);
            return -1;
        case IPPROTO_UDP:
            // return udp_rcv(skb);
            free_skb(skb);
            return -1;
        default:
            free_skb(skb);
            return -1;
    }
}

/*
 * IP数据包发送
 */
int ip_send(struct sk_buff *skb, uint32_t daddr, uint8_t protocol)
{
    struct net_device *dev = skb->dev;
    if (!dev) {
        free_skb(skb);
        return -1;
    }
    
    /* 构造IP头部 */
    struct iphdr *iph = (struct iphdr *)skb_push(skb, sizeof(struct iphdr));
    iph->version_ihl = 0x45;  /* IPv4, 20字节头部 */
    iph->tos = 0;
    iph->tot_len = htons(skb->len);
    iph->id = 0;
    iph->frag_off = 0;
    iph->ttl = 64;
    iph->protocol = protocol;
    iph->saddr = htonl(dev->ip_addr);
    iph->daddr = htonl(daddr);
    iph->check = 0;
    iph->check = ip_checksum(iph, sizeof(struct iphdr));
    
    /* 解析目的MAC地址 */
    uint8_t dest_mac[ETH_ALEN];
    if (arp_resolve(dev, daddr, dest_mac) < 0) {
        free_skb(skb);
        return -1;
    }
    
    /* 构造以太网头部 */
    eth_header(skb, dev, ETH_P_IP, dest_mac, NULL, skb->len);
    
    /* 发送 */
    if (dev->netdev_ops && dev->netdev_ops->ndo_start_xmit) {
        return dev->netdev_ops->ndo_start_xmit(skb, dev);
    }
    
    free_skb(skb);
    return -1;
}

/*
 * ARP数据包接收
 */
int arp_rcv(void *skb_ptr)
{
    struct sk_buff *skb = (struct sk_buff *)skb_ptr;
    struct arphdr *arph = (struct arphdr *)skb->data;
    
    /* 验证硬件类型和协议类型 */
    if (ntohs(arph->ar_hrd) != 1 || ntohs(arph->ar_pro) != ETH_P_IP) {
        free_skb(skb);
        return -1;
    }
    
    /* 获取ARP数据 */
    uint8_t *arp_data = (uint8_t *)(arph + 1);
    uint8_t *sha = arp_data;                    /* 源MAC */
    uint32_t *spa = (uint32_t *)(arp_data + 6); /* 源IP */
    uint32_t *tpa = (uint32_t *)(arp_data + 16);/* 目的IP */
    
    struct net_device *dev = skb->dev;
    
    /* 更新ARP缓存 */
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (!arp_cache[i].valid || arp_cache[i].ip == ntohl(*spa)) {
            arp_cache[i].ip = ntohl(*spa);
            memcpy(arp_cache[i].mac, sha, ETH_ALEN);
            arp_cache[i].valid = true;
            break;
        }
    }
    
    /* 处理ARP请求 */
    if (ntohs(arph->ar_op) == ARPOP_REQUEST && ntohl(*tpa) == dev->ip_addr) {
        /* 构造ARP应答 */
        struct sk_buff *reply = alloc_skb_simple(ETH_FRAME_LEN);
        if (!reply) {
            free_skb(skb);
            return -1;
        }
        
        reply->dev = dev;
        skb_reserve(reply, sizeof(struct ethhdr));
        
        /* 填充ARP头部 */
        struct arphdr *reply_arph = (struct arphdr *)skb_put(reply, sizeof(struct arphdr) + 20);
        reply_arph->ar_hrd = htons(1);
        reply_arph->ar_pro = htons(ETH_P_IP);
        reply_arph->ar_hln = 6;
        reply_arph->ar_pln = 4;
        reply_arph->ar_op = htons(ARPOP_REPLY);
        
        uint8_t *reply_data = (uint8_t *)(reply_arph + 1);
        memcpy(reply_data, dev->dev_addr, ETH_ALEN);        /* 源MAC */
        *(uint32_t *)(reply_data + 6) = htonl(dev->ip_addr);/* 源IP */
        memcpy(reply_data + 10, sha, ETH_ALEN);             /* 目的MAC */
        *(uint32_t *)(reply_data + 16) = *spa;              /* 目的IP */
        
        /* 构造以太网头部 */
        eth_header(reply, dev, ETH_P_ARP, sha, NULL, reply->len);
        
        /* 发送 */
        if (dev->netdev_ops && dev->netdev_ops->ndo_start_xmit) {
            dev->netdev_ops->ndo_start_xmit(reply, dev);
        } else {
            free_skb(reply);
        }
    }
    
    free_skb(skb);
    return 0;
}

/*
 * ARP地址解析
 */
int arp_resolve(struct net_device *dev, uint32_t ip, uint8_t *mac)
{
    /* 查找ARP缓存 */
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (arp_cache[i].valid && arp_cache[i].ip == ip) {
            memcpy(mac, arp_cache[i].mac, ETH_ALEN);
            return 0;
        }
    }
    
    /* 发送ARP请求 */
    struct sk_buff *skb = alloc_skb_simple(ETH_FRAME_LEN);
    if (!skb) {
        return -1;
    }
    
    skb->dev = dev;
    skb_reserve(skb, sizeof(struct ethhdr));
    
    /* 填充ARP头部 */
    struct arphdr *arph = (struct arphdr *)skb_put(skb, sizeof(struct arphdr) + 20);
    arph->ar_hrd = htons(1);
    arph->ar_pro = htons(ETH_P_IP);
    arph->ar_hln = 6;
    arph->ar_pln = 4;
    arph->ar_op = htons(ARPOP_REQUEST);
    
    uint8_t *arp_data = (uint8_t *)(arph + 1);
    memcpy(arp_data, dev->dev_addr, ETH_ALEN);          /* 源MAC */
    *(uint32_t *)(arp_data + 6) = htonl(dev->ip_addr);  /* 源IP */
    memset(arp_data + 10, 0, ETH_ALEN);                 /* 目的MAC (未知) */
    *(uint32_t *)(arp_data + 16) = htonl(ip);           /* 目的IP */
    
    /* 构造以太网头部 (广播) */
    eth_header(skb, dev, ETH_P_ARP, dev->broadcast, NULL, skb->len);
    
    /* 发送 */
    if (dev->netdev_ops && dev->netdev_ops->ndo_start_xmit) {
        dev->netdev_ops->ndo_start_xmit(skb, dev);
    } else {
        free_skb(skb);
    }
    
    /* TODO: 实现ARP应答等待机制 */
    return -1;
}

/*
 * ICMP数据包接收
 */
int icmp_rcv(void *skb_ptr)
{
    struct sk_buff *skb = (struct sk_buff *)skb_ptr;
    struct iphdr *iph = (struct iphdr *)skb->data;
    struct icmphdr *icmph = (struct icmphdr *)((uint8_t *)iph + sizeof(struct iphdr));
    
    /* 检查ICMP类型 */
    if (icmph->type == ICMP_ECHO) {
        icmp_echo_request_count++;  /* 统计Echo Request */
        return icmp_send_echo_reply(skb_ptr);
    }
    
    /* 释放skb */
    free_skb(skb);
    return 0;
}

/*
 * 发送ICMP Echo Reply
 */
int icmp_send_echo_reply(void *skb_ptr)
{
    struct sk_buff *skb = (struct sk_buff *)skb_ptr;
    struct iphdr *iph = (struct iphdr *)skb->data;
    struct icmphdr *icmph = (struct icmphdr *)((uint8_t *)iph + sizeof(struct iphdr));
    
    struct net_device *dev = skb->dev;
    if (!dev) {
        free_skb(skb);
        return -1;
    }
    
    /* 分配新的skb用于回复 */
    struct sk_buff *reply = alloc_skb_simple(ETH_FRAME_LEN);
    if (!reply) {
        free_skb(skb);
        return -1;
    }
    
    /* 复制ICMP数据 */
    uint8_t *data = skb_put(reply, sizeof(struct icmphdr) + 32);  /* 32字节数据 */
    struct icmphdr *reply_icmph = (struct icmphdr *)data;
    
    /* 构造ICMP Echo Reply */
    reply_icmph->type = ICMP_ECHOREPLY;
    reply_icmph->code = 0;
    reply_icmph->checksum = 0;
    reply_icmph->un.echo.id = icmph->un.echo.id;
    reply_icmph->un.echo.sequence = icmph->un.echo.sequence;
    
    /* 复制数据部分 */
    memcpy(data + sizeof(struct icmphdr), (uint8_t *)icmph + sizeof(struct icmphdr), 32);
    
    /* 计算校验和 */
    reply_icmph->checksum = ip_checksum(reply_icmph, sizeof(struct icmphdr) + 32);
    
    /* 发送ICMP Echo Reply */
    reply->dev = dev;
    int ret = ip_send(reply, ntohl(iph->saddr), IPPROTO_ICMP);
    
    /* 释放原始skb */
    free_skb(skb);
    
    return ret;
}

