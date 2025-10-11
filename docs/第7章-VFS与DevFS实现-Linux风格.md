# 第7章：VFS与DevFS实现 - Linux风格

## 📚 概述

本章实现一个**简化但遵循Linux设计理念**的虚拟文件系统（VFS）和设备文件系统（DevFS），真正实现"一切皆文件"的Unix哲学。

---

## 🎯 设计目标

### 核心理念
```
1. 一切皆文件（Everything is a file）
   - 普通文件：数据存储
   - 设备：/dev/tty, /dev/null
   - 进程信息：/proc/[pid]
   - 系统信息：/sys

2. 统一接口
   - open() / close()
   - read() / write()
   - ioctl() - 设备控制

3. 分层设计
   VFS层（抽象）
     ↓
   具体文件系统层（ramfs, devfs, procfs）
```

---

## 🏗️ 简化的Linux VFS架构

### 1. 四大核心对象（简化版）

```c
/* 1. Superblock - 文件系统实例 */
struct vfs_superblock {
    const char *fstype;              /* 文件系统类型名 */
    struct vfs_inode *root;          /* 根inode */
    struct vfs_sb_operations *ops;   /* 操作表 */
    void *private_data;              /* 私有数据 */
};

/* 2. Inode - 文件元数据 */
struct vfs_inode {
    uint32_t ino;                    /* inode号 */
    uint32_t mode;                   /* 类型+权限 */
    uint32_t size;                   /* 文件大小 */
    struct vfs_superblock *sb;       /* 所属sb */
    struct vfs_inode_operations *i_op;  /* inode操作 */
    struct vfs_file_operations *f_op;   /* 文件操作 */
    void *private_data;              /* 私有数据（如设备号、数据指针） */
};

/* 3. Dentry - 目录项（路径缓存） */
struct vfs_dentry {
    char name[256];                  /* 文件名 */
    struct vfs_inode *inode;         /* 指向的inode */
    struct vfs_dentry *parent;       /* 父目录 */
    struct vfs_dentry *child;        /* 第一个子项 */
    struct vfs_dentry *sibling;      /* 兄弟项 */
};

/* 4. File - 打开文件对象 */
struct vfs_file {
    struct vfs_dentry *dentry;       /* 目录项 */
    struct vfs_inode *inode;         /* inode */
    struct vfs_file_operations *f_op;/* 操作表 */
    uint32_t flags;                  /* 打开标志 */
    off_t pos;                       /* 当前位置 */
    void *private_data;              /* 私有数据 */
};
```

### 2. 操作表（实现多态）

```c
/* Superblock操作 */
struct vfs_sb_operations {
    struct vfs_inode* (*alloc_inode)(struct vfs_superblock *sb);
    void (*destroy_inode)(struct vfs_inode *inode);
};

/* Inode操作（目录相关） */
struct vfs_inode_operations {
    struct vfs_dentry* (*lookup)(struct vfs_inode *dir, const char *name);
    int (*create)(struct vfs_inode *dir, const char *name, uint32_t mode);
    int (*mkdir)(struct vfs_inode *dir, const char *name, uint32_t mode);
};

/* File操作 */
struct vfs_file_operations {
    int (*open)(struct vfs_inode *inode, struct vfs_file *file);
    int (*release)(struct vfs_file *file);
    ssize_t (*read)(struct vfs_file *file, char *buf, size_t count);
    ssize_t (*write)(struct vfs_file *file, const char *buf, size_t count);
    int (*ioctl)(struct vfs_file *file, uint32_t cmd, unsigned long arg);
};
```

---

## 📁 DevFS - 设备文件系统

### 设计理念

Linux的DevFS让设备像文件一样访问：

```
/dev/null   - 空设备（写入丢弃，读取返回0）
/dev/zero   - 零设备（读取返回0字节）
/dev/tty    - 控制终端
/dev/console - 系统控制台
/dev/random - 随机数生成器
```

### DevFS实现

```c
/* 设备类型 */
#define DEV_TYPE_CHAR  0x2000   /* 字符设备 */
#define DEV_TYPE_BLOCK 0x6000   /* 块设备 */

/* 设备注册 */
struct dev_device {
    const char *name;           /* 设备名，如"null", "tty" */
    uint32_t major;             /* 主设备号 */
    uint32_t minor;             /* 次设备号 */
    uint32_t type;              /* 设备类型 */
    struct vfs_file_operations *fops;  /* 操作表 */
};

/* 注册设备 */
int devfs_register_device(const char *name, uint32_t major, 
                          uint32_t minor, uint32_t type,
                          struct vfs_file_operations *fops);

/* 访问设备 */
int fd = open("/dev/null", O_WRONLY);
write(fd, data, size);  // 数据被丢弃
close(fd);
```

---

## 💻 具体实现

### 1. /dev/null 实现

