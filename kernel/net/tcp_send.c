/*
 * tcp_send.c - TCP发送功能实现
 */

#include <net/tcp.h>
#include <net/ip.h>
#include <mm/kmalloc.h>
#include <string.h>
#include <errno.h>

/* TCP发送函数 */
int tcp_send(struct sock *sk, const void *data, size_t len)
{
    struct tcp_sock *tp = (struct tcp_sock *)sk->sk_prot_data;
    if (!tp || !data || len == 0) {
        return -EINVAL;
    }
    
    /* 检查连接状态 */
    if (tp->state != TCP_ESTABLISHED) {
        return -ENOTCONN;
    }
    
    /* 分配skb */
    struct sk_buff *skb = alloc_skb(len);
    if (!skb) {
        return -ENOMEM;
    }
    
    /* 复制数据 */
    memcpy(skb_put(skb, len), data, len);
    
    /* 设置socket引用 */
    skb->sk = sk;
    
    /* 传输数据 */
    return tcp_transmit_skb(tp, skb, tp->snd_nxt, TCP_ACK | TCP_PSH);
}