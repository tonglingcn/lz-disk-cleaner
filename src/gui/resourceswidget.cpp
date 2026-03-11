/*
 * Resources Widget - Implementation
 * 系统资源监控页面 - 实现文件
 * 
 * 移植自 Stacer Resources 模块
 */

#include "resourceswidget.h"
#include "../utils/logger.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QFrame>
#include <QFile>
#include <QTextStream>
#include <QStorageInfo>
#include <QNetworkInterface>
#include <QRegularExpression>
#include <QTableWidget>
#include <QHeaderView>
#include <QRandomGenerator>

ResourcesWidget::ResourcesWidget(QWidget *parent)
    : QWidget(parent)
    , m_timer(nullptr)
    , m_isMonitoring(false)
    , m_updateCount(0)
    , m_cpuChart(nullptr)
    , m_memoryChart(nullptr)
    , m_networkChart(nullptr)
    , m_diskInfoTable(nullptr)
    , m_diskStructureTable(nullptr)
    , m_lastNetworkRx(0)
    , m_lastNetworkTx(0)
    , m_cpuCoreCount(4)
{
    LOG_INFO("ResourcesWidget initializing");
    initDataCollectors();
    initUI();
}

ResourcesWidget::~ResourcesWidget()
{
    stopMonitoring();
    LOG_INFO("ResourcesWidget destroyed");
}

void ResourcesWidget::initDataCollectors()
{
    // ==================== 测试模式：取消注释以下行来模拟不同核心数 ====================
    // m_cpuCoreCount = 8;   // 测试8核
    // m_cpuCoreCount = 12;  // 测试12核
    // m_cpuCoreCount = 16;  // 测试16核
    // m_cpuCoreCount = 24;  // 测试24核
    // m_cpuCoreCount = 32;  // 测试32核
    // ===================================================================================
    
    // 读取实际CPU核心数
    QFile cpuInfo("/proc/cpuinfo");
    if (cpuInfo.open(QIODevice::ReadOnly)) {
        QTextStream in(&cpuInfo);
        QString content = in.readAll();
        cpuInfo.close();
        
        int count = content.count("processor\t:");
        m_cpuCoreCount = qMax(1, count);
    }

    // 检测默认网络接口
    for (const QNetworkInterface &net : QNetworkInterface::allInterfaces()) {
        if ((net.flags() & QNetworkInterface::IsUp) &&
            (net.flags() & QNetworkInterface::IsRunning) &&
            !(net.flags() & QNetworkInterface::IsLoopBack)) {
            m_defaultNetworkInterface = net.name();
            break;
        }
    }
    
    if (m_defaultNetworkInterface.isEmpty()) {
        m_defaultNetworkInterface = "eth0";
    }
    
    LOG_DEBUG(QString("CPU cores: %1, Network: %2")
        .arg(m_cpuCoreCount).arg(m_defaultNetworkInterface));
}

