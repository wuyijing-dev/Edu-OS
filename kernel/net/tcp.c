/*
 * tcp.c - TCP协议实现
 * 对标Linux net/ipv4/tcp.c
 */

#include <net/tcp.h>
#include <net/ip.h>
#include <net/sock.h>
#include <mm/kmalloc.h>
#include <string.h>
#include <errno.h>

/* TCP头部结构 */
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

/* TCP状态枚举 */
enum {
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

/* TCP控制块 */
struct tcp_sock {
    struct sock *sk;                    /* 基础socket */
    
    /* 序列号管理 */
    uint32_t snd_una;                   /* 发送未确认 */
    uint32_t snd_nxt;                   /* 发送下一个 */
    uint32_t snd_wnd;                   /* 发送窗口 */
    uint32_t rcv_nxt;                   /* 接收下一个 */
    uint32_t rcv_wnd;                   /* 接收窗口 */
    
    /* 重传管理 */
    uint32_t rto;                       /* 重传超时 */
    uint32_t rtt;                       /* 往返时间 */
    uint32_t srtt;                      /* 平滑往返时间 */
    uint8_t  rtt_cnt;                   /* RTT计数 */
    
    /* 拥塞控制 */
    uint32_t cwnd;                      /* 拥塞窗口 */
    uint32_t ssthresh;                  /* 慢启动阈值 */
    uint32_t snd_ssthresh;              /* 发送阈值 */
    
    /* 状态 */
    uint8_t  state;                     /* TCP状态 */
    uint8_t  retransmits;               /* 重传次数 */
    uint32_t timeout;                   /* 超时时间 */
    
    /* 发送队列 */
    struct tcp_skb_cb *send_queue;      /* 发送队列 */
    uint32_t send_queue_len;
    
