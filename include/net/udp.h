/*
 * udp.h - UDP协议头文件
 * 对标Linux include/net/udp.h
 */

#ifndef _UDP_H
#define _UDP_H

#include <stdint.h>
#include <net/sock.h>
#include <net/skbuff.h>

/* UDP协议号 */
#define IPPROTO_UDP     17

/* UDP头部 */
struct udphdr {
    uint16_t source;        /* 源端口 */
    uint16_t dest;          /* 目的端口 */
    uint16_t len;           /* UDP长度 */
    uint16_t check;         /* 校验和 */
};

/* UDP socket */
struct udp_sock;

/* UDP函数声明 */
int udp_send(struct sock *sk, const void *data, size_t len);
int udp_rcv(struct sk_buff *skb);
struct udp_sock *udp_create_sock(struct sock *sk);
void udp_release_sock(struct udp_sock *up);

/* UDP校验和计算 */
uint16_t udp_checksum(struct sk_buff *skb, uint32_t saddr, uint32_t daddr);

#endif /* _UDP_H */