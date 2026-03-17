/*
 * Hardware Monitor - Implementation
 * 硬件温度监控 - 实现文件
 */

#include "hardwaremonitor.h"
#include "../utils/logger.h"
#include <QProcess>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QDir>

HardwareMonitor* HardwareMonitor::s_instance = nullptr;

HardwareMonitor::HardwareMonitor(QObject *parent)
    : QObject(parent)
    , m_sensorsAvailable(false)
    , m_nvidiaSmiAvailable(false)
    , m_smartctlAvailable(false)
{
    checkAvailability();
}

HardwareMonitor::~HardwareMonitor()
{
}

HardwareMonitor* HardwareMonitor::instance()
{
    if (!s_instance) {
        s_instance = new HardwareMonitor();
    }
    return s_instance;
}

void HardwareMonitor::checkAvailability()
{
    // 检查 lm-sensors
    QProcess process;
    process.start("sensors", QStringList());
    process.waitForFinished(3000);
    m_sensorsAvailable = (process.exitCode() == 0);
    
    // 检查 nvidia-smi
    process.start("nvidia-smi", QStringList());
    process.waitForFinished(3000);
    m_nvidiaSmiAvailable = (process.exitCode() == 0);
    
    // 检查 smartctl
    process.start("which", QStringList() << "smartctl");
    process.waitForFinished(3000);
    m_smartctlAvailable = (process.exitCode() == 0);
    
    // 检查是否支持 AMD/Intel GPU 温度 (通过 DRM hwmon)
    m_drmHwmonAvailable = false;
    QDir drmDir("/sys/class/drm");
    QStringList cardList = drmDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &card : cardList) {
        if (card.startsWith("card") && !card.contains("-")) {
            QString hwmonPath = QString("/sys/class/drm/%1/device/hwmon").arg(card);
            if (QDir(hwmonPath).exists()) {
                m_drmHwmonAvailable = true;
                break;
            }
        }
    }
    
    LOG_INFO(QString("HardwareMonitor availability - sensors: %1, nvidia-smi: %2, smartctl: %3, drm-hwmon: %4")
        .arg(m_sensorsAvailable).arg(m_nvidiaSmiAvailable).arg(m_smartctlAvailable).arg(m_drmHwmonAvailable));
}

bool HardwareMonitor::isSensorsAvailable()
{
    return m_sensorsAvailable;
}

bool HardwareMonitor::isNvidiaSmiAvailable()
{
    return m_nvidiaSmiAvailable;
}

bool HardwareMonitor::isSmartctlAvailable()
{
    return m_smartctlAvailable;
}

bool HardwareMonitor::isDrmHwmonAvailable()
{
    return m_drmHwmonAvailable;
}

QString HardwareMonitor::getBoardTempUnavailableReason() const
{
    if (!m_sensorsAvailable) {
        return tr("未安装 lm-sensors，请运行: sudo apt install lm-sensors && sudo sensors-detect");
    }
    return tr("主板传感器未识别，可能本机不支持主板温度监控");
}

QString HardwareMonitor::getGpuTempUnavailableReason() const
{
    if (!m_nvidiaSmiAvailable && !m_drmHwmonAvailable) {
        return tr("本机使用集成显卡，温度与CPU共享");
    }
    if (m_nvidiaSmiAvailable) {
        return tr("NVIDIA 显卡温度获取失败");
    }
    return tr("集成显卡与CPU共享温度传感器");
}

HardwareTemps HardwareMonitor::getAllTemperatures()
{
    HardwareTemps temps;
    temps.timestamp = QDateTime::currentMSecsSinceEpoch();
    
    // 获取 sensors 输出 (一次获取，多次解析)
    if (m_sensorsAvailable) {
        QProcess process;
        process.start("sensors", QStringList());
        process.waitForFinished(3000);
        m_lastSensorsOutput = QString::fromUtf8(process.readAllStandardOutput());
        
        temps.cpu = parseCpuTemp(m_lastSensorsOutput);
        temps.board = parseBoardTemp(m_lastSensorsOutput);
        temps.fans = parseFanInfo(m_lastSensorsOutput);
    }
    
    // 获取显卡温度
    temps.gpus = getGpuTemperatures();
    
    // 获取硬盘温度
    temps.disks = getDiskTemperatures();
    
    return temps;
}

