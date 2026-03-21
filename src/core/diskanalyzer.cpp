/*
 * Disk Analyzer - Implementation
 * 磁盘分析器 - 实现
 */

#include "diskanalyzer.h"
#include "../utils/logger.h"
#include <QRegularExpression>
#include <QDir>
#include <QFileInfo>
#include <QTextStream>
#include <QDebug>

DiskAnalyzer::DiskAnalyzer(QObject *parent)
    : QObject(parent)
    , m_process(new QProcess(this))
{
}

DiskAnalyzer::~DiskAnalyzer()
{
    if (m_process->state() == QProcess::Running) {
        m_process->kill();
    }
}

QList<DiskUsage> DiskAnalyzer::analyzeDiskUsage()
{
    LOG_INFO("Starting disk usage analysis");
    
    m_process->start("df", QStringList() << "-Th");
    m_process->waitForFinished();
    
    QString output = QString::fromUtf8(m_process->readAllStandardOutput());
    return parseDfOutput(output);
}

QList<DiskUsage> DiskAnalyzer::analyzePersistentPartition()
{
    LOG_INFO("Analyzing persistent partition");
    
    QList<DiskUsage> allDisks = analyzeDiskUsage();
    QList<DiskUsage> persistentDisks;
    
    for (const DiskUsage &disk : allDisks) {
        if (disk.mountpoint.contains("persistent") || 
            disk.mountpoint.contains("/home")) {
            persistentDisks.append(disk);
        }
    }
    
    return persistentDisks;
}

QList<DirectoryUsage> DiskAnalyzer::analyzeHomeDirectory(const QString &path, int maxItems)
{
    LOG_INFO(QString("Analyzing home directory: %1").arg(path));
    
    QList<DirectoryUsage> directories;
    QDir dir(path);
    
    if (!dir.exists()) {
        LOG_ERROR(QString("Directory does not exist: %1").arg(path));
        return directories;
    }
    
    QFileInfoList entries = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Size);
    
    for (const QFileInfo &info : entries) {
        if (directories.size() >= maxItems) break;
        
        QString dirPath = info.absoluteFilePath();
        qint64 size = getDirectorySize(dirPath);
        
        DirectoryUsage usage;
        usage.path = dirPath;
        usage.size = size;
        usage.fileCount = QDir(dirPath).entryInfoList(QDir::Files).size();
        
        directories.append(usage);
    }
    
    // 按大小排序
    std::sort(directories.begin(), directories.end(), 
              [](const DirectoryUsage &a, const DirectoryUsage &b) {
                  return a.size > b.size;
              });
    
    return directories;
}

