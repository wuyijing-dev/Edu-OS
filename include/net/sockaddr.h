#ifndef NET_SOCKADDR_H
#define NET_SOCKADDR_H

#include <stdint.h>
#include <types.h>

typedef uint32_t socklen_t;

/* Generic socket address */
struct sockaddr_storage {
    uint16_t ss_family;     /* Address family */
    char     __ss_padding[128 - sizeof(uint16_t)];
    uint64_t __ss_align;    /* Force alignment */
};

/* Unix domain socket address */
struct sockaddr_un {
    uint16_t sun_family;    /* AF_UNIX */
    char     sun_path[108]; /* Pathname */
};

#endif /* NET_SOCKADDR_H */