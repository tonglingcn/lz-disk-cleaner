/*
 * System Slimmer - Implementation
 * 系统瘦身 - 实现文件
 */

#include "systemslimmer.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QDebug>
#include <QtGlobal>

#ifdef Q_OS_LINUX
#include <sys/stat.h>
#include <unistd.h>
#endif

// ==================== SystemSlimmerWorker ====================

SystemSlimmerWorker::SystemSlimmerWorker(QObject *parent)
    : QObject(parent)
    , m_scanLargeFiles(false)
    , m_scanDuplicates(false)
    , m_stopRequested(false)
    , m_totalScannedSize(0)
    , m_totalScannedFiles(0)
    , m_totalScannedDirs(0)
{
}

void SystemSlimmerWorker::setOptions(const SlimmerScanOptions &options)
{
    m_options = options;
}

void SystemSlimmerWorker::setScanMode(bool scanLargeFiles, bool scanDuplicates)
{
    m_scanLargeFiles = scanLargeFiles;
    m_scanDuplicates = scanDuplicates;
}

void SystemSlimmerWorker::startScan()
{
    m_stopRequested = false;
    m_largeFiles.clear();
    m_sizeGroups.clear();
    m_hashGroups.clear();
    m_totalScannedSize = 0;
    m_totalScannedFiles = 0;
    m_totalScannedDirs = 0;

    // 默认搜索路径
    if (m_options.searchPaths.isEmpty()) {
        m_options.searchPaths << QDir::homePath();
    }

    // 执行扫描
    for (const QString &path : m_options.searchPaths) {
        if (m_stopRequested) break;
        scanDirectory(path);
    }

    if (m_stopRequested) {
        emit scanError(tr("扫描已取消"));
        return;
    }

    // 处理大文件结果
    if (m_scanLargeFiles) {
        std::sort(m_largeFiles.begin(), m_largeFiles.end());
    }

    // 处理重复文件结果
    if (m_scanDuplicates) {
        findDuplicateFiles();
    }

    SlimmerScanResult result;
    result.success = true;
    result.largeFiles = m_largeFiles;
    result.totalScannedSize = m_totalScannedSize;
    result.totalScannedFiles = m_totalScannedFiles;
    result.totalScannedDirs = m_totalScannedDirs;
    
    // 将重复文件组添加到结果中
    for (auto it = m_hashGroups.begin(); it != m_hashGroups.end(); ++it) {
        SlimmerDuplicateGroup group;
        group.hash = it.key();
        group.size = 0;  // 将在下面设置
        group.paths = it.value();
        // 从第一个文件获取大小
        if (!group.paths.isEmpty()) {
            QFileInfo info(group.paths.first());
            group.size = info.size();
        }
        result.duplicateGroups.append(group);
    }

    emit scanFinished(result);
}

void SystemSlimmerWorker::stopScan()
{
    m_stopRequested = true;
}

void SystemSlimmerWorker::scanDirectory(const QString &path)
{
    if (m_stopRequested) return;
    if (shouldSkipPath(path)) return;

    QDir dir(path);
    if (!dir.exists()) return;

    // 基于已扫描文件数和目录数估算进度，从较低值开始逐步增长
    // 文件贡献：每扫描 500 个文件增加 1%
    // 目录贡献：每扫描 50 个目录增加 1%
    // 限制最大值为 95%，完成时由 UI 设置为 100%
    int fileProgress = m_totalScannedFiles / 500;
    int dirProgress = m_totalScannedDirs / 50;
    int estimatedProgress = qMin(95, fileProgress + dirProgress);
    emit scanProgress(path, estimatedProgress, m_totalScannedFiles, m_largeFiles.size());

    QFileInfoList entries = dir.entryInfoList(
        QDir::AllEntries | QDir::NoDotAndDotDot | 
        (m_options.scanHiddenFiles ? QDir::Hidden : QDir::Files),
        QDir::Name
    );

    for (const QFileInfo &info : entries) {
        if (m_stopRequested) return;

        if (info.isDir()) {
            m_totalScannedDirs++;
            scanDirectory(info.absoluteFilePath());
        } else if (info.isFile()) {
            m_totalScannedFiles++;
            m_totalScannedSize += info.size();

            // 跳过小于最小大小的文件
            if (info.size() < m_options.minFileSize) {
                continue;
            }

            SlimmerLargeFileInfo fileInfo;
            fileInfo.path = info.absoluteFilePath();
            fileInfo.name = info.fileName();
            fileInfo.size = info.size();
            fileInfo.lastModified = info.lastModified().toString("yyyy-MM-dd hh:mm");

            // 收集大文件
            if (m_scanLargeFiles) {
                m_largeFiles.append(fileInfo);
            }

            // 收集重复文件信息
            if (m_scanDuplicates) {
                m_sizeGroups[info.size()].append(info.absoluteFilePath());
            }
        }
    }
}

