# 第7章：虚拟文件系统（VFS）核心 - 详细教程

## 📚 概述

虚拟文件系统（Virtual File System, VFS）是Linux"一切皆文件"哲学的核心实现。它提供了一个统一的抽象层，使得不同的文件系统、设备、网络资源都可以通过统一的接口访问。

---

## 🎯 为什么需要VFS？

### 问题：多样化的存储设备

```
操作系统需要支持：
- 硬盘文件系统（ext2, ext4, FAT32, NTFS...）
- 设备（键盘、鼠标、串口...）
- 伪文件系统（/proc, /sys, /dev...）
- 网络文件系统（NFS, CIFS...）

如果没有VFS：
应用程序需要为每种文件系统写不同的代码 ❌
read_ext2(), read_fat32(), read_device()... 太混乱！
```

### 解决：统一的抽象层

```
应用程序
    │
    ├─→ open("/file.txt")
    ├─→ read(fd, buf, size)
    ├─→ write(fd, buf, size)
    │
    VFS（统一接口）
    │
    ├─→ Ext2文件系统
    ├─→ FAT32文件系统
    ├─→ 设备驱动
    └─→ procfs/sysfs

一切皆文件！统一接口访问所有资源 ✅
```

---

## 🏗️ VFS核心数据结构

VFS由四大核心数据结构组成：

### 1. Superblock（超级块）

**作用**：描述整个文件系统的元信息

```c
struct superblock {
    uint32_t magic;              /* 魔数（识别文件系统类型） */
    uint32_t block_size;         /* 块大小 */
    uint32_t total_blocks;       /* 总块数 */
    uint32_t free_blocks;        /* 空闲块数 */
    struct inode *root_inode;    /* 根目录inode */
    struct fs_operations *ops;   /* 文件系统操作表 */
    void *private_data;          /* 文件系统私有数据 */
};
```

**类比**：超级块就像一本书的封面和目录
- 告诉你这是什么类型的文件系统
- 有多少空间，已用多少
- 如何访问根目录

### 2. Inode（索引节点）

**作用**：描述文件的元数据（但不包含文件名）

```c
struct inode {
    uint32_t ino;                /* inode编号（文件唯一标识） */
    uint32_t mode;               /* 文件类型和权限 */
    uint32_t uid;                /* 所有者用户ID */
    uint32_t gid;                /* 所有者组ID */
    uint32_t size;               /* 文件大小（字节） */
    uint32_t blocks;             /* 占用的块数 */
    uint64_t atime;              /* 最后访问时间 */
    uint64_t mtime;              /* 最后修改时间 */
    uint64_t ctime;              /* 最后状态改变时间 */
    uint32_t nlinks;             /* 硬链接数 */
    
    struct inode_operations *ops; /* inode操作表 */
    struct file_operations *fops; /* 文件操作表 */
    
    void *private_data;          /* 文件系统私有数据 */
};
```

**重要概念**：Inode不存储文件名！

```
文件名存储在目录项（dentry）中
Inode只关心文件的属性和数据位置

示例：
/home/user/file.txt

Inode #12345:
- 类型：普通文件
- 大小：1024字节
- 权限：rw-r--r--
- 数据块：[100, 101, 102]

文件名"file.txt"存储在目录/home/user/的数据中
```

### 3. Dentry（目录项）

**作用**：文件名到Inode的映射，构建目录树

```c
struct dentry {
    char name[256];              /* 文件名 */
    struct inode *inode;         /* 指向的inode */
    struct dentry *parent;       /* 父目录项 */
    struct dentry *children;     /* 子目录项链表 */
    struct dentry *sibling;      /* 兄弟目录项 */
    uint32_t ref_count;          /* 引用计数 */
};
```

**目录树结构**：

```
                 / (root dentry)
                 │
        ┌────────┼────────┐
        │        │        │
       bin      home     etc
                 │
         ┌───────┴───────┐
         │               │
        user            admin
         │
    ┌────┼────┐
    │    │    │
  file1 file2 dir1

每个dentry包含：
- 名字（如"file1"）
- 指向inode的指针
- 父子关系指针
```

### 4. File（打开文件对象）

**作用**：表示进程打开的文件

```c
struct file {
    uint32_t fd;                 /* 文件描述符 */
    struct dentry *dentry;       /* 关联的目录项 */
    struct inode *inode;         /* 关联的inode */
    uint32_t flags;              /* 打开标志（O_RDONLY, O_WRONLY...） */
    uint32_t pos;                /* 当前文件位置 */
    uint32_t ref_count;          /* 引用计数 */
    struct file_operations *ops; /* 文件操作表 */
};
```

**File vs Inode**：

```
Inode：文件本身的属性（在磁盘上）
File：进程打开文件的实例（在内存中）

示例：
两个进程同时打开同一个文件：
- 共享同一个Inode（文件只有一个）
- 各有独立的File对象（各自的读写位置）

进程A: file1 { pos=0 }  ─┐
                          ├→ Inode #12345
进程B: file2 { pos=100 } ─┘
```

---

## 🔧 操作表（Operations）

VFS使用函数指针表实现多态，不同文件系统提供不同实现。

