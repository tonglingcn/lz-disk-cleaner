/*
 * Disk Cleaner - Header
 * 磁盘清理器 - 头文件
 */

#ifndef DISKCLEANER_H
#define DISKCLEANER_H

#include <QObject>
#include <QProcess>
#include <QStringList>

struct CleanupResult {
    QString itemName;
    qint64 freedSpace;
    bool success;
    QString errorMessage;
};

class DiskCleaner : public QObject
{
    Q_OBJECT

public:
    explicit DiskCleaner(QObject *parent = nullptr);
    ~DiskCleaner();
    
    // 清理功能
    CleanupResult cleanUserCache();
    CleanupResult cleanThumbnailCache();
    CleanupResult cleanAptCache();
    CleanupResult cleanJournalLogs(int keepDays = 7);
    CleanupResult cleanJournalLogsBySize(int maxSizeMB = 100);
    CleanupResult cleanTempFiles();
    CleanupResult cleanTrash();
    CleanupResult cleanDownloadedPackages();
    CleanupResult cleanSnapshots(int keepCount = 3);
    CleanupResult cleanLinglongApps(const QStringList &appIds);
    CleanupResult cleanBrowserCache();
    CleanupResult cleanDevCache();
    
    // 智能清理
    QList<CleanupResult> smartCleanup();
    
    // 检查清理权限
    bool checkSudoAccess();
    
signals:
    void cleanupProgress(const QString &itemName, int percent);
    void cleanupFinished(const QList<CleanupResult> &results);
    void cleanupError(const QString &error);

private:
    QProcess *m_process;
    
    bool removeDirectory(const QString &path);
    qint64 getDirectorySize(const QString &path);
    bool isSafeToClean(const QString &itemName);
    QStringList getProtectedDirs();
    bool cleanDirContent(const QString &path);  // 清理目录内容但保留目录本身
};

#endif // DISKCLEANER_H