# EduOS引导调试配置

# 连接到QEMU
target remote localhost:1234

# 设置16位实模式
set architecture i8086

# Intel汇编语法
set disassembly-flavor intel

# 在引导扇区开始处设置断点
break *0x7C00

# 自定义命令
define hook-stop
    x/8i $pc
    info registers
end

echo \n========================================\n
echo  EduOS Boot Debugger\n
echo ========================================\n
echo 已连接到QEMU\n
echo 断点设置在0x7C00\n
echo 使用 'c' 继续，'si' 单步执行\n
echo ========================================\n\n