    /* 接收队列 */
    struct tcp_skb_cb *recv_queue;      /* 接收队列 */
    uint32_t recv_queue_len;
};

/* TCP段控制块 */
struct tcp_skb_cb {
    struct sk_buff *skb;                /* 数据包 */
    uint32_t seq;                       /* 序列号 */
    uint32_t end_seq;                   /* 结束序列号 */
    uint32_t ack_seq;                   /* 确认序列号 */
    uint8_t flags;                      /* TCP标志 */
    struct tcp_skb_cb *next;            /* 链表指针 */
};

/* TCP校验和计算 */
static uint16_t tcp_checksum(struct sk_buff *skb, uint32_t saddr, uint32_t daddr)
{
    /* 伪头部 + TCP头部 + 数据的校验和 */
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
    pseudohdr.protocol = IPPROTO_TCP;
    pseudohdr.len = htons(skb->len);
    
    /* 计算校验和 */
    uint32_t sum = 0;
    uint16_t *p = (uint16_t *)&pseudohdr;
    
    /* 伪头部校验和 */
    for (int i = 0; i < sizeof(pseudohdr) / 2; i++) {
        sum += p[i];
    }
    
    /* TCP数据校验和 */
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

/* TCP段传输 */
int tcp_transmit_skb(struct tcp_sock *tp, struct sk_buff *skb, uint32_t seq, uint8_t flags)
{
    struct sock *sk = tp->sk;
    struct tcphdr *th = (struct tcphdr *)skb_push(skb, sizeof(struct tcphdr));
    
    /* 填充TCP头部 */
    th->source = sk->sk_src.sin_port;
    th->dest = sk->sk_dst.sin_port;
    th->seq = htonl(seq);
    th->ack_seq = htonl(tp->rcv_nxt);
    th->doff = sizeof(struct tcphdr) / 4;
    th->fin = (flags & 0x01) ? 1 : 0;
    th->syn = (flags & 0x02) ? 1 : 0;
    th->rst = (flags & 0x04) ? 1 : 0;
    th->psh = (flags & 0x08) ? 1 : 0;
    th->ack = (flags & 0x10) ? 1 : 0;
    th->urg = 0;
    th->window = htons(tp->rcv_wnd);
    th->urg_ptr = 0;
    
    /* 计算校验和 */
    th->check = 0;
    th->check = tcp_checksum(skb, sk->sk_src.sin_addr.s_addr, sk->sk_dst.sin_addr.s_addr);
    
    /* 发送数据包 */
    return ip_send(skb, ntohl(sk->sk_dst.sin_addr.s_addr), IPPROTO_TCP);
}

/* TCP连接建立 */
int tcp_connect(struct sock *sk)
{
    struct tcp_sock *tp = (struct tcp_sock *)sk->sk_prot_data;
    if (!tp) {
        return -EINVAL;
    }
    
    /* 分配发送缓冲区 */
    struct sk_buff *skb = alloc_skb(1460);
    if (!skb) {
        return -ENOMEM;
    }
    
    /* 初始化序列号 */
    tp->snd_nxt = 0x12345678;  /* 简化实现 */
    tp->snd_una = tp->snd_nxt;
    
    /* 发送SYN包 */
    tp->state = TCP_SYN_SENT;
    int ret = tcp_transmit_skb(tp, skb, tp->snd_nxt, 0x02);  /* SYN */
    
    if (ret == 0) {
        tp->snd_nxt++;
    }
    
    return ret;
}

/* TCP数据接收 */
int tcp_rcv(struct sk_buff *skb)
{
    struct tcphdr *th = (struct tcphdr *)skb->data;
    
    /* 移除TCP头部 */
    skb_pull(skb, sizeof(struct tcphdr));
    
    /* 查找对应的socket */
    struct sockaddr_in local_addr, remote_addr;
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = th->dest;
    local_addr.sin_addr.s_addr = 0;  /* TODO: 从IP头部获取 */
    
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_port = th->source;
    remote_addr.sin_addr.s_addr = 0;  /* TODO: 从IP头部获取 */
    
    struct sock *sk = sock_lookup((struct sockaddr *)&local_addr, 
                                   (struct sockaddr *)&remote_addr);
    if (!sk) {
        free_skb(skb);
        return -1;
    }
    
    struct tcp_sock *tp = (struct tcp_sock *)sk->sk_prot_data;
    if (!tp) {
        free_skb(skb);
        return -1;
    }
    
    /* 处理TCP状态机 */
    switch (tp->state) {
    case TCP_LISTEN:
        /* 处理连接请求 */
        if (th->syn) {
            /* TODO: 发送SYN+ACK */
            tp->state = TCP_SYN_RECV;
        }
        break;
        
    case TCP_SYN_SENT:
        /* 处理SYN+ACK */
        if (th->syn && th->ack) {
            tp->state = TCP_ESTABLISHED;
            tp->rcv_nxt = ntohl(th->seq) + 1;
            /* TODO: 发送ACK */
        }
        break;
        
    case TCP_SYN_RECV:
        /* 处理ACK */
        if (th->ack) {
            tp->state = TCP_ESTABLISHED;
        }
        break;
        
    case TCP_ESTABLISHED:
        /* 处理数据 */
        if (skb->len > 0) {
            /* 复制数据到接收缓冲区 */
            uint32_t space = (sk->sk_rcvbuf_size - sk->sk_rcvbuf_tail) % sk->sk_rcvbuf_size;
            if (space >= skb->len) {
                memcpy(sk->sk_rcvbuf + sk->sk_rcvbuf_tail, skb->data, skb->len);
                sk->sk_rcvbuf_tail = (sk->sk_rcvbuf_tail + skb->len) % sk->sk_rcvbuf_size;
            }
        }
        break;
    }
    
    free_skb(skb);
    return 0;
}

/* TCP socket创建 */
struct tcp_sock *tcp_create_sock(struct sock *sk)
{
    struct tcp_sock *tp = kmalloc(sizeof(struct tcp_sock));
    if (!tp) {
        return NULL;
    }
    
    memset(tp, 0, sizeof(struct tcp_sock));
    
    tp->sk = sk;
    tp->state = TCP_CLOSE;
    tp->cwnd = 1;           /* 初始拥塞窗口 */
    tp->ssthresh = 65535;   /* 初始阈值 */
    tp->rto = 1000;         /* 初始重传超时 */
    
    return tp;
}