ImmutableSystemInfo DiskAnalyzer::analyzeImmutableSystem()
{
    LOG_INFO("Analyzing immutable system");
    
    ImmutableSystemInfo info;
    info.enabled = false;
    info.totalSnapshotSize = 0;
    info.snapshotCount = 0;
    info.modifiedSize = 0;
    info.ostreeRepoSize = 0;
    info.deploySize = 0;
    info.overlaySize = 0;
    
    // 检查 deepin-immutable-ctl 是否存在
    m_process->start("which", QStringList() << "deepin-immutable-ctl");
    m_process->waitForFinished(3000);
    
    if (m_process->exitCode() != 0) {
        LOG_WARNING("deepin-immutable-ctl not found, not an immutable system");
        return info;
    }
    
    // 检查是否为磐石系统模式
    m_process->start("deepin-immutable-ctl", QStringList() << "-s");
    m_process->waitForFinished(5000);
    
    QString statusOutput = QString::fromUtf8(m_process->readAllStandardOutput());
    LOG_INFO(QString("Immutable status: %1").arg(statusOutput));
    
    // 只要工具存在就认为有磐石系统组件
    info.enabled = true;
    
    // 获取部署信息 (需要 root 权限，可能失败)
    m_process->start("deepin-immutable-ctl", QStringList() << "admin" << "status" << "-d" << "all");
    if (m_process->waitForFinished(5000)) {
        info.deploymentInfo = QString::fromUtf8(m_process->readAllStandardOutput());
        LOG_DEBUG(QString("Deployment info: %1").arg(info.deploymentInfo.left(200)));
    }
    
    // 获取修改层信息
    m_process->start("deepin-immutable-ctl", QStringList() << "admin" << "status" << "-d" << "modified");
    if (m_process->waitForFinished(5000)) {
        QString modifiedOutput = QString::fromUtf8(m_process->readAllStandardOutput());
        LOG_DEBUG(QString("Modified output: %1").arg(modifiedOutput));
        // 解析修改层大小
        QRegularExpression sizeRe("size[:\\s]+(\\d+)", QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch match = sizeRe.match(modifiedOutput);
        if (match.hasMatch()) {
            info.modifiedSize = match.captured(1).toLongLong();
        }
    }
    
    // 获取快照列表
    m_process->start("deepin-immutable-ctl", QStringList() << "snapshot" << "list");
    if (m_process->waitForFinished(5000)) {
        QString snapshotOutput = QString::fromUtf8(m_process->readAllStandardOutput());
        info.snapshots = snapshotOutput.split('\n', Qt::SkipEmptyParts);
        info.snapshotCount = info.snapshots.size();
        LOG_DEBUG(QString("Snapshot count: %1").arg(info.snapshotCount));
    }
    
    // 计算各目录大小
    // 1. OSTree 仓库
    QDir ostreeRepo("/ostree/repo");
    if (ostreeRepo.exists()) {
        info.ostreeRepoSize = getDirectorySize("/ostree/repo");
        LOG_INFO(QString("OSTree repo size: %1").arg(info.ostreeRepoSize));
    }
    
    // 2. 部署目录
    QDir deployDir("/ostree/deploy");
    if (deployDir.exists()) {
        info.deploySize = getDirectorySize("/ostree/deploy");
        LOG_INFO(QString("Deploy size: %1").arg(info.deploySize));
    }
    
    // 3. Overlay 目录
    QDir overlayDir("/root/persistent/overlay");
    if (overlayDir.exists()) {
        info.overlaySize = getDirectorySize("/root/persistent/overlay");
        LOG_INFO(QString("Overlay size: %1").arg(info.overlaySize));
    }
    
    // 4. 快照目录
    QDir snapshotDir("/boot/deepin-snapshots");
    if (snapshotDir.exists()) {
        info.totalSnapshotSize = getDirectorySize("/boot/deepin-snapshots");
        LOG_INFO(QString("Snapshot size: %1").arg(info.totalSnapshotSize));
    }
    
    return info;
}

QList<LinglongAppInfo> DiskAnalyzer::analyzeLinglongApps()
{
    LOG_INFO("Analyzing Linglong apps");
    
    QList<LinglongAppInfo> apps;
    
    // 检查 ll-cli 是否可用
    m_process->start("ll-cli", QStringList() << "list");
    m_process->waitForFinished(5000);
    
    if (m_process->exitCode() == 0) {
        QString output = QString::fromUtf8(m_process->readAllStandardOutput());
        apps = parseLinglongOutput(output);
    } else {
        LOG_WARNING("ll-cli command not available");
    }
    
    return apps;
}

QMap<QString, qint64> DiskAnalyzer::analyzeSpecialApps()
{
    LOG_INFO("Analyzing special app data");
    
    QMap<QString, qint64> appSizes;
    QString home = QDir::homePath();
    
    // 钉钉
    QString dingtalkPath = home + "/.dingtalk/";
    appSizes["钉钉"] = getDirectorySize(dingtalkPath);
    
    // 微信
    QString wechatPath = home + "/.wechat/";
    appSizes["微信"] = getDirectorySize(wechatPath);
    
    // QQ
    QString qqPath = home + "/.qq/";
    appSizes["QQ"] = getDirectorySize(qqPath);
    
    // Chrome
    QString chromePath = home + ".config/google-chrome/";
    appSizes["Chrome"] = getDirectorySize(chromePath);
    
    // Firefox
    QString firefoxPath = home + "/.mozilla/firefox/";
    appSizes["Firefox"] = getDirectorySize(firefoxPath);
    
    return appSizes;
}

qint64 DiskAnalyzer::calculateCacheSize(const QString &path)
{
    return getDirectorySize(path);
}

qint64 DiskAnalyzer::getDirectorySize(const QString &path)
{
    QDir dir(path);
    
    if (!dir.exists()) {
        return 0;
    }
    
    // 优先使用 du 命令（快速且准确）
    QProcess duProcess;
    duProcess.start("du", QStringList() << "-sb" << path);
    duProcess.waitForFinished(30000);  // 30秒超时
    
    if (duProcess.exitCode() == 0) {
        QString output = QString::fromUtf8(duProcess.readAllStandardOutput());
        QStringList parts = output.split('\t');
        if (!parts.isEmpty()) {
            bool ok;
            qint64 size = parts[0].toLongLong(&ok);
            if (ok) {
                return size;
            }
        }
    }
    
    // 备用方案：递归计算（跳过符号链接避免重复计算）
    qint64 totalSize = 0;
    
    QFileInfoList files = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo &file : files) {
        if (!file.isSymLink()) {
            totalSize += file.size();
        }
    }
    
    QFileInfoList dirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &subdir : dirs) {
        if (!subdir.isSymLink()) {
            totalSize += getDirectorySize(subdir.absoluteFilePath());
        }
    }
    
    return totalSize;
}

