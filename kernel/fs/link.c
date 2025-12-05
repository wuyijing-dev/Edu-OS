/*
 * link.c - 链接系统调用实现
 * 对标Linux fs/namei.c中的链接实现
 */

#include <fs/link.h>
#include <fs/vfs.h>
#include <fs/dcache.h>
#include <fs/inode.h>
#include <fs/super.h>
#include <errno.h>
#include <string.h>
#include <kernel.h>

/* 硬链接系统调用 */
int sys_link(const char *oldpath, const char *newpath)
{
    struct path old_path, new_path;
    struct dentry *new_dentry;
    struct nameidata nd;
    int error;
    
    /* 解析源路径 */
    error = user_path_at(AT_FDCWD, oldpath, 0, &old_path);
    if (error) {
        return error;
    }
    
    /* 检查源文件是否存在 */
    error = -ENOENT;
    if (!old_path.dentry->d_inode) {
        goto out;
    }
    
    /* 检查源文件是否允许创建硬链接 */
    error = -EPERM;
    if (S_ISDIR(old_path.dentry->d_inode->i_mode)) {
        goto out;
    }
    
    /* 解析新路径的父目录 */
    error = path_lookup(newpath, LOOKUP_PARENT, &nd);
    if (error) {
        goto out;
    }
    
    new_dentry = lookup_create(&nd, 0);
    error = PTR_ERR(new_dentry);
    if (IS_ERR(new_dentry)) {
        goto out_unlock;
    }
    
    /* 执行链接操作 */
    error = vfs_link(old_path.dentry, nd.path.dentry->d_inode, new_dentry);
    
    dput(new_dentry);
out_unlock:
    mutex_unlock(&nd.path.dentry->d_inode->i_mutex);
    path_put(&nd.path);
out:
    path_put(&old_path);
    return error;
}

/* 符号链接系统调用 */
int sys_symlink(const char *target, const char *linkpath)
{
    struct path path;
    struct dentry *dentry;
    struct nameidata nd;
    int error;
    char *name;
    
    /* 复制目标路径 */
    name = getname(target);
    if (IS_ERR(name)) {
        return PTR_ERR(name);
    }
    
    /* 解析链接路径的父目录 */
    error = path_lookup(linkpath, LOOKUP_PARENT, &nd);
    if (error) {
        goto out_putname;
    }
    
    dentry = lookup_create(&nd, 0);
    error = PTR_ERR(dentry);
    if (IS_ERR(dentry)) {
        goto out_unlock;
    }
    
    /* 执行符号链接操作 */
    error = vfs_symlink(nd.path.dentry->d_inode, dentry, name);
    
    dput(dentry);
out_unlock:
    mutex_unlock(&nd.path.dentry->d_inode->i_mutex);
    path_put(&nd.path);
out_putname:
    putname(name);
    return error;
}

/* 读取符号链接系统调用 */
int sys_readlink(const char *pathname, char *buf, int bufsiz)
{
    struct path path;
    int error;
    char *page;
    
    /* 分配临时缓冲区 */
    page = (char *)__get_free_page(GFP_KERNEL);
    if (!page) {
        return -ENOMEM;
    }
    
    /* 解析符号链接路径 */
    error = user_path_at(AT_FDCWD, pathname, LOOKUP_FOLLOW, &path);
    if (error) {
        goto out_free_page;
    }
    
    /* 读取符号链接内容 */
    error = vfs_readlink(path.dentry, page, PAGE_SIZE);
    if (error < 0) {
        goto out_put_path;
    }
    
    /* 复制到用户空间 */
    if (error > bufsiz) {
        error = bufsiz;
    }
    
    if (copy_to_user(buf, page, error)) {
        error = -EFAULT;
    }

out_put_path:
    path_put(&path);
out_free_page:
    free_page((unsigned long)page);
    return error;
}

/* 删除链接系统调用 */
int sys_unlink(const char *pathname)
{
    struct path path;
    struct inode *inode;
    int error;
    
    /* 解析路径 */
    error = user_path_at(AT_FDCWD, pathname, 0, &path);
    if (error) {
        return error;
    }
    
    inode = path.dentry->d_inode;
    if (!inode) {
        error = -ENOENT;
        goto out;
    }
    
    /* 检查权限 */
    error = mnt_want_write(path.mnt);
    if (error) {
        goto out;
    }
    
    /* 执行删除操作 */
    error = vfs_unlink(path.dentry->d_parent->d_inode, path.dentry);
    
    mnt_drop_write(path.mnt);
out:
    path_put(&path);
    return error;
}

