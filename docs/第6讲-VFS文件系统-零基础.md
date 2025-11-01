# 第6讲：VFS虚拟文件系统 - 从零开始

> "一切皆文件" - Unix/Linux的设计哲学

## 🎯 课程目标

学完本课，你将理解：

1. **什么是"一切皆文件"？为什么这样设计？**
2. **VFS（虚拟文件系统）的作用**
3. **inode、dentry、file的区别**
4. **文件操作：open、read、write、close**
5. **路径解析：从"/dev/fb0"找到设备**
6. **EduOS的VFS实现**

**前置知识：**
- ✅ 物理内存管理（第4讲）
- ✅ 虚拟内存管理（第5讲）
- ✅ 基础数据结构（链表）
- ❌ 不需要任何文件系统经验

**涉及文件：**
- `kernel/fs/vfs_core.c` - VFS核心实现
- `include/fs/vfs.h` - VFS接口定义

---

## 📖 第一课：什么是"一切皆文件"

### 1.1 从生活例子理解

**没有文件系统的世界：**

```c
// 读取硬盘
ide_read_sector(0, buffer);

// 显示文本
vga_putchar('A');

// 发送网络数据
network_send(data, len);

// 读取键盘
char ch = keyboard_read();

每个设备都有自己的接口！
需要记住很多函数！
```

**有了"一切皆文件"：**

```c
// 所有操作都用同样的接口！

// 读取硬盘
int fd = open("/dev/hda", O_RDONLY);
read(fd, buffer, 512);
close(fd);

// 写入显示器
int fd = open("/dev/console", O_WRONLY);
write(fd, "Hello", 5);
close(fd);

// 网络通信
int fd = open("/net/tcp/192.168.1.1:80", O_RDWR);
write(fd, request, len);
read(fd, response, sizeof(response));
close(fd);

// 读取键盘
int fd = open("/dev/keyboard", O_RDONLY);
read(fd, &ch, 1);
close(fd);

统一的接口：open/read/write/close
```

**优势：**

```
1. 简单：只需要4个函数
2. 统一：所有设备用同样的方式
3. 可组合：管道、重定向等
4. 灵活：新设备只需实现4个函数
```

### 1.2 文件的本质

**文件 ≠ 磁盘上的数据**

在Unix/Linux中，**文件是一个抽象概念**：

```
文件 = 一个可以读写的"东西"

"东西"可以是：
  - 磁盘文件（/home/user/document.txt）
  - 设备（/dev/fb0，显卡）
  - 进程信息（/proc/1/status）
  - 网络连接（/net/tcp/...）
  - 管道（/pipe/my_pipe）
  - 内存（/dev/zero）
  - ...

只要实现read/write接口，就是"文件"！
```

---

## 📖 第二课：VFS的三大核心概念

### 2.1 inode（索引节点）

**inode = Index Node（文件的元数据）**

```c
/* EduOS的inode结构（简化）*/
struct vfs_inode {
    uint32_t ino;           // inode号（唯一标识）
    uint32_t mode;          // 文件类型和权限
    uint32_t size;          // 文件大小
    uint32_t blocks;        // 占用的块数
    uint32_t nlinks;        // 硬链接计数
    
    /* 时间戳 */
    uint32_t atime;         // 最后访问时间
    uint32_t mtime;         // 最后修改时间
    uint32_t ctime;         // 创建时间
    
    /* 操作函数 */
    struct vfs_inode_operations *ops;
    
    /* 私有数据 */
    void *private_data;
};
```

**inode的作用：**

```
inode是文件的"身份证"：
  - 唯一编号（ino）
  - 大小、权限、时间
  - 如何读写（ops）
  
不包含：
  ✗ 文件名（在dentry中）
  ✗ 路径（在dentry中）
  ✗ 文件内容（在磁盘/设备中）
```

### 2.2 dentry（目录项）

**dentry = Directory Entry（路径缓存）**

```c
/* EduOS的dentry结构 */
struct vfs_dentry {
    char name[VFS_MAX_NAME];        // 文件名（不含路径）
    struct vfs_inode *inode;        // 指向inode
    struct vfs_dentry *parent;      // 父目录
    struct vfs_dentry *next;        // 同级下一个
};
```

