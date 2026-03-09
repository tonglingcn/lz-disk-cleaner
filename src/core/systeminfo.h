/*
 * System Info - Header
 * 系统信息 - 头文件
 */

#ifndef SYSTEMINFO_H
#define SYSTEMINFO_H

#include <QObject>
#include <QString>
#include <QMap>

struct SystemInfoData {
    QString osName;
    QString osVersion;
    QString kernelVersion;
    QString architecture;
    QString hostname;
    QString username;
    qint64 totalMemory;
    qint64 availableMemory;
    QString cpuModel;
    int cpuCores;
};

class SystemInfo : public QObject
{
    Q_OBJECT

public:
    explicit SystemInfo(QObject *parent = nullptr);
    ~SystemInfo();
    
    SystemInfoData getSystemInfo();
    QString getOSName();
    QString getOSVersion();
    QString getKernelVersion();
    QString getArchitecture();
    QString getHostname();
    QString getUsername();
    qint64 getTotalMemory();
    qint64 getAvailableMemory();
    QString getCpuModel();
    int getCpuCores();
    
    bool isImmutableSystem();
    bool isLinglongAvailable();
    
private:
    QString readFileContent(const QString &path);
    QString executeCommand(const QString &command);
};

#endif // SYSTEMINFO_H