/*
 * Dashboard Widget - Implementation
 * 仪表盘组件 - 实现
 */

#include "dashboardwidget.h"
#include "../utils/logger.h"
#include <QPainter>
#include <QPainterPath>
#include <QGridLayout>
#include <QFrame>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QProgressBar>
#include <QRegularExpression>
#include <QApplication>
#include <cmath>

// ============================================================================
// CircularProgressWidget Implementation
// ============================================================================

CircularProgressWidget::CircularProgressWidget(const QString &title, const QColor &color, QWidget *parent)
    : QWidget(parent)
    , m_title(title)
    , m_color(color)
    , m_value(0)
    , m_maxValue(100)
    , m_text("0%")
    , m_darkMode(false)
{
    setMinimumSize(250, 200);
}

void CircularProgressWidget::setDarkMode(bool dark)
{
    m_darkMode = dark;
    update();
}

void CircularProgressWidget::setValue(double value)
{
    m_value = value;
    update();
}

void CircularProgressWidget::setText(const QString &text)
{
    m_text = text;
    update();
}

void CircularProgressWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    int width = this->width();
    int height = this->height();
    
    // 计算圆形区域
    int size = qMin(width - 40, height - 120);
    int centerX = width / 2;
    int centerY = height / 2 - 20;
    int radius = size / 2;
    int ringWidth = 18;
    
    // 根据主题选择颜色
    QColor bgRingColor = m_darkMode ? QColor(80, 80, 80) : QColor(230, 230, 230);
    QColor titleColor = m_darkMode ? QColor(160, 160, 160) : QColor(100, 100, 100);
    QColor descColor = m_darkMode ? QColor(180, 180, 180) : QColor(120, 120, 120);
    
    // 绘制背景圆环
    painter.setPen(QPen(bgRingColor, ringWidth, Qt::SolidLine, Qt::RoundCap));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(QPointF(centerX, centerY), radius - ringWidth/2, radius - ringWidth/2);
    
    // 绘制进度圆环
    double percent = (m_value / m_maxValue) * 100.0;
    double angle = (percent / 100.0) * 360.0;
    
    painter.setPen(QPen(m_color, ringWidth, Qt::SolidLine, Qt::RoundCap));
    painter.setBrush(Qt::NoBrush);
    
    // 从顶部开始绘制（-90度）
    int startAngle = 90 * 16;
    int spanAngle = -angle * 16;
    
    QRectF rect(centerX - radius + ringWidth/2, centerY - radius + ringWidth/2, 
                (radius - ringWidth/2) * 2, (radius - ringWidth/2) * 2);
    painter.drawArc(rect, startAngle, spanAngle);
    
    // 绘制标题（在圆环上方）
    painter.setPen(titleColor);
    QFont titleFont = painter.font();
    titleFont.setPointSize(14);
    titleFont.setBold(false);
    painter.setFont(titleFont);
    painter.drawText(QRect(0, 10, width, 30), Qt::AlignCenter, m_title);
    
    // 绘制中心百分比
    painter.setPen(m_color);
    QFont percentFont = painter.font();
    percentFont.setPointSize(20);
    percentFont.setBold(true);
    painter.setFont(percentFont);
    
    QString percentText = QString("%1%").arg(percent, 0, 'f', 1);
    painter.drawText(QRect(centerX - radius, centerY - 20, radius * 2, 40), 
                     Qt::AlignCenter, percentText);
    
    // 绘制底部文字说明
    painter.setPen(descColor);
    QFont descFont = painter.font();
    descFont.setPointSize(12);
    descFont.setBold(false);
    painter.setFont(descFont);
    painter.drawText(QRect(0, height - 40, width, 30), Qt::AlignCenter, m_text);
}

// ============================================================================
// DashboardWidget Implementation
// ============================================================================

DashboardWidget::DashboardWidget(QWidget *parent)
    : QWidget(parent)
    , m_cpuWidget(nullptr)
    , m_memoryWidget(nullptr)
    , m_diskWidget(nullptr)
    , m_systemInfo(nullptr)
    , m_updateTimer(nullptr)
{
    LOG_INFO("Initializing dashboard widget");
    
    m_systemInfo = new SystemInfo(this);
    
    initUI();
    
    // 初始化时立即分析磁盘
    DiskAnalyzer *analyzer = new DiskAnalyzer(this);
    QList<DiskUsage> disks = analyzer->analyzeDiskUsage();
    updateDiskUsage(disks);
    
    // 启动定时器，每秒更新一次
    m_updateTimer = new QTimer(this);
    connect(m_updateTimer, &QTimer::timeout, this, &DashboardWidget::updateSystemStats);
    m_updateTimer->start(1000);
}

