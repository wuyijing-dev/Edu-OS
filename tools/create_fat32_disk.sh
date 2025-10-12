#!/bin/bash
# 创建 FAT32 测试磁盘镜像并编译用户程序

DISK_IMAGE="build/fat32_test.img"
DISK_SIZE_MB=128
MOUNT_POINT="/tmp/eduos_fat32_mount"

echo "=========================================="
echo " 创建 FAT32 测试磁盘 + 用户程序"
echo "=========================================="

# 创建构建目录
mkdir -p build
mkdir -p user

# 0. 编译用户程序
echo "[0/6] 编译用户程序..."

# 编译 hello.c 为目标文件
gcc -m32 -ffreestanding -nostdlib -nostdinc \
    -fno-builtin -fno-stack-protector \
    -fno-pic -fno-pie -O0 -g \
    -c user/hello.c -o build/hello.o

if [ $? -ne 0 ]; then
    echo "错误：编译 hello.c 失败"
    exit 1
fi

# 链接为 ELF 可执行文件
ld -m elf_i386 -nostdlib \
   -Ttext=0x08000000 \
   -e _start \
   build/hello.o -o build/hello.elf

if [ $? -ne 0 ]; then
    echo "错误：链接 hello.elf 失败"
    exit 1
fi

echo "   ✓ hello.elf 编译成功"
readelf -h build/hello.elf | grep "Entry point"

# 1. 创建空磁盘镜像
echo "[1/6] 创建 ${DISK_SIZE_MB}MB 磁盘镜像..."
dd if=/dev/zero of=$DISK_IMAGE bs=1M count=$DISK_SIZE_MB 2>/dev/null

# 2. 格式化为 FAT32
echo "[2/6] 格式化为 FAT32..."
mkfs.vfat -F 32 -n "EDUOS_TEST" $DISK_IMAGE >/dev/null 2>&1

# 3. 挂载磁盘
echo "[3/6] 挂载磁盘到 $MOUNT_POINT..."
sudo mkdir -p $MOUNT_POINT
sudo mount -o loop $DISK_IMAGE $MOUNT_POINT

# 4. 复制用户程序
echo "[4/6] 复制用户程序到磁盘..."
sudo mkdir -p $MOUNT_POINT/bin
sudo cp build/hello.elf $MOUNT_POINT/hello.elf
sudo cp build/hello.elf $MOUNT_POINT/bin/hello
echo "   ✓ hello.elf 已复制"

# 5. 创建测试文件
echo "[5/6] 创建测试文件..."
sudo bash -c "echo 'Hello from EduOS!' > $MOUNT_POINT/test.txt"
sudo bash -c "echo 'FAT32 filesystem works!' > $MOUNT_POINT/readme.txt"
sudo mkdir -p $MOUNT_POINT/testdir
sudo bash -c "echo 'File in directory' > $MOUNT_POINT/testdir/file.txt"

# 列出文件
echo ""
echo "磁盘内容："
ls -lh $MOUNT_POINT/

# 6. 卸载磁盘
echo ""
echo "[6/6] 卸载磁盘..."
sudo umount $MOUNT_POINT
sudo rmdir $MOUNT_POINT

echo ""
echo "=========================================="
echo " FAT32 磁盘创建成功！"
echo "=========================================="
echo "位置: $DISK_IMAGE"
echo "大小: ${DISK_SIZE_MB}MB"
echo "格式: FAT32"
echo ""
echo "文件列表："
echo "  - hello.elf (用户程序)"
echo "  - bin/hello (用户程序副本)"
echo "  - test.txt (测试文件)"
echo "  - readme.txt (说明文件)"
echo "  - testdir/ (测试目录)"
echo "    └── file.txt"
echo ""
echo "ELF 信息："
readelf -h build/hello.elf | grep -E "Class|Machine|Entry"
echo ""
echo "提示: 运行 'make run' 将自动挂载此磁盘"
echo "      EduOS 将尝试从 FAT32 加载 hello.elf"
echo "=========================================="

