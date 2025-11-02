#
# Kbuild - EduOS内核构建配置
# 类似Linux内核的Kbuild系统
#

# 编译器配置
ARCH := i386
CROSS_COMPILE :=
CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
AS := $(CROSS_COMPILE)as
AR := $(CROSS_COMPILE)ar

# 编译选项
CFLAGS_KERNEL := -m32 -nostdlib -nostdinc -fno-builtin -fno-stack-protector \
                 -Wall -Wextra -O2 -D__KERNEL__

# 包含路径
INCLUDEDIR := $(srctree)/include

# 内核对象目录定义（类似Linux的obj-y）
# 这些目录会被递归构建

# 核心子系统
core-y := kernel/ mm/ fs/ ipc/ net/

# 架构相关
core-y += arch/$(ARCH)/

# 驱动程序
drivers-y := drivers/

# 库
libs-y := lib/

# 所有需要构建的目录
vmlinux-dirs := $(patsubst %/,%,$(filter %/, $(core-y) $(drivers-y) $(libs-y)))

# 构建顺序
vmlinux-deps := $(vmlinux-dirs)

# 导出给子Makefile使用的变量
export ARCH CROSS_COMPILE CC LD AS AR
export CFLAGS_KERNEL INCLUDEDIR

