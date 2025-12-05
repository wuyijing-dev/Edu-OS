/*
 * sock.c - 网络层socket实现
 * 对标Linux net/core/sock.c
 */

#include <net/sock.h>
#include <net/socket.h>
#include <mm/kmalloc.h>
#include <string.h>
#include <errno.h>
#include <net/tcp.h>
#include <net/udp.h>

/* Socket哈希表 */
static struct sock *sock_hash_table[SOCK_HASH_SIZE];
static int sock_hash_lock = 0;

/*
 * 计算socket哈希值
 */
static uint32_t sock_hash(struct sockaddr *local_addr, struct sockaddr *remote_addr)
{
    uint32_t hash = 0;
    
    if (local_addr) {
        hash ^= ((struct sockaddr_in *)local_addr)->sin_addr.s_addr;
        hash ^= ((struct sockaddr_in *)local_addr)->sin_port;
    }
    
    if (remote_addr) {
        hash ^= ((struct sockaddr_in *)remote_addr)->sin_addr.s_addr;
        hash ^= ((struct sockaddr_in *)remote_addr)->sin_port;
    }
    
    return hash % SOCK_HASH_SIZE;
}

/*
 * 创建socket
 */
struct sock *sock_create(int family, int type, int protocol)
{
    struct sock *sk = kmalloc(sizeof(struct sock));
    if (!sk) {
        return NULL;
    }
    
    memset(sk, 0, sizeof(struct sock));
    
    sk->sk_family = family;
    sk->sk_type = type;
    sk->sk_protocol = protocol;
    sk->sk_state = TCP_CLOSE;
    sk->sk_refcnt = 1;
    
    /* 分配缓冲区 */
    sk->sk_rcvbuf_size = 65536;  /* 64KB接收缓冲区 */
    sk->sk_sndbuf_size = 65536;  /* 64KB发送缓冲区 */
    
    sk->sk_rcvbuf = kmalloc(sk->sk_rcvbuf_size);
    sk->sk_sndbuf = kmalloc(sk->sk_sndbuf_size);
    
    if (!sk->sk_rcvbuf || !sk->sk_sndbuf) {
        if (sk->sk_rcvbuf) kfree(sk->sk_rcvbuf);
        if (sk->sk_sndbuf) kfree(sk->sk_sndbuf);
        kfree(sk);
        return NULL;
    }
    
    sk->sk_rcvbuf_head = sk->sk_rcvbuf_tail = 0;
    sk->sk_sndbuf_head = sk->sk_sndbuf_tail = 0;
    
    return sk;
}

/*
 * 释放socket
 */
void sock_release(struct sock *sk)
{
    if (!sk) {
        return;
    }
    
    sk->sk_refcnt--;
    if (sk->sk_refcnt > 0) {
        return;
    }
    
    /* 从哈希表中移除 */
    if (sk->sk_next) {
        uint32_t hash = sock_hash((struct sockaddr *)&sk->sk_src, 
                                   (struct sockaddr *)&sk->sk_dst);
        struct sock **p = &sock_hash_table[hash];
        while (*p) {
            if (*p == sk) {
                *p = sk->sk_next;
                break;
            }
            p = &(*p)->sk_next;
        }
    }
    
    /* 释放缓冲区 */
    if (sk->sk_rcvbuf) {
        kfree(sk->sk_rcvbuf);
    }
    if (sk->sk_sndbuf) {
        kfree(sk->sk_sndbuf);
    }
    
    /* 释放协议特定数据 */
    if (sk->sk_type == SOCK_STREAM && sk->sk_prot_data) {
        /* TCP释放 */
        kfree(sk->sk_prot_data);
    } else if (sk->sk_type == SOCK_DGRAM && sk->sk_prot_data) {
        /* UDP释放 */
        udp_release_sock((struct udp_sock *)sk->sk_prot_data);
    }
    
    kfree(sk);
}

/*
 * socket绑定
 */
