#!/bin/bash
# 创建 FAT32 测试磁盘镜像并编译用户程序（交互式）

# 获取脚本所在目录
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

# 切换到项目根目录
cd "$PROJECT_DIR"

DISK_IMAGE="build/fat32_test.img"
DISK_SIZE_MB=128
MOUNT_POINT="/tmp/eduos_fat32_mount"

echo "=========================================="
echo " 创建 FAT32 测试磁盘 + 用户程序"
echo "=========================================="
echo "Working directory: $(pwd)"
echo ""

# 创建构建目录
mkdir -p build
mkdir -p user

# 0. 查找所有可用的 ELF 文件
echo "[Step 1] 查找可用的 ELF 文件..."
echo ""

# 扫描 build/ 和 user/ 目录下的 .elf 文件
ELF_FILES=()
ELF_NAMES=()

# 查找 build/ 目录
if [ -d "build" ]; then
    while IFS= read -r -d '' file; do
        if [ -f "$file" ] && file "$file" | grep -q "ELF.*executable"; then
            ELF_FILES+=("$file")
            ELF_NAMES+=("$(basename "$file")")
        fi
    done < <(find build -name "*.elf" -print0 2>/dev/null)
fi

# 查找 user/ 目录
if [ -d "user" ]; then
    while IFS= read -r -d '' file; do
        if [ -f "$file" ] && file "$file" | grep -q "ELF.*executable"; then
            ELF_FILES+=("$file")
            ELF_NAMES+=("$(basename "$file")")
        fi
    done < <(find user -name "*.elf" -print0 2>/dev/null)
fi

# 如果没有找到ELF文件，编译默认的 hello.elf
if [ ${#ELF_FILES[@]} -eq 0 ]; then
    echo "未找到 ELF 文件，编译默认的 hello.elf..."
    
    gcc -m32 -ffreestanding -nostdlib -nostdinc \
        -fno-builtin -fno-stack-protector \
        -fno-pic -fno-pie -O0 -g \
        -c user/hello.c -o build/hello.o
    
    if [ $? -ne 0 ]; then
        echo "错误：编译 hello.c 失败"
        exit 1
    fi
    
    ld -m elf_i386 -nostdlib \
       -Ttext=0x08000000 \
       -e _start \
       build/hello.o -o build/hello.elf
    
    if [ $? -ne 0 ]; then
        echo "错误：链接 hello.elf 失败"
        exit 1
    fi
    
    ELF_FILES=("build/hello.elf")
    ELF_NAMES=("hello.elf")
    echo "   ✓ hello.elf 编译成功"
fi

# 显示找到的 ELF 文件
echo "找到 ${#ELF_FILES[@]} 个 ELF 文件："
echo ""
for i in "${!ELF_FILES[@]}"; do
    size=$(ls -lh "${ELF_FILES[$i]}" | awk '{print $5}')
    entry=$(readelf -h "${ELF_FILES[$i]}" 2>/dev/null | grep "Entry point" | awk '{print $4}')
    printf "  [%d] %-20s  (Size: %s, Entry: %s)\n" "$((i+1))" "${ELF_NAMES[$i]}" "$size" "$entry"
done

echo ""
echo "=========================================="
echo " 选择要包含到磁盘的 ELF 文件"
echo "=========================================="
echo ""
echo "输入选项："
echo "  - 输入单个数字 (如: 1)"
echo "  - 输入多个数字用空格分隔 (如: 1 3 5)"
echo "  - 输入 'all' 包含所有文件"
echo "  - 按 Enter 跳过此步骤"
echo ""
read -p "请选择: " selection

# 解析选择
SELECTED_FILES=()
SELECTED_NAMES=()

if [ -z "$selection" ]; then
    echo "未选择文件，使用第一个 ELF"
    SELECTED_FILES=("${ELF_FILES[0]}")
    SELECTED_NAMES=("${ELF_NAMES[0]}")
elif [ "$selection" = "all" ]; then
    echo "选择了所有文件"
    SELECTED_FILES=("${ELF_FILES[@]}")
    SELECTED_NAMES=("${ELF_NAMES[@]}")
else
    # 解析数字
    for num in $selection; do
        if [[ "$num" =~ ^[0-9]+$ ]]; then
            idx=$((num - 1))
            if [ $idx -ge 0 ] && [ $idx -lt ${#ELF_FILES[@]} ]; then
                SELECTED_FILES+=("${ELF_FILES[$idx]}")
                SELECTED_NAMES+=("${ELF_NAMES[$idx]}")
            else
                echo "警告: 无效的选择 $num"
            fi
        fi
    done
fi

if [ ${#SELECTED_FILES[@]} -eq 0 ]; then
    echo "错误: 没有选择有效的文件"
    exit 1
fi

echo ""
echo "已选择 ${#SELECTED_FILES[@]} 个文件："
for name in "${SELECTED_NAMES[@]}"; do
    echo "  ✓ $name"
done
echo ""

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

# 复制选中的所有 ELF 文件
for i in "${!SELECTED_FILES[@]}"; do
    src="${SELECTED_FILES[$i]}"
    name="${SELECTED_NAMES[$i]}"
    
    # 复制到根目录
    sudo cp "$src" "$MOUNT_POINT/$name"
    echo "   ✓ $name 已复制到根目录"
    
    # 第一个文件也复制到 /bin 并创建符号链接
    if [ $i -eq 0 ]; then
        sudo cp "$src" "$MOUNT_POINT/bin/${name%.elf}"
        sudo cp "$src" "$MOUNT_POINT/hello.elf"  # 默认文件
        echo "   ✓ $name 已复制为 /bin/${name%.elf}"
        echo "   ✓ $name 已复制为 /hello.elf (默认)"
    fi
done

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
echo "  用户程序："
for name in "${SELECTED_NAMES[@]}"; do
    echo "    - $name"
done
echo "  测试文件："
echo "    - test.txt"
echo "    - readme.txt"
echo "    - testdir/file.txt"
echo ""
echo "ELF 信息："
for i in "${!SELECTED_FILES[@]}"; do
    echo "  ${SELECTED_NAMES[$i]}:"
    readelf -h "${SELECTED_FILES[$i]}" 2>/dev/null | grep -E "Entry point address"
done
echo ""
echo "提示: 运行 'make run' 将自动挂载此磁盘"
echo "      EduOS 默认加载 /hello.elf"
echo "=========================================="

