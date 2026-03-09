# Deepin 磁盘清理工具 - 架构设计文档

> 版本：1.0.0  
> 最后更新：2026-03-07

---

## 目录

1. [项目概述](#1-项目概述)
2. [系统架构](#2-系统架构)
3. [核心模块](#3-核心模块)
4. [GUI模块](#4-gui模块)
5. [工具模块](#5-工具模块)
6. [数据流设计](#6-数据流设计)
7. [安全机制](#7-安全机制)
8. [技术栈](#8-技术栈)
9. [构建与部署](#9-构建与部署)
10. [扩展指南](#10-扩展指南)

---

## 1. 项目概述

### 1.1 项目简介

Deepin 磁盘清理工具是一款专为 Deepin V25 操作系统设计的磁盘清理和系统优化工具。支持 Deepin 特有的磐石系统（Immutable System）和玲珑应用（Linglong）管理。

### 1.2 核心功能

| 功能模块 | 描述 |
|----------|------|
| **系统仪表盘** | 实时显示CPU、内存、磁盘使用率 |
| **磁盘分析** | 扫描可清理项目，分类展示 |
| **智能清理** | 一键清理安全缓存，释放磁盘空间 |
| **自定义清理** | 用户自选分区/目录进行清理 |
| **资源监控** | 实时监控CPU、内存、磁盘IO、网络 |
| **磐石系统支持** | 管理系统快照，清理修改层 |
| **玲珑应用管理** | 查看和卸载玲珑格式应用 |

### 1.3 设计原则

- **安全性优先**：只清理绝对安全的项目，保护用户数据
- **模块化设计**：Core/GUI/Utils 三层架构，职责清晰
- **原生体验**：使用 Qt6 原生控件，适配深色/浅色主题
- **Deepin 特性**：深度集成磐石系统和玲珑应用

---

## 2. 系统架构

### 2.1 整体架构图

```
┌─────────────────────────────────────────────────────────────────┐
│                        Presentation Layer                        │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐           │
│  │ MainWindow   │  │ CleanupDialog│  │ ProgressDialog│          │
│  └──────┬───────┘  └──────────────┘  └──────────────┘           │
│         │                                                        │
│  ┌──────▼───────────────────────────────────────────────┐       │
│  │                    QTabWidget                         │       │
│  │  ┌──────────────┬──────────────┬──────────────┐      │       │
│  │  │ Dashboard    │ Analyze      │ Resources    │      │       │
│  │  │ Widget       │ Widget       │ Widget       │      │       │
│  │  └──────────────┴──────────────┴──────────────┘      │       │
│  └──────────────────────────────────────────────────────┘       │
├─────────────────────────────────────────────────────────────────┤
│                        Business Layer                            │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐           │
│  │ DiskAnalyzer │  │ DiskCleaner  │  │ SystemInfo   │           │
│  │ 磁盘分析器    │  │ 磁盘清理器    │  │ 系统信息      │           │
│  └──────────────┘  └──────────────┘  └──────────────┘           │
├─────────────────────────────────────────────────────────────────┤
│                        Foundation Layer                          │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐           │
│  │ Logger       │  │ Config       │  │ HistoryChart │           │
│  │ 日志系统      │  │ 配置管理      │  │ 图表组件      │           │
│  └──────────────┘  └──────────────┘  └──────────────┘           │
└─────────────────────────────────────────────────────────────────┘
```

### 2.2 目录结构

```
deepin-disk-cleaner/
├── CMakeLists.txt              # CMake 构建配置
├── README.md                   # 项目说明文档
├── ARCH.md                     # 架构设计文档（本文件）
├── DEVELOPMENT_STATUS.md       # 开发状态记录
│
├── src/                        # 源代码目录
│   ├── main.cpp                # 程序入口
│   │
│   ├── core/                   # 核心业务层
│   │   ├── diskanalyzer.cpp/h  # 磁盘分析器
│   │   ├── diskcleaner.cpp/h   # 磁盘清理器
│   │   └── systeminfo.cpp/h    # 系统信息获取
│   │
│   ├── gui/                    # 图形界面层
│   │   ├── mainwindow.cpp/h    # 主窗口
│   │   ├── dashboardwidget.cpp/h  # 仪表盘组件
│   │   ├── analyzewidget.cpp/h    # 磁盘分析界面
│   │   ├── cleanupdialog.cpp/h    # 自定义清理对话框
│   │   ├── resourceswidget.cpp/h  # 资源监控页面
│   │   ├── historychart.cpp/h     # 历史图表组件
│   │   └── progressdialog.cpp/h   # 进度对话框
│   │
│   └── utils/                  # 工具类
│       ├── logger.cpp/h        # 日志系统
│       └── config.cpp/h        # 配置管理
│
├── resources/                  # 资源文件
│   ├── resources.qrc           # Qt 资源定义
│   ├── icons/                  # SVG 图标
│   ├── styles/                 # QSS 样式表
│   └── translations/           # 翻译文件
│
├── translations/               # 翻译源文件
└── build/                      # 构建输出目录
```

---

## 3. 核心模块

### 3.1 DiskAnalyzer - 磁盘分析器

**文件位置**：`src/core/diskanalyzer.cpp/h`

**职责**：分析磁盘使用情况，获取系统可清理项目

#### 数据结构

```cpp
// 磁盘使用信息
struct DiskUsage {
    QString filesystem;    // 文件系统设备
    QString type;          // 文件系统类型 (ext4, ntfs, etc.)
    qint64 total;          // 总容量
    qint64 used;           // 已用空间
    qint64 available;      // 可用空间
    double percent;        // 使用百分比
    QString mountpoint;    // 挂载点
};

// 磐石系统信息 (Deepin V25 特有)
struct ImmutableSystemInfo {
    bool enabled;                    // 是否启用磐石系统
    QString currentSnapshot;         // 当前快照名称
    qint64 modificationLayerSize;    // 修改层大小
    QStringList snapshots;           // 快照列表
};

// 玲珑应用信息
struct LinglongAppInfo {
    QString name;         // 应用名称
    QString version;      // 版本号
    QString id;           // 应用ID
    qint64 size;          // 占用大小
    QString channel;      // 渠道
    QStringList dependencies;  // 依赖列表
};

// 目录使用信息
struct DirectoryUsage {
    QString path;         // 路径
    qint64 size;          // 大小
    int fileCount;        // 文件数量
    int dirCount;         // 子目录数量
};
```

#### 主要接口

```cpp
class DiskAnalyzer : public QObject {
    Q_OBJECT
    
public:
    // 磁盘分析
    QList<DiskUsage> analyzeDiskUsage();
    QList<DirectoryUsage> analyzeHomeDirectory();
    
    // Deepin 特有功能
    ImmutableSystemInfo analyzeImmutableSystem();
    QList<LinglongAppInfo> analyzeLinglongApps();
    
    // 特殊应用数据
    QMap<QString, qint64> analyzeSpecialApps();  // 钉钉、微信、QQ等
    
signals:
    void analysisProgress(int percent);
    void analysisFinished();
    void errorOccurred(const QString &error);
};
```

#### 数据来源

| 数据 | 来源 |
|------|------|
| 磁盘使用 | `df -Th` 命令 |
| 目录大小 | `du -sb` 命令 |
| 磐石系统 | `deepin-immutable-ctl` 命令 |
| 玲珑应用 | `ll-cli list` 命令 |
| 内存信息 | `/proc/meminfo` |
| CPU信息 | `/proc/cpuinfo` |

---

### 3.2 DiskCleaner - 磁盘清理器

**文件位置**：`src/core/diskcleaner.cpp/h`

**职责**：执行各种磁盘清理操作

#### 数据结构

```cpp
struct CleanupResult {
    QString itemName;      // 清理项目名称
    qint64 freedSpace;     // 释放的空间 (bytes)
    bool success;          // 是否成功
    QString errorMessage;  // 错误信息
};
```

#### 清理方法矩阵

| 方法 | 功能 | 路径 | 权限 | 安全性 |
|------|------|------|------|--------|
| `cleanThumbnailCache()` | 缩略图缓存 | `~/.cache/thumbnails` | 用户 | ✅ 安全 |
| `cleanAptCache()` | APT包缓存 | `/var/cache/apt/archives` | sudo | ✅ 安全 |
| `cleanUserCache()` | 用户缓存 | `~/.cache` (受保护) | 用户 | ⚠️ 需过滤 |
| `cleanDevCache()` | 开发工具缓存 | pip/npm/go/cargo | 用户 | ✅ 安全 |
| `cleanTrash()` | 回收站 | `~/.local/share/Trash` | 用户 | ⚠️ 需确认 |
| `cleanJournalLogs()` | 系统日志 | `/var/log/journal` | sudo | ⚠️ 需确认 |
| `cleanTempFiles()` | 临时文件 | `/tmp` | 用户/sudo | ⚠️ 需确认 |
| `cleanBrowserCache()` | 浏览器缓存 | Chrome/Firefox | 用户 | ✅ 安全 |
| `smartCleanup()` | 智能清理 | 仅安全项目 | 混合 | ✅ 安全 |

#### 受保护目录

```cpp
QStringList DiskCleaner::getProtectedDirs() {
    return QStringList()
        << "fontconfig"        // 字体配置缓存
        << "dconf"             // GNOME 配置系统
        << "mesa_shader_cache" // Mesa 着色器缓存
        << "kioexec"           // KDE IO 缓存
        << "pulse"             // PulseAudio 音频
        << "xsession-errors"   // X 会话错误日志
        << "QtShaderCache"     // Qt 着色器缓存
        << "deepin"            // Deepin 系统缓存
        << "flatpak"           // Flatpak 应用数据
        << "linglong";         // 玲珑应用数据
}
```

---

### 3.3 SystemInfo - 系统信息

**文件位置**：`src/core/systeminfo.cpp/h`

**职责**：获取系统基本信息

```cpp
struct SystemInfoData {
    QString osName;          // 操作系统名称
    QString osVersion;       // 系统版本
    QString kernelVersion;   // 内核版本 (uname -r)
    QString architecture;    // 架构 (x86_64)
    QString hostname;        // 主机名
    QString username;        // 用户名
    qint64 totalMemory;      // 总内存 (bytes)
    qint64 availableMemory;  // 可用内存 (bytes)
    QString cpuModel;        // CPU 型号
    int cpuCores;            // CPU 核心数
};
```

---

## 4. GUI模块

### 4.1 MainWindow - 主窗口

**文件位置**：`src/gui/mainwindow.cpp/h`

**职责**：应用程序主窗口，整合所有功能模块

#### UI 布局

```
┌─────────────────────────────────────────────────────┐
│ 菜单栏: [文件] [工具] [设置] [帮助]                    │
├─────────────────────────────────────────────────────┤
│                                                     │
│  ┌─────────────────────────────────────────────┐   │
│  │              QTabWidget                       │   │
│  │  [仪表盘] [磁盘分析] [资源监控]                 │   │
│  │  ┌─────────────────────────────────────────┐│   │
│  │  │                                          ││   │
│  │  │           当前选中的标签页内容             ││   │
│  │  │                                          ││   │
│  │  └─────────────────────────────────────────┘│   │
│  └─────────────────────────────────────────────┘   │
│                                                     │
│  ┌─────────┬─────────┬─────────┬─────────┐         │
│  │ 仪表盘  │ 分析磁盘 │ 智能清理 │ 自定义  │         │
│  │  (蓝)  │  (青)   │  (绿)   │  (橙)   │         │
│  └─────────┴─────────┴─────────┴─────────┘         │
│                                                     │
│  ⚠️ 警告：数据一旦删除，将无法恢复，请慎重操作        │
├─────────────────────────────────────────────────────┤
│ 状态栏: [状态文本]                    [进度条]      │
└─────────────────────────────────────────────────────┘
```

#### 核心成员

```cpp
class MainWindow : public QMainWindow {
    // 核心组件
    DiskAnalyzer *m_analyzer;
    DiskCleaner *m_cleaner;
    
    // GUI 组件
    DashboardWidget *m_dashboardWidget;
    AnalyzeWidget *m_analyzeWidget;
    ResourcesWidget *m_resourcesWidget;
    CleanupDialog *m_cleanupDialog;
    ProgressDialog *m_progressDialog;
    
    // UI 元素
    QTabWidget *m_tabWidget;
    QProgressBar *m_progressBar;
    QLabel *m_statusLabel;
    QPushButton *m_smartCleanupButton;
};
```

---

### 4.2 DashboardWidget - 仪表盘组件

**文件位置**：`src/gui/dashboardwidget.cpp/h`

**职责**：显示系统状态概览

#### 组件结构

```
┌─────────────────────────────────────────────────────┐
│                                                     │
│    ┌─────────┐    ┌─────────┐    ┌─────────┐       │
│    │   CPU   │    │   内存   │    │ 磁盘容量 │       │
│    │  ⚡      │    │   💾    │    │         │       │
│    │  25.5%  │    │  68.2%  │    │  45.3%  │       │
│    │ CPU空闲  │    │ 4G/16G  │    │ 90G/200G│       │
│    └─────────┘    └─────────┘    └─────────┘       │
│                                                     │
│  ┌─────────────┬─────────────┬─────────────┐       │
│  │🖥️ 主机名称   │⚡ CPU型号    │💾 物理内存   │       │
│  │ deepin-pc   │ Intel i7    │ 16 GB      │       │
│  ├─────────────┼─────────────┼─────────────┤       │
│  │🐧 内核版本   │🎮 显卡      │🖼️ 显示适配器 │       │
│  │ 6.1.0-amd64 │ NVIDIA RTX  │ HDMI-A-1   │       │
│  └─────────────┴─────────────┴─────────────┘       │
│                                                     │
└─────────────────────────────────────────────────────┘
```

#### 圆形进度条实现

```cpp
void CircularProgressWidget::paintEvent(QPaintEvent *event) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // 绘制背景圆环
    painter.setPen(QPen(bgRingColor, ringWidth));
    painter.drawEllipse(center, radius, radius);
    
    // 绘制进度圆环 (从顶部开始)
    int startAngle = 90 * 16;  // Qt 使用 1/16 度
    int spanAngle = -percent * 3.6 * 16;
    painter.drawArc(rect, startAngle, spanAngle);
    
    // 绘制中心百分比文字
    painter.drawText(center, QString("%1%").arg(percent));
}
```

---

### 4.3 AnalyzeWidget - 磁盘分析界面

**文件位置**：`src/gui/analyzewidget.cpp/h`

**职责**：扫描和显示可清理项目

#### 扫描类别

```cpp
enum class ScanCategory {
    USER_CACHE,           // 用户缓存 ~/.cache
    THUMBNAIL_CACHE,      // 缩略图缓存 ~/.cache/thumbnails
    APT_CACHE,            // APT包缓存 /var/cache/apt/archives
    SYSTEM_LOGS,          // 系统日志 /var/log
    JOURNAL_LOGS,         // Journald日志
    CRASH_REPORTS,        // 崩溃报告 /var/crash
    TEMP_FILES,           // 临时文件 /tmp
    TRASH,                // 回收站 ~/.local/share/Trash
    BROWSER_CACHE,        // 浏览器缓存
    DEV_CACHE,            // 开发工具缓存
    LINGLONG_APPS,        // 玲珑应用
    IMMUTABLE_SNAPSHOTS   // 磐石系统快照
};
```

#### 扫描结果结构

```cpp
struct ScanResult {
    ScanCategory category;      // 类别
    QString name;               // 显示名称
    QString path;               // 实际路径
    qint64 size;                // 文件大小
    qint64 fileCount;           // 文件数量
    bool isDirectory;           // 是否目录
    bool isDeletable = true;    // 是否可安全删除
    bool isDangerous = false;   // 是否危险操作
    QString description;        // 描述信息
    QString purpose;            // 目录用途
    QList<ScanResult> children; // 子项目
};
```

---

### 4.4 CleanupDialog - 自定义清理对话框

**文件位置**：`src/gui/cleanupdialog.cpp/h`

**职责**：自定义清理，支持分区选择和文件浏览

#### 功能特性

1. **分区选择页**：显示所有硬盘分区，支持勾选
2. **扫描结果页**：显示扫描结果，支持勾选清理
3. **内置文件浏览器**：双击进入文件夹
4. **路径导航**：返回上一级、返回初始目录

#### 数据结构

```cpp
struct PartitionInfo {
    QString device;        // 设备名 /dev/sda1
    QString filesystem;    // 文件系统类型
    QString mountpoint;    // 挂载点
    qint64 total;          // 总容量
    qint64 used;           // 已用空间
    qint64 available;      // 可用空间
    double percent;        // 使用百分比
    bool isSystemPartition;
};

struct ScanItem {
    QString path;          // 完整路径
    QString name;          // 名称
    qint64 size;           // 大小
    int fileCount;         // 文件数量
    int dirCount;          // 文件夹数量
    bool isDir;            // 是否为目录
    QString mimeType;      // MIME类型
};
```

---

### 4.5 ResourcesWidget - 资源监控页面

**文件位置**：`src/gui/resourceswidget.cpp/h`

**职责**：实时监控CPU、内存、磁盘IO、网络使用情况

#### 数据采集

| 指标 | 数据源 | 采集频率 |
|------|--------|----------|
| CPU | `/proc/stat` | 1秒 |
| 内存 | `/proc/meminfo` | 1秒 |
| 磁盘IO | `/proc/diskstats` | 1秒 |
| 网络 | `/proc/net/dev` | 1秒 |

#### 自绘图表

```cpp
class HistoryChart : public QWidget {
    // 使用 QPainter 自绘历史曲线
    // 支持多数据系列
    // 默认保留 60 秒历史数据
    // 自动缩放 Y 轴
};
```

---

## 5. 工具模块

### 5.1 Logger - 日志系统

**文件位置**：`src/utils/logger.cpp/h`

```cpp
class Logger : public QObject {
    Q_OBJECT
    
public:
    static Logger* instance();  // 单例模式
    
    void debug(const QString &message);
    void info(const QString &message);
    void warning(const QString &message);
    void error(const QString &message);
    void critical(const QString &message);
    
private:
    QFile m_logFile;
    QMutex m_mutex;  // 线程安全
};

// 便捷宏
#define LOG_DEBUG(msg) Logger::instance()->debug(msg)
#define LOG_INFO(msg) Logger::instance()->info(msg)
#define LOG_WARNING(msg) Logger::instance()->warning(msg)
#define LOG_ERROR(msg) Logger::instance()->error(msg)
```

**日志文件位置**：`~/.local/share/deepin-disk-cleaner/deepin-disk-cleaner.log`

---

### 5.2 Config - 配置管理

**文件位置**：`src/utils/config.cpp/h`

```cpp
class Config : public QObject {
    Q_OBJECT
    
public:
    static Config* instance();
    
    // 通用设置
    QString language() const;
    bool autoUpdate() const;
    bool showNotifications() const;
    
    // 清理设置
    bool autoCleanCache() const;
    int journalKeepDays() const;
    int snapshotKeepCount() const;
    bool confirmBeforeCleanup() const;
    
    // 界面设置
    bool darkMode() const;
    int refreshInterval() const;
    
    void setValue(const QString &group, const QString &key, const QVariant &value);
    QVariant value(const QString &group, const QString &key, const QVariant &default = QVariant()) const;
};
```

**配置文件位置**：`~/.config/deepin-disk-cleaner/deepin-disk-cleaner.conf`

---

## 6. 数据流设计

### 6.1 应用启动流程

```
main()
  │
  ├── QApplication app(argc, argv)
  ├── QCoreApplication::setApplicationName("DeepinDiskCleaner")
  ├── QCoreApplication::setApplicationVersion("1.0.0")
  │
  ├── Logger::instance()->init()
  │     └── 创建日志文件
  │
  ├── MainWindow window
  │     │
  │     ├── initUI()
  │     │     ├── createMenuBar()
  │     │     ├── 创建 QTabWidget
  │     │     ├── 创建 DashboardWidget
  │     │     ├── 创建 AnalyzeWidget
  │     │     ├── 创建 ResourcesWidget
  │     │     └── 创建底部按钮区域
  │     │
  │     ├── connectSignals()
  │     │     └── 连接信号槽
  │     │
  │     └── applyTheme()
  │           └── 根据系统主题设置样式
  │
  ├── window.show()
  └── app.exec()
```

### 6.2 智能清理流程

```
用户点击"一键智能清理"
  │
  ▼
MainWindow::onSmartCleanupClicked()
  │
  ├── QMessageBox::question() 确认对话框
  │     │
  │     └── 用户确认
  │
  ├── 显示 ProgressDialog
  │
  └── DiskCleaner::smartCleanup()
        │
        ├── cleanThumbnailCache()    // 缩略图缓存
        ├── cleanAptCache()          // APT缓存
        ├── cleanDevCache()          // 开发工具缓存
        │
        └── emit cleanupFinished(results)
              │
              ▼
        MainWindow::onCleanupFinished()
              │
              ├── 计算总释放空间
              ├── 关闭 ProgressDialog
              └── QMessageBox::information() 显示结果
```

### 6.3 磁盘分析流程

```
用户点击"分析磁盘"
  │
  ▼
MainWindow::onAnalyzeClicked()
  │
  ├── 切换到分析标签页
  │
  └── AnalyzeWidget::startScan()
        │
        ├── 显示进度页面
        │
        └── ScanThread::run() (后台线程)
              │
              ├── 扫描各个类别目录
              │     ├── scanUserCache()
              │     ├── scanThumbnailCache()
              │     ├── scanAptCache()
              │     ├── scanSystemLogs()
              │     ├── scanTrash()
              │     ├── scanBrowserCache()
              │     ├── scanDevCache()
              │     ├── scanLinglongApps()
              │     └── scanImmutableSnapshots()
              │
              ├── emit categoryScanned(category, result)
              │
              └── emit scanFinished(allResults)
                    │
                    ▼
              AnalyzeWidget::onScanFinished()
                    │
                    ├── 更新结果树
                    ├── 计算总大小
                    └── 显示统计信息
```

---

## 7. 安全机制

### 7.1 多层安全保护

```
┌─────────────────────────────────────────────────────────┐
│                      用户确认层                          │
│  清理前弹出确认对话框，列出将清理的项目                    │
├─────────────────────────────────────────────────────────┤
│                      安全判断层                          │
│  isSafeToClean() - 检查是否在受保护目录列表中             │
├─────────────────────────────────────────────────────────┤
│                      受保护目录                          │
│  fontconfig, dconf, deepin, linglong, flatpak 等         │
├─────────────────────────────────────────────────────────┤
│                      危险标记层                          │
│  isDangerous = true 的项目需要额外确认                    │
├─────────────────────────────────────────────────────────┤
│                      权限检查层                          │
│  checkSudoAccess() - 检查 sudo 权限                      │
└─────────────────────────────────────────────────────────┘
```

### 7.2 智能清理安全策略

智能清理**只清理以下安全项目**：

| 项目 | 安全性说明 |
|------|------------|
| 缩略图缓存 | 系统自动重新生成 |
| APT缓存 | 已安装软件包的缓存，可重新下载 |
| 开发工具缓存 | pip/npm/go/cargo 缓存，可重新下载 |

**注意**：回收站清理已从智能清理中移除，避免误删用户可能想恢复的文件。

---

## 8. 技术栈

### 8.1 开发环境

| 类别 | 技术 | 版本要求 |
|------|------|----------|
| **编程语言** | C++ | C++17 |
| **GUI框架** | Qt6 | >= 6.2 |
| **Qt模块** | Core, Gui, Widgets, Network | - |
| **构建系统** | CMake | >= 3.16 |
| **编译器** | GCC / Clang | 支持 C++17 |

### 8.2 Qt 特性使用

| 特性 | 用途 |
|------|------|
| `Q_OBJECT` | 信号槽机制 |
| `CMAKE_AUTOMOC` | MOC 自动处理 |
| `CMAKE_AUTORCC` | 资源文件自动处理 |
| `QThread` | 后台扫描线程 |
| `QProcess` | 执行系统命令 |
| `QSettings` | 配置持久化 |
| `QPainter` | 自绘图形 |
| `QStyle` | Fusion 样式 |

### 8.3 外部命令依赖

| 命令 | 用途 |
|------|------|
| `df` | 磁盘空间分析 |
| `du` | 目录大小计算 |
| `deepin-immutable-ctl` | 磐石系统管理 |
| `ll-cli` | 玲珑应用管理 |
| `lspci` | 显卡信息 |
| `xrandr` | 显示器信息 |

---

## 9. 构建与部署

### 9.1 构建命令

```bash
# 创建构建目录
mkdir -p build && cd build

# 配置项目
cmake .. -DCMAKE_BUILD_TYPE=Release

# 编译 (使用所有 CPU 核心)
make -j$(nproc)

# 运行
./bin/DeepinDiskCleaner

# 安装 (可选)
sudo make install
```

### 9.2 CMake 配置

```cmake
cmake_minimum_required(VERSION 3.16)
project(DeepinDiskCleaner VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt 自动处理
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 查找 Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

# 源文件
set(SOURCES
    src/main.cpp
    src/core/diskanalyzer.cpp
    src/core/diskcleaner.cpp
    src/core/systeminfo.cpp
    src/gui/mainwindow.cpp
    src/gui/dashboardwidget.cpp
    src/gui/analyzewidget.cpp
    src/gui/cleanupdialog.cpp
    src/gui/resourceswidget.cpp
    src/gui/historychart.cpp
    src/gui/progressdialog.cpp
    src/utils/logger.cpp
    src/utils/config.cpp
)

# 资源文件
set(RESOURCES resources/resources.qrc)

# 创建可执行文件
add_executable(${PROJECT_NAME} ${SOURCES} ${RESOURCES})

# 链接 Qt 库
target_link_libraries(${PROJECT_NAME}
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
)

# 安装
install(TARGETS ${PROJECT_NAME} DESTINATION bin)
```

---

## 10. 扩展指南

### 10.1 添加新的清理类别

1. 在 `analyzewidget.h` 的 `ScanCategory` 枚举中添加新类别
2. 在 `AnalyzeWidget::scanCategory()` 中实现扫描逻辑
3. 在 `diskcleaner.cpp` 中添加清理方法
4. 更新 `smartCleanup()` 方法（如果需要）

### 10.2 添加新的 UI 组件

1. 在 `src/gui/` 目录创建新的 `.cpp/.h` 文件
2. 继承 `QWidget` 或其他 Qt 控件
3. 在 `mainwindow.cpp` 中集成新组件
4. 更新 `CMakeLists.txt` 添加源文件

### 10.3 添加新的配置项

1. 在 `config.h` 中添加 getter/setter 声明
2. 在 `config.cpp` 中实现并设置默认值
3. 在 UI 中添加对应的设置控件

---

## 附录

### A. 文件大小统计

| 文件 | 行数 | 大小 |
|------|------|------|
| analyzewidget.cpp | ~1500 | 52.76 KB |
| cleanupdialog.cpp | ~1450 | 45.61 KB |
| resourceswidget.cpp | ~1260 | 41.08 KB |
| mainwindow.cpp | ~500 | 15.79 KB |
| dashboardwidget.cpp | ~630 | 19.77 KB |
| diskcleaner.cpp | ~460 | 12.34 KB |
| diskanalyzer.cpp | ~310 | 8.33 KB |
| historychart.cpp | ~250 | 7.25 KB |
| systeminfo.cpp | ~220 | 5.32 KB |
| progressdialog.cpp | ~150 | 4.87 KB |
| logger.cpp | ~144 | 3.32 KB |
| config.cpp | ~150 | 2.89 KB |
| main.cpp | ~52 | 1.31 KB |

### B. 参考资料

- [Qt 6 Documentation](https://doc.qt.io/qt-6/)
- [Deepin Developer Documentation](https://github.com/linuxdeepin/developer-center)
- [Linglong Application Format](https://linglong.dev/)
- [Deepin Immutable System](https://github.com/linuxdeepin/immutable-image)

---

*本文档由 AI 自动生成，基于项目源码分析*