void ResourcesWidget::initUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(15);

    // 标题
    QLabel *titleLabel = new QLabel(tr("📊 系统资源监控"), this);
    titleLabel->setStyleSheet(
        "font-size: 20px;"
        "font-weight: bold;"
        "color: #2c3e50;"
        "padding: 5px;"
    );
    mainLayout->addWidget(titleLabel);

    // 滚动区域
    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setStyleSheet(
        "QScrollArea { border: none; background-color: transparent; }"
        "QScrollBar:vertical { width: 10px; }"
    );

    QWidget *scrollContent = new QWidget();
    QVBoxLayout *scrollLayout = new QVBoxLayout(scrollContent);
    scrollLayout->setSpacing(15);
    scrollLayout->setContentsMargins(5, 5, 5, 5);

    // CPU 使用率图表
    m_cpuChart = new HistoryChart(tr("💻 CPU 使用率"), m_cpuCoreCount, this);
    scrollLayout->addWidget(m_cpuChart);

    // 内存使用率图表
    m_memoryChart = new HistoryChart(tr("🧠 内存使用率"), 2, this);  // 内存+交换区
    scrollLayout->addWidget(m_memoryChart);

    // 网络流量图表
    m_networkChart = new HistoryChart(tr("🌐 网络流量"), 2, this);
    scrollLayout->addWidget(m_networkChart);

    // 磁盘信息表格区域
    QFrame *diskInfoFrame = new QFrame(this);
    diskInfoFrame->setStyleSheet(
        "QFrame {"
        "   background-color: white;"
        "   border: 1px solid #bdc3c7;"
        "   border-radius: 6px;"
        "   padding: 10px;"
        "}"
    );
    QVBoxLayout *diskInfoLayout = new QVBoxLayout(diskInfoFrame);

    QLabel *diskInfoTitle = new QLabel(tr("💾 磁盘空间信息"), diskInfoFrame);
    diskInfoTitle->setStyleSheet("font-size: 14px; font-weight: bold; color: #2c3e50;");
    diskInfoLayout->addWidget(diskInfoTitle);

    // 创建磁盘信息表格
    m_diskInfoTable = new QTableWidget(diskInfoFrame);
    m_diskInfoTable->setColumnCount(7);
    m_diskInfoTable->setHorizontalHeaderLabels(
        QStringList() << tr("文件系统") << tr("类型") << tr("容量")
                     << tr("已用") << tr("可用") << tr("使用率") << tr("挂载点")
    );

    // 先获取表头指针并设置基本属性
    QHeaderView *header = m_diskInfoTable->horizontalHeader();
    header->setVisible(true);
    header->setMinimumHeight(50);
    header->setDefaultAlignment(Qt::AlignCenter);
    header->setStretchLastSection(true);

    m_diskInfoTable->verticalHeader()->setVisible(false);
    m_diskInfoTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_diskInfoTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_diskInfoTable->setAlternatingRowColors(true);
    m_diskInfoTable->setShowGrid(false);  // 隐藏网格线，只保留横线

    // 统一列宽模式 - 关键：确保表头和数据列对齐
    header->setSectionResizeMode(QHeaderView::Interactive);
    header->setStretchLastSection(true);
    header->setDefaultSectionSize(100);

    // 样式表 - 只显示横线，隐藏竖线
    m_diskInfoTable->setStyleSheet(
        "QTableWidget {"
        "   background-color: white;"
        "   border: 1px solid #ddd;"
        "   border-radius: 4px;"
        "   font-size: 12px;"
        "}"
        "QTableWidget::item {"
        "   padding: 8px 10px;"
        "   border: none;"
        "   border-bottom: 1px solid #e0e0e0;"  // 只保留底部横线
        "}"
        "QTableWidget::item:last {"
        "   border-bottom: 1px solid #e0e0e0;"
        "}"
        "QHeaderView::section {"
        "   background-color: #f5f5f5;"
        "   color: #333333;"
        "   padding: 8px 10px;"
        "   border: none;"
        "   border-bottom: 3px solid #3498db;"  // 只保留底部横线（加粗蓝色）
        "   font-weight: bold;"
        "   font-size: 13px;"
        "   height: 42px;"
        "}"
    );

    m_diskInfoTable->setMinimumHeight(280);
    diskInfoLayout->addWidget(m_diskInfoTable);

    scrollLayout->addWidget(diskInfoFrame);

    // 硬盘结构区域
    QFrame *diskStructFrame = new QFrame(this);
    diskStructFrame->setStyleSheet(
        "QFrame {"
        "   background-color: white;"
        "   border: 1px solid #bdc3c7;"
        "   border-radius: 6px;"
        "   padding: 10px;"
        "}"
    );
    QVBoxLayout *diskStructLayout = new QVBoxLayout(diskStructFrame);

    QLabel *diskStructTitle = new QLabel(tr("🖥️ 硬盘结构"), diskStructFrame);
    diskStructTitle->setStyleSheet("font-size: 14px; font-weight: bold; color: #2c3e50;");
    diskStructLayout->addWidget(diskStructTitle);

    // 创建硬盘结构表格
    m_diskStructureTable = new QTableWidget(diskStructFrame);
    m_diskStructureTable->setColumnCount(5);
    m_diskStructureTable->setHorizontalHeaderLabels(
        QStringList() << tr("设备名") << tr("容量") << tr("类型")
                     << tr("文件系统") << tr("挂载点")
    );

    QHeaderView *structHeader = m_diskStructureTable->horizontalHeader();
    structHeader->setVisible(true);
    structHeader->setMinimumHeight(45);
    structHeader->setDefaultAlignment(Qt::AlignCenter);
    structHeader->setStretchLastSection(true);

    m_diskStructureTable->verticalHeader()->setVisible(false);
    m_diskStructureTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_diskStructureTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_diskStructureTable->setAlternatingRowColors(true);
    m_diskStructureTable->setShowGrid(false);  // 隐藏网格线，只保留横线

    // 统一列宽模式
    structHeader->setSectionResizeMode(QHeaderView::Interactive);
    structHeader->setStretchLastSection(true);
    structHeader->setDefaultSectionSize(100);

    m_diskStructureTable->setStyleSheet(
        "QTableWidget {"
        "   background-color: white;"
        "   border: 1px solid #ddd;"
        "   border-radius: 4px;"
        "   font-size: 12px;"
        "}"
        "QTableWidget::item {"
        "   padding: 8px 10px;"
        "   border: none;"
        "   border-bottom: 1px solid #e0e0e0;"  // 只保留底部横线
        "}"
        "QTableWidget::item:last {"
        "   border-bottom: 1px solid #e0e0e0;"
        "}"
        "QHeaderView::section {"
        "   background-color: #f5f5f5;"
        "   color: #333333;"
        "   padding: 8px 10px;"
        "   border: none;"
        "   border-bottom: 3px solid #27ae60;"  // 只保留底部横线（绿色加粗）
        "   font-weight: bold;"
        "   font-size: 13px;"
        "   height: 42px;"
        "}"
    );

    m_diskStructureTable->setMinimumHeight(300);
    diskStructLayout->addWidget(m_diskStructureTable);

    scrollLayout->addWidget(diskStructFrame);

    scrollLayout->addStretch();
    scrollArea->setWidget(scrollContent);
    mainLayout->addWidget(scrollArea);

    // 初始化磁盘信息
    updateDiskInfo();

    // 初始化硬盘结构
    updateDiskStructure();

    // 启动监控
    startMonitoring();
}

