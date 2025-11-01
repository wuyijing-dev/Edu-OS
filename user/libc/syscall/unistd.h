/*
 * unistd.h - POSIX 系统调用和常量
 */

#ifndef _UNISTD_H
#define _UNISTD_H

/* 基本类型 */
typedef int pid_t;
typedef int ssize_t;
typedef unsigned int size_t;

/* 标准文件描述符 */
#define STDIN_FILENO    0
#define STDOUT_FILENO   1
#define STDERR_FILENO   2

/* 进程控制 */
void _exit(int status);
pid_t fork(void);
pid_t getpid(void);
pid_t getppid(void);
pid_t wait(int *status);
pid_t waitpid(pid_t pid, int *status, int options);
int execve(const char *path, char *const argv[], char *const envp[]);

/* I/O 操作 */
ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
int close(int fd);

/* 内存映射 */
typedef int off_t;
void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
int munmap(void *addr, size_t length);

#endif /* _UNISTD_H */
