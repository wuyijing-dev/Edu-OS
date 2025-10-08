#!/bin/bash

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "========================================="
echo "     EduOS 开发环境检查工具"
echo "========================================="
echo ""

# 检查函数
check_command() {
    command=$1
    version_flag=$2
    
    if command -v $command &> /dev/null; then
        version=$($command $version_flag 2>&1 | head -n 1)
        echo -e "${GREEN}✓${NC} $command: $version"
        return 0
    else
        echo -e "${RED}✗${NC} $command: 未安装"
        return 1
    fi
}

# 检查工具
echo "检查必需工具..."
check_command gcc --version
check_command nasm --version
check_command ld --version
check_command gdb --version
check_command make --version
check_command git --version
check_command qemu-system-i386 --version

echo ""
echo "检查QEMU系统支持..."
if command -v qemu-system-i386 &> /dev/null; then
    echo -e "${GREEN}✓${NC} qemu-system-i386 可用"
fi
if command -v qemu-system-x86_64 &> /dev/null; then
    echo -e "${GREEN}✓${NC} qemu-system-x86_64 可用"
fi

echo ""
echo "检查gcc的32位支持..."
echo "int main() { return 0; }" > /tmp/test.c
if gcc -m32 /tmp/test.c -o /tmp/test 2>/dev/null; then
    echo -e "${GREEN}✓${NC} gcc支持32位编译"
    rm -f /tmp/test /tmp/test.c
else
    echo -e "${RED}✗${NC} gcc不支持32位编译"
    echo -e "${YELLOW}  提示: 运行 sudo apt install gcc-multilib${NC}"
fi

echo ""
echo "检查系统架构..."
echo "  架构: $(uname -m)"
echo "  内核: $(uname -r)"
echo "  系统: $(uname -s)"

echo ""
echo "========================================="
echo "环境检查完成！"
echo "========================================="