CpuTemperature HardwareMonitor::getCpuTemperature()
{
    if (!m_sensorsAvailable) {
        return CpuTemperature();
    }
    
    QProcess process;
    process.start("sensors", QStringList());
    process.waitForFinished(3000);
    QString output = QString::fromUtf8(process.readAllStandardOutput());
    
    return parseCpuTemp(output);
}

CpuTemperature HardwareMonitor::parseCpuTemp(const QString &sensorsOutput)
{
    CpuTemperature cpu;
    cpu.package = 0;
    cpu.isValid = false;
    
    QStringList lines = sensorsOutput.split('\n');
    
    for (const QString &line : lines) {
        QString trimmed = line.trimmed();
        
        // 匹配 CPU 核心温度: coretemp-isa-0000 或 k10temp
        // Package id 0:  +45.0°C
        // Tdie:         +45.0°C (AMD)
        // temp1:        +45.0°C (通用)
        
        QRegularExpression packageRe("Package id\\s*\\d+:\\s*\\+([\\d.]+)°C");
        QRegularExpression amdRe("Tdie:\\s*\\+([\\d.]+)°C");
        QRegularExpression coreRe("Core\\s*\\d+:\\s*\\+([\\d.]+)°C");
        QRegularExpression tempRe("temp\\d+:\\s*\\+([\\d.]+)°C");
        
        QRegularExpressionMatch match;
        
        // Package 温度 (Intel)
        match = packageRe.match(trimmed);
        if (match.hasMatch()) {
            cpu.package = match.captured(1).toDouble();
            cpu.isValid = true;
            continue;
        }
        
        // AMD Tdie 温度
        match = amdRe.match(trimmed);
        if (match.hasMatch()) {
            cpu.package = match.captured(1).toDouble();
            cpu.isValid = true;
            continue;
        }
        
        // 如果没有找到 Package，尝试使用 temp1
        if (!cpu.isValid) {
            match = tempRe.match(trimmed);
            if (match.hasMatch()) {
                cpu.package = match.captured(1).toDouble();
                cpu.isValid = true;
            }
        }
        
        // 各核心温度
        match = coreRe.match(trimmed);
        if (match.hasMatch()) {
            static int coreIndex = 0;
            cpu.cores[coreIndex++] = match.captured(1).toDouble();
        }
    }
    
    // 尝试从 sysfs 读取
    if (!cpu.isValid) {
        QDir hwmonDir("/sys/class/hwmon");
        QStringList hwmonList = hwmonDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        
        for (const QString &hwmon : hwmonList) {
            QString namePath = QString("/sys/class/hwmon/%1/name").arg(hwmon);
            QFile nameFile(namePath);
            if (nameFile.open(QIODevice::ReadOnly)) {
                QString name = QString::fromUtf8(nameFile.readAll()).trimmed().toLower();
                nameFile.close();
                
                // CPU 相关的 hwmon
                if (name.contains("coretemp") || name.contains("k10temp") || 
                    name.contains("k8temp") || name.contains("cpu_thermal")) {
                    
                    QString tempPath = QString("/sys/class/hwmon/%1/temp1_input").arg(hwmon);
                    QFile tempFile(tempPath);
                    if (tempFile.open(QIODevice::ReadOnly)) {
                        QString tempStr = QString::fromUtf8(tempFile.readAll()).trimmed();
                        cpu.package = tempStr.toDouble() / 1000.0;  // 毫摄氏度转摄氏度
                        cpu.isValid = true;
                        tempFile.close();
                        break;
                    }
                }
            }
        }
    }
    
    return cpu;
}

BoardTemperature HardwareMonitor::getBoardTemperature()
{
    if (!m_sensorsAvailable) {
        return BoardTemperature();
    }
    
    QProcess process;
    process.start("sensors", QStringList());
    process.waitForFinished(3000);
    QString output = QString::fromUtf8(process.readAllStandardOutput());
    
    return parseBoardTemp(output);
}