/* VFS链接操作 */
int vfs_link(struct dentry *old_dentry, struct inode *dir, struct dentry *new_dentry)
{
    struct inode *inode = old_dentry->d_inode;
    int error;
    
    /* 检查权限 */
    if (!inode || !dir->i_op || !dir->i_op->link) {
        return -EPERM;
    }
    
    error = may_create(dir, new_dentry);
    if (error) {
        return error;
    }
    
    /* 检查文件系统是否只读 */
    error = mnt_want_write(new_dentry->d_sb->s_mnt);
    if (error) {
        return error;
    }
    
    /* 加锁保护 */
    mutex_lock(&inode->i_mutex);
    mutex_lock(&dir->i_mutex);
    
    /* 检查源文件是否已被删除 */
    error = -ENOENT;
    if (!old_dentry->d_inode) {
        goto out_unlock;
    }
    
    /* 检查链接数限制 */
    error = -EMLINK;
    if (old_dentry->d_inode->i_nlink >= old_dentry->d_sb->s_max_links) {
        goto out_unlock;
    }
    
    /* 调用文件系统特定的link函数 */
    error = dir->i_op->link(old_dentry, dir, new_dentry);
    if (!error) {
        /* 更新inode链接计数 */
        old_dentry->d_inode->i_nlink++;
        old_dentry->d_inode->i_ctime = CURRENT_TIME;
        mark_inode_dirty(old_dentry->d_inode);
    }

out_unlock:
    mutex_unlock(&dir->i_mutex);
    mutex_unlock(&inode->i_mutex);
    mnt_drop_write(new_dentry->d_sb->s_mnt);
    return error;
}

/* VFS符号链接操作 */
int vfs_symlink(struct inode *dir, struct dentry *dentry, const char *oldname)
{
    int error;
    
    /* 检查权限 */
    if (!dir->i_op || !dir->i_op->symlink) {
        return -EPERM;
    }
    
    error = may_create(dir, dentry);
    if (error) {
        return error;
    }
    
    /* 检查文件系统是否只读 */
    error = mnt_want_write(dir->i_sb->s_mnt);
    if (error) {
        return error;
    }
    
    /* 加锁保护 */
    mutex_lock(&dir->i_mutex);
    
    /* 调用文件系统特定的symlink函数 */
    error = dir->i_op->symlink(dir, dentry, oldname);
    
    mutex_unlock(&dir->i_mutex);
    mnt_drop_write(dir->i_sb->s_mnt);
    return error;
}

/* VFS读取符号链接操作 */
int vfs_readlink(struct dentry *dentry, char *buffer, int buflen)
{
    struct inode *inode = dentry->d_inode;
    int error;
    
    /* 检查是否为符号链接 */
    if (!inode || !S_ISLNK(inode->i_mode)) {
        return -EINVAL;
    }
    
    /* 检查权限 */
    if (!inode->i_op || !inode->i_op->readlink) {
        return -EPERM;
    }
    
    /* 调用文件系统特定的readlink函数 */
    error = inode->i_op->readlink(dentry, buffer, buflen);
    
    return error;
}

/* VFS删除链接操作 */
int vfs_unlink(struct inode *dir, struct dentry *dentry)
{
    struct inode *inode = dentry->d_inode;
    int error;
    
    /* 检查权限 */
    if (!dir->i_op || !dir->i_op->unlink) {
        return -EPERM;
    }
    
    /* 检查文件系统是否只读 */
    error = mnt_want_write(dir->i_sb->s_mnt);
    if (error) {
        return error;
    }
    
    /* 加锁保护 */
    mutex_lock(&dir->i_mutex);
    mutex_lock(&inode->i_mutex);
    
    /* 检查文件是否已被删除 */
    error = -ENOENT;
    if (!dentry->d_inode) {
        goto out_unlock;
    }
    
    /* 检查是否为目录 */
    error = -EISDIR;
    if (S_ISDIR(inode->i_mode)) {
        goto out_unlock;
    }
    
    /* 调用文件系统特定的unlink函数 */
    error = dir->i_op->unlink(dir, dentry);
    if (!error) {
        /* 更新inode链接计数 */
        inode->i_nlink--;
        inode->i_ctime = dir->i_ctime;
        mark_inode_dirty(inode);
    }

out_unlock:
    mutex_unlock(&inode->i_mutex);
    mutex_unlock(&dir->i_mutex);
    mnt_drop_write(dir->i_sb->s_mnt);
    return error;
}