### 1. Superblock Operations

```c
struct superblock_operations {
    int (*mount)(struct superblock *sb, void *data);
    int (*unmount)(struct superblock *sb);
    struct inode* (*alloc_inode)(struct superblock *sb);
    void (*free_inode)(struct inode *inode);
    int (*sync)(struct superblock *sb);
};
```

### 2. Inode Operations

```c
struct inode_operations {
    struct dentry* (*lookup)(struct inode *dir, const char *name);
    int (*create)(struct inode *dir, struct dentry *dentry, uint32_t mode);
    int (*mkdir)(struct inode *dir, struct dentry *dentry, uint32_t mode);
    int (*rmdir)(struct inode *dir, struct dentry *dentry);
    int (*unlink)(struct inode *dir, struct dentry *dentry);
    int (*rename)(struct inode *old_dir, struct dentry *old_dentry,
                  struct inode *new_dir, struct dentry *new_dentry);
};
```

### 3. File Operations

```c
struct file_operations {
    int (*open)(struct inode *inode, struct file *file);
    int (*close)(struct file *file);
    ssize_t (*read)(struct file *file, char *buf, size_t count);
    ssize_t (*write)(struct file *file, const char *buf, size_t count);
    int (*seek)(struct file *file, off_t offset, int whence);
    int (*ioctl)(struct file *file, uint32_t cmd, uint32_t arg);
};
```

**多态示例**：

```c
/* 不同文件系统提供不同的read实现 */

// Ext2文件系统
ssize_t ext2_read(struct file *file, char *buf, size_t count) {
    /* 从磁盘块读取数据 */
}

// 设备驱动
ssize_t keyboard_read(struct file *file, char *buf, size_t count) {
    /* 从键盘缓冲区读取 */
}

// procfs
ssize_t proc_read(struct file *file, char *buf, size_t count) {
    /* 生成动态信息 */
}

// VFS统一调用
ssize_t vfs_read(struct file *file, char *buf, size_t count) {
    return file->ops->read(file, buf, count);  // 多态！
}
```

---

## 📖 VFS工作流程

### 示例：打开并读取文件

```c
int fd = open("/home/user/file.txt", O_RDONLY);
read(fd, buffer, 100);
```

#### 步骤详解：

**1. 路径解析（Path Resolution）**

```
/home/user/file.txt

开始：当前目录 = 根目录dentry
│
├─ 查找"home"
│  - 在根目录inode中查找名为"home"的dentry
│  - 找到dentry，获取其inode
│
├─ 查找"user"
│  - 在"home"的inode中查找名为"user"的dentry
│  - 找到dentry，获取其inode
│
└─ 查找"file.txt"
   - 在"user"的inode中查找名为"file.txt"的dentry
   - 找到dentry，获取其inode
   
结果：得到file.txt的inode
```

**2. 分配文件描述符**

```c
struct file *file = kmalloc(sizeof(struct file));
file->inode = found_inode;
file->dentry = found_dentry;
file->flags = O_RDONLY;
file->pos = 0;
file->ops = found_inode->fops;

int fd = allocate_fd();  // 假设得到3
current_process->fd_table[fd] = file;

返回3给用户空间
```

**3. 执行读操作**

```c
read(3, buffer, 100);

内核：
1. 根据fd=3，从进程的fd_table获取file对象
2. 调用file->ops->read(file, buffer, 100)
3. 具体文件系统执行读取逻辑
4. 返回读取的字节数
```

---

## 🗂️ 文件类型

VFS支持多种文件类型：

```c
#define S_IFREG  0x8000   /* 普通文件 */
#define S_IFDIR  0x4000   /* 目录 */
#define S_IFCHR  0x2000   /* 字符设备 */
#define S_IFBLK  0x6000   /* 块设备 */
#define S_IFIFO  0x1000   /* FIFO（命名管道） */
#define S_IFLNK  0xA000   /* 符号链接 */
#define S_IFSOCK 0xC000   /* Socket */

/* 检查文件类型的宏 */
#define S_ISREG(m)  (((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m)  (((m) & S_IFMT) == S_IFDIR)
#define S_ISCHR(m)  (((m) & S_IFMT) == S_IFCHR)
```

---

## 🔐 权限系统

### 权限位

```
mode = 0755 (八进制)

0   7   5   5
│   │   │   │
│   │   │   └─→ 其他用户：r-x (101 = 5)
│   │   └─────→ 组用户：r-x (101 = 5)
│   └─────────→ 所有者：rwx (111 = 7)
└─────────────→ 特殊位

权限位：
r (read)    = 4
w (write)   = 2
x (execute) = 1
```

### 权限检查

```c
int vfs_check_permission(struct inode *inode, int mask)
{
    uint32_t mode = inode->mode;
    uint32_t uid = current_process->uid;
    uint32_t gid = current_process->gid;
    
    /* 超级用户有所有权限 */
    if (uid == 0) {
        return 0;  // 允许
    }
    
    /* 检查所有者权限 */
    if (uid == inode->uid) {
        return (mode & (mask << 6)) == (mask << 6) ? 0 : -EACCES;
    }
    
    /* 检查组权限 */
    if (gid == inode->gid) {
        return (mode & (mask << 3)) == (mask << 3) ? 0 : -EACCES;
    }
    
    /* 检查其他用户权限 */
    return (mode & mask) == mask ? 0 : -EACCES;
}
```

