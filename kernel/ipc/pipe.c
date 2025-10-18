/*
 * pipe.c - 管道实现（进程间通信）
 * 
 * 实现匿名管道和命名管道（FIFO）
 */

#include <kernel.h>
#include <mm/kmalloc.h>
#include <fs/vfs.h>
#include <process/process.h>
#include <string.h>

#define PIPE_BUF_SIZE 4096  /* 管道缓冲区大小 */

/* 管道结构 */
struct pipe {
    char buffer[PIPE_BUF_SIZE];  /* 环形缓冲区 */
    uint32_t read_pos;           /* 读位置 */
    uint32_t write_pos;          /* 写位置 */
    uint32_t data_size;          /* 当前数据量 */
    
    /* 等待队列（简化版） */
    struct process *readers;     /* 等待读的进程 */
    struct process *writers;     /* 等待写的进程 */
    
    /* 引用计数 */
    int read_count;              /* 读端引用数 */
    int write_count;             /* 写端引用数 */
};

/*
 * 创建管道
 */
static struct pipe *pipe_create(void)
{
    struct pipe *pipe = kmalloc(sizeof(struct pipe));
    if (!pipe) {
        return NULL;
    }
    
    memset(pipe, 0, sizeof(struct pipe));
    pipe->read_count = 0;
    pipe->write_count = 0;
    
    return pipe;
}

/*
 * 管道读操作
 */
static int pipe_read(struct vfs_file *file, char *buf, size_t count)
{
    struct pipe *pipe = file->private_data;
    
    if (!pipe || !buf) {
        return -EINVAL;
    }
    
    /* 如果管道为空且没有写端，返回EOF */
    if (pipe->data_size == 0 && pipe->write_count == 0) {
        return 0;  /* EOF */
    }
    
    /* 如果管道为空但有写端，阻塞等待 */
    while (pipe->data_size == 0) {
        /* TODO: 阻塞当前进程，等待写入 */
        kprintf("[PIPE] Reader blocked, waiting for data\n");
        return -EAGAIN;  /* 临时返回 */
    }
    
    /* 读取数据 */
    size_t to_read = count;
    if (to_read > pipe->data_size) {
        to_read = pipe->data_size;
    }
    
    size_t bytes_read = 0;
    while (bytes_read < to_read) {
        buf[bytes_read++] = pipe->buffer[pipe->read_pos];
        pipe->read_pos = (pipe->read_pos + 1) % PIPE_BUF_SIZE;
    }
    
    pipe->data_size -= bytes_read;
    
    /* TODO: 唤醒等待的写进程 */
    
    return bytes_read;
}

/*
 * 管道写操作
 */
static int pipe_write(struct vfs_file *file, const char *buf, size_t count)
{
    struct pipe *pipe = file->private_data;
    
    if (!pipe || !buf) {
        return -EINVAL;
    }
    
    /* 如果没有读端，发送SIGPIPE */
    if (pipe->read_count == 0) {
        kprintf("[PIPE] No readers, broken pipe\n");
        return -EPIPE;
    }
    
    /* 如果管道满，阻塞等待 */
    while (pipe->data_size >= PIPE_BUF_SIZE) {
        /* TODO: 阻塞当前进程，等待读取 */
        kprintf("[PIPE] Writer blocked, pipe full\n");
        return -EAGAIN;  /* 临时返回 */
    }
    
    /* 写入数据 */
    size_t to_write = count;
    if (to_write > PIPE_BUF_SIZE - pipe->data_size) {
        to_write = PIPE_BUF_SIZE - pipe->data_size;
    }
    
    size_t bytes_written = 0;
    while (bytes_written < to_write) {
        pipe->buffer[pipe->write_pos] = buf[bytes_written++];
        pipe->write_pos = (pipe->write_pos + 1) % PIPE_BUF_SIZE;
    }
    
    pipe->data_size += bytes_written;
    
    /* TODO: 唤醒等待的读进程 */
    
    return bytes_written;
}

/*
 * 管道关闭操作
 */
static int pipe_close(struct vfs_file *file, bool is_read_end)
{
    struct pipe *pipe = file->private_data;
    
    if (!pipe) {
        return -EINVAL;
    }
    
    if (is_read_end) {
        pipe->read_count--;
        kprintf("[PIPE] Read end closed, readers=%d\n", pipe->read_count);
    } else {
        pipe->write_count--;
        kprintf("[PIPE] Write end closed, writers=%d\n", pipe->write_count);
    }
    
    /* 如果两端都关闭，释放管道 */
    if (pipe->read_count == 0 && pipe->write_count == 0) {
        kprintf("[PIPE] Both ends closed, freeing pipe\n");
        kfree(pipe);
        file->private_data = NULL;
    }
    
    return 0;
}

/* 管道文件操作表 */
static struct vfs_file_operations pipe_fops = {
    .read = pipe_read,
    .write = pipe_write,
    /* .release 需要额外信息区分读写端 */
};

/*
 * sys_pipe - 创建管道
 * 
 * @param pipefd: 返回的文件描述符对 [0]=读端, [1]=写端
 * @return: 成功返回0，失败返回-1
 */
int sys_pipe(int pipefd[2])
{
    if (!pipefd) {
        return -EINVAL;
    }
    
    kprintf("[PIPE] Creating pipe...\n");
    
    /* 创建管道 */
    struct pipe *pipe = pipe_create();
    if (!pipe) {
        return -ENOMEM;
    }
    
    /* 创建两个VFS文件对象（读端和写端） */
    /* 这里简化：直接返回特殊fd，实际应通过VFS分配 */
    
    /* 为读端和写端各分配一个inode和file */
    extern struct vfs_inode *vfs_alloc_inode(struct vfs_superblock *sb, uint32_t ino);
    extern struct vfs_file *vfs_alloc_file(void);
    
    struct vfs_inode *inode_r = vfs_alloc_inode(NULL, 0);
    struct vfs_inode *inode_w = vfs_alloc_inode(NULL, 0);
    
    if (!inode_r || !inode_w) {
        kfree(pipe);
        if (inode_r) kfree(inode_r);
        if (inode_w) kfree(inode_w);
        return -ENOMEM;
    }
    
    /* 设置inode的操作表 */
    inode_r->f_op = &pipe_fops;
    inode_w->f_op = &pipe_fops;
    
    /* 分配文件描述符（简化：使用固定值，实际应从VFS分配） */
    /* 这里需要调用VFS的alloc_fd */
    
    pipe->read_count = 1;
    pipe->write_count = 1;
    
    /* 临时方案：返回特殊的fd值 */
    pipefd[0] = 100;  /* 读端 */
    pipefd[1] = 101;  /* 写端 */
    
    kprintf("[PIPE] Pipe created: read_fd=%d, write_fd=%d\n", 
            pipefd[0], pipefd[1]);
    
    kprintf("[PIPE] Note: Full implementation requires VFS integration\n");
    
    return 0;
}