BoardTemperature HardwareMonitor::parseBoardTemp(const QString &sensorsOutput)
{
    BoardTemperature board;
    board.temperature = 0;
    board.isValid = false;
    board.label = "主板";
    
    QStringList lines = sensorsOutput.split('\n');
    QString currentAdapter;  // 当前适配器名称
    
    for (const QString &line : lines) {
        QString trimmed = line.trimmed();
        
        // 记录当前适配器名称
        if (trimmed.startsWith("Adapter:")) {
            currentAdapter = trimmed.mid(8).trimmed();
            continue;
        }
        
        // 匹配主板温度
        // Board:         +38.0°C
        // temp2:         +38.0°C
        // SIO:           +38.0°C
        // SYSTIN:        +38.0°C (Super I/O)
        
        QRegularExpression boardRe("(Board|MB|SIO|SYSTIN|temp2|temp3):\\s*\\+?([\\d.]+)°?C");
        QRegularExpressionMatch match = boardRe.match(trimmed);
        
        if (match.hasMatch()) {
            board.temperature = match.captured(2).toDouble();
            board.label = match.captured(1);
            board.isValid = true;
            break;
        }
        
        // 笔记本场景：使用 acpitz 的 temp1 作为主板温度备选
        // acpitz-acpi-0
        // Adapter: ACPI interface
        // temp1:        +44.0°C
        if (currentAdapter.contains("ACPI") && trimmed.startsWith("temp1:")) {
            QRegularExpression tempRe("temp1:\\s*\\+?([\\d.]+)°?C");
            QRegularExpressionMatch tempMatch = tempRe.match(trimmed);
            if (tempMatch.hasMatch() && !board.isValid) {
                board.temperature = tempMatch.captured(1).toDouble();
                board.label = "系统温度";
                board.isValid = true;
                // 不 break，继续查找更精确的主板温度
            }
        }
    }
    
    // 尝试从 sysfs 读取
    if (!board.isValid) {
        QDir hwmonDir("/sys/class/hwmon");
        QStringList hwmonList = hwmonDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        
        for (const QString &hwmon : hwmonList) {
            QString namePath = QString("/sys/class/hwmon/%1/name").arg(hwmon);
            QFile nameFile(namePath);
            if (nameFile.open(QIODevice::ReadOnly)) {
                QString name = QString::fromUtf8(nameFile.readAll()).trimmed().toLower();
                nameFile.close();
                
                // 主板相关的 hwmon
                if (name.contains("nct") || name.contains("it87") || 
                    name.contains("f718") || name.contains("w836") ||
                    name.contains("asus") || name.contains("gigabyte") ||
                    name.contains("dell") || name.contains("hp")) {
                    
                    // 尝试读取 temp2-5 (temp1 通常是 CPU)
                    for (int i = 2; i <= 5; ++i) {
                        QString tempPath = QString("/sys/class/hwmon/%1/temp%2_input").arg(hwmon).arg(i);
                        QFile tempFile(tempPath);
                        if (tempFile.open(QIODevice::ReadOnly)) {
                            QString tempStr = QString::fromUtf8(tempFile.readAll()).trimmed();
                            double temp = tempStr.toDouble() / 1000.0;
                            if (temp > 0 && temp < 100) {  // 合理范围
                                board.temperature = temp;
                                board.isValid = true;
                                tempFile.close();
                                break;
                            }
                            tempFile.close();
                        }
                    }
                    if (board.isValid) break;
                }
            }
        }
    }
    
    // 尝试从 thermal_zone 读取 (ACPI 热区)
    if (!board.isValid) {
        QDir thermalDir("/sys/class/thermal");
        QStringList thermalList = thermalDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        
        for (const QString &thermal : thermalList) {
            if (!thermal.startsWith("thermal_zone")) continue;
            
            QString typePath = QString("/sys/class/thermal/%1/type").arg(thermal);
            QFile typeFile(typePath);
            if (typeFile.open(QIODevice::ReadOnly)) {
                QString type = QString::fromUtf8(typeFile.readAll()).trimmed().toLower();
                typeFile.close();
                
                // 查找主板相关的 thermal zone
                if (type.contains("pch") || type.contains("board") || 
                    type.contains("sys") || type.contains("ambient")) {
                    
                    QString tempPath = QString("/sys/class/thermal/%1/temp").arg(thermal);
                    QFile tempFile(tempPath);
                    if (tempFile.open(QIODevice::ReadOnly)) {
                        QString tempStr = QString::fromUtf8(tempFile.readAll()).trimmed();
                        double temp = tempStr.toDouble() / 1000.0;
                        if (temp > 0 && temp < 100) {
                            board.temperature = temp;
                            board.label = type;
                            board.isValid = true;
                            tempFile.close();
                            break;
                        }
                        tempFile.close();
                    }
                }
            }
        }
    }
    
    return board;
}

