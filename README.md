# LZ 磁盘清理工具 (LZ Disk Cleaner)

一个专为 Deepin/Debian 系统设计的磁盘清理工具，采用 C++17 + Qt6 开发。

## 功能特性

- 📊 **磁盘分析** - 全面分析磁盘使用情况
- 🧹 **智能清理** - 一键清理安全项目
- ⚙️ **自定义清理** - 灵活选择清理项目
- 🗂️ **磐石系统支持** - 分析和管理系统快照
- 📦 **玲珑应用支持** - 管理玲珑应用和依赖
- 🎨 **现代化界面** - 原生 Qt6 GUI，适配 Deepin 主题
- 🔒 **安全可靠** - 多重确认机制，保护重要数据

## 技术栈

- **编程语言**: C++17
- **GUI 框架**: Qt6 (Core, Gui, Widgets, Charts)
- **构建工具**: CMake
- **目标平台**: Deepin V25 (Debian 11 based)

## 项目结构

```
deepin-disk-cleaner/
├── CMakeLists.txt              # CMake 构建配置
├── README.md                   # 项目文档
├── src/                        # 源代码
│   ├── main.cpp               # 程序入口
│   ├── core/                  # 核心功能模块
│   │   ├── diskanalyzer.cpp/h # 磁盘分析器
│   │   ├── diskcleaner.cpp/h  # 磁盘清理器
│   │   └── systeminfo.cpp/h   # 系统信息获取

│   ├── gui/                   # GUI 界面模块
│   │   ├── mainwindow.cpp/h   # 主窗口
│   │   ├── dashboardwidget.cpp/h # 仪表盘
│   │   ├── cleanupdialog.cpp/h    # 清理对话框
│   │   └── progressdialog.cpp/h   # 进度对话框
│   └── utils/                 # 工具类
│       ├── logger.cpp/h       # 日志系统
│       └── config.cpp/h       # 配置管理
├── resources/                  # 资源文件
│   ├── icons/                 # 图标资源
│   ├── styles/                # 样式文件
│   └── translations/          # 翻译文件
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
sudo apt install libstdc++-11-dev
```

### 编译依赖

- CMake >= 3.16
- GCC/Clang with C++17 support
- Qt6 >= 6.2

## 编译方法

### 1. 克隆或下载项目

```bash
cd /home/deepin/Documents/deepin-disk-cleaner
```

### 2. 创建构建目录

```bash
mkdir -p build
cd build
```

### 3. 配置项目

```bash
cmake ..
```

### 4. 编译

```bash
make -j$(nproc)
```

### 5. 运行

```bash
./bin/DeepinDiskCleaner
```

## 打包为二进制文件

### 使用 CMake 安装

```bash
cd build
sudo make install
```

程序将被安装到 `/usr/local/bin/DeepinDiskCleaner`

### 手动打包

```bash
# 创建 AppImage (需要 linuxdeploy)
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# 复制可执行文件
cp bin/DeepinDiskCleaner DeepinDiskCleaner

# 使用 linuxdeploy 打包 (可选)
linuxdeploy --appdir AppDir --executable DeepinDiskCleaner --output appimage
```

## 使用说明

### 基本操作

1. **分析磁盘** - 点击"分析磁盘"按钮查看磁盘使用情况
2. **智能清理** - 点击"一键智能清理"自动清理安全项目
3. **自定义清理** - 点击"自定义清理"选择要清理的项目

### 清理项目说明

| 项目 | 说明 | 安全性 |
|------|------|--------|
| 用户缓存 | 应用程序缓存数据 | ⚠️ 中等 |
| 缩略图缓存 | 文件缩略图 | ✅ 安全 |
| APT缓存 | 已下载的软件包 | ✅ 安全 |
| 系统日志 | 系统日志文件 | ⚠️ 中等 |
| 临时文件 | 系统临时文件 | ⚠️ 中等 |
| 回收站 | 已删除的文件 | ✅ 安全 |
| 浏览器缓存 | Chrome/Firefox缓存 | ✅ 安全 |
| 开发工具缓存 | Pip/NPM/Go/ Cargo缓存 | ✅ 安全 |
| 系统快照 | 磐石系统快照 | ⚠️ 危险 |

## 配置文件

配置文件位置: `~/.config/deepin-disk-cleaner/deepin-disk-cleaner.conf`

```ini
[General]
language=zh_CN
autoUpdate=false
showNotifications=true

[Cleanup]
autoCleanCache=false
journalKeepDays=7
snapshotKeepCount=3
confirmBeforeCleanup=true

[Interface]
darkMode=false
refreshInterval=30
```

## 日志文件

日志文件位置: `~/.local/share/deepin-disk-cleaner/deepin-disk-cleaner.log`

## 开发计划

- [ ] 添加数据可视化图表
- [ ] 实现定时清理功能
- [ ] 添加系统托盘支持
- [ ] 实现清理历史记录
- [ ] 添加更多清理项目
- [ ] 支持多语言
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
- Stacer 项目 (资源监控模块参考)

## 联系方式

- 项目主页: https://github.com/tonglingcn/lz-disk-cleaner
- 问题反馈: https://github.com/tonglingcn/lz-disk-cleaner/issues

---

**注意**: 本工具专为 Deepin/Debian 系统设计，在其他系统上使用可能无法正常工作。