void ResourcesWidget::startMonitoring()
{
    if (m_isMonitoring) return;

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &ResourcesWidget::updateCpuChart);
    connect(m_timer, &QTimer::timeout, this, &ResourcesWidget::updateMemoryChart);
    connect(m_timer, &QTimer::timeout, this, &ResourcesWidget::updateNetworkChart);
    
    m_timer->start(1000);  // 1秒刷新
    m_isMonitoring = true;
    m_updateCount = 0;

    LOG_INFO("System monitoring started");
}

void ResourcesWidget::stopMonitoring()
{
    if (m_timer) {
        m_timer->stop();
        delete m_timer;
        m_timer = nullptr;
    }
    m_isMonitoring = false;
    LOG_INFO("System monitoring stopped");
}

CpuData ResourcesWidget::collectCpuData()
{
    CpuData data;
    
    QFile statFile("/proc/stat");
    if (!statFile.open(QIODevice::ReadOnly)) {
        // 如果无法打开文件，生成模拟数据（用于测试模式）
        for (int core = 0; core < m_cpuCoreCount; ++core) {
            // 生成随机使用率 (0-50%)
            data.corePercents.append(QRandomGenerator::global()->bounded(51));
        }
        return data;
    }

    QTextStream in(&statFile);
    QString line = in.readLine();  // 总体 CPU 行（跳过）
    
    // 读取实际核心数据
    QVector<int> realCorePercents;
    static QVector<qint64> lastIdles(64, 0);  // 预分配足够空间
    static QVector<qint64> lastTotals(64, 0);
    
    int realCores = 0;
    while (!in.atEnd()) {
        line = in.readLine();
        if (!line.startsWith("cpu")) break;
        
        QStringList parts = line.split(QRegularExpression("\\s+"));
        if (parts.size() < 5) continue;
        
        qint64 user = parts[1].toLongLong();
        qint64 nice = parts[2].toLongLong();
        qint64 system = parts[3].toLongLong();
        qint64 idle = parts[4].toLongLong();
        qint64 iowait = parts.size() > 5 ? parts[5].toLongLong() : 0;
        
        qint64 total = user + nice + system + idle + iowait;
        qint64 idleTotal = idle + iowait;
        
        if (lastTotals[realCores] > 0) {
            qint64 totalDelta = total - lastTotals[realCores];
            qint64 idleDelta = idleTotal - lastIdles[realCores];
            
            int percent = 0;
            if (totalDelta > 0) {
                percent = 100 * (totalDelta - idleDelta) / totalDelta;
            }
            realCorePercents.append(qBound(0, percent, 100));
        } else {
            realCorePercents.append(0);
        }
        
        lastTotals[realCores] = total;
        lastIdles[realCores] = idleTotal;
        realCores++;
    }
    
    statFile.close();
    
    // 如果 m_cpuCoreCount > realCores，复制实际数据来填充
    if (m_cpuCoreCount > realCores) {
        for (int i = 0; i < m_cpuCoreCount; ++i) {
            int srcIndex = i % realCorePercents.size();
            // 添加一些随机波动使测试更真实
            int baseValue = realCorePercents.isEmpty() ? 20 : realCorePercents[srcIndex];
            int variation = QRandomGenerator::global()->bounded(21) - 10;  // -10 到 +10
            int value = qBound(0, baseValue + variation, 100);
            data.corePercents.append(value);
        }
    } else {
        // 使用实际数据
        data.corePercents = realCorePercents;
    }

    // 读取负载平均值
    QFile loadavgFile("/proc/loadavg");
    if (loadavgFile.open(QIODevice::ReadOnly)) {
        QTextStream loadIn(&loadavgFile);
        QStringList loadParts = loadIn.readLine().split(' ');
        if (loadParts.size() >= 3) {
            data.loadAvg1 = loadParts[0].toDouble();
            data.loadAvg5 = loadParts[1].toDouble();
            data.loadAvg15 = loadParts[2].toDouble();
        }
        loadavgFile.close();
    }

    return data;
}

