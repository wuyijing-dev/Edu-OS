/*
 * link.h - 链接系统调用头文件
 * 对标Linux include/linux/fs.h中的链接相关定义
 */

#ifndef _LINK_H
#define _LINK_H

#include <sys/types.h>
#include <fs/vfs.h>

/* 链接类型 */
#define LINK_HARD   0   /* 硬链接 */
#define LINK_SYM    1   /* 符号链接 */

/* 链接系统调用 */
int sys_link(const char *oldpath, const char *newpath);
int sys_symlink(const char *target, const char *linkpath);
int sys_readlink(const char *pathname, char *buf, int bufsiz);
int sys_unlink(const char *pathname);

/* 内部链接函数 */
int vfs_link(struct dentry *old_dentry, struct inode *dir, struct dentry *new_dentry);
int vfs_symlink(struct inode *dir, struct dentry *dentry, const char *oldname);
int vfs_readlink(struct dentry *dentry, char *buffer, int buflen);
int vfs_unlink(struct inode *dir, struct dentry *dentry);

/* 路径解析辅助函数 */
struct path {
    struct vfsmount *mnt;
    struct dentry *dentry;
};

int user_path_at(int dfd, const char __user *name, unsigned flags, struct path *path);
void path_put(struct path *path);

#endif /* _LINK_H */