# 磁盘清理工具-Deepin版 (LZ Disk Cleaner)

一个专为 Deepin V25 系统设计的磁盘清理工具，采用 C++17 + Qt6 开发。

**当前版本**: v1.2.0

## 功能特性

### 核心功能
- 📊 **磁盘分析** - 全面分析磁盘使用情况，支持多级目录扫描
- 🧹 **智能清理** - 一键清理安全项目
- ⚙️ **自定义清理** - 灵活选择清理项目和磁盘分区
- 🗂️ **磐石系统支持** - 分析和管理系统快照
- 📦 **玲珑应用管理** - 查看和卸载玲珑应用
- 🌐 **源管理** - APT 软件源添加、编辑、启用/禁用和删除

### 系统工具
- 🚀 **自启动管理** - 管理系统自启动应用程序
- 🔐 **文件粉碎** - 安全删除敏感文件
- 🖥️ **资源监控** - 实时监控系统资源使用情况

### 界面特性
- 🎨 **现代化界面** - 原生 Qt6 GUI，适配 Deepin V25 主题
- 🌓 **主题自适应** - 自动适应系统深色/浅色主题
- 🔒 **安全可靠** - 多重确认机制，保护重要数据

## 技术栈

- **编程语言**: C++17
- **GUI 框架**: Qt6 (Core, Gui, Widgets, Charts)
- **构建工具**: CMake
- **目标平台**: Deepin V25 (Debian 12 based)

## 项目结构

```
deepin-disk-cleaner/
├── CMakeLists.txt              # CMake 构建配置
├── README.md                   # 项目文档
├── src/                        # 源代码
│   ├── main.cpp               # 程序入口
│   ├── helper/                # 辅助程序（权限提升）
│   │   └── main.cpp
│   ├── core/                  # 核心功能模块
│   │   ├── diskanalyzer.cpp/h     # 磁盘分析器
│   │   ├── diskcleaner.cpp/h      # 磁盘清理器
│   │   ├── fileshredder.cpp/h     # 文件粉碎器
│   │   ├── hardwaremonitor.cpp/h  # 硬件监控
│   │   ├── systeminfo.cpp/h       # 系统信息获取
│   │   └── systemslimmer.cpp/h    # 系统瘦身
│   ├── gui/                   # GUI 界面模块
│   │   ├── mainwindow.cpp/h       # 主窗口
│   │   ├── dashboardwidget.cpp/h  # 仪表盘
│   │   ├── analyzewidget.cpp/h    # 磁盘分析
│   │   ├── cleanupdialog.cpp/h    # 清理对话框
│   │   ├── aptsourcemanagerwidget.cpp/h  # 源管理
│   │   ├── startupappswidget.cpp/h       # 自启动管理
│   │   ├── fileshredderwidget.cpp/h      # 文件粉碎
│   │   ├── resourceswidget.cpp/h         # 资源监控
│   │   ├── systemslimmerwidget.cpp/h     # 系统瘦身
│   │   ├── settingsdialog.cpp/h          # 设置对话框
│   │   └── progressdialog.cpp/h          # 进度对话框
│   └── utils/                 # 工具类
│       ├── logger.cpp/h       # 日志系统
│       └── config.cpp/h       # 配置管理
├── resources/                  # 资源文件
│   ├── icons/                 # 图标资源 (SVG)
│   └── resources.qrc          # 资源文件
├── debian/                     # Debian 打包文件
└── build/                      # 构建输出目录
```

## 依赖项

### 系统依赖

```bash
# Qt6 开发库
sudo apt install qt6-base-dev qt6-base-dev-tools
sudo apt install qt6-charts-dev
sudo apt install qt6-l10n-tools

# 构建工具
sudo apt install cmake build-essential

# 其他依赖
sudo apt install libstdc++-12-dev
```

### 编译依赖

- CMake >= 3.16
- GCC/Clang with C++17 support
- Qt6 >= 6.4

## 编译方法

### 快速编译

```bash
./build.sh
```

### 手动编译

```bash
# 创建构建目录
mkdir -p build && cd build

# 配置项目
cmake .. -DCMAKE_BUILD_TYPE=Release

# 编译
make -j$(nproc)
```

### 运行

```bash
./build/bin/lz-disk-cleaner
```

## 安装

### Debian 包安装

```bash
cd build
cpack -G DEB
sudo dpkg -i lz-disk-cleaner-*.deb
```

### 手动安装

```bash
cd build
sudo make install
```