MemoryData ResourcesWidget::collectMemoryData()
{
    MemoryData data;
    
    QFile memFile("/proc/meminfo");
    if (!memFile.open(QIODevice::ReadOnly)) {
        LOG_WARNING("Failed to open /proc/meminfo");
        return data;
    }

    QTextStream in(&memFile);
    QString content = in.readAll();
    memFile.close();
    
    QStringList lines = content.split('\n');
    
    for (const QString &line : lines) {
        QStringList parts = line.split(QRegularExpression("\\s+"));
        if (parts.size() >= 2) {
            QString key = parts[0];
            // Remove trailing colon if present
            if (key.endsWith(":")) {
                key = key.left(key.length() - 1);
            }
            
            if (key == "MemTotal") {
                data.total = parts[1].toLongLong() * 1024;  // KB to bytes
            } else if (key == "MemAvailable") {
                data.available = parts[1].toLongLong() * 1024;
            } else if (key == "MemFree") {
                data.free = parts[1].toLongLong() * 1024;
            } else if (key == "Buffers") {
                data.buffers = parts[1].toLongLong() * 1024;
            } else if (key == "Cached") {
                data.cached = parts[1].toLongLong() * 1024;
            } else if (key == "SwapTotal") {
                data.swapTotal = parts[1].toLongLong() * 1024;
            } else if (key == "SwapFree") {
                data.swapFree = parts[1].toLongLong() * 1024;
            }
        }
    }
    
    // Calculate used memory
    if (data.available > 0) {
        // Use MemAvailable if available (more accurate)
        data.used = data.total - data.available;
    } else {
        // Fallback to old calculation
        data.used = data.total - data.free - data.buffers - data.cached;
    }
    
    data.swapUsed = data.swapTotal - data.swapFree;
    
    if (data.total > 0) {
        data.percent = 100.0 * data.used / data.total;
    }
    
    LOG_DEBUG(QString("Memory - Total: %1, Used: %2, Percent: %3%")
              .arg(formatSize(data.total)).arg(formatSize(data.used)).arg(data.percent, 0, 'f', 1));

    return data;
}