QList<GpuTemperature> HardwareMonitor::getGpuTemperatures()
{
    QList<GpuTemperature> gpus;
    
    // NVIDIA 显卡
    if (m_nvidiaSmiAvailable) {
        QList<GpuTemperature> nvidiaGpus = getNvidiaGpuTemps();
        gpus.append(nvidiaGpus);
    }
    
    // AMD/Intel 显卡 (通过 sensors 或 sysfs)
    QList<GpuTemperature> otherGpus = getAmdGpuTemps();
    gpus.append(otherGpus);
    
    // 笔记本场景：如果没有检测到任何独显，添加集成显卡信息
    if (gpus.isEmpty()) {
        GpuTemperature integratedGpu;
        integratedGpu.name = tr("集成显卡");
        // 获取CPU温度用于显示
        CpuTemperature cpuTemp = getCpuTemperature();
        integratedGpu.temperature = cpuTemp.isValid ? cpuTemp.package : 0;
        integratedGpu.fanSpeed = -1;
        integratedGpu.isNvidia = false;
        integratedGpu.isValid = true;   // 标记为有效，但使用共享温度
        integratedGpu.isIntegrated = true;
        gpus.append(integratedGpu);
    }
    
    return gpus;
}

QList<GpuTemperature> HardwareMonitor::getNvidiaGpuTemps()
{
    QList<GpuTemperature> gpus;
    
    if (!m_nvidiaSmiAvailable) {
        return gpus;
    }
    
    QProcess process;
    process.start("nvidia-smi", QStringList() 
        << "--query-gpu=name,temperature.gpu,fan.speed"
        << "--format=csv,noheader,nounits");
    process.waitForFinished(3000);
    
    QString output = QString::fromUtf8(process.readAllStandardOutput());
    QStringList lines = output.split('\n');
    
    for (const QString &line : lines) {
        if (line.trimmed().isEmpty()) continue;
        
        QStringList parts = line.split(',');
        if (parts.size() >= 2) {
            GpuTemperature gpu;
            gpu.name = parts[0].trimmed();
            gpu.temperature = parts[1].trimmed().toDouble();
            gpu.fanSpeed = parts.size() >= 3 ? parts[2].trimmed().toInt() : -1;
            gpu.isNvidia = true;
            gpu.isValid = true;
            gpu.isIntegrated = false;
            gpus.append(gpu);
        }
    }
    
    return gpus;
}

