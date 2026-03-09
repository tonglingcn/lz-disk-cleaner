/*
 * System Slimmer - Header
 * 系统瘦身 - 头文件
 * 用于查找大文件和重复文件
 */

#ifndef SYSTEMSLIMMER_H
#define SYSTEMSLIMMER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <QThread>
#include <QCryptographicHash>
#include <QMap>
#include <QPointer>

// 大文件信息
struct SlimmerLargeFileInfo {
    QString path;
    QString name;
    qint64 size;
    QString lastModified;
    
    bool operator<(const SlimmerLargeFileInfo &other) const {
        return size > other.size; // 从大到小排序
    }
};

// 重复文件组
struct SlimmerDuplicateGroup {
    QString hash;
    qint64 size;
    QList<QString> paths;
};

// 扫描选项
struct SlimmerScanOptions {
    qint64 minFileSize;          // 最小文件大小（字节）
    QStringList searchPaths;     // 搜索路径
    QStringList excludePaths;    // 排除路径
    bool scanHiddenFiles;        // 是否扫描隐藏文件
    int duplicateCompareMethod;  // 0=快速(只比较大小), 1=标准(前4KB+大小), 2=完整(MD5)
};

// 扫描结果
struct SlimmerScanResult {
    bool success;
    QString errorMessage;
    QList<SlimmerLargeFileInfo> largeFiles;
    QList<SlimmerDuplicateGroup> duplicateGroups;
    qint64 totalScannedSize;
    int totalScannedFiles;
    int totalScannedDirs;
};

class SystemSlimmerWorker : public QObject {
    Q_OBJECT

public:
    explicit SystemSlimmerWorker(QObject *parent = nullptr);
    void setOptions(const SlimmerScanOptions &options);
    void setScanMode(bool scanLargeFiles, bool scanDuplicates);

public slots:
    void startScan();
    void stopScan();

signals:
    void scanProgress(const QString &currentPath, int percent, int fileCount, int largeFileCount);
    void scanFinished(const SlimmerScanResult &result);
    void scanError(const QString &error);

private:
    void scanDirectory(const QString &path);
    void findLargeFiles();
    void findDuplicateFiles();
    QString calculateFileHash(const QString &filePath, int method);
    bool shouldSkipPath(const QString &path);
    
    SlimmerScanOptions m_options;
    bool m_scanLargeFiles;
    bool m_scanDuplicates;
    bool m_stopRequested;
    
    QList<SlimmerLargeFileInfo> m_largeFiles;
    QMap<qint64, QStringList> m_sizeGroups;  // 用于快速查找重复
    QMap<QString, QStringList> m_hashGroups; // 按hash分组的文件
    
    qint64 m_totalScannedSize;
    int m_totalScannedFiles;
    int m_totalScannedDirs;
};

class SystemSlimmer : public QObject {
    Q_OBJECT

public:
    explicit SystemSlimmer(QObject *parent = nullptr);
    ~SystemSlimmer();

    void startScan(const SlimmerScanOptions &options, bool scanLargeFiles, bool scanDuplicates);
    void stopScan();
    bool isScanning() const;
    
    // 删除文件
    static bool deleteFile(const QString &filePath, QString *errorMsg = nullptr);
    static bool moveToTrash(const QString &filePath, QString *errorMsg = nullptr);

signals:
    void scanProgress(const QString &currentPath, int percent, int fileCount, int largeFileCount);
    void scanFinished(const SlimmerScanResult &result);
    void scanError(const QString &error);

private:
    QPointer<QThread> m_workerThread;
    QPointer<SystemSlimmerWorker> m_worker;
};

#endif // SYSTEMSLIMMER_H
