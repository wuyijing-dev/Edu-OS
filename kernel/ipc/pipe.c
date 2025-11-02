/*
 * pipe.c - POSIX管道实现
 * 
 * Linux风格的匿名管道，支持半双工通信
 */

#include <pipe.h>
#include <process/process.h>
#include <fs/vfs.h>
#include <mm/kmalloc.h>
#include <kernel.h>
#include <string.h>

/* 外部函数声明 */
extern int fd_table_alloc(struct file_descriptor_table *table, struct vfs_file *file);
extern void fd_table_free(struct file_descriptor_table *table, int fd);

/* errno错误码 */
#define ENOMEM  12
#define EBADF   9
#define EINVAL  22
#define EPIPE   32
#define EAGAIN  11

/*
 * pipe_create - 创建管道
 */
struct pipe *pipe_create(void)
{
    struct pipe *pipe = (struct pipe*)kmalloc(sizeof(struct pipe));
    if (!pipe) {
        return NULL;
    }
    
    memset(pipe, 0, sizeof(struct pipe));
    pipe->read_pos = 0;
    pipe->write_pos = 0;
    pipe->size = 0;
    pipe->readers = 1;
    pipe->writers = 1;
    
    kprintf("[PIPE] Created pipe at %p\n", pipe);
    return pipe;
}

/*
 * pipe_destroy - 销毁管道
 */
void pipe_destroy(struct pipe *pipe)
{
    if (!pipe) return;
    
    kprintf("[PIPE] Destroying pipe at %p\n", pipe);
    kfree(pipe);
}

/*
 * pipe_read - 从管道读取数据
 * 
 * @pipe: 管道对象
 * @buf: 目标缓冲区
 * @count: 要读取的字节数
 * 
 * 返回：实际读取的字节数，0表示EOF，-1表示错误
 */
ssize_t pipe_read(struct pipe *pipe, char *buf, size_t count)
{
    if (!pipe || !buf) {
        extern int set_errno(int error_code);
        set_errno(EINVAL);
        return -1;
    }
    
    /* 如果管道为空且没有写者，返回EOF */
    if (pipe->size == 0 && pipe->writers == 0) {
        return 0;  /* EOF */
    }
    
    /* 如果管道为空且有写者，暂时返回EAGAIN（非阻塞）
     * TODO: 实现阻塞等待
     */
    if (pipe->size == 0) {
        extern int set_errno(int error_code);
        set_errno(EAGAIN);
        return -1;
    }
    
    /* 读取数据（最多读取当前可用数据量） */
    size_t to_read = (count < pipe->size) ? count : pipe->size;
    size_t bytes_read = 0;
    
    while (bytes_read < to_read) {
        buf[bytes_read] = pipe->buffer[pipe->read_pos];
        pipe->read_pos = (pipe->read_pos + 1) % PIPE_BUF;
        bytes_read++;
        pipe->size--;
    }
    
    kprintf("[PIPE] Read %u bytes from pipe %p\n", bytes_read, pipe);
    return bytes_read;
}

/*
 * pipe_write - 向管道写入数据
 * 
 * @pipe: 管道对象
 * @buf: 源缓冲区
 * @count: 要写入的字节数
 * 
 * 返回：实际写入的字节数，-1表示错误
 */
ssize_t pipe_write(struct pipe *pipe, const char *buf, size_t count)
{
    if (!pipe || !buf) {
        extern int set_errno(int error_code);
        set_errno(EINVAL);
        return -1;
    }
    
    /* 如果没有读者，返回EPIPE（Broken pipe） */
    if (pipe->readers == 0) {
        extern int set_errno(int error_code);
        set_errno(EPIPE);
        /* TODO: 发送SIGPIPE信号 */
        return -1;
    }
    
    /* 如果管道已满，暂时返回EAGAIN（非阻塞）
     * TODO: 实现阻塞等待
     */
    if (pipe->size >= PIPE_BUF) {
        extern int set_errno(int error_code);
        set_errno(EAGAIN);
        return -1;
    }
    
    /* 写入数据（最多写满管道） */
    size_t space = PIPE_BUF - pipe->size;
    size_t to_write = (count < space) ? count : space;
    size_t bytes_written = 0;
    
    while (bytes_written < to_write) {
        pipe->buffer[pipe->write_pos] = buf[bytes_written];
        pipe->write_pos = (pipe->write_pos + 1) % PIPE_BUF;
        bytes_written++;
        pipe->size++;
    }
    
    kprintf("[PIPE] Wrote %u bytes to pipe %p\n", bytes_written, pipe);
    return bytes_written;
}

