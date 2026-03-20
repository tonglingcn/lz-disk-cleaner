#!/bin/bash

# 为 OBS (Open Build Service) 打包所有需要的文件
# 项目: lz-disk-cleaner
# 版本: 1.1.1
# 用途: 准备上传到 https://build.opensuse.org/ 的所有文件

set -e

VERSION="1.1.1"
PACKAGE_NAME="lz-disk-cleaner"
MAINTAINER="克亮 <315707022@qq.com>"
HOMEPAGE="https://github.com/tonglingcn/lz-disk-cleaner"

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${BLUE}  为 OBS 准备所有文件${NC}"
echo -e "${BLUE}  项目: ${PACKAGE_NAME}${NC}"
echo -e "${BLUE}  版本: ${VERSION}${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""

# 检查必要文件
echo -e "${YELLOW}检查必要文件...${NC}"
MISSING_FILES=0

check_file() {
    if [ ! -f "$1" ]; then
        echo -e "${RED}✗ 缺少文件: $1${NC}"
        MISSING_FILES=$((MISSING_FILES + 1))
    else
        echo -e "${GREEN}✓ $1${NC}"
    fi
}

check_file "CMakeLists.txt"
check_file "lz-disk-cleaner.spec"
check_file "README.md"
check_file "debian/changelog"
check_file "debian/control"
check_file "debian/copyright"
check_file "debian/lz-disk-cleaner.desktop"
check_file "debian/rules"

if [ $MISSING_FILES -gt 0 ]; then
    echo -e "${RED}错误: 缺少 $MISSING_FILES 个必要文件${NC}"
    exit 1
fi

echo ""

# 1. 创建源码 tarball
echo -e "${YELLOW}1. 创建源码包...${NC}"
TEMP_DIR=$(mktemp -d)
SOURCE_DIR="${TEMP_DIR}/${PACKAGE_NAME}-${VERSION}"

mkdir -p "${SOURCE_DIR}"

echo "   复制文件到临时目录..."
rsync -av \
    --exclude='.git' \
    --exclude='.gitignore' \
    --exclude='build' \
    --exclude='*.deb' \
    --exclude='*.rpm' \
    --exclude='*.tar.gz' \
    --exclude='*.dsc' \
    --exclude='.vscode' \
    --exclude='.kiro' \
    --exclude='*.o' \
    --exclude='*.so' \
    --exclude='moc_*' \
    --exclude='ui_*' \
    --exclude='obs_package' \
    --exclude='obs' \
    --exclude='obj-*' \
    --exclude='generate_obs_files.sh' \
    --exclude='package_for_obs.sh' \
    --exclude='.codebuddy' \
    ./ "${SOURCE_DIR}/" > /dev/null

echo "   创建 tar.gz 压缩包..."
cd "${TEMP_DIR}"
tar czf "${PACKAGE_NAME}-${VERSION}.tar.gz" "${PACKAGE_NAME}-${VERSION}"
mv "${PACKAGE_NAME}-${VERSION}.tar.gz" "${OLDPWD}/"
cd "${OLDPWD}"
rm -rf "${TEMP_DIR}"

SIZE=$(du -h "${PACKAGE_NAME}-${VERSION}.tar.gz" | cut -f1)
echo -e "${GREEN}   ✓ 已创建: ${PACKAGE_NAME}-${VERSION}.tar.gz (${SIZE})${NC}"

# 2. 打包 debian 目录
echo ""
echo -e "${YELLOW}2. 打包 debian 目录...${NC}"
tar czf debian.tar.gz debian/
SIZE=$(du -h debian.tar.gz | cut -f1)
echo -e "${GREEN}   ✓ 已创建: debian.tar.gz (${SIZE})${NC}"

# 3. 创建 .dsc 文件
echo ""
echo -e "${YELLOW}3. 创建 .dsc 文件...${NC}"

# 计算文件的 MD5 和大小
TAR_SIZE=$(stat -c%s "${PACKAGE_NAME}-${VERSION}.tar.gz" 2>/dev/null || stat -f%z "${PACKAGE_NAME}-${VERSION}.tar.gz")
TAR_MD5=$(md5sum "${PACKAGE_NAME}-${VERSION}.tar.gz" 2>/dev/null | cut -d' ' -f1 || md5 -q "${PACKAGE_NAME}-${VERSION}.tar.gz")
DEB_SIZE=$(stat -c%s "debian.tar.gz" 2>/dev/null || stat -f%z "debian.tar.gz")
DEB_MD5=$(md5sum "debian.tar.gz" 2>/dev/null | cut -d' ' -f1 || md5 -q "debian.tar.gz")

cat > "${PACKAGE_NAME}.dsc" << EOF
Format: 3.0 (quilt)
Source: ${PACKAGE_NAME}
Binary: ${PACKAGE_NAME}
Architecture: any
Version: ${VERSION}-1
Maintainer: ${MAINTAINER}
Homepage: ${HOMEPAGE}
Standards-Version: 4.6.0
Vcs-Browser: ${HOMEPAGE}
Vcs-Git: ${HOMEPAGE}.git
Build-Depends: debhelper-compat (= 13), cmake (>= 3.16), qt6-base-dev (>= 6.2), qt6-svg-dev (>= 6.2), libgl1-mesa-dev | libgl-dev, libegl1-mesa-dev | libegl-dev, pkg-config
Package-List:
 ${PACKAGE_NAME} deb utils optional arch=any