QList<GpuTemperature> HardwareMonitor::getAmdGpuTemps()
{
    QList<GpuTemperature> gpus;
    
    // 通过 sysfs 获取 AMD/Intel 显卡温度
    QDir drmDir("/sys/class/drm");
    QStringList cardList = drmDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    
    for (const QString &card : cardList) {
        if (!card.startsWith("card")) continue;
        if (card.contains("-")) continue;  // 跳过 card0-HDMI-A-1 这种
        
        QString hwmonPath = QString("/sys/class/drm/%1/device/hwmon").arg(card);
        QDir hwmonDir(hwmonPath);
        QStringList hwmonList = hwmonDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        
        for (const QString &hwmon : hwmonList) {
            QString tempPath = QString("%1/%2/temp1_input").arg(hwmonPath).arg(hwmon);
            QFile tempFile(tempPath);
            if (tempFile.open(QIODevice::ReadOnly)) {
                QString tempStr = QString::fromUtf8(tempFile.readAll()).trimmed();
                double temp = tempStr.toDouble() / 1000.0;
                tempFile.close();
                
                if (temp > 0 && temp < 150) {  // 合理温度范围
                    GpuTemperature gpu;
                    gpu.name = QString("GPU (%1)").arg(card);
                    gpu.temperature = temp;
                    gpu.fanSpeed = -1;
                    gpu.isNvidia = false;
                    gpu.isValid = true;
                    gpu.isIntegrated = false;
                    gpus.append(gpu);
                }
            }
        }
        
        // 尝试直接读取 amdgpu 驱动的温度 (适用于部分 AMD 显卡)
        if (gpus.isEmpty()) {
            QString amdgpuTempPath = QString("/sys/class/drm/%1/device/hwmon/hwmon*/temp1_input").arg(card);
            QProcess findProcess;
            findProcess.start("sh", QStringList() << "-c" << "cat " + amdgpuTempPath + " 2>/dev/null | head -1");
            findProcess.waitForFinished(1000);
            QString output = QString::fromUtf8(findProcess.readAllStandardOutput()).trimmed();
            if (!output.isEmpty()) {
                double temp = output.toDouble() / 1000.0;
                if (temp > 0 && temp < 150) {
                    GpuTemperature gpu;
                    gpu.name = QString("AMD GPU (%1)").arg(card);
                    gpu.temperature = temp;
                    gpu.fanSpeed = -1;
                    gpu.isNvidia = false;
                    gpu.isValid = true;
                    gpu.isIntegrated = false;
                    gpus.append(gpu);
                }
            }
        }
    }
    
    // 通过 sensors 命令获取 AMD GPU 温度 (备用方案)
    if (gpus.isEmpty() && m_sensorsAvailable) {
        QProcess process;
        process.start("sensors", QStringList() << "-u");
        process.waitForFinished(3000);
        QString output = QString::fromUtf8(process.readAllStandardOutput());
        
        // 匹配 amdgpu 或 radeon 传感器的温度
        QRegularExpression gpuTempRe(R"((amdgpu|radeon|edge|junction).*?:\s*([0-9.]+))");
        gpuTempRe.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatchIterator matchIter = gpuTempRe.globalMatch(output);
        
        while (matchIter.hasNext()) {
            QRegularExpressionMatch match = matchIter.next();
            double temp = match.captured(2).toDouble();
            if (temp > 0 && temp < 150) {
                GpuTemperature gpu;
                gpu.name = "AMD GPU";
                gpu.temperature = temp;
                gpu.fanSpeed = -1;
                gpu.isNvidia = false;
                gpu.isValid = true;
                gpu.isIntegrated = false;
                gpus.append(gpu);
                break;  // 只取第一个有效的
            }
        }
    }
    
    return gpus;
}