DashboardWidget::~DashboardWidget()
{
    if (m_updateTimer) {
        m_updateTimer->stop();
    }
}

void DashboardWidget::initUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(30);
    mainLayout->setContentsMargins(30, 30, 30, 30);
    
    // 检测深色主题并应用相应样式
    applyTheme();
    
    createTopSection();
    createBottomSection();
    
    mainLayout->addStretch();
}

bool DashboardWidget::isDarkTheme()
{
    // 检测系统是否使用深色主题
    // 方法1: 检查环境变量
    QString desktopSession = qEnvironmentVariable("DESKTOP_SESSION");
    QString deepinTheme = qEnvironmentVariable("DEEPIN_THEME");
    
    // 方法2: 通过QPalette检测
    QPalette palette = qApp->palette();
    QColor windowColor = palette.color(QPalette::Window);
    // 如果窗口背景较暗，认为是深色主题
    int brightness = (windowColor.red() * 299 + windowColor.green() * 587 + windowColor.blue() * 114) / 1000;
    
    LOG_DEBUG(QString("Theme detection - brightness: %1").arg(brightness));
    
    return brightness < 128;
}

void DashboardWidget::applyTheme()
{
    bool darkMode = isDarkTheme();
    
    if (darkMode) {
        // 深色主题样式
        setStyleSheet(
            "DashboardWidget { "
            "   background-color: #2d2d2d; "
            "}"
            "QLabel { "
            "   color: #e0e0e0; "
            "   background-color: transparent; "
            "}"
        );
    } else {
        // 浅色主题样式
        setStyleSheet(
            "DashboardWidget { "
            "   background-color: #F5F5F5; "
            "}"
            "QLabel { "
            "   color: #333333; "
            "   background-color: transparent; "
            "}"
        );
    }
}

void DashboardWidget::createTopSection()
{
    // 检测深色主题
    bool darkMode = isDarkTheme();
    
    // 顶部三个圆形进度指示器
    QHBoxLayout *topLayout = new QHBoxLayout();
    topLayout->setSpacing(50);
    
    // CPU 指示器 - 蓝色
    m_cpuWidget = new CircularProgressWidget("CPU", QColor(64, 158, 255), this);
    m_cpuWidget->setMinimumHeight(240);
    m_cpuWidget->setDarkMode(darkMode);
    topLayout->addWidget(m_cpuWidget);
    
    // 内存指示器 - 青绿色
    m_memoryWidget = new CircularProgressWidget("内存", QColor(19, 194, 194), this);
    m_memoryWidget->setMinimumHeight(240);
    m_memoryWidget->setDarkMode(darkMode);
    topLayout->addWidget(m_memoryWidget);
    
    // 磁盘指示器 - 紫色
    m_diskWidget = new CircularProgressWidget("磁盘容量", QColor(114, 46, 209), this);
    m_diskWidget->setMinimumHeight(240);
    m_diskWidget->setDarkMode(darkMode);
    topLayout->addWidget(m_diskWidget);
    
    QVBoxLayout *mainLayout = qobject_cast<QVBoxLayout*>(layout());
    if (mainLayout) {
        mainLayout->addLayout(topLayout);
    }
}

void DashboardWidget::createBottomSection()
{
    // 创建卡片网格布局
    QGridLayout *cardsLayout = new QGridLayout();
    cardsLayout->setSpacing(15);
    cardsLayout->setContentsMargins(0, 15, 0, 0);
    cardsLayout->setColumnStretch(0, 1);
    cardsLayout->setColumnStretch(1, 1);
    cardsLayout->setColumnStretch(2, 1);
    cardsLayout->setRowStretch(0, 0);
    cardsLayout->setRowStretch(1, 0);
    
    SystemInfoData info = m_systemInfo->getSystemInfo();
    
    // 第一行
    cardsLayout->addWidget(createInfoCard("主机名称", info.hostname, "🖥️", QColor(64, 158, 255)), 0, 0);
    cardsLayout->addWidget(createInfoCard("CPU 型号", info.cpuModel, "⚡", QColor(103, 194, 58)), 0, 1);
    cardsLayout->addWidget(createInfoCard("物理内存", formatSize(info.totalMemory), "💾", QColor(230, 162, 60)), 0, 2);

    // 第二行
    cardsLayout->addWidget(createInfoCard("内核版本", info.kernelVersion, "🐧", QColor(64, 158, 255)), 1, 0);
    cardsLayout->addWidget(createInfoCard("显卡", getGPUInfo(), "🎮", QColor(103, 194, 58)), 1, 1);
    cardsLayout->addWidget(createInfoCard("显示适配器", getDisplayInfo(), "🖼️", QColor(230, 162, 60)), 1, 2);
    
    QVBoxLayout *mainLayout = qobject_cast<QVBoxLayout*>(layout());
    if (mainLayout) {
        mainLayout->addLayout(cardsLayout);
    }
    
    // 添加警告标签
    QLabel *warningLabel = new QLabel(tr("⚠️ 警告：数据一旦删除，将无法恢复，请慎重操作！！！"), this);
    warningLabel->setAlignment(Qt::AlignCenter);
    warningLabel->setStyleSheet(
        "QLabel { "
        "   color: #e74c3c; "
        "   font-weight: bold; "
        "   font-size: 13px; "
        "   background-color: rgba(231, 76, 60, 0.1); "
        "   border: 1px solid #e74c3c; "
        "   border-radius: 5px; "
        "   padding: 8px 15px; "
        "   margin: 10px 0px; "
        "}"
    );
    mainLayout->addWidget(warningLabel);
}

