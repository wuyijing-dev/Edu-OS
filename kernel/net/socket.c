/*
 * socket.c - Socket API实现
 * 对标Linux net/socket.c
 */

#include <net/socket.h>
#include <net/sock.h>
#include <fs/vfs.h>
#include <process/process.h>
#include <string.h>
#include <errno.h>

/* Socket哈希表大小 */
#define SOCK_HASH_SIZE 32

/* 全局socket表 */
static struct socket *socket_table[SOCK_HASH_SIZE];
static int socket_table_lock = 0;

/* Socket到文件描述符的映射 */
static struct socket *fd_to_socket[MAX_FILES_PER_PROCESS];

/*
 * 分配socket结构
 */
struct socket *sock_alloc(void)
{
    struct socket *sock = kmalloc(sizeof(struct socket));
    if (!sock) {
        return NULL;
    }
    
    memset(sock, 0, sizeof(struct socket));
    sock->state = SS_UNCONNECTED;
    sock->error = 0;
    
    return sock;
}

/*
 * 释放socket结构
 */
void sock_free(struct socket *sock)
{
    if (sock->sk) {
        sock_release(sock->sk);
        sock->sk = NULL;
    }
    
    kfree(sock);
}

/*
 * 将socket映射到文件描述符
 */
int sock_map_fd(struct socket *sock, int flags)
{
    struct process *proc = process_get_current();
    if (!proc || !proc->fd_table) {
        return -EBADF;
    }
    
    /* 分配文件描述符 */
    int fd = -1;
    for (int i = 0; i < MAX_FILES_PER_PROCESS; i++) {
        if (proc->fd_table->files[i] == NULL) {
            fd = i;
            break;
        }
    }
    
    if (fd < 0) {
        return -EMFILE;
    }
    
    /* 创建VFS文件结构 */
    struct vfs_file *file = kmalloc(sizeof(struct vfs_file));
    if (!file) {
        return -ENOMEM;
    }
    
    memset(file, 0, sizeof(struct vfs_file));
    file->ref_count = 1;
    file->offset = 0;
    file->flags = flags;
    file->private_data = sock;
    
    /* 设置文件操作 */
    extern struct vfs_file_operations socket_file_ops;
    file->ops = &socket_file_ops;
    
    /* 关联socket和文件 */
    sock->file = file;
    proc->fd_table->files[fd] = file;
    fd_to_socket[fd] = sock;
    
    return fd;
}

/*
 * 通过文件描述符查找socket
 */
struct socket *sockfd_lookup(int fd, int *err)
{
    if (fd < 0 || fd >= MAX_FILES_PER_PROCESS) {
        *err = -EBADF;
        return NULL;
    }
    
    struct socket *sock = fd_to_socket[fd];
    if (!sock) {
        *err = -ENOTSOCK;
        return NULL;
    }
    
    *err = 0;
    return sock;
}

/*
 * socket系统调用
 */
int sys_socket(int family, int type, int protocol)
{
    /* 参数验证 */
    if (family != AF_INET && family != AF_UNIX) {
        return -EAFNOSUPPORT;
    }
    
    if (type != SOCK_STREAM && type != SOCK_DGRAM && type != SOCK_RAW) {
        return -ESOCKTNOSUPPORT;
    }
    
    /* 分配socket */
    struct socket *sock = sock_alloc();
    if (!sock) {
        return -ENOMEM;
    }
    
    sock->family = family;
    sock->type = type;
    sock->protocol = protocol;
    sock->state = SS_UNCONNECTED;
    
    /* 创建对应的sock结构 */
    sock->sk = sock_create(family, type, protocol);
    if (!sock->sk) {
        sock_free(sock);
        return -ENOMEM;
    }
    
    /* 映射到文件描述符 */
    int fd = sock_map_fd(sock, 0);
    if (fd < 0) {
        sock_free(sock);
        return fd;
    }
    
    return fd;
}

/*
 * bind系统调用
 */
int sys_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    int err;
    struct socket *sock = sockfd_lookup(sockfd, &err);
    if (!sock) {
        return err;
    }
    
    /* 验证地址 */
    if (!addr || addrlen < sizeof(struct sockaddr)) {
        return -EINVAL;
    }
    
    if (addr->sa_family != sock->family) {
        return -EAFNOSUPPORT;
    }
    
    /* 绑定地址 */
    return sock_bind(sock->sk, (struct sockaddr *)addr, addrlen);
}

/*
 * listen系统调用
 */
int sys_listen(int sockfd, int backlog)
{
    int err;
    struct socket *sock = sockfd_lookup(sockfd, &err);
    if (!sock) {
        return err;
    }
    
    if (sock->type != SOCK_STREAM) {
        return -ESOCKTNOSUPPORT;
    }
    
    if (sock->state != SS_UNCONNECTED) {
        return -EINVAL;
    }
    
    return sock_listen(sock->sk, backlog);
}

/*
 * accept系统调用
 */
int sys_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
    int err;
    struct socket *sock = sockfd_lookup(sockfd, &err);
    if (!sock) {
        return err;
    }
    
    if (sock->type != SOCK_STREAM) {
        return -ESOCKTNOSUPPORT;
    }
    
    /* 接受连接 */
    int addr_len = 0;
    struct sock *newsk = sock_accept(sock->sk, addr, &addr_len);
    if (!newsk) {
        return -EAGAIN;
    }
    
    /* 创建新的socket */
    struct socket *newsock = sock_alloc();
    if (!newsock) {
        sock_release(newsk);
        return -ENOMEM;
    }
    
    newsock->family = sock->family;
    newsock->type = sock->type;
    newsock->protocol = sock->protocol;
    newsock->state = SS_CONNECTED;
    newsock->sk = newsk;
    
    /* 更新地址长度 */
    if (addrlen) {
        *addrlen = addr_len;
    }
    
    /* 映射到文件描述符 */
    int newfd = sock_map_fd(newsock, 0);
    if (newfd < 0) {
        sock_free(newsock);
        sock_release(newsk);
        return newfd;
    }
    
    return newfd;
}

/*
 * connect系统调用
 */
int sys_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    int err;
    struct socket *sock = sockfd_lookup(sockfd, &err);
    if (!sock) {
        return err;
    }
    
    /* 验证地址 */
    if (!addr || addrlen < sizeof(struct sockaddr)) {
        return -EINVAL;
    }
    
    if (addr->sa_family != sock->family) {
        return -EAFNOSUPPORT;
    }
    
    /* 连接 */
    err = sock_connect(sock->sk, (struct sockaddr *)addr, addrlen);
    if (err == 0) {
        sock->state = SS_CONNECTED;
    }
    
    return err;
}

/*
 * send系统调用
 */
ssize_t sys_send(int sockfd, const void *buf, size_t len, int flags)
{
    int err;
    struct socket *sock = sockfd_lookup(sockfd, &err);
    if (!sock) {
        return err;
    }
    
    if (!buf || len == 0) {
        return -EINVAL;
    }
    
    return sock_send(sock->sk, buf, len, flags);
}

/*
 * recv系统调用
 */
ssize_t sys_recv(int sockfd, void *buf, size_t len, int flags)
{
    int err;
    struct socket *sock = sockfd_lookup(sockfd, &err);
    if (!sock) {
        return err;
    }
    
    if (!buf || len == 0) {
        return -EINVAL;
    }
    
    return sock_recv(sock->sk, buf, len, flags);
}