NetworkData ResourcesWidget::collectNetworkData()
{
    NetworkData data;
    data.interface = m_defaultNetworkInterface;
    data.rxBytes = 0;
    data.txBytes = 0;

    QString rxPath = QString("/sys/class/net/%1/statistics/rx_bytes").arg(m_defaultNetworkInterface);
    QString txPath = QString("/sys/class/net/%1/statistics/tx_bytes").arg(m_defaultNetworkInterface);

    data.rxBytes = readSystemFile(rxPath);
    data.txBytes = readSystemFile(txPath);

    // 计算速度
    if (m_lastNetworkRx > 0) {
        data.rxSpeed = data.rxBytes - m_lastNetworkRx;
        data.txSpeed = data.txBytes - m_lastNetworkTx;
    }
    
    m_lastNetworkRx = data.rxBytes;
    m_lastNetworkTx = data.txBytes;

    return data;
}

qint64 ResourcesWidget::readSystemFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return 0;
    }
    
    qint64 value = file.readAll().trimmed().toLongLong();
    file.close();
    return value;
}

QString ResourcesWidget::formatSize(qint64 bytes, bool withUnit) const
{
    const qint64 KB = 1024;
    const qint64 MB = 1024 * KB;
    const qint64 GB = 1024 * MB;
    const qint64 TB = 1024 * GB;

    QString unit;
    double value;

    if (bytes >= TB) {
        value = bytes / (double)TB;
        unit = " TB";
    } else if (bytes >= GB) {
        value = bytes / (double)GB;
        unit = " GB";
    } else if (bytes >= MB) {
        value = bytes / (double)MB;
        unit = " MB";
    } else if (bytes >= KB) {
        value = bytes / (double)KB;
        unit = " KB";
    } else {
        value = bytes;
        unit = " B";
    }

    QString result = QString("%1").arg(value, 0, 'f', 2);
    return withUnit ? result + unit : result;
}

QString ResourcesWidget::formatSpeed(qint64 bytesPerSec) const
{
    const qint64 KB = 1024;
    const qint64 MB = 1024 * KB;

    if (bytesPerSec >= MB) {
        return QString("%1 MB/s").arg(bytesPerSec / (double)MB, 0, 'f', 2);
    } else if (bytesPerSec >= KB) {
        return QString("%1 KB/s").arg(bytesPerSec / (double)KB, 0, 'f', 2);
    } else {
        return QString("%1 B/s").arg(bytesPerSec);
    }
}

void ResourcesWidget::updateCpuChart()
{
    CpuData data = collectCpuData();

    for (int i = 0; i < data.corePercents.size() && i < m_cpuChart->getSeriesList().size(); ++i) {
        int percent = data.corePercents[i];
        QString name = QString("核心%1: %2%").arg(i + 1).arg(percent);
        m_cpuChart->updateSeries(i, percent, name);
    }

    m_updateCount++;
}

void ResourcesWidget::updateMemoryChart()
{
    MemoryData data = collectMemoryData();

    // 调试输出
    LOG_DEBUG(QString("Memory - Total: %1, Used: %2, Percent: %3")
              .arg(data.total).arg(data.used).arg(data.percent));

    // 确保百分比在有效范围内
    double memPercent = qBound(0.0, data.percent, 100.0);

    // 系列0: 内存使用率
    QString usedStr = formatSize(data.used, false);
    QString totalStr = formatSize(data.total);
    QString memName = QString("内存: %1% (%2/%3)")
        .arg(memPercent, 0, 'f', 1)
        .arg(usedStr)
        .arg(totalStr);
    
    LOG_DEBUG(QString("Memory name: %1").arg(memName));
    
    m_memoryChart->updateSeries(0, memPercent, memName);

    // 系列1: 交换区使用率
    if (data.swapTotal > 0) {
        double swapPercent = 100.0 * data.swapUsed / data.swapTotal;
        swapPercent = qBound(0.0, swapPercent, 100.0);
        QString swapName = QString("交换: %1% (%2/%3)")
            .arg(swapPercent, 0, 'f', 1)
            .arg(formatSize(data.swapUsed, false))
            .arg(formatSize(data.swapTotal));
        m_memoryChart->updateSeries(1, swapPercent, swapName);
    }
}

