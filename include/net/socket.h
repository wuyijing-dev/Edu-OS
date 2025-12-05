#ifndef NET_SOCKET_H
#define NET_SOCKET_H

#include <stdint.h>
#include <types.h>
#include <net/sockaddr.h>

/* Address families */
#define AF_UNSPEC       0
#define AF_UNIX         1       /* Unix domain sockets */
#define AF_INET         2       /* Internet IP Protocol */
#define AF_INET6        10      /* IP version 6 */

/* Socket types */
#define SOCK_STREAM     1       /* stream (connection) socket */
#define SOCK_DGRAM      2       /* datagram (conn.less) socket */
#define SOCK_RAW        3       /* raw socket */
#define SOCK_RDM        4       /* reliably-delivered message */
#define SOCK_SEQPACKET  5       /* sequential packet socket */

/* Socket-level options */
#define SOL_SOCKET      1

/* Socket options */
#define SO_DEBUG        1
#define SO_REUSEADDR    2
#define SO_TYPE         3
#define SO_ERROR        4
#define SO_DONTROUTE    5
#define SO_BROADCAST    6
#define SO_SNDBUF       7
#define SO_RCVBUF       8
#define SO_KEEPALIVE    9
#define SO_OOBINLINE    10
#define SO_NO_CHECK     11
#define SO_PRIORITY     12
#define SO_LINGER       13
#define SO_BSDCOMPAT    14

/* Shutdown options */
#define SHUT_RD         0       /* shutdown for reading */
#define SHUT_WR         1       /* shutdown for writing */
#define SHUT_RDWR       2       /* shutdown for reading and writing */

/* Socket address structure */
struct sockaddr {
    uint16_t sa_family;     /* address family */
    char     sa_data[14];   /* protocol-specific address */
};

/* Internet address structure */
struct in_addr {
    uint32_t s_addr;        /* 32-bit IPv4 address */
};

/* Internet socket address structure */
struct sockaddr_in {
    uint16_t        sin_family;   /* AF_INET */
    uint16_t        sin_port;     /* port number */
    struct in_addr  sin_addr;     /* IP address */
    char            sin_zero[8];  /* zero padding */
};

/* Socket structure (internal) */
struct socket {
    int             type;         /* SOCK_STREAM, SOCK_DGRAM, etc */
    int             family;       /* AF_INET, AF_UNIX, etc */
    int             protocol;     /* protocol number */
    uint32_t        state;        /* socket state */
    struct sock     *sk;          /* network layer representation */
    struct file     *file;        /* associated file structure */
    uint32_t        flags;        /* socket flags */
    int             error;        /* error code */
};

/* Socket state */
#define SS_FREE         0       /* not allocated */
#define SS_UNCONNECTED  1       /* unconnected to any socket */
#define SS_CONNECTING   2       /* in process of connecting */
#define SS_CONNECTED    3       /* connected to socket */
#define SS_DISCONNECTING 4      /* in process of disconnecting */

/* System calls */
int sys_socket(int family, int type, int protocol);
int sys_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int sys_listen(int sockfd, int backlog);
int sys_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int sys_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
ssize_t sys_send(int sockfd, const void *buf, size_t len, int flags);
ssize_t sys_recv(int sockfd, void *buf, size_t len, int flags);
int sys_shutdown(int sockfd, int how);
int sys_setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen);
int sys_getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen);

/* Internal functions */
struct socket *sock_alloc(void);
void sock_free(struct socket *sock);
int sock_map_fd(struct socket *sock, int flags);
struct socket *sockfd_lookup(int fd, int *err);

#endif /* NET_SOCKET_H */