```c
static ssize_t dev_null_read(struct vfs_file *file, char *buf, size_t count)
{
    (void)file; (void)buf; (void)count;
    return 0;  /* 总是返回EOF */
}

static ssize_t dev_null_write(struct vfs_file *file, const char *buf, size_t count)
{
    (void)file; (void)buf;
    return count;  /* 假装写入成功 */
}

struct vfs_file_operations dev_null_fops = {
    .open = NULL,
    .release = NULL,
    .read = dev_null_read,
    .write = dev_null_write,
    .ioctl = NULL,
};

/* 注册 */
devfs_register_device("null", 1, 3, DEV_TYPE_CHAR, &dev_null_fops);
```

### 2. /dev/zero 实现

```c
static ssize_t dev_zero_read(struct vfs_file *file, char *buf, size_t count)
{
    (void)file;
    memset(buf, 0, count);
    return count;
}

struct vfs_file_operations dev_zero_fops = {
    .read = dev_zero_read,
    .write = dev_null_write,  /* 复用null的write */
};

devfs_register_device("zero", 1, 5, DEV_TYPE_CHAR, &dev_zero_fops);
```

### 3. /dev/console 实现（连接VGA）

```c
static ssize_t dev_console_write(struct vfs_file *file, const char *buf, size_t count)
{
    (void)file;
    for (size_t i = 0; i < count; i++) {
        vga_putc(buf[i]);
    }
    return count;
}

struct vfs_file_operations dev_console_fops = {
    .write = dev_console_write,
};

devfs_register_device("console", 5, 1, DEV_TYPE_CHAR, &dev_console_fops);
```

---

## 🔄 VFS与DevFS集成

### 系统调用实现

```c
/* open系统调用 */
int sys_open(const char *path, int flags, int mode)
{
    /* 1. 路径解析 */
    struct vfs_dentry *dentry = vfs_lookup(path);
    
    /* 2. 检查权限 */
    if (!dentry) {
        if (flags & O_CREAT) {
            // 创建文件
        } else {
            return -ENOENT;
        }
    }
    
    /* 3. 分配file结构 */
    struct vfs_file *file = alloc_file();
    file->dentry = dentry;
    file->inode = dentry->inode;
    file->f_op = dentry->inode->f_op;
    file->flags = flags;
    file->pos = 0;
    
    /* 4. 调用open */
    if (file->f_op && file->f_op->open) {
        int ret = file->f_op->open(file->inode, file);
        if (ret < 0) {
            free_file(file);
            return ret;
        }
    }
    
    /* 5. 分配文件描述符 */
    int fd = alloc_fd();
    current->files[fd] = file;
    
    return fd;
}

/* read系统调用 */
ssize_t sys_read(int fd, char *buf, size_t count)
{
    struct vfs_file *file = current->files[fd];
    if (!file || !file->f_op || !file->f_op->read) {
        return -EBADF;
    }
    
    return file->f_op->read(file, buf, count);
}

/* write系统调用 */
ssize_t sys_write(int fd, const char *buf, size_t count)
{
    struct vfs_file *file = current->files[fd];
    if (!file || !file->f_op || !file->f_op->write) {
        return -EBADF;
    }
    
    return file->f_op->write(file, buf, count);
}
```

---

## 🧪 使用示例

### 示例1：写入/dev/null

```c
int fd = open("/dev/null", O_WRONLY);
if (fd >= 0) {
    char data[] = "This data will be discarded";
    write(fd, data, sizeof(data));
    close(fd);
}
```

### 示例2：读取/dev/zero

```c
int fd = open("/dev/zero", O_RDONLY);
if (fd >= 0) {
    char buf[100];
    read(fd, buf, 100);  // buf全为0
    close(fd);
}
```

### 示例3：输出到控制台

```c
int fd = open("/dev/console", O_WRONLY);
if (fd >= 0) {
    write(fd, "Hello from VFS!\n", 16);
    close(fd);
}
```

---

## 🎯 实现要点

### 1. 保持简单
- 不实现完整的dentry缓存（太复杂）
- 使用静态数组而非动态分配
- 路径解析只支持绝对路径
- 每次都重新查找（性能换简单性）

### 2. 设备优先
- 先实现DevFS（最有用）
- /dev/null, /dev/zero, /dev/console
- 后续可扩展/dev/tty, /dev/random等

### 3. 渐进式
- 第一步：只实现open/close/read/write
- 第二步：添加设备
- 第三步：扩展功能

---

## ✅ 检查清单

VFS核心：
- [ ] 定义简化的4大对象
- [ ] 实现操作表机制
- [ ] 路径解析（绝对路径）
- [ ] 文件描述符管理

DevFS：
- [ ] 设备注册机制
- [ ] /dev/null
- [ ] /dev/zero  
- [ ] /dev/console

系统调用：
- [ ] open()
- [ ] close()
- [ ] read()
- [ ] write()

---

**目标：让设备像文件一样访问，真正实现"一切皆文件"！** 🎉

