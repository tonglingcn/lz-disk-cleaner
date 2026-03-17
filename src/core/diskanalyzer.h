/*
 * Disk Analyzer - Header
 * 磁盘分析器 - 头文件
 */

#ifndef DISKANALYZER_H
#define DISKANALYZER_H

#include <QObject>
#include <QProcess>
#include <QMap>
#include <QStringList>
#include <QPair>

struct DiskUsage {
    QString filesystem;
    QString type;
    qint64 total;
    qint64 used;
    qint64 available;
    double percent;
    QString mountpoint;
};

struct ImmutableSystemInfo {
    bool enabled;
    QString currentSnapshot;
    QString modifiedLayer;
    qint64 modifiedSize;
    QStringList snapshots;
    // 新增部署信息
    QString deploymentInfo;
    qint64 totalSnapshotSize;
    int snapshotCount;
    // 新增：各组件大小
    qint64 ostreeRepoSize;     // /ostree/repo 大小
    qint64 deploySize;         // /ostree/deploy 大小
    qint64 overlaySize;        // /root/persistent/overlay 大小
};

struct LinglongAppInfo {
    QString name;
    QString version;
    QString id;
    qint64 size;
    QStringList dependencies;
};

struct DirectoryUsage {
    QString path;
    qint64 size;
    int fileCount;
};

class DiskAnalyzer : public QObject
{
    Q_OBJECT

public:
    explicit DiskAnalyzer(QObject *parent = nullptr);
    ~DiskAnalyzer();
    
    // 磁盘分析
    QList<DiskUsage> analyzeDiskUsage();
    QList<DiskUsage> analyzePersistentPartition();
    QList<DirectoryUsage> analyzeHomeDirectory(const QString &path, int maxItems = 20);
    
    // 磐石系统分析
    ImmutableSystemInfo analyzeImmutableSystem();
    
    // 玲珑应用分析
    QList<LinglongAppInfo> analyzeLinglongApps();
    
    // 特殊应用数据
    QMap<QString, qint64> analyzeSpecialApps();
    
    // 缓存大小计算
    qint64 calculateCacheSize(const QString &path);
    
signals:
    void analysisProgress(int percent);
    void analysisFinished();
    void errorOccurred(const QString &error);

private:
    QProcess *m_process;
    
    qint64 getDirectorySize(const QString &path);
    qint64 parseSizeToBytes(const QString &sizeStr);
    qint64 parseSizeString(const QString &sizeStr);
    QString formatSize(qint64 bytes);
    QList<DiskUsage> parseDfOutput(const QString &output);
    ImmutableSystemInfo parseImmutableOutput(const QString &output);
    QList<LinglongAppInfo> parseLinglongOutput(const QString &output);
};

#endif // DISKANALYZER_H