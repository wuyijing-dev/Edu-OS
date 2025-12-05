#
# kernel/process/Makefile - 进程管理子系统
#

# 编译到built-in.o中的对象文件
obj-y := process.o scheduler.o fd_table.o signal.o
obj-y += priority_sched.o mlfq_sched.o
obj-y += fork.o exec.o wait.o

# 子目录（如果有）
# obj-y += subdir/

obj-y := entry.o trap.o sched.o process.o wait.o clone.o thread.o vfork.o