QWidget* DashboardWidget::createInfoCard(const QString &title, const QString &value, const QString &icon, const QColor &iconColor)
{
    bool darkMode = isDarkTheme();
    
    QFrame *card = new QFrame(this);
    card->setFrameShape(QFrame::NoFrame);
    
    // 根据主题设置卡片背景色
    QString cardBgColor = darkMode ? "#3d3d3d" : "white";
    card->setStyleSheet(
        "QFrame { "
        "   background-color: " + cardBgColor + "; "
        "   border-radius: 8px; "
        "}"
    );
    card->setMinimumHeight(90);
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    
    QHBoxLayout *layout = new QHBoxLayout(card);
    layout->setSpacing(12);
    layout->setContentsMargins(15, 12, 15, 12);
    
    // 图标
    QLabel *iconLabel = new QLabel(icon, card);
    iconLabel->setStyleSheet(QString(
        "QLabel { "
        "   font-size: 36px; "
        "   color: %1; "
        "   background-color: transparent; "
        "}"
    ).arg(iconColor.name()));
    iconLabel->setFixedSize(50, 50);
    iconLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(iconLabel);
    
    // 文字区域
    QVBoxLayout *textLayout = new QVBoxLayout();
    textLayout->setSpacing(3);
    textLayout->setContentsMargins(0, 0, 0, 0);
    
    // 标题颜色
    QString titleColor = darkMode ? "#a0a0a0" : "#909399";
    QLabel *titleLabel = new QLabel(title, card);
    titleLabel->setStyleSheet(
        "QLabel { "
        "   font-size: 12px; "
        "   color: " + titleColor + "; "
        "   background-color: transparent; "
        "}"
    );
    textLayout->addWidget(titleLabel);
    
    // 值颜色
    QString valueColor = darkMode ? "#ffffff" : "#303133";
    QLabel *valueLabel = new QLabel(value, card);
    valueLabel->setStyleSheet(
        "QLabel { "
        "   font-size: 14px; "
        "   font-weight: bold; "
        "   color: " + valueColor + "; "
        "   background-color: transparent; "
        "}"
    );
    valueLabel->setWordWrap(true);
    textLayout->addWidget(valueLabel);
    textLayout->addStretch();
    
    layout->addLayout(textLayout, 1);
    
    return card;
}

QString DashboardWidget::getUptime()
{
    QFile file("/proc/uptime");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return "N/A";
    }
    
    QTextStream in(&file);
    QString content = in.readAll();
    file.close();
    
    double uptimeSeconds = content.split(' ')[0].toDouble();
    int days = uptimeSeconds / 86400;
    int hours = (int(uptimeSeconds) % 86400) / 3600;
    int minutes = (int(uptimeSeconds) % 3600) / 60;
    
    if (days > 0) {
        return QString("%1天%2小时").arg(days).arg(hours);
    } else if (hours > 0) {
        return QString("%1:%2:%3").arg(hours, 2, 10, QChar('0'))
                                  .arg(minutes, 2, 10, QChar('0'))
                                  .arg(int(uptimeSeconds) % 60, 2, 10, QChar('0'));
    } else {
        return QString("%1分钟").arg(minutes);
    }
}