int sock_bind(struct sock *sk, struct sockaddr *addr, int addr_len)
{
    if (!addr || addr_len < sizeof(struct sockaddr_in)) {
        return -EINVAL;
    }
    
    struct sockaddr_in *sin = (struct sockaddr_in *)addr;
    
    /* 验证地址族 */
    if (sin->sin_family != AF_INET) {
        return -EAFNOSUPPORT;
    }
    
    /* 验证端口 */
    if (sin->sin_port == 0) {
        return -EINVAL;
    }
    
    /* 复制地址 */
    memcpy(&sk->sk_src, sin, sizeof(struct sockaddr_in));
    
    /* 添加到哈希表 */
    uint32_t hash = sock_hash(addr, NULL);
    sk->sk_next = sock_hash_table[hash];
    sock_hash_table[hash] = sk;
    
    return 0;
}

/*
 * socket连接
 */
int sock_connect(struct sock *sk, struct sockaddr *addr, int addr_len)
{
    if (!addr || addr_len < sizeof(struct sockaddr_in)) {
        return -EINVAL;
    }
    
    struct sockaddr_in *sin = (struct sockaddr_in *)addr;
    
    /* 验证地址族 */
    if (sin->sin_family != AF_INET) {
        return -EAFNOSUPPORT;
    }
    
    /* 验证地址 */
    if (sin->sin_addr.s_addr == 0 || sin->sin_port == 0) {
        return -EINVAL;
    }
    
    /* 复制目的地址 */
    memcpy(&sk->sk_dst, sin, sizeof(struct sockaddr_in));
    
    /* 对于TCP，需要三次握手 */
    if (sk->sk_type == SOCK_STREAM) {
        /* TODO: 实现TCP连接建立 */
        sk->sk_state = TCP_ESTABLISHED;
    }
    
    return 0;
}

/*
 * socket监听
 */
int sock_listen(struct sock *sk, int backlog)
{
    if (sk->sk_type != SOCK_STREAM) {
        return -ESOCKTNOSUPPORT;
    }
    
    if (sk->sk_state != TCP_CLOSE) {
        return -EINVAL;
    }
    
    sk->sk_state = TCP_LISTEN;
    
    /* TODO: 设置监听队列 */
    
    return 0;
}

/*
 * socket接受连接
 */
struct sock *sock_accept(struct sock *sk, struct sockaddr *addr, int *addr_len)
{
    if (sk->sk_type != SOCK_STREAM) {
        return NULL;
    }
    
    if (sk->sk_state != TCP_LISTEN) {
        return NULL;
    }
    
    /* TODO: 从监听队列获取连接 */
    /* 简化实现：创建新的socket */
    struct sock *newsk = sock_create(sk->sk_family, sk->sk_type, sk->sk_protocol);
    if (!newsk) {
        return NULL;
    }
    
    /* 复制地址信息 */
    memcpy(&newsk->sk_src, &sk->sk_src, sizeof(struct sockaddr_in));
    
    /* 设置状态 */
    newsk->sk_state = TCP_ESTABLISHED;
    
    /* 返回对端地址 */
    if (addr && addr_len) {
        memcpy(addr, &newsk->sk_dst, sizeof(struct sockaddr_in));
        *addr_len = sizeof(struct sockaddr_in);
    }
    
    return newsk;
}

/*
 * socket发送数据
 */
ssize_t sock_send(struct sock *sk, const void *buf, size_t len, int flags)
{
    if (!buf || len == 0) {
        return -EINVAL;
    }
    
    if (sk->sk_state != TCP_ESTABLISHED && sk->sk_type == SOCK_STREAM) {
        return -ENOTCONN;
    }
    
    /* 简化实现：直接复制到发送缓冲区 */
    uint32_t space = (sk->sk_sndbuf_size - sk->sk_sndbuf_tail) % sk->sk_sndbuf_size;
    if (space < len) {
        return -EAGAIN;
    }
    
    /* 复制数据 */
    const uint8_t *src = (const uint8_t *)buf;
    uint32_t tail = sk->sk_sndbuf_tail;
    
    for (size_t i = 0; i < len; i++) {
        sk->sk_sndbuf[tail] = src[i];
        tail = (tail + 1) % sk->sk_sndbuf_size;
    }
    
    sk->sk_sndbuf_tail = tail;
    
    /* TODO: 实际发送数据到网络层 */
    
    return len;
}

/*
 * socket接收数据
 */