Checksums-Md5:
 ${TAR_MD5} ${TAR_SIZE} ${PACKAGE_NAME}-${VERSION}.tar.gz
 ${DEB_MD5} ${DEB_SIZE} debian.tar.gz
Files:
 ${TAR_MD5} ${TAR_SIZE} ${PACKAGE_NAME}-${VERSION}.tar.gz
 ${DEB_MD5} ${DEB_SIZE} debian.tar.gz
EOF

echo -e "${GREEN}   ✓ 已创建: ${PACKAGE_NAME}.dsc${NC}"

# 4. 验证 spec 文件
echo ""
echo -e "${YELLOW}4. 验证 spec 文件...${NC}"

if grep -q "Version:.*${VERSION}" lz-disk-cleaner.spec; then
    echo -e "${GREEN}   ✓ spec 文件版本正确: ${VERSION}${NC}"
else
    echo -e "${RED}   ✗ spec 文件版本不匹配${NC}"
    echo -e "${YELLOW}   当前版本应该是: ${VERSION}${NC}"
    exit 1
fi

# 检查 spec 文件语法
if command -v rpmlint >/dev/null 2>&1; then
    echo "   运行 rpmlint 检查..."
    if rpmlint lz-disk-cleaner.spec 2>&1 | grep -q "error"; then
        echo -e "${YELLOW}   ⚠️  发现一些警告，但可以继续${NC}"
    else
        echo -e "${GREEN}   ✓ spec 文件语法正确${NC}"
    fi
else
    echo -e "${YELLOW}   ⚠️  rpmlint 未安装，跳过语法检查${NC}"
fi

# 5. 创建 OBS 包目录
echo ""
echo -e "${YELLOW}5. 创建 OBS 包目录...${NC}"
OBS_DIR="obs_package"
rm -rf "${OBS_DIR}"
mkdir -p "${OBS_DIR}"

# 复制所有需要的文件
cp "${PACKAGE_NAME}-${VERSION}.tar.gz" "${OBS_DIR}/"
cp "debian.tar.gz" "${OBS_DIR}/"
cp "${PACKAGE_NAME}.dsc" "${OBS_DIR}/"
cp "${PACKAGE_NAME}.spec" "${OBS_DIR}/"

# 注意: _service 文件不复制，因为私有仓库 OBS 无法访问
# 如需使用 _service，请确保仓库为公开
# cp "_service" "${OBS_DIR}/"

echo -e "${GREEN}   ✓ 已创建目录: ${OBS_DIR}/${NC}"

# 6. 创建 README
echo ""
echo -e "${YELLOW}6. 创建上传说明...${NC}"

cat > "${OBS_DIR}/README.txt" << EOF
OBS 上传文件说明
================

这个目录包含了上传到 OBS (Open Build Service) 所需的所有文件。

文件列表:
---------
1. ${PACKAGE_NAME}-${VERSION}.tar.gz  - 源码包（必需）
2. ${PACKAGE_NAME}.spec              - RPM spec 文件（必需）
3. _service                          - OBS 服务配置文件（可选，用于自动获取源码）
4. debian.tar.gz                     - Debian 打包文件（可选，用于 Debian/Ubuntu）
5. ${PACKAGE_NAME}.dsc               - Debian 源码控制文件（可选，用于 Debian/Ubuntu）

上传步骤:
---------

方法 1: Web 界面（推荐）
1. 访问 https://build.opensuse.org/
2. 登录账号
3. 创建或进入项目（home:你的用户名）
4. 创建包 "${PACKAGE_NAME}"
5. 上传文件：
   - ${PACKAGE_NAME}-${VERSION}.tar.gz（必需）
   - ${PACKAGE_NAME}.spec（必需）
   - _service（可选，用于自动更新）
   - debian.tar.gz（可选）
   - ${PACKAGE_NAME}.dsc（可选）
6. 等待构建完成

方法 2: osc 命令行
1. 安装 osc: sudo apt-get install osc
2. 配置: osc -A https://api.opensuse.org
3. 检出: osc co home:你的用户名
4. 创建包: osc mkpac ${PACKAGE_NAME}
5. 复制文件: cp * ~/home:你的用户名/${PACKAGE_NAME}/
6. 添加: cd ~/home:你的用户名/${PACKAGE_NAME} && osc add *
7. 提交: osc commit -m "Update to ${VERSION}"

支持的发行版:
-------------
- openSUSE Tumbleweed
- openSUSE Leap 15.5, 15.6
- Fedora 39, 40
- Debian 11, 12
- Ubuntu 20.04, 22.04, 24.04
- Deepin V23

项目信息:
---------
- 名称: ${PACKAGE_NAME}
- 版本: ${VERSION}
- 主页: ${HOMEPAGE}
- 维护者: ${MAINTAINER}

