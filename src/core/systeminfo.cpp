/*
 * System Info - Implementation
 * 系统信息 - 实现
 */

#include "systeminfo.h"
#include <QThread>
#include <QProcess>
#include <QRegularExpression>
#include <QFile>
#include <QTextStream>
#include <QSysInfo>
#include <QStandardPaths>
#include <QDebug>

SystemInfo::SystemInfo(QObject *parent)
    : QObject(parent)
{
}

SystemInfo::~SystemInfo()
{
}

SystemInfoData SystemInfo::getSystemInfo()
{
    SystemInfoData info;
    
    info.osName = getOSName();
    info.osVersion = getOSVersion();
    info.kernelVersion = getKernelVersion();
    info.architecture = getArchitecture();
    info.hostname = getHostname();
    info.username = getUsername();
    info.totalMemory = getTotalMemory();
    info.availableMemory = getAvailableMemory();
    info.cpuModel = getCpuModel();
    info.cpuCores = getCpuCores();
    
    return info;
}

QString SystemInfo::getOSName()
{
    QString content = readFileContent("/etc/os-release");
    QStringList lines = content.split('\n');
    
    for (const QString &line : lines) {
        if (line.startsWith("PRETTY_NAME")) {
            return line.split('=').last().remove('"').remove('"');
        }
    }
    
    return QSysInfo::prettyProductName();
}

QString SystemInfo::getOSVersion()
{
    QString content = readFileContent("/etc/os-release");
    QStringList lines = content.split('\n');
    
    for (const QString &line : lines) {
        if (line.startsWith("VERSION")) {
            return line.split('=').last().remove('"').remove('"');
        }
    }
    
    return QSysInfo::productVersion();
}

QString SystemInfo::getKernelVersion()
{
    return QSysInfo::kernelVersion();
}

QString SystemInfo::getArchitecture()
{
    return QSysInfo::currentCpuArchitecture();
}

QString SystemInfo::getHostname()
{
    return QSysInfo::machineHostName();
}

QString SystemInfo::getUsername()
{
    QString user = qgetenv("USER");
    if (user.isEmpty()) {
        user = qgetenv("USERNAME");
    }
    return user;
}

qint64 SystemInfo::getTotalMemory()
{
    QString content = readFileContent("/proc/meminfo");
    QStringList lines = content.split('\n');
    
    for (const QString &line : lines) {
        if (line.startsWith("MemTotal")) {
            QStringList parts = line.split(QRegularExpression("\\s+"));
            if (parts.size() >= 2) {
                return parts[1].toLongLong() * 1024; // KB to bytes
            }
        }
    }
    
    return 0;
}

qint64 SystemInfo::getAvailableMemory()
{
    QString content = readFileContent("/proc/meminfo");
    QStringList lines = content.split('\n');
    
    qint64 memAvailable = 0;
    qint64 memFree = 0;
    qint64 buffers = 0;
    qint64 cached = 0;
    
    for (const QString &line : lines) {
        if (line.startsWith("MemAvailable")) {
            QStringList parts = line.split(QRegularExpression("\\s+"));
            if (parts.size() >= 2) {
                memAvailable = parts[1].toLongLong() * 1024;
            }
        } else if (line.startsWith("MemFree")) {
            QStringList parts = line.split(QRegularExpression("\\s+"));
            if (parts.size() >= 2) {
                memFree = parts[1].toLongLong() * 1024;
            }
        } else if (line.startsWith("Buffers")) {
            QStringList parts = line.split(QRegularExpression("\\s+"));
            if (parts.size() >= 2) {
                buffers = parts[1].toLongLong() * 1024;
            }
        } else if (line.startsWith("Cached")) {
            QStringList parts = line.split(QRegularExpression("\\s+"));
            if (parts.size() >= 2) {
                cached = parts[1].toLongLong() * 1024;
            }
        }
    }
    
    // 如果 MemAvailable 不存在，使用 MemFree + Buffers + Cached
    if (memAvailable == 0) {
        memAvailable = memFree + buffers + cached;
    }
    
    return memAvailable;
}

