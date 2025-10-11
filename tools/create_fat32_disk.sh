#!/bin/bash
# 创建 FAT32 测试磁盘镜像

DISK_IMAGE="build/fat32_test.img"
DISK_SIZE_MB=128  # 64MB 测试磁盘（FAT32 最小推荐大小）
MOUNT_POINT="/tmp/eduos_fat32_mount"

echo "=========================================="
echo " 创建 FAT32 测试磁盘镜像"
echo "=========================================="

# 创建构建目录
mkdir -p build

# 1. 创建空磁盘镜像
echo "[1/5] 创建 ${DISK_SIZE_MB}MB 磁盘镜像..."
dd if=/dev/zero of=$DISK_IMAGE bs=1M count=$DISK_SIZE_MB 2>/dev/null

# 2. 格式化为 FAT32
echo "[2/5] 格式化为 FAT32..."
mkfs.vfat -F 32 -n "EDUOS_TEST" $DISK_IMAGE >/dev/null 2>&1

# 3. 挂载磁盘
echo "[3/5] 挂载磁盘到 $MOUNT_POINT..."
sudo mkdir -p $MOUNT_POINT
sudo mount -o loop $DISK_IMAGE $MOUNT_POINT

# 4. 创建测试文件
echo "[4/5] 创建测试文件..."
sudo bash -c "echo 'Hello from EduOS!' > $MOUNT_POINT/test.txt"
sudo bash -c "echo 'FAT32 filesystem works!' > $MOUNT_POINT/readme.txt"
sudo mkdir -p $MOUNT_POINT/testdir
sudo bash -c "echo 'File in directory' > $MOUNT_POINT/testdir/file.txt"

# 列出文件
echo ""
echo "磁盘内容："
ls -lh $MOUNT_POINT/

# 5. 卸载磁盘
echo ""
echo "[5/5] 卸载磁盘..."
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
echo "  - test.txt (测试文件)"
echo "  - readme.txt (说明文件)"
echo "  - testdir/ (测试目录)"
echo "    └── file.txt"
echo ""
echo "提示: 运行 'make run' 将自动挂载此磁盘"
echo "=========================================="