QString DiskAnalyzer::formatSize(qint64 bytes)
{
    const qint64 KB = 1024;
    const qint64 MB = 1024 * KB;
    const qint64 GB = 1024 * MB;
    const qint64 TB = 1024 * GB;
    
    if (bytes >= TB) {
        return QString("%1 TB").arg(bytes / (double)TB, 0, 'f', 2);
    } else if (bytes >= GB) {
        return QString("%1 GB").arg(bytes / (double)GB, 0, 'f', 2);
    } else if (bytes >= MB) {
        return QString("%1 MB").arg(bytes / (double)MB, 0, 'f', 2);
    } else if (bytes >= KB) {
        return QString("%1 KB").arg(bytes / (double)KB, 0, 'f', 2);
    } else {
        return QString("%1 B").arg(bytes);
    }
}

QList<DiskUsage> DiskAnalyzer::parseDfOutput(const QString &output)
{
    QList<DiskUsage> disks;
    QStringList lines = output.split('\n');
    
    // 跳过标题行
    for (int i = 1; i < lines.size(); ++i) {
        QString line = lines[i].trimmed();
        if (line.isEmpty()) continue;
        
        QStringList parts = line.split(QRegularExpression("\\s+"));
        if (parts.size() >= 7) {
            DiskUsage disk;
            disk.filesystem = parts[0];
            disk.type = parts[1];
            
            // 解析大小（带单位的字符串，如 "57G", "1.5T"）
            disk.total = parseSizeString(parts[2]);
            disk.used = parseSizeString(parts[3]);
            disk.available = parseSizeString(parts[4]);
            disk.percent = parts[5].replace("%", "").toDouble();
            disk.mountpoint = parts[6];
            
            disks.append(disk);
        }
    }
    
    return disks;
}

qint64 DiskAnalyzer::parseSizeString(const QString &sizeStr)
{
    QString str = sizeStr.trimmed().toUpper();
    double value = 0;
    QString unit;
    
    // 提取数字和单位
    QRegularExpression re("([0-9.]+)([KMGT]?)");
    QRegularExpressionMatch match = re.match(str);
    
    if (match.hasMatch()) {
        value = match.captured(1).toDouble();
        unit = match.captured(2);
    } else {
        return str.toLongLong() * 1024; // 默认按KB处理
    }
    
    // 转换为字节
    qint64 bytes = value;
    if (unit == "K") {
        bytes = value * 1024;
    } else if (unit == "M") {
        bytes = value * 1024 * 1024;
    } else if (unit == "G") {
        bytes = value * 1024 * 1024 * 1024;
    } else if (unit == "T") {
        bytes = value * 1024LL * 1024 * 1024 * 1024;
    } else {
        bytes = value * 1024; // 默认KB
    }
    
    return bytes;
}

ImmutableSystemInfo DiskAnalyzer::parseImmutableOutput(const QString &output)
{
    ImmutableSystemInfo info;
    info.enabled = true;
    
    QStringList lines = output.split('\n');
    for (const QString &line : lines) {
        if (line.contains("enabled")) {
            info.enabled = line.contains("true");
        } else if (line.contains("snapshot")) {
            info.currentSnapshot = line.split(":").last().trimmed();
        }
    }
    
    return info;
}

QList<LinglongAppInfo> DiskAnalyzer::parseLinglongOutput(const QString &output)
{
    QList<LinglongAppInfo> apps;
    QStringList lines = output.split('\n');
    
    for (const QString &line : lines) {
        if (line.isEmpty() || line.startsWith("ID")) continue;
        
        QStringList parts = line.split(QRegularExpression("\\s+"));
        if (parts.size() >= 3) {
            LinglongAppInfo app;
            app.id = parts[0];
            app.name = parts[1];
            app.version = parts[2];
            app.size = 0; // 需要额外命令获取
            
            apps.append(app);
        }
    }
    
    return apps;
}