---

## 📁 目录结构

### 目录是特殊的文件

```
目录本质上是一个特殊的文件，存储（文件名 → inode编号）的映射

目录内容示例：
┌──────────┬────────┐
│ 文件名   │ inode  │
├──────────┼────────┤
│ .        │ 12345  │  (当前目录)
│ ..       │ 10000  │  (父目录)
│ file1.txt│ 20001  │
│ file2.txt│ 20002  │
│ subdir   │ 20003  │
└──────────┴────────┘

目录的inode类型为S_IFDIR
读取目录返回目录项列表
```

### 目录操作

```c
/* 在目录中查找文件 */
struct dentry *dir_lookup(struct inode *dir, const char *name)
{
    // 读取目录内容
    // 查找匹配的文件名
    // 返回对应的dentry
}

/* 在目录中创建文件 */
int dir_create(struct inode *dir, const char *name, uint32_t mode)
{
    // 分配新的inode
    // 在目录中添加（name → inode）条目
    // 更新目录的mtime
}
```

---

## 🔗 硬链接与符号链接

### 硬链接（Hard Link）

```
硬链接：多个文件名指向同一个inode

创建：ln file1.txt file2.txt

┌─────────┐
│ file1.txt│──┐
└─────────┘  │
             ├→ Inode #12345 { nlinks=2 }
┌─────────┐  │
│ file2.txt│──┘
└─────────┘

特点：
- 删除一个文件名不影响另一个
- 只有当nlinks=0时才真正删除inode
- 不能跨文件系统
- 不能链接目录（防止环）
```

### 符号链接（Symbolic Link）

```
符号链接：文件内容是另一个文件的路径

创建：ln -s /home/user/file.txt link.txt

┌────────┐
│link.txt│ → Inode #99999
└────────┘     │
               type = S_IFLNK
               data = "/home/user/file.txt"

访问link.txt时：
1. 读取link.txt的内容："/home/user/file.txt"
2. 解析路径"/home/user/file.txt"
3. 访问目标文件

特点：
- 可以跨文件系统
- 可以链接目录
- 目标文件删除后，符号链接失效（悬空链接）
```

---

## 🚀 VFS实现要点

### 1. 缓存策略

```c
/* Dentry缓存（加速路径查找） */
struct dentry_cache {
    struct hash_table *table;
    struct list_head lru;
};

/* Inode缓存（减少磁盘访问） */
struct inode_cache {
    struct hash_table *table;
    struct list_head dirty;
};
```

### 2. 引用计数

```c
/* 防止正在使用的对象被释放 */
void dentry_get(struct dentry *dentry) {
    dentry->ref_count++;
}

void dentry_put(struct dentry *dentry) {
    if (--dentry->ref_count == 0) {
        dentry_free(dentry);
    }
}
```

### 3. 同步机制

```c
/* 保护VFS数据结构的互斥访问 */
struct mutex {
    uint32_t locked;
    struct process *owner;
};

void mutex_lock(struct mutex *m);
void mutex_unlock(struct mutex *m);
```

---

## 📊 完整示例：实现简单文件系统

### RamFS（内存文件系统）

```c
/* Superblock操作 */
struct superblock_operations ramfs_sb_ops = {
    .alloc_inode = ramfs_alloc_inode,
    .free_inode = ramfs_free_inode,
};

/* Inode操作 */
struct inode_operations ramfs_inode_ops = {
    .lookup = ramfs_lookup,
    .create = ramfs_create,
    .mkdir = ramfs_mkdir,
};

/* File操作 */
struct file_operations ramfs_file_ops = {
    .open = ramfs_open,
    .read = ramfs_read,
    .write = ramfs_write,
    .close = ramfs_close,
};

/* 读取实现 */
ssize_t ramfs_read(struct file *file, char *buf, size_t count)
{
    struct inode *inode = file->inode;
    char *data = (char*)inode->private_data;  // RAM中的数据
    
    size_t remaining = inode->size - file->pos;
    size_t to_read = (count < remaining) ? count : remaining;
    
    memcpy(buf, data + file->pos, to_read);
    file->pos += to_read;
    
    return to_read;
}
```

---

## ✅ 实现检查清单

- [ ] Superblock结构和操作
- [ ] Inode结构和操作
- [ ] Dentry结构和缓存
- [ ] File结构和文件描述符表
- [ ] 路径解析算法
- [ ] 文件操作接口（open/read/write/close）
- [ ] 目录操作接口（mkdir/rmdir/readdir）
- [ ] 权限检查
- [ ] 硬链接和符号链接
- [ ] 引用计数管理
- [ ] 错误处理

---

## 🎓 学习资源

1. **《Linux内核设计与实现》** - Robert Love，VFS章节
2. **《深入理解Linux内核》** - VFS架构
3. **Linux内核源码** - fs/ 目录

---

**VFS是现代操作系统最优雅的设计之一！** 🎉