void SystemSlimmerWorker::findDuplicateFiles()
{
    // 只保留相同大小的文件组（潜在重复）
    QMap<qint64, QStringList>::iterator it = m_sizeGroups.begin();
    while (it != m_sizeGroups.end()) {
        if (it.value().size() < 2) {
            it = m_sizeGroups.erase(it);
        } else {
            ++it;
        }
    }

    // 进一步比较hash
    for (auto it = m_sizeGroups.begin(); it != m_sizeGroups.end() && !m_stopRequested; ++it) {
        qint64 size = it.key();
        const QStringList &files = it.value();

        // 按hash分组
        QMap<QString, QStringList> hashGroups;
        for (const QString &filePath : files) {
            if (m_stopRequested) return;

            QString hash = calculateFileHash(filePath, m_options.duplicateCompareMethod);
            if (!hash.isEmpty()) {
                hashGroups[hash].append(filePath);
            }
        }

        // 保存真正的重复组
        for (auto hit = hashGroups.begin(); hit != hashGroups.end(); ++hit) {
            if (hit.value().size() >= 2) {
                SlimmerDuplicateGroup group;
                group.hash = hit.key();
                group.size = size;
                group.paths = hit.value();
                m_hashGroups.insert(hit.key(), hit.value());
            }
        }
    }
}

QString SystemSlimmerWorker::calculateFileHash(const QString &filePath, int method)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }

    if (method == 0) {
        // 快速模式：只返回大小作为"hash"
        return QString::number(QFileInfo(filePath).size());
    } else if (method == 1) {
        // 标准模式：前4KB + 大小
        QByteArray data = file.read(4096);
        data.append(QString::number(file.size()).toUtf8());
        return QString(QCryptographicHash::hash(data, QCryptographicHash::Md5).toHex());
    } else {
        // 完整模式：MD5
        QCryptographicHash hash(QCryptographicHash::Md5);
        while (!file.atEnd() && !m_stopRequested) {
            hash.addData(file.read(1024 * 1024)); // 每次1MB
        }
        return QString(hash.result().toHex());
    }
}

bool SystemSlimmerWorker::shouldSkipPath(const QString &path)
{
    // 排除路径检查
    for (const QString &exclude : m_options.excludePaths) {
        if (path.startsWith(exclude)) {
            return true;
        }
    }

    // 系统目录保护
    static const QStringList protectedPaths = {
        "/proc", "/sys", "/dev", "/run", "/boot",
        "/etc", "/usr", "/bin", "/sbin", "/lib", "/lib64"
    };
    
    for (const QString &protectedPath : protectedPaths) {
        if (path.startsWith(protectedPath) && path != protectedPath + "/") {
            return true;
        }
    }

    // 隐藏文件检查
    if (!m_options.scanHiddenFiles) {
        QFileInfo info(path);
        if (info.fileName().startsWith(".")) {
            return true;
        }
    }

    return false;
}

// ==================== SystemSlimmer ====================

SystemSlimmer::SystemSlimmer(QObject *parent)
    : QObject(parent)
    , m_workerThread(nullptr)
    , m_worker(nullptr)
{
}

