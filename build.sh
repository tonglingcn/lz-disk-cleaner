#!/bin/bash

# LZ Disk Cleaner - Build Script
# LZ 磁盘清理工具 - 编译脚本

set -e

echo "========================================"
echo "  LZ Disk Cleaner Build Script  "
echo "========================================"
echo ""

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 检查依赖
echo -e "${YELLOW}[1/5] 检查依赖...${NC}"

if ! command -v cmake &> /dev/null; then
    echo -e "${RED}错误: 未找到 cmake${NC}"
    echo "请安装: sudo apt install cmake"
    exit 1
fi

if ! command -v qmake6 &> /dev/null; then
    echo -e "${RED}错误: 未找到 qmake6 (Qt6)${NC}"
    echo "请安装: sudo apt install qt6-base-dev qt6-base-dev-tools qt6-charts-dev"
    exit 1
fi

echo -e "${GREEN}✓ 依赖检查通过${NC}"
echo ""

# 清理旧的构建文件
echo -e "${YELLOW}[2/5] 清理旧的构建文件...${NC}"
rm -rf build
mkdir -p build
cd build
echo -e "${GREEN}✓ 清理完成${NC}"
echo ""

# 配置项目
echo -e "${YELLOW}[3/5] 配置项目...${NC}"
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_STANDARD=17 \
    -DCMAKE_INSTALL_PREFIX=/usr/local

echo -e "${GREEN}✓ 配置完成${NC}"
echo ""

# 编译
echo -e "${YELLOW}[4/5] 编译项目...${NC}"
make -j$(nproc)
echo -e "${GREEN}✓ 编译完成${NC}"
echo ""

# 完成
echo -e "${YELLOW}[5/5] 构建完成！${NC}"
echo ""
echo "可执行文件位置: build/bin/lz-disk-cleaner"
echo ""
echo "运行程序:"
echo "  cd build"
echo "  ./bin/lz-disk-cleaner"
echo ""
echo "安装到系统:"
echo "  cd build"
echo "  sudo make install"
echo ""
echo -e "${GREEN}========================================"
echo -e "  ${GREEN}构建成功！${NC}"
echo -e "========================================"