QString DashboardWidget::getGPUInfo()
{
    // 尝试使用 lspci 获取显卡信息
    QProcess process;
    process.start("lspci", QStringList() << "-v");
    process.waitForFinished(2000);
    
    QString output = QString::fromUtf8(process.readAllStandardOutput());
    
    // 查找所有 VGA 或 3D 控制器
    QStringList lines = output.split('\n');
    QStringList gpuList;
    
    for (const QString &line : lines) {
        if (line.contains("VGA compatible controller", Qt::CaseInsensitive) ||
            line.contains("3D controller", Qt::CaseInsensitive)) {
            // 提取显卡型号（在冒号后面）
            int colonPos = line.indexOf(':');
            if (colonPos != -1) {
                QString gpuInfo = line.mid(colonPos + 1).trimmed();
                
                // 移除多余的信息
                gpuInfo = gpuInfo.replace(QRegularExpression("\\(rev \\w+\\)"), "").trimmed();
                
                // 优化显示：提取关键信息
                // 如果包含方括号，优先显示方括号内的内容（通常是具体型号）
                QRegularExpression bracketRe("\\[([^\\]]+)\\]");
                QRegularExpressionMatch match = bracketRe.match(gpuInfo);
                if (match.hasMatch()) {
                    QString model = match.captured(1);
                    
                    // 如果原文包含厂商名称（NVIDIA, AMD, Intel），保留厂商名
                    QString vendor;
                    if (gpuInfo.contains("NVIDIA", Qt::CaseInsensitive)) {
                        vendor = "NVIDIA ";
                    } else if (gpuInfo.contains("AMD", Qt::CaseInsensitive)) {
                        vendor = "AMD ";
                    } else if (gpuInfo.contains("Intel", Qt::CaseInsensitive)) {
                        vendor = "Intel ";
                    }
                    
                    gpuList.append(vendor + model);
                    continue;
                }
                
                // 如果没有方括号，尝试简化
                // 移除 "Corporation", "Technology", "Inc." 等词
                gpuInfo = gpuInfo.replace("Corporation", "").trimmed();
                gpuInfo = gpuInfo.replace("Technology", "").trimmed();
                gpuInfo = gpuInfo.replace("Inc.", "").trimmed();
                gpuInfo = gpuInfo.replace(QRegularExpression("\\s+"), " ").trimmed();
                
                // 如果还是太长（超过40字符），截断
                if (gpuInfo.length() > 40) {
                    gpuInfo = gpuInfo.left(37) + "...";
                }
                
                gpuList.append(gpuInfo);
            }
        }
    }
    
    if (gpuList.isEmpty()) {
        return "未检测到";
    }
    
    // 将多个显卡信息用换行符分隔显示
    return gpuList.join("\n");
}

QString DashboardWidget::getDisplayInfo()
{
    // 尝试使用 xrandr 获取显示器信息
    QProcess process;
    process.start("xrandr", QStringList() << "--query");
    process.waitForFinished(2000);
    
    QString output = QString::fromUtf8(process.readAllStandardOutput());
    
    // 查找所有连接的显示器
    QStringList lines = output.split('\n');
    QStringList displayList;
    
    for (int i = 0; i < lines.size(); ++i) {
        const QString &line = lines[i];
        // 查找 "connected" 关键字（排除 "disconnected"）
        if (line.contains(" connected", Qt::CaseInsensitive) && 
            !line.contains("disconnected", Qt::CaseInsensitive)) {
            // 提取显示器名称（第一个单词）
            QStringList parts = line.split(QRegularExpression("\\s+"));
            if (!parts.isEmpty()) {
                QString displayName = parts[0];
                QString resolution;
                
                // 尝试从当前行提取分辨率（格式如：1920x1080+0+0）
                QRegularExpression resRe("(\\d+x\\d+)\\+");
                QRegularExpressionMatch match = resRe.match(line);
                if (match.hasMatch()) {
                    resolution = match.captured(1);
                } else {
                    // 如果当前行没有，查找下一行的分辨率
                    if (i + 1 < lines.size()) {
                        QString nextLine = lines[i + 1].trimmed();
                        QRegularExpression resRe2("(\\d+x\\d+)");
                        QRegularExpressionMatch match2 = resRe2.match(nextLine);
                        if (match2.hasMatch()) {
                            resolution = match2.captured(1);
                        }
                    }
                }
                
                // 添加到列表
                if (!resolution.isEmpty()) {
                    displayList.append(QString("%1 (%2)").arg(displayName).arg(resolution));
                } else {
                    displayList.append(displayName);
                }
            }
        }
    }
    
    if (displayList.isEmpty()) {
        return "未检测到";
    }
    
    // 如果有多个显示器，用换行符分隔
    if (displayList.size() > 1) {
        return displayList.join("\n");
    }
    
    return displayList.first();
}