SystemSlimmer::~SystemSlimmer()
{
    stopScan();
}

void SystemSlimmer::startScan(const SlimmerScanOptions &options, bool scanLargeFiles, bool scanDuplicates)
{
    // 先停止并清理之前的扫描
    stopScan();
    
    // 清理之前的 worker 和 thread
    if (m_workerThread) {
        if (m_workerThread->isRunning()) {
            m_workerThread->quit();
            m_workerThread->wait(3000);
        }
        // 不使用 deleteLater，手动删除
        m_workerThread->deleteLater();
        m_workerThread = nullptr;
    }
    if (m_worker) {
        m_worker->deleteLater();
        m_worker = nullptr;
    }

    m_workerThread = new QThread(this);
    m_worker = new SystemSlimmerWorker();
    m_worker->moveToThread(m_workerThread);

    m_worker->setOptions(options);
    m_worker->setScanMode(scanLargeFiles, scanDuplicates);

    connect(m_workerThread, &QThread::started, m_worker, &SystemSlimmerWorker::startScan);
    connect(m_worker, &SystemSlimmerWorker::scanFinished, this, &SystemSlimmer::scanFinished);
    connect(m_worker, &SystemSlimmerWorker::scanProgress, this, &SystemSlimmer::scanProgress);
    connect(m_worker, &SystemSlimmerWorker::scanError, this, &SystemSlimmer::scanError);
    connect(m_worker, &SystemSlimmerWorker::scanFinished, m_workerThread, &QThread::quit);
    connect(m_worker, &SystemSlimmerWorker::scanError, m_workerThread, &QThread::quit);

    m_workerThread->start();
}

void SystemSlimmer::stopScan()
{
    if (m_worker) {
        m_worker->stopScan();
    }
    if (m_workerThread && m_workerThread->isRunning()) {
        m_workerThread->quit();
        m_workerThread->wait(3000);
    }
    // QPointer 会自动变为 nullptr 当对象被删除时
    // 不需要手动设置为 nullptr
}

bool SystemSlimmer::isScanning() const
{
    return m_workerThread && m_workerThread->isRunning();
}

bool SystemSlimmer::deleteFile(const QString &filePath, QString *errorMsg)
{
    QFile file(filePath);
    if (!file.remove()) {
        if (errorMsg) *errorMsg = file.errorString();
        return false;
    }
    return true;
}

bool SystemSlimmer::moveToTrash(const QString &filePath, QString *errorMsg)
{
#ifdef Q_OS_LINUX
    // Linux 使用 trash-cli 或移动到 ~/.local/share/Trash
    QString trashPath = QDir::homePath() + "/.local/share/Trash/files/";
    QString infoPath = QDir::homePath() + "/.local/share/Trash/info/";
    
    QDir().mkpath(trashPath);
    QDir().mkpath(infoPath);
    
    QFileInfo originalInfo(filePath);
    QString trashName = originalInfo.fileName();
    QString destPath = trashPath + trashName;
    
    // 处理重名
    int counter = 1;
    while (QFile::exists(destPath)) {
        trashName = QString("%1 (%2)").arg(originalInfo.fileName()).arg(counter);
        destPath = trashPath + trashName;
        counter++;
    }
    
    // 移动文件
    if (!QFile::rename(filePath, destPath)) {
        if (errorMsg) *errorMsg = QObject::tr("无法移动文件到回收站");
        return false;
    }
    
    // 创建 .trashinfo 文件
    QString infoFile = infoPath + trashName + ".trashinfo";
    QFile info(infoFile);
    if (info.open(QIODevice::WriteOnly)) {
        QString content = QString("[Trash Info]\nPath=%1\nDeletionDate=%2\n")
            .arg(filePath)
            .arg(QDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ss"));
        info.write(content.toUtf8());
        info.close();
    }
    
    return true;
#else
    // 其他平台直接删除
    return deleteFile(filePath, errorMsg);
#endif
}
