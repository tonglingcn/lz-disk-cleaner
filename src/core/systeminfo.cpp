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
    
    for (const QString &line : lines) {
        if (line.startsWith("model name")) {
            return line.split(':').last().trimmed();
        }
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