#!/bin/bash
# Debian 包构建脚本

set -e

echo "=========================================="
echo "  LZ Disk Cleaner - Debian Package Build"
echo "=========================================="
echo ""

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# 检查必要的工具
echo -e "${YELLOW}[1/6] 检查构建工具...${NC}"
MISSING_TOOLS=""

if ! command -v dpkg-buildpackage &> /dev/null; then
    MISSING_TOOLS="$MISSING_TOOLS dpkg-dev"
fi

if ! command -v fakeroot &> /dev/null; then
    MISSING_TOOLS="$MISSING_TOOLS fakeroot"
fi

if ! command -v debhelper &> /dev/null; then
    MISSING_TOOLS="$MISSING_TOOLS debhelper"
fi

if [ -n "$MISSING_TOOLS" ]; then
    echo -e "${RED}缺少构建工具: $MISSING_TOOLS${NC}"
    echo "请运行: sudo apt install -y build-essential dpkg-dev fakeroot debhelper"
    exit 1
fi

echo -e "${GREEN}✓ 构建工具检查通过${NC}"
echo ""

# 检查构建依赖
echo -e "${YELLOW}[2/6] 检查构建依赖...${NC}"
if ! dpkg-checkbuilddeps 2>/dev/null; then
    echo -e "${YELLOW}缺少构建依赖，尝试安装...${NC}"
    sudo apt-get build-dep -y . || {
        echo -e "${RED}无法自动安装依赖，请手动安装${NC}"
        echo "运行: sudo apt-get install cmake qt6-base-dev qt6-svg-dev g++"
        exit 1
    }
fi
echo -e "${GREEN}✓ 构建依赖检查通过${NC}"
echo ""

# 清理旧的构建文件
echo -e "${YELLOW}[3/6] 清理旧的构建文件...${NC}"
rm -rf build
rm -f ../*.deb ../*.changes ../*.buildinfo ../*.dsc
echo -e "${GREEN}✓ 清理完成${NC}"
echo ""

# 构建包
echo -e "${YELLOW}[4/6] 开始构建 Debian 包...${NC}"
dpkg-buildpackage -us -uc -b -j$(nproc) 2>&1 | tee /tmp/lz-disk-cleaner-build.log

if [ ${PIPESTATUS[0]} -ne 0 ]; then
    echo -e "${RED}构建失败！查看日志: /tmp/lz-disk-cleaner-build.log${NC}"
    exit 1
fi

echo -e "${GREEN}✓ 构建完成${NC}"
echo ""

# 检查生成的包
echo -e "${YELLOW}[5/6] 检查生成的包...${NC}"
DEB_FILE=$(ls ../*.deb 2>/dev/null | head -1)

if [ -z "$DEB_FILE" ]; then
    echo -e "${RED}未找到生成的 .deb 文件${NC}"
    exit 1
fi

echo -e "${GREEN}✓ 找到包文件: $DEB_FILE${NC}"
dpkg-deb -I "$DEB_FILE" | head -20
echo ""

# 显示包信息
echo -e "${YELLOW}[6/6] 包信息${NC}"
ls -lh ../*.deb 2>/dev/null
echo ""

echo -e "${GREEN}=========================================="
echo -e "  构建成功！"
echo -e "==========================================${NC}"
echo ""
echo "生成的文件:"
ls -1 ../*.deb 2>/dev/null
echo ""
echo "安装命令:"
echo "  sudo dpkg -i $DEB_FILE"
echo ""
echo "或者:"
echo "  sudo apt install $DEB_FILE"