QList<DiskTemperature> HardwareMonitor::getDiskTemperatures()
{
    QList<DiskTemperature> disks;
    
    // 获取所有块设备
    QDir blockDir("/sys/block");
    QStringList blockList = blockDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    
    for (const QString &block : blockList) {
        // 过滤 loop 设备
        if (block.startsWith("loop")) continue;
        if (block.startsWith("ram")) continue;
        if (block.startsWith("zram")) continue;
        
        DiskTemperature disk;
        disk.device = "/dev/" + block;
        disk.isValid = false;
        disk.temperature = 0;
        disk.isSSD = false;
        disk.isNVMe = false;
        
        // 检查是否为 NVMe
        disk.isNVMe = block.startsWith("nvme");
        
        // 检查是否为 SSD
        QString rotationalPath = QString("/sys/block/%1/queue/rotational").arg(block);
        QFile rotFile(rotationalPath);
        if (rotFile.open(QIODevice::ReadOnly)) {
            QString rot = QString::fromUtf8(rotFile.readAll()).trimmed();
            disk.isSSD = (rot == "0");
            rotFile.close();
        }
        
        // 获取型号
        QString modelPath = QString("/sys/block/%1/device/model").arg(block);
        QFile modelFile(modelPath);
        if (modelFile.open(QIODevice::ReadOnly)) {
            disk.model = QString::fromUtf8(modelFile.readAll()).trimmed();
            modelFile.close();
        }
        if (disk.model.isEmpty()) {
            // NVMe 设备型号路径不同
            QString vendorPath = QString("/sys/block/%1/device/device/vendor").arg(block);
            QFile vendorFile(vendorPath);
            if (vendorFile.open(QIODevice::ReadOnly)) {
                disk.model = QString::fromUtf8(vendorFile.readAll()).trimmed();
                vendorFile.close();
            }
        }
        if (disk.model.isEmpty()) {
            disk.model = block.toUpper();
        }
        
        // NVMe 设备通过 hwmon 获取温度
        if (disk.isNVMe) {
            // 从 nvme0n1 提取 nvme0
            QRegularExpression nvmeRe("^(nvme\\d+)");
            QRegularExpressionMatch match = nvmeRe.match(block);
            if (match.hasMatch()) {
                QString nvmeDev = match.captured(1);
                QString hwmonPath = QString("/sys/class/nvme/%1").arg(nvmeDev);
                QDir hwmonDir(hwmonPath);
                QStringList hwmonList = hwmonDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
                
                for (const QString &entry : hwmonList) {
                    if (entry.startsWith("hwmon")) {
                        QString tempPath = QString("%1/%2/temp1_input").arg(hwmonPath).arg(entry);
                        QFile tempFile(tempPath);
                        if (tempFile.open(QIODevice::ReadOnly)) {
                            QString tempStr = QString::fromUtf8(tempFile.readAll()).trimmed();
                            disk.temperature = tempStr.toDouble() / 1000.0;
                            disk.isValid = true;
                            tempFile.close();
                            break;
                        }
                    }
                }
            }
        }
        
        // SATA/SAS 硬盘通过 smartctl 获取温度
        if (!disk.isValid && m_smartctlAvailable) {
            QProcess process;
            process.start("smartctl", QStringList() << "-A" << disk.device);
            process.waitForFinished(5000);
            QString output = QString::fromUtf8(process.readAllStandardOutput());
            
            // 解析温度: 194 Temperature_Celsius     0x0022   038   045   000    Old_age   Always       -       38
            QRegularExpression tempRe("(194|Temperature)[:\\s]+.*?\\s+(\\d+)\\s*$");
            QStringList lines = output.split('\n');
            for (const QString &line : lines) {
                QRegularExpressionMatch match = tempRe.match(line);
                if (match.hasMatch()) {
                    disk.temperature = match.captured(2).toDouble();
                    disk.isValid = true;
                    break;
                }
            }
        }
        
        // drivetemp 内核模块方式
        if (!disk.isValid) {
            QString drivetempPath = QString("/sys/block/%1/device/hwmon").arg(block);
            QDir hwmonDir(drivetempPath);
            QStringList hwmonList = hwmonDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            
            for (const QString &hwmon : hwmonList) {
                QString tempPath = QString("%1/%2/temp1_input").arg(drivetempPath).arg(hwmon);
                QFile tempFile(tempPath);
                if (tempFile.open(QIODevice::ReadOnly)) {
                    QString tempStr = QString::fromUtf8(tempFile.readAll()).trimmed();
                    disk.temperature = tempStr.toDouble() / 1000.0;
                    disk.isValid = true;
                    tempFile.close();
                    break;
                }
            }
        }
        
        if (disk.isValid && disk.temperature > 0) {
            disks.append(disk);
        }
    }
    
    return disks;
}

QList<FanInfo> HardwareMonitor::getFanSpeeds()
{
    if (!m_sensorsAvailable) {
        return QList<FanInfo>();
    }
    
    QProcess process;
    process.start("sensors", QStringList());
    process.waitForFinished(3000);
    QString output = QString::fromUtf8(process.readAllStandardOutput());
    
    return parseFanInfo(output);
}