**dentry的作用：**

```
dentry是"路标"：
  - 文件名
  - 指向inode
  - 组成目录树
  
示例：
  /dev/fb0
  
  dentry链：
    / (root)
     └─ dev (dentry: name="dev")
         └─ fb0 (dentry: name="fb0", inode=...)
```

### 2.3 file（打开的文件）

**file = 打开文件的实例**

```c
/* EduOS的file结构 */
struct vfs_file {
    struct vfs_dentry *dentry;      // 指向dentry
    struct vfs_inode *inode;        // 指向inode（快速访问）
    uint32_t flags;                 // 打开标志（O_RDONLY等）
    uint32_t offset;                // 当前读写位置
    uint32_t ref_count;             // 引用计数
    
    /* 操作函数 */
    struct vfs_file_operations *ops;
};
```

**三者关系：**

```
进程打开文件的过程：

1. open("/dev/fb0", O_RDWR)
   ↓
2. 路径解析：查找dentry
   ↓
3. dentry指向inode
   ↓
4. 创建file对象
   ↓
5. file.dentry = dentry
   file.inode = inode
   file.offset = 0
   ↓
6. 返回文件描述符fd
   
使用：
  write(fd, buf, len)
  ↓
  通过fd找到file对象
  ↓
  调用file->ops->write()
```

**类比：**

```
inode  = 书的内容（实体）
dentry = 图书馆目录（索引）
file   = 你借的书（实例）

一本书（inode）：
  可以有多个目录条目（dentry）：硬链接
  可以被多人借阅（file）：多次打开
```

---

## 📖 第三课：文件操作接口

### 3.1 打开文件（open）

**系统调用接口：**

```c
int open(const char *path, int flags, int mode);

flags：
  O_RDONLY  = 0  // 只读
  O_WRONLY  = 1  // 只写
  O_RDWR    = 2  // 读写
  O_CREAT   = 4  // 创建（如果不存在）
  O_TRUNC   = 8  // 清空
  O_APPEND  = 16 // 追加

mode：文件权限（暂时不用）
```

**VFS内部实现（EduOS）：**

基于 `kernel/fs/vfs_core.c`:

```c
int vfs_open(const char *path, int flags, int mode)
{
    if (!path) return -EINVAL;
    
    /* 路径解析 */
    struct vfs_dentry *dentry = vfs_lookup(path);
    if (!dentry) {
        return -ENOENT;  // 文件不存在
    }
    
    if (!dentry->inode) {
        return -ENOENT;
    }
    
    /* 分配file对象 */
    struct vfs_file *file = alloc_file();
    if (!file) {
        return -ENOMEM;
    }
    
    file->dentry = dentry;
    file->inode = dentry->inode;
    file->flags = flags;
    file->offset = 0;
    
    /* 调用inode的open方法 */
    if (file->inode->i_fop && file->inode->i_fop->open) {
        int ret = file->inode->i_fop->open(file->inode, file);
        if (ret < 0) {
            free_file(file);
            return ret;
        }
    }
    
    /* 分配文件描述符 */
    int fd = alloc_fd(file);
    return fd;
}
```

**流程图：**

```
open("/dev/fb0", O_RDWR)
   ↓
1. vfs_lookup("/dev/fb0")
   ↓ 解析路径
   找到dentry（name="fb0"）
   ↓
2. dentry->inode（fb0设备的inode）
   ↓
3. alloc_file()（分配file结构）
   ↓
4. file->dentry = dentry
   file->inode = inode
   file->offset = 0
   ↓
5. inode->i_fop->open()（设备初始化）
   ↓
6. alloc_fd(file)（分配fd号）
   ↓
7. 返回fd（比如4）
```

### 3.2 读文件（read）

