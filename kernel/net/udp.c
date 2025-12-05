/*
 * udp.c - UDP协议实现
 * 对标Linux net/ipv4/udp.c
 */

#include <net/udp.h>
#include <net/ip.h>
#include <net/sock.h>
#include <mm/kmalloc.h>
#include <string.h>
#include <errno.h>

/* UDP头部结构 */
struct udphdr {
    uint16_t source;        /* 源端口 */
    uint16_t dest;          /* 目的端口 */
    uint16_t len;           /* UDP长度 */
    uint16_t check;         /* 校验和 */
};

/* UDP socket */
struct udp_sock {
    struct sock *sk;                    /* 基础socket */
    uint32_t pending;                   /* 待处理数据 */
};

/* UDP校验和计算 */
static uint16_t udp_checksum(struct sk_buff *skb, uint32_t saddr, uint32_t daddr)
{
    /* 伪头部 + UDP头部 + 数据的校验和 */
    struct {
        uint32_t saddr;
        uint32_t daddr;
        uint8_t  zero;
        uint8_t  protocol;
        uint16_t len;
    } pseudohdr;
    
    pseudohdr.saddr = saddr;
    pseudohdr.daddr = daddr;
    pseudohdr.zero = 0;
    pseudohdr.protocol = IPPROTO_UDP;
    pseudohdr.len = htons(skb->len + sizeof(struct udphdr));
    
    /* 计算校验和 */
    uint32_t sum = 0;
    uint16_t *p = (uint16_t *)&pseudohdr;
    
    /* 伪头部校验和 */
    for (int i = 0; i < sizeof(pseudohdr) / 2; i++) {
        sum += p[i];
    }
    
    /* UDP头部校验和 */
    struct udphdr hdr;
    hdr.source = ((struct udp_sock *)skb->sk)->sk->sk_src.sin_port;
    hdr.dest = ((struct udp_sock *)skb->sk)->sk->sk_dst.sin_port;
    hdr.len = htons(skb->len + sizeof(struct udphdr));
    hdr.check = 0;
    
    p = (uint16_t *)&hdr;
    for (int i = 0; i < sizeof(struct udphdr) / 2; i++) {
        sum += p[i];
    }
    
    /* UDP数据校验和 */
    p = (uint16_t *)skb->data;
    for (uint32_t i = 0; i < skb->len / 2; i++) {
        sum += p[i];
    }
    
    if (skb->len & 1) {
        sum += ((uint8_t *)skb->data)[skb->len - 1];
    }
    
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    return ~sum;
}

/* UDP数据发送 */
int udp_send(struct sock *sk, const void *data, size_t len)
{
    struct sk_buff *skb;
    struct udphdr *uh;
    
    /* 分配skb */
    skb = alloc_skb(len + sizeof(struct udphdr));
    if (!skb) {
        return -ENOMEM;
    }
    
    /* 设置socket引用 */
    skb->sk = sk;
    
    /* 复制数据 */
    memcpy(skb_put(skb, len), data, len);
    
    /* 添加UDP头部 */
    uh = (struct udphdr *)skb_push(skb, sizeof(struct udphdr));
    uh->source = sk->sk_src.sin_port;
    uh->dest = sk->sk_dst.sin_port;
    uh->len = htons(skb->len);
    uh->check = 0;
    
    /* 计算校验和 */
    uh->check = udp_checksum(skb, sk->sk_src.sin_addr.s_addr, 
                               sk->sk_dst.sin_addr.s_addr);
    
    /* 通过IP层发送 */
    return ip_send(skb, ntohl(sk->sk_dst.sin_addr.s_addr), IPPROTO_UDP);
}

/* UDP数据接收 */
int udp_rcv(struct sk_buff *skb)
{
    struct udphdr *uh;
    struct sock *sk;
    struct sockaddr_in local_addr, remote_addr;
    
    /* 检查最小长度 */
    if (skb->len < sizeof(struct udphdr)) {
        free_skb(skb);
        return -1;
    }
    
    /* 解析UDP头部 */
    uh = (struct udphdr *)skb->data;
    skb_pull(skb, sizeof(struct udphdr));
    
    /* 设置地址 */
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = uh->dest;
    local_addr.sin_addr.s_addr = 0;  /* TODO: 从IP头部获取 */
    
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_port = uh->source;
    remote_addr.sin_addr.s_addr = 0;  /* TODO: 从IP头部获取 */
    
    /* 查找socket */
    sk = sock_lookup((struct sockaddr *)&local_addr, 
                     (struct sockaddr *)&remote_addr);
    
    if (!sk) {
        /* 查找监听socket */
        remote_addr.sin_port = 0;
        sk = sock_lookup((struct sockaddr *)&local_addr, 
                         (struct sockaddr *)&remote_addr);
    }
    
    if (!sk) {
        free_skb(skb);
        return -1;
    }
    
    /* 复制数据到接收缓冲区 */
    uint32_t space = (sk->sk_rcvbuf_size - sk->sk_rcvbuf_tail) % sk->sk_rcvbuf_size;
    if (space >= skb->len) {
        memcpy(sk->sk_rcvbuf + sk->sk_rcvbuf_tail, skb->data, skb->len);
        sk->sk_rcvbuf_tail = (sk->sk_rcvbuf_tail + skb->len) % sk->sk_rcvbuf_size;
    }
    
    free_skb(skb);
    return 0;
}

/* UDP socket创建 */
struct udp_sock *udp_create_sock(struct sock *sk)
{
    struct udp_sock *up = kmalloc(sizeof(struct udp_sock));
    if (!up) {
        return NULL;
    }
    
    memset(up, 0, sizeof(struct udp_sock));
    up->sk = sk;
    
    return up;
}

/* UDP socket释放 */
void udp_release_sock(struct udp_sock *up)
{
    if (up) {
        kfree(up);
    }
}