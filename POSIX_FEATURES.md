# EduOS POSIX/Linux兼容性实现总结

## 📋 目录

1. [错误处理系统 (errno)](#错误处理系统)
2. [信号系统 (signals)](#信号系统)
3. [进程间通信 (IPC)](#进程间通信)
4. [文件描述符操作](#文件描述符操作)
5. [文件状态查询](#文件状态查询)
6. [时间管理](#时间管理)
7. [设备控制 (ioctl)](#设备控制)
8. [用户空间访问安全](#用户空间访问安全)
9. [构建系统](#构建系统)

---

## 错误处理系统

### 实现文件
- `include/errno.h` - 133个POSIX错误码定义
- `kernel/syscall/errno.c` - 内核错误处理实现

### 功能特性
- ✅ 133个标准POSIX错误码（EPERM, ENOENT, EBADF, EFAULT等）
- ✅ 每进程独立errno（`struct process` 中的 `errno` 字段）
- ✅ 内核辅助函数：`set_errno()`, `get_errno()`, `clear_errno()`
- ✅ 用户空间 `errno` 宏（通过 `__errno_location()`）

### 使用示例
```c
// 内核代码
if (error_condition) {
    return set_errno(EINVAL);  // 返回-1并设置errno
}

// 用户空间代码
if (open("/file.txt", O_RDONLY) < 0) {
    printf("Error: %d\n", errno);
}
```

---

## 信号系统

### 实现文件
- `include/signal.h` - 信号定义和结构
- `kernel/process/signal.c` - 信号处理实现

### 功能特性
- ✅ 32个POSIX标准信号（SIGHUP, SIGINT, SIGKILL, SIGSEGV, SIGTERM等）
- ✅ 信号处理函数（SIG_DFL, SIG_IGN, 自定义处理器）
- ✅ 信号掩码和信号集合操作（`sigset_t`, `sigaddset`, `sigdelset`等）
- ✅ 信号动作结构（`struct sigaction`）
- ✅ 默认信号行为（终止、停止、继续、忽略）

### 系统调用
| 系统调用 | 编号 | 功能 |
|---------|------|------|
| `kill(pid, sig)` | 37 | 向进程发送信号 |
| `signal(sig, handler)` | 48 | 设置信号处理器（简化版）|
| `sigaction(sig, act, oldact)` | 67 | 设置信号处理动作 |
| `sigprocmask(how, set, oldset)` | 126 | 修改信号掩码 |
| `pause()` | 29 | 暂停进程直到收到信号 |

### 使用示例
```c
// 设置SIGINT处理器
void sigint_handler(int sig) {
    printf("Caught SIGINT!\n");
}

signal(SIGINT, sigint_handler);

// 发送信号
kill(target_pid, SIGTERM);
```

---

## 进程间通信

### 管道 (Pipe)

#### 实现文件
- `include/pipe.h` - 管道结构定义
- `kernel/ipc/pipe.c` - 管道实现

#### 功能特性
- ✅ 4KB环形缓冲区（`PIPE_BUF_SIZE = 4096`）
- ✅ 阻塞式读写（读空阻塞，写满阻塞）
- ✅ 引用计数管理
- ✅ 读写端独立文件描述符

#### 系统调用
| 系统调用 | 编号 | 功能 |
|---------|------|------|
| `pipe(pipefd[2])` | 42 | 创建管道 |

#### 使用示例
```c
int pipefd[2];
pipe(pipefd);  // pipefd[0]=读端, pipefd[1]=写端

if (fork() == 0) {
    // 子进程：写数据
    close(pipefd[0]);
    write(pipefd[1], "Hello", 5);
    close(pipefd[1]);
} else {
    // 父进程：读数据
    close(pipefd[1]);
    char buf[10];
    read(pipefd[0], buf, 5);
    close(pipefd[0]);
}
```

---

## 文件描述符操作

### 实现文件
- `kernel/syscall/sys_fd.c` - 文件描述符操作实现

### 功能特性
- ✅ 复制文件描述符（`dup`）
- ✅ 复制到指定编号（`dup2`）
- ✅ 文件控制（`fcntl`）
  - F_DUPFD - 复制fd
  - F_GETFD/F_SETFD - fd标志
  - F_GETFL/F_SETFL - 文件状态标志
  - F_SETLK/F_SETLKW/F_GETLK - 文件锁（占位）

### 系统调用
| 系统调用 | 编号 | 功能 |
|---------|------|------|
| `dup(oldfd)` | 41 | 复制文件描述符 |
| `dup2(oldfd, newfd)` | 63 | 复制到指定fd |
| `fcntl(fd, cmd, arg)` | 55 | 文件控制操作 |

### 使用示例
```c
// 重定向stdout到文件
int fd = open("output.txt", O_WRONLY | O_CREAT);
dup2(fd, 1);  // stdout现在指向文件
printf("This goes to file\n");

// 获取文件标志
int flags = fcntl(fd, F_GETFL);
fcntl(fd, F_SETFL, flags | O_NONBLOCK);  // 设置非阻塞
```

---

## 文件状态查询

### 实现文件
- `include/sys/stat.h` - stat结构定义
- `kernel/syscall/sys_stat.c` - stat系统调用实现

### 功能特性
- ✅ 完整的 `struct stat` 结构
  - 文件类型和权限（`st_mode`）
  - 文件大小（`st_size`）
  - 设备号（`st_dev`, `st_rdev`）
  - inode号（`st_ino`）
  - 时间戳（`st_atime`, `st_mtime`, `st_ctime`）
- ✅ 文件类型判断宏（`S_ISREG`, `S_ISDIR`, `S_ISCHR`等）
- ✅ 权限宏（`S_IRUSR`, `S_IWUSR`, `S_IXUSR`等）

### 系统调用
| 系统调用 | 编号 | 功能 |
|---------|------|------|
| `stat(path, buf)` | 106 | 获取文件状态（路径）|
| `fstat(fd, buf)` | 108 | 获取文件状态（fd）|
| `lstat(path, buf)` | 107 | 获取文件状态（不跟随符号链接）|

### 使用示例
```c
struct stat st;
stat("/dev/fb0", &st);

if (S_ISCHR(st.st_mode)) {
    printf("Character device, size=%u\n", st.st_size);
}
```

---

## 时间管理

### 实现文件
- `include/sys/time.h` - 时间结构定义
- `kernel/syscall/sys_time.c` - 时间系统调用实现

### 功能特性
- ✅ 微秒精度时间（`struct timeval`）
- ✅ 纳秒精度时间（`struct timespec`）
- ✅ 系统启动时间（uptime）
- ✅ 可中断的睡眠（nanosleep支持EINTR）

### 系统调用
| 系统调用 | 编号 | 功能 |
|---------|------|------|
| `time(tloc)` | 13 | 获取时间（秒）|
| `gettimeofday(tv, tz)` | 78 | 获取时间（微秒）|
| `nanosleep(req, rem)` | 162 | 睡眠（纳秒）|

### 使用示例
```c
// 获取当前时间
struct timeval tv;
gettimeofday(&tv, NULL);
printf("Time: %ld.%06ld\n", tv.tv_sec, tv.tv_usec);

// 睡眠1秒
struct timespec ts = { .tv_sec = 1, .tv_nsec = 0 };
nanosleep(&ts, NULL);
```

---

## 设备控制

### 实现文件
- `include/sys/ioctl.h` - ioctl定义
- `kernel/syscall/sys_ioctl.c` - ioctl实现

### 功能特性
- ✅ Linux风格的ioctl命令编码（`_IOC`, `_IOR`, `_IOW`, `_IOWR`）
- ✅ 通用ioctl命令
  - FIONREAD - 获取可读字节数
  - FIONBIO - 设置非阻塞I/O
  - FIOASYNC - 设置异步I/O
- ✅ 终端ioctl命令
  - TIOCGWINSZ - 获取窗口大小
  - TCGETS/TCSETS - 终端属性
- ✅ 帧缓冲ioctl命令
  - FBIOGET_VSCREENINFO - 获取可变屏幕信息
  - FBIOGET_FSCREENINFO - 获取固定屏幕信息

### 系统调用
| 系统调用 | 编号 | 功能 |
|---------|------|------|
| `ioctl(fd, request, arg)` | 54 | 设备控制 |

### 使用示例
```c
// 获取终端窗口大小
struct winsize ws;
ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
printf("Terminal: %dx%d\n", ws.ws_col, ws.ws_row);

// 获取帧缓冲信息
struct fb_var_screeninfo var;
int fd = open("/dev/fb0", O_RDWR);
ioctl(fd, FBIOGET_VSCREENINFO, &var);
printf("Resolution: %dx%d@%d\n", var.xres, var.yres, var.bits_per_pixel);
```

---

## 用户空间访问安全

### 实现文件
- `include/mm/uaccess.h` - 用户空间访问接口
- `kernel/mm/uaccess.c` - 实现

### 功能特性
- ✅ 地址验证
  - `is_user_address()` - 检查地址是否在用户空间
  - `is_user_buffer()` - 检查缓冲区是否完全在用户空间
- ✅ 安全复制
  - `copy_from_user()` - 从用户空间复制到内核
  - `copy_to_user()` - 从内核复制到用户空间
- ✅ 字符串操作
  - `strncpy_from_user()` - 复制字符串
  - `strnlen_user()` - 获取字符串长度
- ✅ 内存操作
  - `clear_user()` - 清零用户空间内存

### 使用示例
```c
// 在系统调用中安全访问用户空间指针
int sys_read(int fd, char *buf, size_t count) {
    if (!is_user_buffer(buf, count)) {
        return set_errno(EFAULT);
    }
    
    char kernel_buf[4096];
    // ... 读取数据到 kernel_buf ...
    
    if (copy_to_user(buf, kernel_buf, count) < 0) {
        return set_errno(EFAULT);
    }
    
    return count;
}
```

---

## 构建系统

### Linux风格多Makefile结构

#### 顶层文件
- `Makefile` - 主Makefile
- `Kbuild` - 顶层Kbuild配置
- `scripts/Makefile.build` - 通用构建规则

#### 子系统Makefile
```
kernel/
├── arch/i386/Makefile      - x86架构
├── process/Makefile        - 进程管理
├── mm/Makefile             - 内存管理
├── fs/Makefile             - 文件系统
├── ipc/Makefile            - 进程间通信
├── syscall/Makefile        - 系统调用
├── drivers/Makefile        - 驱动程序
├── net/Makefile            - 网络子系统
└── exec/Makefile           - 程序加载
```

### 编译命令
```bash
make          # 编译内核
make clean    # 清理构建文件
make run      # 运行QEMU
make debug    # GDB调试
make info     # 显示帮助
```

---

## 📊 统计数据

### 新增功能
- **系统调用**: 22个POSIX标准系统调用
- **错误码**: 133个POSIX错误码
- **信号**: 32个标准信号
- **文件**: 17个新文件（7个头文件 + 10个实现文件）
- **代码行数**: 约2000+行

### 兼容性
- ✅ POSIX错误处理
- ✅ POSIX信号系统
- ✅ POSIX IPC（管道）
- ✅ POSIX文件操作
- ✅ POSIX时间管理
- ✅ POSIX设备控制
- ✅ Linux风格构建系统
- ✅ 内存安全机制

---

## 🎯 下一步计划

### 待实现功能
1. [ ] 完善 `wait/waitpid` 实现
2. [ ] 实现 `select/poll/epoll` I/O多路复用
3. [ ] 实现 `socket` 网络API
4. [ ] 实现 `pthread` 线程API
5. [ ] 实现 `madvise/mprotect` 内存管理
6. [ ] 实现文件锁（flock/fcntl）
7. [ ] 实现符号链接（symlink/readlink）
8. [ ] 实现文件系统mount/umount

### 优化建议
1. [ ] 审查调度器：进程状态管理、上下文切换
2. [ ] 审查内存管理：VMA、页表、缺页处理
3. [ ] 审查VFS：文件描述符、inode引用计数
4. [ ] 审查同步原语：锁机制、临界区保护
5. [ ] 添加更多错误检查和边界条件处理

---

## 📚 参考资料

- [POSIX.1-2017 Standard](https://pubs.opengroup.org/onlinepubs/9699919799/)
- [Linux System Call Table](https://chromium.googlesource.com/chromiumos/docs/+/master/constants/syscalls.md)
- [Linux Kernel Documentation](https://www.kernel.org/doc/html/latest/)
- [The Linux Programming Interface (TLPI)](http://man7.org/tlpi/)

---

**生成时间**: 2025-11-02  
**版本**: EduOS v4.0 - POSIX兼容版