```c
ssize_t vfs_read(int fd, char *buf, size_t count)
{
    /* 获取file对象 */
    struct vfs_file *file = get_file(fd);
    if (!file) {
        return -EBADF;  // 无效fd
    }
    
    /* 检查权限 */
    if (file->flags & O_WRONLY) {
        return -EBADF;  // 只写文件不能读
    }
    
    /* 调用文件的read方法 */
    if (!file->inode->i_fop || !file->inode->i_fop->read) {
        return -EINVAL;  // 不支持read
    }
    
    ssize_t ret = file->inode->i_fop->read(file, buf, count);
    
    /* 更新偏移 */
    if (ret > 0) {
        file->offset += ret;
    }
    
    return ret;
}
```

### 3.3 写文件（write）

```c
ssize_t vfs_write(int fd, const char *buf, size_t count)
{
    /* 获取file对象 */
    struct vfs_file *file = get_file(fd);
    if (!file) {
        return -EBADF;
    }
    
    /* 检查权限 */
    if (file->flags & O_RDONLY) {
        return -EBADF;  // 只读文件不能写
    }
    
    /* 调用文件的write方法 */
    if (!file->inode->i_fop || !file->inode->i_fop->write) {
        return -EINVAL;
    }
    
    ssize_t ret = file->inode->i_fop->write(file, buf, count);
    
    /* 更新偏移 */
    if (ret > 0) {
        file->offset += ret;
    }
    
    return ret;
}
```

### 3.4 关闭文件（close）

```c
int vfs_close(int fd)
{
    struct vfs_file *file = get_file(fd);
    if (!file) {
        return -EBADF;
    }
    
    /* 调用文件的release方法 */
    if (file->inode->i_fop && file->inode->i_fop->release) {
        file->inode->i_fop->release(file);
    }
    
    /* 释放file对象 */
    free_file(file);
    
    /* 释放fd */
    free_fd(fd);
    
    return 0;
}
```

---

## 📖 第四课：路径解析

### 4.1 从路径到inode

**路径：** `/dev/fb0`

**解析过程：**

```
步骤1：拆分路径
  "/" → 根目录
  "dev" → 第1级
  "fb0" → 第2级（目标）

步骤2：从根开始查找
  current = root_dentry
  
步骤3：查找"dev"
  在root目录中查找name="dev"的dentry
  current = dev_dentry
  
步骤4：查找"fb0"
  在dev目录中查找name="fb0"的dentry
  current = fb0_dentry
  
步骤5：返回
  return fb0_dentry
```

**EduOS的实现：**

基于 `kernel/fs/vfs_core.c`:

```c
struct vfs_dentry *vfs_lookup(const char *path)
{
    if (!path) return NULL;
    
    /* 空路径或根路径 */
    if (path[0] == '\0' || (path[0] == '/' && path[1] == '\0')) {
        return vfs_state.root_dentry;
    }
    
    /* 必须是绝对路径 */
    if (path[0] != '/') {
        return NULL;
    }
    
    /* 从根目录开始 */
    struct vfs_dentry *current = vfs_state.root_dentry;
    const char *p = path + 1;  // 跳过开头的'/'
    
    /* 逐个解析路径组件 */
    while (*p) {
        /* 跳过连续的'/' */
        while (*p == '/') p++;
        
        if (*p == '\0') break;
        
        /* 提取一个组件 */
        char component[VFS_MAX_NAME];
        int i = 0;
        while (*p && *p != '/' && i < VFS_MAX_NAME - 1) {
            component[i++] = *p++;
        }
        component[i] = '\0';
        
        /* 在当前目录查找 */
        if (!current->inode || !current->inode->i_op || 
            !current->inode->i_op->lookup) {
            return NULL;  // 不是目录
        }
        
        current = current->inode->i_op->lookup(current->inode, component);
        if (!current) {
            return NULL;  // 组件不存在
        }
    }
    
    return current;
}
```

**示例：**

```c
/* 查找/dev/fb0 */
struct vfs_dentry *dentry = vfs_lookup("/dev/fb0");

if (dentry) {
    kprintf("Found: %s\n", dentry->name);
    kprintf("inode: %p\n", dentry->inode);
} else {
    kprintf("Not found\n");
}
```

---

## 📖 第五课：文件操作表