ssize_t sock_recv(struct sock *sk, void *buf, size_t len, int flags)
{
    if (!buf || len == 0) {
        return -EINVAL;
    }
    
    if (sk->sk_state != TCP_ESTABLISHED && sk->sk_type == SOCK_STREAM) {
        return -ENOTCONN;
    }
    
    /* 检查接收缓冲区 */
    uint32_t available = (sk->sk_rcvbuf_tail - sk->sk_rcvbuf_head) % sk->sk_rcvbuf_size;
    if (available == 0) {
        return -EAGAIN;  /* 没有数据 */
    }
    
    /* 读取数据 */
    size_t to_read = (available < len) ? available : len;
    uint8_t *dst = (uint8_t *)buf;
    uint32_t head = sk->sk_rcvbuf_head;
    
    for (size_t i = 0; i < to_read; i++) {
        dst[i] = sk->sk_rcvbuf[head];
        head = (head + 1) % sk->sk_rcvbuf_size;
    }
    
    sk->sk_rcvbuf_head = head;
    
    return to_read;
}

/*
 * socket查找
 */
struct sock *sock_lookup(struct sockaddr *local_addr, struct sockaddr *remote_addr)
{
    uint32_t hash = sock_hash(local_addr, remote_addr);
    struct sock *sk = sock_hash_table[hash];
    
    while (sk) {
        bool match = true;
        
        /* 检查本地地址 */
        if (local_addr) {
            struct sockaddr_in *local_sin = (struct sockaddr_in *)local_addr;
            if (sk->sk_src.sin_addr.s_addr != local_sin->sin_addr.s_addr ||
                sk->sk_src.sin_port != local_sin->sin_port) {
                match = false;
            }
        }
        
        /* 检查远程地址 */
        if (match && remote_addr) {
            struct sockaddr_in *remote_sin = (struct sockaddr_in *)remote_addr;
            if (sk->sk_dst.sin_addr.s_addr != remote_sin->sin_addr.s_addr ||
                sk->sk_dst.sin_port != remote_sin->sin_port) {
                match = false;
            }
        }
        
        if (match) {
            return sk;
        }
        
        sk = sk->sk_next;
    }
    
    return NULL;
}

    case SOCK_STREAM:
        if (protocol == IPPROTO_TCP) {
            sk->sk_prot_data = tcp_create_sock(sk);
            if (!sk->sk_prot_data) {
                kfree(sk);
                return NULL;
            }
        }
        break;
    case SOCK_DGRAM:
        if (protocol == IPPROTO_UDP) {
            sk->sk_prot_data = udp_create_sock(sk);
            if (!sk->sk_prot_data) {
                kfree(sk);
                return NULL;
            }
        }
        break;

int sock_send(struct sock *sk, const void *data, size_t len)
{
    if (!sk || !data) {
        return -EINVAL;
    }
    
    /* 根据协议类型调用相应的发送函数 */
    if (sk->sk_type == SOCK_STREAM && sk->sk_protocol == IPPROTO_TCP) {
        return tcp_send(sk, data, len);
    } else if (sk->sk_type == SOCK_DGRAM && sk->sk_protocol == IPPROTO_UDP) {
        return udp_send(sk, data, len);
    }
    
    return -EINVAL;
}

int sock_recv(struct sock *sk, void *data, size_t len)
{
    if (!sk || !data) {
        return -EINVAL;
    }
    
    /* 检查接收缓冲区是否有数据 */
    if (sk->sk_rcvbuf_head == sk->sk_rcvbuf_tail) {
        return -EAGAIN;  /* 没有数据 */
    }
    
    /* 计算可读数据量 */
    uint32_t available;
    if (sk->sk_rcvbuf_tail >= sk->sk_rcvbuf_head) {
        available = sk->sk_rcvbuf_tail - sk->sk_rcvbuf_head;
    } else {
        available = sk->sk_rcvbuf_size - sk->sk_rcvbuf_head;
    }
    
    uint32_t to_read = (len < available) ? len : available;
    
    /* 复制数据 */
    memcpy(data, sk->sk_rcvbuf + sk->sk_rcvbuf_head, to_read);
    sk->sk_rcvbuf_head = (sk->sk_rcvbuf_head + to_read) % sk->sk_rcvbuf_size;
    
    return to_read;
}