QString SystemInfo::getCpuModel()
{
    QString content = readFileContent("/proc/cpuinfo");
    QStringList lines = content.split('\n');
    
    // 收集所有可能的CPU型号信息
    QString cpuModel;
    QString modelName;
    QString systemType;
    QString hardware;
    QString processor;
    
    for (const QString &line : lines) {
        QString trimmedLine = line.trimmed();
        
        // 使用正则表达式匹配各种格式的字段名
        QRegularExpressionMatch match;
        
        // 匹配 "cpu model" 或 "CPU model" (龙芯/LoongArch)
        QRegularExpression cpuModelRe("^[Cc][Pp][Uu]\\s+[Mm]odel\\s*:", QRegularExpression::CaseInsensitiveOption);
        if ((match = cpuModelRe.match(trimmedLine)).hasMatch()) {
            QString value = trimmedLine.split(':').last().trimmed();
            if (!value.isEmpty() && !value.contains("generic", Qt::CaseInsensitive)) {
                cpuModel = value;
            }
        }
        
        // 匹配 "model name" (x86)
        QRegularExpression modelNameRe("^[Mm]odel\\s+[Nn]ame\\s*:", QRegularExpression::CaseInsensitiveOption);
        if ((match = modelNameRe.match(trimmedLine)).hasMatch()) {
            QString value = trimmedLine.split(':').last().trimmed();
            if (!value.isEmpty()) {
                modelName = value;
            }
        }
        
        // 匹配 "system type" (备用)
        QRegularExpression systemTypeRe("^[Ss]ystem\\s+[Tt]ype\\s*:", QRegularExpression::CaseInsensitiveOption);
        if ((match = systemTypeRe.match(trimmedLine)).hasMatch()) {
            QString value = trimmedLine.split(':').last().trimmed();
            if (!value.isEmpty()) {
                systemType = value;
            }
        }
        
        // 匹配 "Hardware" (ARM)
        QRegularExpression hardwareRe("^[Hh]ardware\\s*:", QRegularExpression::CaseInsensitiveOption);
        if ((match = hardwareRe.match(trimmedLine)).hasMatch()) {
            QString value = trimmedLine.split(':').last().trimmed();
            if (!value.isEmpty()) {
                hardware = value;
            }
        }
        
        // 匹配 "Processor" (ARM/RISC-V)
        QRegularExpression processorRe("^[Pp]rocessor\\s*:", QRegularExpression::CaseInsensitiveOption);
        // 注意：Processor 后面通常是数字(如 processor : 0)，跳过纯数字
        if ((match = processorRe.match(trimmedLine)).hasMatch()) {
            QString value = trimmedLine.split(':').last().trimmed();
            if (!value.isEmpty() && !value.toInt()) {  // 不是纯数字
                processor = value;
            }
        }
    }
    
    // 按优先级返回
    if (!cpuModel.isEmpty()) return cpuModel;           // 龙芯优先
    if (!modelName.isEmpty()) return modelName;         // x86
    if (!hardware.isEmpty()) return hardware;           // ARM
    if (!processor.isEmpty()) return processor;         // 其他
    if (!systemType.isEmpty() && !systemType.contains("generic", Qt::CaseInsensitive)) {
        return systemType;                               // 备用
    }
    
    return "Unknown";
}

int SystemInfo::getCpuCores()
{
    return QThread::idealThreadCount();
}

bool SystemInfo::isImmutableSystem()
{
    QProcess process;
    process.start("which", QStringList() << "deepin-immutable-ctl");
    process.waitForFinished();
    
    if (process.exitCode() != 0) {
        return false;
    }
    
    process.start("deepin-immutable-ctl", QStringList() << "status");
    process.waitForFinished();
    
    QString output = QString::fromUtf8(process.readAllStandardOutput());
    return output.contains("enabled: true");
}

bool SystemInfo::isLinglongAvailable()
{
    QProcess process;
    process.start("which", QStringList() << "ll-cli");
    process.waitForFinished();
    
    return (process.exitCode() == 0);
}

QString SystemInfo::readFileContent(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    
    QTextStream in(&file);
    QString content = in.readAll();
    file.close();
    
    return content;
}

QString SystemInfo::executeCommand(const QString &command)
{
    QProcess process;
    process.start("bash", QStringList() << "-c" << command);
    process.waitForFinished();

    return QString::fromUtf8(process.readAllStandardOutput()).trimmed();
}

QString SystemInfo::getDistroName()
{
    QString content = readFileContent("/etc/os-release");
    if (content.isEmpty()) {
        return "Linux";
    }

    QStringList lines = content.split('\n');
    QString id;
    QString versionId;
    QString prettyName;

    for (const QString &line : lines) {
        if (line.startsWith("ID=")) {
            id = line.mid(3).remove('"').trimmed();
        } else if (line.startsWith("VERSION_ID=")) {
            versionId = line.mid(11).remove('"').trimmed();
        } else if (line.startsWith("PRETTY_NAME=")) {
            prettyName = line.mid(12).remove('"').trimmed();
        }
    }

    // 发行版名称映射表
    QMap<QString, QString> distroNames = {
        {"deepin", "Deepin"},
        {"uos", "UOS"},
        {"debian", "Debian"},
        {"ubuntu", "Ubuntu"},
        {"fedora", "Fedora"},
        {"centos", "CentOS"},
        {"arch", "Arch Linux"},
        {"manjaro", "Manjaro"},
        {"opensuse-tumbleweed", "openSUSE"},
        {"opensuse-leap", "openSUSE"},
        {"linuxmint", "Linux Mint"},
        {"elementary", "elementary OS"},
        {"kali", "Kali Linux"},
        {"gentoo", "Gentoo"},
        {"slackware", "Slackware"},
        {"redhat", "RHEL"},
        {"rhel", "RHEL"},
        {"almalinux", "AlmaLinux"},
        {"rocky", "Rocky Linux"},
        {"kylin", "Kylin"},
        {"uniontech", "UOS"}
    };

    // 获取发行版显示名称
    QString distroDisplay;
    if (distroNames.contains(id)) {
        distroDisplay = distroNames[id];
    } else if (!id.isEmpty()) {
        // 首字母大写
        distroDisplay = id.at(0).toUpper() + id.mid(1);
    } else {
        distroDisplay = "Linux";
    }

    // 特殊处理 Deepin/UOS 版本号
    if (id == "deepin" || id == "uos" || id == "uniontech") {
        if (!versionId.isEmpty()) {
            // Deepin 版本号映射
            if (versionId == "23") {
                return distroDisplay + " V23";
            } else if (versionId == "25") {
                return distroDisplay + " V25";
            } else if (versionId.startsWith("20")) {
                // UOS 20 系列
                return distroDisplay + " " + versionId;
            } else {
                return distroDisplay + " " + versionId;
            }
        }
        return distroDisplay;
    }

    // 其他发行版直接加上版本号
    if (!versionId.isEmpty()) {
        return distroDisplay + " " + versionId;
    }

    return distroDisplay;
}