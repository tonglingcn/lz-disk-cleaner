/*
 * Hardware Monitor - Header
 * 硬件温度监控 - 头文件
 * 
 * 获取 CPU、主板、显卡、硬盘温度及风扇转速
 */

#ifndef HARDWAREMONITOR_H
#define HARDWAREMONITOR_H

#include <QObject>
#include <QList>
#include <QString>
#include <QMap>

// 温度相关常量
namespace HardwareConstants {
    constexpr double MIN_REASONABLE_TEMP = 0.0;      // 最低合理温度（摄氏度）
    constexpr double MAX_CPU_TEMP = 150.0;           // CPU 最高合理温度
    constexpr double MAX_BOARD_TEMP = 100.0;         // 主板最高合理温度
    constexpr double MAX_GPU_TEMP = 150.0;           // GPU 最高合理温度
    constexpr double MAX_DISK_TEMP = 100.0;          // 硬盘最高合理温度
    
    constexpr int PROCESS_TIMEOUT_MS = 3000;         // 进程超时时间（毫秒）
    constexpr int LONG_PROCESS_TIMEOUT_MS = 10000;   // 长进程超时时间
    constexpr int TEMP_SCALE_FACTOR = 1000;          // 温度缩放因子（毫摄氏度转摄氏度）
}

// CPU 温度信息
struct CpuTemperature {
    double package;              // CPU 整体温度
    QMap<int, double> cores;     // 各核心温度 (可选)
    bool isValid;
};

// 显卡温度信息
struct GpuTemperature {
    QString name;                // 显卡名称
    double temperature;          // 温度
    int fanSpeed;                // 风扇转速 (-1 表示不可用)
    bool isNvidia;               // 是否为 NVIDIA 显卡
    bool isValid;
    bool isIntegrated;           // 是否为集成显卡
};

// 硬盘温度信息
struct DiskTemperature {
    QString device;              // 设备路径 /dev/sda
    QString model;               // 型号名称
    double temperature;          // 温度
    bool isSSD;                  // 是否为 SSD
    bool isNVMe;                 // 是否为 NVMe
    bool isValid;
};

// 风扇信息
struct FanInfo {
    QString name;                // 风扇名称
    int rpm;                     // 转速
    int min;                     // 最小转速
    int max;                     // 最大转速
    bool isValid;
};

// 主板温度
struct BoardTemperature {
    double temperature;          // 主板温度
    QString label;               // 传感器标签
    bool isValid;
};

// 完整硬件温度数据
struct HardwareTemps {
    CpuTemperature cpu;
    BoardTemperature board;
    QList<GpuTemperature> gpus;
    QList<DiskTemperature> disks;
    QList<FanInfo> fans;
    
    // 时间戳
    qint64 timestamp;
    
    // 是否有任何有效数据
    bool hasValidData() const {
        return cpu.isValid || board.isValid || 
               !gpus.isEmpty() || !disks.isEmpty();
    }
};

class HardwareMonitor : public QObject
{
    Q_OBJECT

public:
    static HardwareMonitor* instance();
    
    // 获取所有硬件温度
    HardwareTemps getAllTemperatures();
    
    // 单独获取各项温度
    CpuTemperature getCpuTemperature();
    BoardTemperature getBoardTemperature();
    QList<GpuTemperature> getGpuTemperatures();
    QList<DiskTemperature> getDiskTemperatures();
    QList<FanInfo> getFanSpeeds();
    
    // 检查各项功能是否可用
    bool isSensorsAvailable();
    bool isNvidiaSmiAvailable();
    bool isSmartctlAvailable();
    bool isDrmHwmonAvailable();
    
    // 获取不可用原因说明
    QString getBoardTempUnavailableReason() const;
    QString getGpuTempUnavailableReason() const;
    
    // 温度等级判断
    static QString getTempLevel(double temp);           // 返回: normal/warm/hot/critical
    static QString getTempColor(double temp);           // 返回颜色代码
    static QString getTempStyle(double temp);           // 返回完整样式

private:
    explicit HardwareMonitor(QObject *parent = nullptr);
    ~HardwareMonitor();
    
    static HardwareMonitor *s_instance;
    
    // 初始化检查
    void checkAvailability();
    
    // 解析方法
    CpuTemperature parseCpuTemp(const QString &sensorsOutput);
    BoardTemperature parseBoardTemp(const QString &sensorsOutput);
    QList<FanInfo> parseFanInfo(const QString &sensorsOutput);
    QList<GpuTemperature> getNvidiaGpuTemps();
    QList<GpuTemperature> getAmdGpuTemps();
    
    // 可用性标志
    bool m_sensorsAvailable;
    bool m_nvidiaSmiAvailable;
    bool m_smartctlAvailable;
    bool m_drmHwmonAvailable;
    
    // 缓存的 sensors 输出
    QString m_lastSensorsOutput;
};

#endif // HARDWAREMONITOR_H
