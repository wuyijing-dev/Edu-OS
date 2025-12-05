/*
 * chmod.c - 文件权限系统调用实现
 * 对标Linux fs/open.c中的权限管理
 */

#include <fs/vfs.h>
#include <fs/inode.h>
#include <fs/dcache.h>
#include <errno.h>
#include <kernel.h>

/* chmod系统调用 */
int sys_chmod(const char *pathname, mode_t mode)
{
    struct path path;
    struct inode *inode;
    int error;
    
    /* 解析路径 */
    error = user_path_at(AT_FDCWD, pathname, LOOKUP_FOLLOW, &path);
    if (error) {
        return error;
    }
    
    inode = path.dentry->d_inode;
    if (!inode) {
        error = -ENOENT;
        goto out;
    }
    
    /* 检查权限 */
    error = inode_permission(inode, MAY_WRITE | MAY_EXEC);
    if (error) {
        goto out;
    }
    
    /* 检查文件系统是否只读 */
    error = mnt_want_write(path.mnt);
    if (error) {
        goto out;
    }
    
    /* 加锁保护 */
    mutex_lock(&inode->i_mutex);
    
    /* 设置新的权限 */
    mode = (mode & S_IALLUGO) | (inode->i_mode & ~S_IALLUGO);
    error = chmod_common(inode, mode);
    
    mutex_unlock(&inode->i_mutex);
    mnt_drop_write(path.mnt);
out:
    path_put(&path);
    return error;
}

/* fchmod系统调用 */
int sys_fchmod(int fd, mode_t mode)
{
    struct file *file;
    struct inode *inode;
    int error;
    
    /* 获取文件结构 */
    file = fget(fd);
    if (!file) {
        return -EBADF;
    }
    
    inode = file->f_path.dentry->d_inode;
    if (!inode) {
        error = -ENOENT;
        goto out_fput;
    }
    
    /* 检查权限 */
    error = inode_permission(inode, MAY_WRITE | MAY_EXEC);
    if (error) {
        goto out_fput;
    }
    
    /* 检查文件系统是否只读 */
    error = mnt_want_write(file->f_path.mnt);
    if (error) {
        goto out_fput;
    }
    
    /* 加锁保护 */
    mutex_lock(&inode->i_mutex);
    
    /* 设置新的权限 */
    mode = (mode & S_IALLUGO) | (inode->i_mode & ~S_IALLUGO);
    error = chmod_common(inode, mode);
    
    mutex_unlock(&inode->i_mutex);
    mnt_drop_write(file->f_path.mnt);
out_fput:
    fput(file);
    return error;
}

/* 通用chmod实现 */
static int chmod_common(struct inode *inode, mode_t mode)
{
    /* 清除setuid和setgid位（安全考虑） */
    mode &= ~(S_ISUID | S_ISGID);
    
    /* 更新inode权限 */
    inode->i_mode = mode;
    inode->i_ctime = CURRENT_TIME;
    mark_inode_dirty(inode);
    
    return 0;
}

/* chown系统调用 */
int sys_chown(const char *pathname, uid_t owner, gid_t group)
{
    struct path path;
    struct inode *inode;
    int error;
    
    /* 解析路径 */
    error = user_path_at(AT_FDCWD, pathname, LOOKUP_FOLLOW, &path);
    if (error) {
        return error;
    }
    
    inode = path.dentry->d_inode;
    if (!inode) {
        error = -ENOENT;
        goto out;
    }
    
    /* 检查权限 */
    error = chown_common(inode, owner, group);
    if (!error) {
        /* 更新ctime */
        inode->i_ctime = CURRENT_TIME;
        mark_inode_dirty(inode);
    }

out:
    path_put(&path);
    return error;
}

/* fchown系统调用 */
int sys_fchown(int fd, uid_t owner, gid_t group)
{
    struct file *file;
    struct inode *inode;
    int error;
    
    /* 获取文件结构 */
    file = fget(fd);
    if (!file) {
        return -EBADF;
    }
    
    inode = file->f_path.dentry->d_inode;
    if (!inode) {
        error = -ENOENT;
        goto out_fput;
    }
    
    /* 检查权限 */
    error = chown_common(inode, owner, group);
    if (!error) {
        /* 更新ctime */
        inode->i_ctime = CURRENT_TIME;
        mark_inode_dirty(inode);
    }

out_fput:
    fput(file);
    return error;
}

/* lchown系统调用 */
int sys_lchown(const char *pathname, uid_t owner, gid_t group)
{
    struct path path;
    struct inode *inode;
    int error;
    
    /* 解析路径（不跟随符号链接） */
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
    error = chown_common(inode, owner, group);
    if (!error) {
        /* 更新ctime */
        inode->i_ctime = CURRENT_TIME;
        mark_inode_dirty(inode);
    }

out:
    path_put(&path);
    return error;
}

/* 通用chown实现 */
static int chown_common(struct inode *inode, uid_t owner, gid_t group)
{
    uid_t old_uid = inode->i_uid;
    gid_t old_gid = inode->i_gid;
    
    /* 检查是否有权限修改所有者 */
    if (!capable(CAP_CHOWN)) {
        /* 普通用户只能修改自己拥有的文件 */
        if (old_uid != current->uid) {
            return -EPERM;
        }
        
        /* 只能将文件交给自己的组 */
        if (group != (gid_t)-1 && !in_group_p(group)) {
            return -EPERM;
        }
    }
    
    /* 更新UID */
    if (owner != (uid_t)-1) {
        inode->i_uid = owner;
    }
    
    /* 更新GID */
    if (group != (gid_t)-1) {
        inode->i_gid = group;
    }
    
    /* 清除setuid和setgid位（安全考虑） */
    if ((old_uid != inode->i_uid) || (old_gid != inode->i_gid)) {
        inode->i_mode &= ~(S_ISUID | S_ISGID);
    }
    
    return 0;
}

/* access系统调用 */
int sys_access(const char *pathname, int mode)
{
    struct path path;
    struct inode *inode;
    int error;
    
    /* 解析路径 */
    error = user_path_at(AT_FDCWD, pathname, LOOKUP_FOLLOW, &path);
    if (error) {
        return error;
    }
    
    inode = path.dentry->d_inode;
    if (!inode) {
        error = -ENOENT;
        goto out;
    }
    
    /* 检查访问权限 */
    error = inode_permission(inode, mode | MAY_ACCESS);

out:
    path_put(&path);
    return error;
}

/* faccessat系统调用 */
int sys_faccessat(int dfd, const char *pathname, int mode)
{
    struct path path;
    struct inode *inode;
    int error;
    
    /* 解析路径 */
    error = user_path_at(dfd, pathname, LOOKUP_FOLLOW, &path);
    if (error) {
        return error;
    }
    
    inode = path.dentry->d_inode;
    if (!inode) {
        error = -ENOENT;
        goto out;
    }
    
    /* 检查访问权限 */
    error = inode_permission(inode, mode | MAY_ACCESS);

out:
    path_put(&path);
    return error;
}