void DashboardWidget::updateSystemStats()
{
    // 更新 CPU 使用率
    double cpuUsage = getCpuUsage();
    m_cpuWidget->setValue(cpuUsage);
    m_cpuWidget->setText(QString("CPU 空闲"));
    
    // 更新内存使用率
    qint64 totalMem = m_systemInfo->getTotalMemory();
    qint64 availMem = m_systemInfo->getAvailableMemory();
    qint64 usedMem = totalMem - availMem;
    double memPercent = (usedMem * 100.0) / totalMem;
    
    m_memoryWidget->setValue(memPercent);
    m_memoryWidget->setText(QString("%1 / %2")
        .arg(formatSize(usedMem))
        .arg(formatSize(totalMem)));
}

void DashboardWidget::updateDiskUsage(const QList<DiskUsage> &disks)
{
    LOG_INFO("Updating disk usage");
    m_diskUsage = disks;
    
    // 累加所有硬盘分区的容量（参考 gxde-system-assistant-gxde 实现）
    qint64 totalDisk = 0;
    qint64 usedDisk = 0;
    
    for (const DiskUsage &disk : disks) {
        // 跳过虚拟文件系统和特殊挂载点
        if (disk.mountpoint.startsWith("/dev") ||
            disk.mountpoint.startsWith("/sys") ||
            disk.mountpoint.startsWith("/proc") ||
            disk.mountpoint.startsWith("/run") ||
            disk.mountpoint.startsWith("/snap") ||
            disk.type == "tmpfs" ||
            disk.type == "devtmpfs" ||
            disk.type == "squashfs" ||
            disk.type == "overlay" ||
            disk.type == "cgmfs") {
            continue;
        }
        totalDisk += disk.total;
        usedDisk += disk.used;
    }
    
    if (totalDisk > 0) {
        double diskPercent = (usedDisk * 100.0) / totalDisk;
        m_diskWidget->setValue(diskPercent);
        m_diskWidget->setText(QString("%1 / %2")
            .arg(formatSize(usedDisk))
            .arg(formatSize(totalDisk)));
    }
}

void DashboardWidget::updateImmutableSystem(const ImmutableSystemInfo &info)
{
    LOG_INFO("Updating immutable system info");
    // 可以在系统信息中添加磐石系统状态
}

void DashboardWidget::updateLinglongApps(const QList<LinglongAppInfo> &apps)
{
    LOG_INFO(QString("Updating Linglong apps: %1 apps").arg(apps.size()));
    // 可以在系统信息中添加玲珑应用数量
}

QString DashboardWidget::formatSize(qint64 bytes)
{
    const qint64 KB = 1024;
    const qint64 MB = 1024 * KB;
    const qint64 GB = 1024 * MB;
    const qint64 TB = 1024 * GB;
    
    if (bytes >= TB) {
        return QString("%1 TiB").arg(bytes / (double)TB, 0, 'f', 1);
    } else if (bytes >= GB) {
        return QString("%1 GiB").arg(bytes / (double)GB, 0, 'f', 1);
    } else if (bytes >= MB) {
        return QString("%1 MiB").arg(bytes / (double)MB, 0, 'f', 1);
    } else if (bytes >= KB) {
        return QString("%1 KiB").arg(bytes / (double)KB, 0, 'f', 1);
    } else {
        return QString("%1 B").arg(bytes);
    }
}

double DashboardWidget::getCpuUsage()
{
    static qint64 lastTotalTime = 0;
    static qint64 lastIdleTime = 0;
    
    QFile file("/proc/stat");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return 0.0;
    }
    
    QTextStream in(&file);
    QString line = in.readLine();
    file.close();
    
    // 解析 CPU 时间
    QStringList parts = line.split(QRegularExpression("\\s+"));
    if (parts.size() < 5) {
        return 0.0;
    }
    
    qint64 user = parts[1].toLongLong();
    qint64 nice = parts[2].toLongLong();
    qint64 system = parts[3].toLongLong();
    qint64 idle = parts[4].toLongLong();
    
    qint64 totalTime = user + nice + system + idle;
    qint64 idleTime = idle;
    
    if (lastTotalTime == 0) {
        lastTotalTime = totalTime;
        lastIdleTime = idleTime;
        return 0.0;
    }
    
    qint64 totalDelta = totalTime - lastTotalTime;
    qint64 idleDelta = idleTime - lastIdleTime;
    
    double usage = 0.0;
    if (totalDelta > 0) {
        usage = 100.0 * (1.0 - (double)idleDelta / totalDelta);
    }
    
    lastTotalTime = totalTime;
    lastIdleTime = idleTime;
    
    return usage;
}
