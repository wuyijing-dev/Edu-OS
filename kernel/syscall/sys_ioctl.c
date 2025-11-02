/*
 * sys_ioctl.c - POSIX ioctl系统调用实现
 * 
 * 设备控制接口
 */

#include <sys/ioctl.h>
#include <process/process.h>
#include <fs/vfs.h>
#include <kernel.h>

/* errno错误码 */
#define EBADF   9
#define EINVAL  22
#define ENOTTY  25
#define EFAULT  14

/*
 * sys_ioctl - 设备控制
 * 
 * @fd: 文件描述符
 * @request: 控制请求
 * @arg: 参数（可选）
 * 
 * 返回：取决于请求，失败返回-1
 */
int sys_ioctl(int fd, unsigned long request, unsigned long arg)
{
    extern struct process *process_get_current(void);
    struct process *current = process_get_current();
    
    if (!current || !current->fd_table) {
        extern int set_errno(int error_code);
        return set_errno(EBADF);
    }
    
    /* 检查fd有效性 */
    if (fd < 0 || fd >= MAX_FILES_PER_PROCESS || !current->fd_table->files[fd]) {
        extern int set_errno(int error_code);
        return set_errno(EBADF);
    }
    
    struct vfs_file *file = current->fd_table->files[fd];
    if (!file->inode) {
        extern int set_errno(int error_code);
        return set_errno(EBADF);
    }
    
    kprintf("[SYS_IOCTL] fd=%d, request=0x%lx, arg=0x%lx\n", fd, request, arg);
    
    /* 根据请求类型分发 */
    switch (request) {
        case FIONREAD: {
            /* 获取可读字节数 */
            int *count = (int*)arg;
            if (!count) {
                extern int set_errno(int error_code);
                return set_errno(EFAULT);
            }
            /* 简化实现：返回文件剩余大小 */
            *count = (file->inode->size > file->pos) ? 
                     (file->inode->size - file->pos) : 0;
            return 0;
        }
        
        case FIONBIO: {
            /* 设置非阻塞模式 */
            int *on = (int*)arg;
            if (!on) {
                extern int set_errno(int error_code);
                return set_errno(EFAULT);
            }
            if (*on) {
                file->flags |= 04000;  /* O_NONBLOCK */
            } else {
                file->flags &= ~04000;
            }
            kprintf("[SYS_IOCTL] Set O_NONBLOCK=%d for fd=%d\n", *on, fd);
            return 0;
        }
        
        case TIOCGWINSZ: {
            /* 获取终端窗口大小 */
            struct winsize *ws = (struct winsize*)arg;
            if (!ws) {
                extern int set_errno(int error_code);
                return set_errno(EFAULT);
            }
            /* 默认80x25终端 */
            ws->ws_row = 25;
            ws->ws_col = 80;
            ws->ws_xpixel = 0;
            ws->ws_ypixel = 0;
            return 0;
        }
        
        case FBIOGET_VSCREENINFO: {
            /* 获取帧缓冲可变信息 */
            struct fb_var_screeninfo *var = (struct fb_var_screeninfo*)arg;
            if (!var) {
                extern int set_errno(int error_code);
                return set_errno(EFAULT);
            }
            /* 假设1024x768x32 */
            var->xres = 1024;
            var->yres = 768;
            var->xres_virtual = 1024;
            var->yres_virtual = 768;
            var->xoffset = 0;
            var->yoffset = 0;
            var->bits_per_pixel = 32;
            var->grayscale = 0;
            kprintf("[SYS_IOCTL] FBIOGET_VSCREENINFO: %ux%u@%u\n",
                    var->xres, var->yres, var->bits_per_pixel);
            return 0;
        }
        
        case FBIOGET_FSCREENINFO: {
            /* 获取帧缓冲固定信息 */
            struct fb_fix_screeninfo *fix = (struct fb_fix_screeninfo*)arg;
            if (!fix) {
                extern int set_errno(int error_code);
                return set_errno(EFAULT);
            }
            /* 简化实现 */
            __builtin_memcpy(fix->id, "EduOS FB", 9);
            fix->smem_start = 0xE0000000;  /* BGA默认地址 */
            fix->smem_len = 1024 * 768 * 4;
            fix->type = 0;  /* FB_TYPE_PACKED_PIXELS */
            fix->line_length = 1024 * 4;
            return 0;
        }
        
        default:
            kprintf("[SYS_IOCTL] Unknown request: 0x%lx\n", request);
            extern int set_errno(int error_code);
            return set_errno(ENOTTY);  /* Not a typewriter (inappropriate ioctl) */
    }
}