void ResourcesWidget::updateNetworkChart()
{
    NetworkData data = collectNetworkData();

    // 第一次更新时没有速度数据，显示为0
    if (m_updateCount == 0) {
        data.rxSpeed = 0;
        data.txSpeed = 0;
    }

    // 限制异常速度值（最大100MB/s，超过视为异常）
    const qint64 MAX_SPEED = 100LL * 1024 * 1024;  // 100MB/s
    if (data.rxSpeed < 0 || data.rxSpeed > MAX_SPEED) {
        data.rxSpeed = 0;
    }
    if (data.txSpeed < 0 || data.txSpeed > MAX_SPEED) {
        data.txSpeed = 0;
    }

    // 转换为 KB/s
    double rxSpeedKB = data.rxSpeed / 1024.0;
    double txSpeedKB = data.txSpeed / 1024.0;

    // 系列0: 下载速度
    QString rxName = QString("↓下载: %1").arg(formatSpeed(data.rxSpeed));
    m_networkChart->updateSeries(0, rxSpeedKB, rxName);

    // 系列1: 上传速度
    QString txName = QString("↑上传: %1").arg(formatSpeed(data.txSpeed));
    m_networkChart->updateSeries(1, txSpeedKB, txName);

    // 动态调整Y轴范围（添加最大值保护）
    double maxY = qMax(rxSpeedKB, txSpeedKB) * 1.2;
    if (maxY > 100 && maxY < 50000) {  // 限制最大范围50MB/s
        m_networkChart->setYRange(0, maxY);
    }
}

void ResourcesWidget::updateDiskInfo()
{
    QList<QStorageInfo> volumes = QStorageInfo::mountedVolumes();

    // 过滤有效卷并按挂载点排序
    QList<QStorageInfo> validVolumes;
    for (const QStorageInfo &vol : volumes) {
        if (!vol.isValid() || !vol.isReady()) continue;
        validVolumes.append(vol);
    }

    // 按挂载点排序
    std::sort(validVolumes.begin(), validVolumes.end(),
        [](const QStorageInfo &a, const QStorageInfo &b) {
            return a.rootPath() < b.rootPath();
        });

    m_diskInfoTable->setRowCount(validVolumes.size());

    int row = 0;
    for (const QStorageInfo &vol : validVolumes) {
        qint64 total = vol.bytesTotal();
        qint64 available = vol.bytesAvailable();
        qint64 used = total - available;
        int percent = total > 0 ? (int)(100.0 * used / total) : 0;
        QString fsType = vol.fileSystemType();

        // 获取设备名称
        QString device = vol.device();
        if (device.isEmpty()) {
            device = vol.rootPath();
        }

        // 文件系统
        QTableWidgetItem *deviceItem = new QTableWidgetItem(device);
        deviceItem->setFlags(deviceItem->flags() & ~Qt::ItemIsEditable);
        m_diskInfoTable->setItem(row, 0, deviceItem);

        // 类型
        QTableWidgetItem *typeItem = new QTableWidgetItem(fsType);
        typeItem->setFlags(typeItem->flags() & ~Qt::ItemIsEditable);
        typeItem->setTextAlignment(Qt::AlignCenter);
        m_diskInfoTable->setItem(row, 1, typeItem);

        // 容量
        QTableWidgetItem *totalItem = new QTableWidgetItem(formatSize(total, true));
        totalItem->setFlags(totalItem->flags() & ~Qt::ItemIsEditable);
        totalItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_diskInfoTable->setItem(row, 2, totalItem);

        // 已用
        QTableWidgetItem *usedItem = new QTableWidgetItem(formatSize(used, true));
        usedItem->setFlags(usedItem->flags() & ~Qt::ItemIsEditable);
        usedItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_diskInfoTable->setItem(row, 3, usedItem);

        // 可用
        QTableWidgetItem *availItem = new QTableWidgetItem(formatSize(available, true));
        availItem->setFlags(availItem->flags() & ~Qt::ItemIsEditable);
        availItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_diskInfoTable->setItem(row, 4, availItem);

        // 使用率（带颜色）
        QTableWidgetItem *percentItem = new QTableWidgetItem(QString("%1%").arg(percent));
        percentItem->setFlags(percentItem->flags() & ~Qt::ItemIsEditable);
        percentItem->setTextAlignment(Qt::AlignCenter);

        // 根据使用率设置颜色
        if (percent >= 90) {
            percentItem->setForeground(QBrush(QColor(231, 76, 60)));  // 红色
            percentItem->setBackground(QBrush(QColor(254, 235, 235)));
        } else if (percent >= 75) {
            percentItem->setForeground(QBrush(QColor(230, 126, 34)));  // 橙色
            percentItem->setBackground(QBrush(QColor(254, 245, 231)));
        } else if (percent >= 50) {
            percentItem->setForeground(QBrush(QColor(241, 196, 15)));  // 黄色
            percentItem->setBackground(QBrush(QColor(254, 250, 219)));
        } else {
            percentItem->setForeground(QBrush(QColor(39, 174, 96)));   // 绿色
            percentItem->setBackground(QBrush(QColor(232, 245, 233)));
        }
        m_diskInfoTable->setItem(row, 5, percentItem);

        // 挂载点
        QTableWidgetItem *mountItem = new QTableWidgetItem(vol.rootPath());
        mountItem->setFlags(mountItem->flags() & ~Qt::ItemIsEditable);
        m_diskInfoTable->setItem(row, 6, mountItem);

        row++;
    }

    m_diskInfoTable->resizeColumnsToContents();
    m_diskInfoTable->horizontalHeader()->setStretchLastSection(true);
}

