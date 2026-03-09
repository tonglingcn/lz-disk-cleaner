/*
 * Resources Widget - Header
 * 系统资源监控页面 - 头文件
 * 
 * 移植自 Stacer Resources 模块
 * 显示 CPU、内存、磁盘、网络的实时历史图表
 */

#ifndef RESOURCESWIDGET_H
#define RESOURCESWIDGET_H

#include <QWidget>
#include <QTimer>
#include <QScrollArea>
#include <QLabel>
#include <QTableWidget>
#include <QMap>
#include <QVBoxLayout>

#include "historychart.h"

// 系统资源数据结构
struct CpuData {
    QList<int> corePercents;    // 各核心使用率
    double loadAvg1;            // 1分钟负载
    double loadAvg5;            // 5分钟负载
    double loadAvg15;           // 15分钟负载
};

struct MemoryData {
    qint64 total = 0;
    qint64 used = 0;
    qint64 free = 0;
    qint64 available = 0;
    qint64 cached = 0;
    qint64 buffers = 0;
    qint64 swapTotal = 0;
    qint64 swapUsed = 0;
    qint64 swapFree = 0;
    double percent = 0.0;
};

struct NetworkData {
    qint64 rxBytes;
    qint64 txBytes;
    qint64 rxSpeed;             // 字节/秒
    qint64 txSpeed;             // 字节/秒
    QString interface;
};

class ResourcesWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ResourcesWidget(QWidget *parent = nullptr);
    ~ResourcesWidget();

    void startMonitoring();
    void stopMonitoring();
    bool isMonitoring() const { return m_isMonitoring; }

private slots:
    void updateCpuChart();
    void updateMemoryChart();
    void updateNetworkChart();
    void updateDiskInfo();
    void updateDiskStructure();

private:
    void initUI();
    void initDataCollectors();
    
    // 数据采集方法
    CpuData collectCpuData();
    MemoryData collectMemoryData();
    NetworkData collectNetworkData();
    
    // 辅助方法
    qint64 readSystemFile(const QString &path);
    QString formatSize(qint64 bytes, bool withUnit = true) const;
    QString formatSpeed(qint64 bytesPerSec) const;

private:
    // 定时器
    QTimer *m_timer;
    bool m_isMonitoring;
    int m_updateCount;          // 更新计数

    // 图表组件
    HistoryChart *m_cpuChart;
    HistoryChart *m_memoryChart;
    HistoryChart *m_networkChart;
    
    // 磁盘信息表格
    QTableWidget *m_diskInfoTable;

    // 硬盘结构表格
    QTableWidget *m_diskStructureTable;

    // 历史数据（用于计算速度）
    qint64 m_lastNetworkRx;
    qint64 m_lastNetworkTx;

    // CPU核心数
    int m_cpuCoreCount;
    
    // 默认网络接口
    QString m_defaultNetworkInterface;
};

#endif // RESOURCESWIDGET_H