功能特性:
---------
- 磁盘使用分析，支持多级目录扫描
- 智能清理和自定义清理模式
- APT 源管理（添加、编辑、启用/禁用、删除）
- 启动应用程序管理
- 文件粉碎机，支持安全删除
- 系统瘦身（大文件、重复文件查找）
- 硬件资源监控（CPU、内存、网络、GPU）
- 玲珑应用管理（Deepin）
- 不可变系统快照支持

更多信息:
---------
- OBS 官网: https://build.opensuse.org/
- 项目主页: ${HOMEPAGE}
EOF

echo -e "${GREEN}   ✓ 已创建: ${OBS_DIR}/README.txt${NC}"

# 7. 显示摘要
echo ""
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${BLUE}  准备完成！${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""
echo -e "${GREEN}已创建的文件：${NC}"
echo ""

# 显示文件列表和大小
cd "${OBS_DIR}"
for file in *; do
    if [ -f "$file" ]; then
        SIZE=$(du -h "$file" | cut -f1)
        case "$file" in
            *.tar.gz)
                echo -e "  📦 ${file} ${YELLOW}(${SIZE})${NC}"
                ;;
            *.spec)
                echo -e "  📄 ${file} ${YELLOW}(${SIZE})${NC}"
                ;;
            *.dsc)
                echo -e "  📄 ${file} ${YELLOW}(${SIZE})${NC}"
                ;;
            _service)
                echo -e "  ⚙️  ${file} ${YELLOW}(${SIZE})${NC}"
                ;;
            *.txt)
                echo -e "  📝 ${file} ${YELLOW}(${SIZE})${NC}"
                ;;
            *)
                echo -e "  📄 ${file} ${YELLOW}(${SIZE})${NC}"
                ;;
        esac
    fi
done
cd ..

echo ""
echo -e "${GREEN}所有文件已复制到: ${OBS_DIR}/${NC}"
echo ""

# 8. 显示下一步操作
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${BLUE}  下一步操作${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""
echo -e "${YELLOW}方法 1: 使用 Web 界面（推荐新手）${NC}"
echo "  1. 访问 https://build.opensuse.org/"
echo "  2. 登录并创建项目（如果还没有）"
echo "  3. 创建包 '${PACKAGE_NAME}'"
echo "  4. 上传以下文件："
echo -e "     ${GREEN}• ${PACKAGE_NAME}-${VERSION}.tar.gz${NC}"
echo -e "     ${GREEN}• ${PACKAGE_NAME}.spec${NC}"
echo -e "     ${GREEN}• _service${NC}"
echo "  5. 等待构建完成"
echo ""
echo -e "${YELLOW}方法 2: 使用 osc 命令行（推荐高级用户）${NC}"
echo "  1. 安装 osc:"
echo "     sudo apt-get install osc"
echo ""
echo "  2. 配置 osc:"
echo "     osc -A https://api.opensuse.org"
echo ""
echo "  3. 上传文件:"
echo "     osc co home:你的用户名"
echo "     osc mkpac ${PACKAGE_NAME}"
echo "     cp ${OBS_DIR}/* home:你的用户名/${PACKAGE_NAME}/"
echo "     cd home:你的用户名/${PACKAGE_NAME}"
echo "     osc add *"
echo "     osc commit -m 'Update to ${VERSION}'"
echo ""
echo "  4. 查看构建状态:"
echo "     osc results"
echo ""
echo -e "${YELLOW}详细说明请查看:${NC}"
echo "  • ${OBS_DIR}/README.txt - 上传说明"
echo "  • OBS_README.md - OBS 完整指南"
echo ""

# 9. 验证检查
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${BLUE}  验证检查${NC}"
echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""

# 检查源码包内容
echo -e "${YELLOW}检查源码包内容...${NC}"
if tar -tzf "${PACKAGE_NAME}-${VERSION}.tar.gz" | grep -q "CMakeLists.txt"; then
    echo -e "${GREEN}✓ 源码包包含 CMakeLists.txt${NC}"
else
    echo -e "${RED}✗ 源码包缺少 CMakeLists.txt${NC}"
fi

if tar -tzf "${PACKAGE_NAME}-${VERSION}.tar.gz" | grep -q "src/main.cpp"; then
    echo -e "${GREEN}✓ 源码包包含源代码${NC}"
else
    echo -e "${RED}✗ 源码包缺少源代码${NC}"
fi

if tar -tzf "${PACKAGE_NAME}-${VERSION}.tar.gz" | grep -q "debian/"; then
    echo -e "${GREEN}✓ 源码包包含 debian 目录${NC}"
else
    echo -e "${YELLOW}⚠️  源码包缺少 debian 目录${NC}"
fi

if tar -tzf "${PACKAGE_NAME}-${VERSION}.tar.gz" | grep -q "resources/"; then
    echo -e "${GREEN}✓ 源码包包含资源文件${NC}"
else
    echo -e "${YELLOW}⚠️  源码包缺少资源文件${NC}"
fi

echo ""
echo -e "${GREEN}✓ 所有文件准备完成！${NC}"
echo ""