QList<FanInfo> HardwareMonitor::parseFanInfo(const QString &sensorsOutput)
{
    QList<FanInfo> fans;
    
    QStringList lines = sensorsOutput.split('\n');
    
    for (const QString &line : lines) {
        QString trimmed = line.trimmed();
        
        // 匹配风扇转速
        // fan1:        1200 RPM
        // CPU Fan:     1200 RPM
        
        QRegularExpression fanRe("(fan\\d+|.*[Ff]an.*):\\s*(\\d+)\\s*RPM");
        QRegularExpressionMatch match = fanRe.match(trimmed);
        
        if (match.hasMatch()) {
            FanInfo fan;
            fan.name = match.captured(1);
            fan.rpm = match.captured(2).toInt();
            fan.min = 0;
            fan.max = 0;
            fan.isValid = true;
            fans.append(fan);
        }
    }
    
    // 从 sysfs hwmon 获取风扇转速
    if (fans.isEmpty()) {
        QDir hwmonDir("/sys/class/hwmon");
        QStringList hwmonList = hwmonDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        
        for (const QString &hwmon : hwmonList) {
            // 读取设备名称
            QString namePath = QString("/sys/class/hwmon/%1/name").arg(hwmon);
            QString deviceName = hwmon;
            QFile nameFile(namePath);
            if (nameFile.open(QIODevice::ReadOnly)) {
                deviceName = QString::fromUtf8(nameFile.readAll()).trimmed();
                nameFile.close();
            }
            
            for (int i = 1; i <= 5; ++i) {
                QString fanPath = QString("/sys/class/hwmon/%1/fan%2_input").arg(hwmon).arg(i);
                QFile fanFile(fanPath);
                if (fanFile.open(QIODevice::ReadOnly)) {
                    QString rpmStr = QString::fromUtf8(fanFile.readAll()).trimmed();
                    int rpm = rpmStr.toInt();
                    if (rpm > 0) {  // 只添加有效的转速
                        FanInfo fan;
                        fan.name = QString("%1 Fan%2").arg(deviceName).arg(i);
                        fan.rpm = rpm;
                        fan.min = 0;
                        fan.max = 0;
                        fan.isValid = true;
                        fans.append(fan);
                    }
                    fanFile.close();
                }
            }
        }
    }
    
    // 从 cooling_device 获取风扇信息
    if (fans.isEmpty()) {
        QDir coolingDir("/sys/class/thermal");
        QStringList thermalList = coolingDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        
        for (const QString &thermal : thermalList) {
            if (!thermal.startsWith("cooling_device")) continue;
            
            QString typePath = QString("/sys/class/thermal/%1/type").arg(thermal);
            QFile typeFile(typePath);
            if (typeFile.open(QIODevice::ReadOnly)) {
                QString type = QString::fromUtf8(typeFile.readAll()).trimmed();
                typeFile.close();
                
                if (type.contains("fan", Qt::CaseInsensitive)) {
                    QString curStatePath = QString("/sys/class/thermal/%1/cur_state").arg(thermal);
                    QFile curStateFile(curStatePath);
                    if (curStateFile.open(QIODevice::ReadOnly)) {
                        QString state = QString::fromUtf8(curStateFile.readAll()).trimmed();
                        FanInfo fan;
                        fan.name = thermal;
                        fan.rpm = state.toInt() * 100;  // 转换为估算RPM
                        fan.min = 0;
                        fan.max = 0;
                        fan.isValid = true;
                        fans.append(fan);
                        curStateFile.close();
                    }
                }
            }
        }
    }
    
    return fans;
}

QString HardwareMonitor::getTempLevel(double temp)
{
    if (temp < 0) return "unknown";
    if (temp < 50) return "normal";
    if (temp < 70) return "warm";
    if (temp < 85) return "hot";
    return "critical";
}

QString HardwareMonitor::getTempColor(double temp)
{
    if (temp < 0) return "#95a5a6";       // 灰色 - 未知
    if (temp < 50) return "#27ae60";       // 绿色 - 正常
    if (temp < 70) return "#f39c12";       // 橙色 - 温暖
    if (temp < 85) return "#e67e22";       // 深橙色 - 热
    return "#e74c3c";                      // 红色 - 危险
}

QString HardwareMonitor::getTempStyle(double temp)
{
    QString color = getTempColor(temp);
    return QString("color: %1; font-weight: bold;").arg(color);
}
