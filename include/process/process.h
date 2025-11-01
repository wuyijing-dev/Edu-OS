/*
 * process.h - 进程管理
 * 
 * 定义进程控制块（PCB）和进程管理接口
 */

#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#include <stdbool.h>
#include <mm/vmm.h>

/* 前向声明 */
struct vma;

/* 进程状态 */
typedef enum {
    PROCESS_STATE_NEW = 0,        /* 新建 */
    PROCESS_STATE_READY,          /* 就绪 */
    PROCESS_STATE_RUNNING,        /* 运行 */
    PROCESS_STATE_BLOCKED,        /* 阻塞 */
    PROCESS_STATE_TERMINATED      /* 终止 */
} process_state_t;

/* 调度策略 */
typedef enum {
    SCHED_NORMAL = 0,             /* 普通分时调度（CFS风格） */
    SCHED_FIFO = 1,               /* 实时FIFO调度 */
    SCHED_RR = 2,                 /* 实时轮转调度 */
    SCHED_BATCH = 3,              /* 批处理调度 */
    SCHED_IDLE = 5,               /* 空闲调度 */
} sched_policy_t;

/* 调度实体（用于CFS） */
struct sched_entity {
    uint64_t vruntime;              /* 虚拟运行时间 */
    uint64_t exec_start;            /* 本次执行开始时间 */
    uint64_t sum_exec_runtime;      /* 累计执行时间 */
    int weight;                     /* 权重（基于nice值） */
};

/* CPU上下文（进程切换时保存的寄存器） */
struct cpu_context {
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp;       /* 栈指针 */
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
    uint32_t eip;       /* 指令指针 */
    uint32_t cs;        /* 代码段 */
    uint32_t eflags;    /* 标志寄存器 */
    uint32_t user_esp;  /* 用户态栈指针（切换特权级时使用） */
    uint32_t ss;        /* 栈段选择器（切换特权级时使用） */
} __attribute__((packed));

/* 前向声明 */
struct vfs_file;

/* 文件描述符表 */
#define MAX_FILES_PER_PROCESS  64
struct file_descriptor_table {
    struct vfs_file *files[MAX_FILES_PER_PROCESS];
    uint32_t count;  /* 打开的文件数 */
};

/* 进程控制块（PCB） */
struct process {
    /* 基本信息 */
    uint32_t pid;                   /* 进程ID */
    char name[32];                  /* 进程名 */
    process_state_t state;          /* 进程状态 */
    uint32_t priority;              /* 优先级（0-139，兼容旧代码） */
    
    /* CPU上下文 */
    struct cpu_context context;     /* 保存的寄存器 */
    
    /* Linux风格：抢占计数 */
    int preempt_count;              /* 0=可抢占, >0=禁止抢占 */
    
    /* 内存管理 */
    struct page_directory *page_dir;  /* 页目录 */
    uint32_t kernel_stack;          /* 内核栈地址 */
    uint32_t kernel_stack_size;     /* 内核栈大小 */
    struct vma *vma_list;           /* 虚拟内存区域链表（用于按需分配） */
    
    /* 文件管理（Linux风格：每个进程独立的FD表）*/
    struct file_descriptor_table *fd_table;  /* 文件描述符表 */
    
    /* 调度信息（基本） */
    uint32_t time_slice;            /* 时间片（tick数） */
    uint32_t time_used;             /* 已使用时间 */
    uint64_t total_runtime;         /* 总运行时间（统计） */
    
    /* 调度信息（高级）- 第6章新增 */
    sched_policy_t policy;          /* 调度策略 */
    int static_priority;            /* 静态优先级（100-139） */
    int dynamic_priority;           /* 动态优先级 */
    int nice;                       /* Nice值（-20到+19） */
    struct sched_entity se;         /* CFS调度实体 */
    uint32_t rt_priority;           /* 实时优先级（0-99） */
    uint32_t time_slice_remaining;  /* 剩余时间片 */
    uint64_t last_run;              /* 上次运行时间戳 */
    int mlfq_level;                 /* MLFQ队列级别 */
    uint64_t wait_time;             /* 等待开始时间（用于优先级老化） */
    uint32_t boost_count;           /* 优先级提升次数 */
    
    /* 链表节点 */
    struct process *next;           /* 下一个进程 */
    struct process *prev;           /* 上一个进程 */
    
    /* 父子关系 */
    struct process *parent;         /* 父进程 */
    
    /* 同步原语支持 */
    struct process *next_waiting;   /* 等待队列链表 */
    
    /* 其他信息 */
    uint32_t exit_code;             /* 退出码 */
    uint64_t start_time;            /* 启动时间（tick） */
};

/* 进程管理器接口 */
void process_init(void);
struct process *process_create_kernel_thread(const char *name, void (*entry)(void), uint32_t priority);
void process_destroy(struct process *proc);
void process_exit(uint32_t exit_code) __attribute__((noreturn));
struct process *process_get_current(void);
struct process *process_find_by_pid(pid_t pid);
void process_set_current(struct process *proc);
uint32_t process_allocate_pid(void);

/* 进程状态转换 */
void process_set_state(struct process *proc, process_state_t new_state);
const char *process_state_to_string(process_state_t state);

/* 统计信息 */
void process_print_info(struct process *proc);
void process_print_all(void);

#endif // PROCESS_H