### 5.1 inode操作表

```c
struct vfs_inode_operations {
    /* 目录操作 */
    struct vfs_dentry *(*lookup)(struct vfs_inode *dir, const char *name);
    
    /* 文件创建 */
    int (*create)(struct vfs_inode *dir, struct vfs_dentry *dentry, int mode);
    int (*mkdir)(struct vfs_inode *dir, struct vfs_dentry *dentry, int mode);
    
    /* 删除 */
    int (*unlink)(struct vfs_inode *dir, struct vfs_dentry *dentry);
    int (*rmdir)(struct vfs_inode *dir, struct vfs_dentry *dentry);
};
```

### 5.2 file操作表

```c
struct vfs_file_operations {
    /* 打开/关闭 */
    int (*open)(struct vfs_inode *inode, struct vfs_file *file);
    int (*release)(struct vfs_file *file);
    
    /* 读写 */
    ssize_t (*read)(struct vfs_file *file, char *buf, size_t count);
    ssize_t (*write)(struct vfs_file *file, const char *buf, size_t count);
    
    /* 定位 */
    off_t (*lseek)(struct vfs_file *file, off_t offset, int whence);
    
    /* 控制 */
    int (*ioctl)(struct vfs_file *file, unsigned int cmd, unsigned long arg);
};
```

**设备实现示例：**

```c
/* /dev/console的实现 */
static int dev_console_open(struct vfs_inode *inode, struct vfs_file *file)
{
    return 0;  // 总是成功
}

static ssize_t dev_console_write(struct vfs_file *file, 
                                 const char *buf, size_t count)
{
    /* 输出到VGA控制台 */
    for (size_t i = 0; i < count; i++) {
        vga_putc(buf[i]);
    }
    return count;
}

static struct vfs_file_operations console_fops = {
    .open = dev_console_open,
    .write = dev_console_write,
    .read = NULL,  // 控制台不支持read（简化）
};
```

**现在printf可以通过VFS工作了：**

```c
int printf(const char *fmt, ...)
{
    char buf[1024];
    /* 格式化字符串 */
    vsprintf(buf, fmt, args);
    
    /* 通过VFS写入stdout */
    write(STDOUT_FILENO, buf, strlen(buf));  // fd=1
    
    // write()会调用：
    //   vfs_write(1, buf, len)
    //   → get_file(1) → console file
    //   → console_fops.write()
    //   → vga_putc()
}
```

---

## 📖 第六课：文件描述符表

### 6.1 什么是文件描述符？

**文件描述符（File Descriptor, fd）：** 整数索引

```c
int fd = open("/dev/fb0", O_RDWR);
// fd = 4

read(fd, buf, 100);   // 用整数4代表文件
write(fd, data, 50);  // 用整数4
close(fd);            // 用整数4

为什么不直接传file指针？
  1. 安全：用户程序不能直接访问内核指针
  2. 简单：整数容易传递
  3. 标准：所有Unix系统都这样
```

### 6.2 标准文件描述符

**Unix约定：**

```c
#define STDIN_FILENO   0  // 标准输入
#define STDOUT_FILENO  1  // 标准输出
#define STDERR_FILENO  2  // 标准错误

所有进程默认打开这3个：
  fd 0 → /dev/console（输入）
  fd 1 → /dev/console（输出）
  fd 2 → /dev/console（错误）
```

**EduOS的实现：**

```c
/* 在创建进程时打开标准fd */
void setup_stdio(void)
{
    int fd0 = vfs_open("/console", O_RDWR, 0);  // stdin
    int fd1 = vfs_open("/console", O_RDWR, 0);  // stdout  
    int fd2 = vfs_open("/console", O_RDWR, 0);  // stderr
    
    if (fd0 != 0 || fd1 != 1 || fd2 != 2) {
        panic("Failed to setup stdio!");
    }
}
```

**用户程序现在可以用printf了：**

```c
void _start(void)
{
    printf("Hello World\n");  // 自动输出到fd 1（stdout）
    fprintf(2, "Error!\n");   // 输出到fd 2（stderr）
}
```