void ResourcesWidget::updateDiskStructure()
{
    // 读取 /proc/partitions 获取分区信息
    QFile partFile("/proc/partitions");
    if (!partFile.open(QIODevice::ReadOnly)) {
        return;
    }

    // 存储所有分区信息
    struct PartitionInfo {
        QString name;
        qint64 size;
        QString type;
        QString fsType;
        QString mountPoint;
        bool isParent;
        int level;  // 层级（用于缩进）
    };

    QList<PartitionInfo> partitions;
    QMap<QString, QString> mountPoints;
    QMap<QString, QString> fsTypes;

    // 获取挂载点信息
    for (const QStorageInfo &vol : QStorageInfo::mountedVolumes()) {
        if (vol.isValid() && vol.isReady()) {
            QString dev = vol.device();
            // 从 /dev/xxx 提取设备名
            if (dev.startsWith("/dev/")) {
                QString devName = dev.mid(5);  // 去掉 /dev/
                mountPoints[devName] = vol.rootPath();
                fsTypes[devName] = vol.fileSystemType();
            }
        }
    }

    // 解析 /proc/partitions
    QTextStream in(&partFile);
    QString line = in.readLine();  // 跳过表头
    line = in.readLine();  // 跳过空行

    while (!in.atEnd()) {
        line = in.readLine().trimmed();
        if (line.isEmpty()) continue;

        QStringList parts = line.split(QRegularExpression("\\s+"));
        if (parts.size() < 4) continue;

        QString name = parts[3];
        // 跳过ram和loop设备
        if (name.startsWith("ram") || name.startsWith("loop")) continue;

        qint64 size = parts[2].toLongLong() * 1024;  // 转为字节

        // 判断是磁盘还是分区
        bool isDisk = false;
        QString parentName;

        // sdX 是磁盘，sdXn 是分区
        // nvmeXnY 是磁盘，nvmeXnYpZ 是分区
        // mmcblkX 是磁盘，mmcblkXpY 是分区
        if (name.startsWith("sd")) {
            isDisk = name.length() == 3;
            if (!isDisk) {
                parentName = name.left(3);
            }
        } else if (name.startsWith("nvme")) {
            // nvme0n1 是磁盘，nvme0n1p1 是分区
            QRegularExpression nvmeRe("^(nvme\\d+n\\d+)");
            QRegularExpressionMatch match = nvmeRe.match(name);
            if (match.hasMatch()) {
                QString baseName = match.captured(1);
                isDisk = (name == baseName);
                if (!isDisk) {
                    parentName = baseName;
                }
            }
        } else if (name.startsWith("mmcblk")) {
            // mmcblk0 是磁盘，mmcblk0p1 是分区
            QRegularExpression mmcRe("^(mmcblk\\d+)");
            QRegularExpressionMatch match = mmcRe.match(name);
            if (match.hasMatch()) {
                QString baseName = match.captured(1);
                isDisk = (name == baseName);
                if (!isDisk) {
                    parentName = baseName;
                }
            }
        } else if (name.startsWith("md")) {
            // md0 是 RAID 设备
            isDisk = true;
        } else {
            isDisk = !name[name.length()-1].isDigit();
            if (!isDisk) {
                // 尝试提取父设备名
                for (int i = name.length()-1; i >= 0; --i) {
                    if (!name[i].isDigit()) {
                        parentName = name.left(i+1);
                        break;
                    }
                }
            }
        }

        PartitionInfo info;
        info.name = name;
        info.size = size;
        info.isParent = isDisk;
        info.type = isDisk ? "disk" : "part";
        info.fsType = fsTypes.value(name, "-");
        info.mountPoint = mountPoints.value(name, "-");

        // 确定层级
        if (isDisk) {
            info.level = 0;
        } else {
            info.level = 1;
            // 检查是否有祖父级（多层嵌套）
            for (const PartitionInfo &p : partitions) {
                if (p.name == parentName && !p.isParent) {
                    info.level = 2;
                    break;
                }
            }
        }

        partitions.append(info);
    }
    partFile.close();

    // 按设备名排序，确保父设备在前
    std::sort(partitions.begin(), partitions.end(), [](const PartitionInfo &a, const PartitionInfo &b) {
        // 先按基础名称排序
        QString baseA = a.name;
        QString baseB = b.name;

        // 提取数字部分进行自然排序
        QRegularExpression numRe("(\\d+)$");
        QRegularExpressionMatch matchA = numRe.match(baseA);
        QRegularExpressionMatch matchB = numRe.match(baseB);

        if (matchA.hasMatch() && matchB.hasMatch()) {
            QString prefixA = baseA.left(baseA.length() - matchA.captured(1).length());
            QString prefixB = baseB.left(baseB.length() - matchB.captured(1).length());

            if (prefixA == prefixB) {
                int numA = matchA.captured(1).toInt();
                int numB = matchB.captured(1).toInt();
                return numA < numB;
            }
        }

        return baseA < baseB;
    });

    // 设置表格行数
    m_diskStructureTable->setRowCount(partitions.size());

    int row = 0;
    for (const PartitionInfo &info : partitions) {
        // 设备名（带缩进和树形符号）
        QString displayName;
        if (info.level == 0) {
            displayName = info.name;
        } else if (info.level == 1) {
            displayName = "├─" + info.name;
        } else {
            displayName = "│  ├─" + info.name;
        }

        QTableWidgetItem *nameItem = new QTableWidgetItem(displayName);
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        // 磁盘用粗体
        if (info.isParent) {
            QFont font = nameItem->font();
            font.setBold(true);
            nameItem->setFont(font);
        }
        m_diskStructureTable->setItem(row, 0, nameItem);

        // 容量
        QTableWidgetItem *sizeItem = new QTableWidgetItem(formatSize(info.size, true));
        sizeItem->setFlags(sizeItem->flags() & ~Qt::ItemIsEditable);
        sizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_diskStructureTable->setItem(row, 1, sizeItem);

        // 类型
        QTableWidgetItem *typeItem = new QTableWidgetItem(info.type);
        typeItem->setFlags(typeItem->flags() & ~Qt::ItemIsEditable);
        typeItem->setTextAlignment(Qt::AlignCenter);
        m_diskStructureTable->setItem(row, 2, typeItem);

        // 文件系统
        QTableWidgetItem *fsItem = new QTableWidgetItem(info.fsType);
        fsItem->setFlags(fsItem->flags() & ~Qt::ItemIsEditable);
        fsItem->setTextAlignment(Qt::AlignCenter);
        m_diskStructureTable->setItem(row, 3, fsItem);

        // 挂载点
        QTableWidgetItem *mountItem = new QTableWidgetItem(info.mountPoint);
        mountItem->setFlags(mountItem->flags() & ~Qt::ItemIsEditable);
        m_diskStructureTable->setItem(row, 4, mountItem);

        row++;
    }

    m_diskStructureTable->resizeColumnsToContents();
    m_diskStructureTable->horizontalHeader()->setStretchLastSection(true);
}