/*
 * pipe_close_read - 关闭读端
 */
void pipe_close_read(struct pipe *pipe)
{
    if (!pipe) return;
    
    if (pipe->readers > 0) {
        pipe->readers--;
        kprintf("[PIPE] Closed read end of pipe %p (readers=%u)\n", 
                pipe, pipe->readers);
    }
    
    /* 如果读写端都关闭，销毁管道 */
    if (pipe->readers == 0 && pipe->writers == 0) {
        pipe_destroy(pipe);
    }
}

/*
 * pipe_close_write - 关闭写端
 */
void pipe_close_write(struct pipe *pipe)
{
    if (!pipe) return;
    
    if (pipe->writers > 0) {
        pipe->writers--;
        kprintf("[PIPE] Closed write end of pipe %p (writers=%u)\n", 
                pipe, pipe->writers);
    }
    
    /* 如果读写端都关闭，销毁管道 */
    if (pipe->readers == 0 && pipe->writers == 0) {
        pipe_destroy(pipe);
    }
}

/*
 * sys_pipe - 创建管道系统调用
 * 
 * @pipefd: 用户空间数组，返回两个文件描述符
 *          pipefd[0] 为读端，pipefd[1] 为写端
 * 
 * 返回：0=成功，-1=失败
 */
int sys_pipe(int pipefd[2])
{
    extern struct process *process_get_current(void);
    struct process *current = process_get_current();
    
    if (!current || !pipefd) {
        extern int set_errno(int error_code);
        return set_errno(EINVAL);
    }
    
    /* 创建管道 */
    struct pipe *pipe = pipe_create();
    if (!pipe) {
        extern int set_errno(int error_code);
        return set_errno(ENOMEM);
    }
    
    /* 创建两个VFS文件对象（读端和写端）*/
    struct vfs_file *read_file = (struct vfs_file*)kmalloc(sizeof(struct vfs_file));
    struct vfs_file *write_file = (struct vfs_file*)kmalloc(sizeof(struct vfs_file));
    
    if (!read_file || !write_file) {
        if (read_file) kfree(read_file);
        if (write_file) kfree(write_file);
        pipe_destroy(pipe);
        extern int set_errno(int error_code);
        return set_errno(ENOMEM);
    }
    
    memset(read_file, 0, sizeof(struct vfs_file));
    memset(write_file, 0, sizeof(struct vfs_file));
    
    /* 设置文件对象 */
    read_file->flags = O_RDONLY;
    read_file->pos = 0;
    read_file->private_data = pipe;
    read_file->ref_count = 1;
    
    write_file->flags = O_WRONLY;
    write_file->pos = 0;
    write_file->private_data = pipe;
    write_file->ref_count = 1;
    
    /* 分配文件描述符 */
    int read_fd = fd_table_alloc(current->fd_table, read_file);
    if (read_fd < 0) {
        kfree(read_file);
        kfree(write_file);
        pipe_destroy(pipe);
        extern int set_errno(int error_code);
        return set_errno(ENOMEM);
    }
    
    int write_fd = fd_table_alloc(current->fd_table, write_file);
    if (write_fd < 0) {
        fd_table_free(current->fd_table, read_fd);
        kfree(write_file);
        pipe_destroy(pipe);
        extern int set_errno(int error_code);
        return set_errno(ENOMEM);
    }
    
    /* 返回文件描述符 */
    pipefd[0] = read_fd;
    pipefd[1] = write_fd;
    
    kprintf("[PIPE] Created pipe: read_fd=%d, write_fd=%d, pipe=%p\n", 
            read_fd, write_fd, pipe);
    
    return 0;
}
