#ifndef NET_SOCK_H
#define NET_SOCK_H

#include <stdint.h>
#include <net/socket.h>
#include <net/sockaddr.h>

/* Socket structure for network layer */
struct sock {
    int             sk_family;      /* Address family */
    int             sk_type;        /* Socket type */
    int             sk_protocol;    /* Protocol */
    uint32_t        sk_state;       /* Connection state */
    
    /* Local address */
    struct sockaddr_in sk_src;
    
    /* Remote address */
    struct sockaddr_in sk_dst;
    
    /* Receive buffer */
    uint8_t         *sk_rcvbuf;
    uint32_t        sk_rcvbuf_size;
    uint32_t        sk_rcvbuf_head;
    uint32_t        sk_rcvbuf_tail;
    
    /* Send buffer */
    uint8_t         *sk_sndbuf;
    uint32_t        sk_sndbuf_size;
    uint32_t        sk_sndbuf_head;
    uint32_t        sk_sndbuf_tail;
    
    /* Protocol specific data */
    void            *sk_prot_data;
    
    /* Reference count */
    int             sk_refcnt;
    
    /* Socket flags */
    uint32_t        sk_flags;
    
    /* Error state */
    int             sk_err;
    
    /* Next in hash table */
    struct sock     *sk_next;
};

/* Socket states for TCP-like protocols */
enum {
    TCP_CLOSE = 0,
    TCP_LISTEN,
    TCP_SYN_SENT,
    TCP_SYN_RECV,
    TCP_ESTABLISHED,
    TCP_FIN_WAIT1,
    TCP_FIN_WAIT2,
    TCP_CLOSE_WAIT,
    TCP_CLOSING,
    TCP_LAST_ACK,
    TCP_TIME_WAIT,
};

/* Socket creation */
struct sock *sock_create(int family, int type, int protocol);
void sock_release(struct sock *sk);

/* Socket binding */
int sock_bind(struct sock *sk, struct sockaddr *addr, int addr_len);

/* Socket connection */
int sock_connect(struct sock *sk, struct sockaddr *addr, int addr_len);
int sock_listen(struct sock *sk, int backlog);
struct sock *sock_accept(struct sock *sk, struct sockaddr *addr, int *addr_len);

/* Socket I/O */
ssize_t sock_send(struct sock *sk, const void *buf, size_t len, int flags);
ssize_t sock_recv(struct sock *sk, void *buf, size_t len, int flags);

/* Socket lookup */
struct sock *sock_lookup(struct sockaddr *local_addr, struct sockaddr *remote_addr);

#endif /* NET_SOCK_H */