---

## 📖 第七课：VFS初始化（EduOS）

基于 `kernel/fs/vfs_core.c`:

```c
/* VFS全局状态 */
static struct {
    struct vfs_dentry *root_dentry;          // 根目录
    struct vfs_file *open_files[VFS_MAX_OPEN_FILES];  // 打开文件表
    uint32_t next_fd;
    bool initialized;
} vfs_state;

void vfs_init(void)
{
    kprintf("[VFS] Initializing Virtual File System...\n");
    
    memset(&vfs_state, 0, sizeof(vfs_state));
    
    /* 创建根inode */
    struct vfs_inode *root_inode = vfs_alloc_inode(NULL, 1);
    root_inode->mode = S_IFDIR | 0755;  // 目录，rwxr-xr-x
    
    /* 创建根dentry */
    vfs_state.root_dentry = vfs_alloc_dentry("/", root_inode);
    
    vfs_state.initialized = true;
    
    kprintf("[VFS] Virtual File System initialized\n");
    kprintf("[VFS] Root directory created\n");
}
```

**挂载DevFS到/：**

```c
/* DevFS初始化后，挂载到根 */
void mount_devfs(void)
{
    /* DevFS有自己的root */
    struct vfs_dentry *devfs_root = devfs_get_root();
    
    /* 简化：直接让VFS root = DevFS root */
    vfs_state.root_dentry = devfs_root;
    
    kprintf("[VFS] Mounted DevFS as root filesystem\n");
}
```

---

## 🧪 实际运行效果（EduOS）

### VFS初始化

```
[VFS] Initializing Virtual File System...
[VFS] Virtual File System initialized
[VFS] Root directory created

[DevFS] Initializing Device File System...
[DevFS] Registered device: null (major=1, minor=3)
[DevFS] Registered device: zero (major=1, minor=5)
[DevFS] Registered device: console (major=5, minor=1)
[DevFS] Registered device: fb0 (major=29, minor=0)
[DevFS] Device File System initialized
```

### 打开文件测试

```c
int fd = vfs_open("/console", O_RDWR, 0);
kprintf("Opened /console: fd=%d\n", fd);

输出：
[DEV] /dev/console opened
Opened /console: fd=0
```

---

## 💡 常见问题

### Q1：为什么需要VFS？直接用FAT32不行吗？

**A：** VFS提供统一接口

```
没有VFS：
  fat32_open("/file.txt")
  devfs_open("/dev/fb0")
  procfs_read("/proc/1/status")
  
  每个文件系统不同的接口！

有VFS：
  vfs_open("/file.txt")     → FAT32
  vfs_open("/dev/fb0")      → DevFS
  vfs_open("/proc/1/status") → ProcFS
  
  统一的接口，VFS自动分发！
```

### Q2：inode号从哪来？

**A：** 由文件系统分配

```
FAT32：
  用文件的第一个簇号作为inode号
  
DevFS：
  自动分配（1, 2, 3, ...）
  
ProcFS：
  用进程PID作为inode号
  
只要在同一文件系统内唯一就行！
```

---

## 🎓 本讲总结

### 核心概念

✅ **一切皆文件** - 统一接口访问所有资源  
✅ **VFS** - 虚拟文件系统抽象层  
✅ **inode** - 文件的元数据  
✅ **dentry** - 目录项（路径缓存）  
✅ **file** - 打开文件的实例  
✅ **fd** - 文件描述符（整数索引）  

### VFS核心API

```c
/* 初始化 */
void vfs_init(void);

/* 文件操作 */
int vfs_open(const char *path, int flags, int mode);
ssize_t vfs_read(int fd, char *buf, size_t count);
ssize_t vfs_write(int fd, const char *buf, size_t count);
int vfs_close(int fd);
off_t vfs_lseek(int fd, off_t offset, int whence);

/* 路径解析 */
struct vfs_dentry *vfs_lookup(const char *path);
```

### 下一步

学习 **第7讲：FAT32文件系统**，实现真正的磁盘文件读写！

---

**VFS - 操作系统的统一接口！** 📁



