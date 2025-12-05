/*
 * tcp.h - TCP协议头文件
 * 对标Linux include/net/tcp.h
 */

#ifndef _TCP_H
#define _TCP_H

#include <stdint.h>
#include <net/sock.h>
#include <net/skbuff.h>

/* TCP协议号 */
#define IPPROTO_TCP     6

/* TCP标志位 */
#define TCP_FIN         0x01
#define TCP_SYN         0x02
#define TCP_RST         0x04
#define TCP_PSH         0x08
#define TCP_ACK         0x10
#define TCP_URG         0x20

/* TCP选项 */
#define TCPOPT_EOL      0
#define TCPOPT_NOP      1
#define TCPOPT_MSS      2
#define TCPOPT_WINDOW   3
#define TCPOPT_SACK_PERM 4
#define TCPOPT_SACK     5
#define TCPOPT_TIMESTAMP 8

/* TCP状态 */
enum tcp_state {
    TCP_ESTABLISHED = 1,
    TCP_SYN_SENT,
    TCP_SYN_RECV,
    TCP_FIN_WAIT1,
    TCP_FIN_WAIT2,
    TCP_TIME_WAIT,
    TCP_CLOSE,
    TCP_CLOSE_WAIT,
    TCP_LAST_ACK,
    TCP_LISTEN,
    TCP_CLOSING
};

/* TCP头部 */
struct tcphdr {
    uint16_t source;        /* 源端口 */
    uint16_t dest;          /* 目的端口 */
    uint32_t seq;           /* 序列号 */
    uint32_t ack_seq;       /* 确认号 */
    uint16_t res1:4,        /* 保留 */
             doff:4,         /* 数据偏移 */
             fin:1,          /* FIN标志 */
             syn:1,          /* SYN标志 */
             rst:1,          /* RST标志 */
             psh:1,          /* PSH标志 */
             ack:1,          /* ACK标志 */
             urg:1,          /* URG标志 */
             res2:2;         /* 保留 */
    uint16_t window;        /* 窗口大小 */
    uint16_t check;         /* 校验和 */
    uint16_t urg_ptr;       /* 紧急指针 */
};

/* TCP socket */
struct tcp_sock;

/* TCP函数声明 */
int tcp_connect(struct sock *sk);
int tcp_rcv(struct sk_buff *skb);
struct tcp_sock *tcp_create_sock(struct sock *sk);

/* TCP选项处理 */
void tcp_parse_options(struct tcp_sock *tp, const uint8_t *ptr, int len);
int tcp_build_options(uint8_t *ptr, int mss);

/* TCP序列号管理 */
static inline int before(uint32_t seq1, uint32_t seq2)
{
    return (int)(seq1 - seq2) < 0;
}

static inline int after(uint32_t seq1, uint32_t seq2)
{
    return (int)(seq1 - seq2) > 0;
}

static inline int between(uint32_t seq, uint32_t start, uint32_t end)
{
    return after(seq, start) && before(seq, end);
}

/* TCP校验和计算 */
uint16_t tcp_checksum(struct sk_buff *skb, uint32_t saddr, uint32_t daddr);

/* TCP窗口管理 */
static inline uint32_t tcp_window(struct tcp_sock *tp)
{
    return tp->rcv_wnd;
}

/* TCP重传管理 */
void tcp_retransmit(struct tcp_sock *tp);
void tcp_update_rtt(struct tcp_sock *tp, uint32_t rtt);

/* TCP发送接收函数 */
int tcp_send(struct sock *sk, const void *data, size_t len);
int tcp_transmit_skb(struct tcp_sock *tp, struct sk_buff *skb, uint32_t seq, uint8_t flags);

#endif /* _TCP_H */