程序将被安装到 `/usr/bin/lz-disk-cleaner`

## 使用说明

### 主要功能模块

| 模块 | 功能说明 |
|------|----------|
| **仪表盘** | 系统概览，显示磁盘使用情况 |
| **磁盘分析** | 扫描系统中的可清理项目 |
| **资源监控** | 实时显示 CPU、内存、网络使用情况 |
| **文件粉碎** | 安全删除敏感文件 |
| **系统瘦身** | 查找大文件、重复文件 |
| **自启动** | 管理系统自启动应用程序 |
| **源管理** | 管理软件源（添加、编辑、启用/禁用、删除） |

### 磁盘分析支持的项目

| 项目 | 说明 | 安全性 |
|------|------|--------|
| 用户缓存 | 应用程序缓存数据 | 🔶 注意 |
| 缩略图缓存 | 文件缩略图 | ✅ 安全 |
| APT缓存 | 已下载的软件包 | ✅ 安全 |
| 系统日志 | 系统日志文件 | ⚠️ 危险 |
| Journald日志 | 系统服务日志 | ✅ 安全 |
| 临时文件 | 系统临时文件 | ⚠️ 危险 |
| 回收站 | 已删除的文件 | ✅ 安全 |
| 浏览器缓存 | Chrome/Chromium/360/龙芯/QQ/Edge/Firefox | 🔶 注意 |
| 开发工具缓存 | Pip/NPM/Yarn/Go/Cargo/Maven/Gradle | ✅ 安全 |
| 玲珑应用 | 玲珑格式应用管理 | 🔶 注意 |
| 磐石系统快照 | 系统快照备份 | ⚠️ 危险 |

### 源管理操作

- **添加**: 添加新的 APT 软件源
- **编辑**: 修改现有源配置
- **启用/禁用**: 切换源的激活状态（无需删除）
- **删除**: 永久删除软件源

## 配置文件

配置文件位置: `~/.config/lz-disk-cleaner/lz-disk-cleaner.conf`

```ini
[General]
language=zh_CN
autoUpdate=false

[Cleanup]
autoCleanCache=false
journalKeepDays=7
snapshotKeepCount=3
confirmBeforeCleanup=true

[Interface]
refreshInterval=30
```

## 日志文件

日志文件位置: `~/.local/share/lz-disk-cleaner/lz-disk-cleaner.log`

## 更新日志

### v1.1.1 (2026-03)
- ✨ 新增系统托盘支持，支持后台运行
- ✨ 新增单实例检测，避免多进程运行
- ✨ 新增赞助支持功能
- ✨ 新增应用缓存清理（WPS、钉钉、腾讯会议等）
- 🐛 修复深色主题下源管理预览内容不可见问题
- 💄 优化 360 浏览器图标显示

### v1.1.0 (2025-03)
- ✨ 新增源管理功能（启用/禁用源）
- ✨ 新增自启动应用管理
- ✨ 新增文件粉碎功能
- ✨ 新增硬件监控功能
- ✨ 扩展浏览器缓存扫描（支持 360、龙芯、QQ、Edge 浏览器）
- 💄 统一各模块选中项样式
- 🔧 精简设置对话框（移除未实现的选项）
- 🐛 修复深色主题下警告框不可见问题
- 🐛 修复多处焦点边框显示问题

### v1.0.0
- 🎉 初始版本发布
- 磁盘分析、智能清理、自定义清理
- 磐石系统支持、玲珑应用支持

## 开发计划

- [x] 添加源管理功能
- [x] 添加自启动管理
- [x] 添加文件粉碎功能
- [x] 添加硬件监控
- [x] 扩展浏览器缓存支持
- [x] 添加系统托盘支持
- [ ] 添加数据可视化图表
- [ ] 实现定时清理功能
- [ ] 添加系统托盘支持
- [ ] 实现清理历史记录
- [ ] 支持多语言（国际化）
- [ ] 添加在线更新功能

## 贡献指南

欢迎提交 Issue 和 Pull Request！

## 许可证

GPL-3.0

## 作者

tonglingcn (克亮)

## 致谢

- Qt 框架开发团队
- Deepin 社区

## 联系方式

- 项目主页: https://github.com/tonglingcn/deepin-disk-cleaner
- 问题反馈: https://github.com/tonglingcn/deepin-disk-cleaner/issues

---

**注意**: 本工具专为 Deepin V25 系统设计，在其他系统上使用可能无法正常工作。
