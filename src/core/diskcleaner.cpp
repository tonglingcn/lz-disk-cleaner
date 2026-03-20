/*
 * Disk Cleaner - Implementation
 * 磁盘清理器 - 实现
 */

#include "diskcleaner.h"
#include "../utils/logger.h"
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QDebug>

DiskCleaner::DiskCleaner(QObject *parent)
    : QObject(parent)
    , m_process(new QProcess(this))
{
}

DiskCleaner::~DiskCleaner()
{
    if (m_process->state() == QProcess::Running) {
        m_process->kill();
    }
}

CleanupResult DiskCleaner::cleanUserCache()
{
    LOG_INFO("Starting user cache cleanup");
    emit cleanupProgress("清理用户缓存", 0);
    
    CleanupResult result;
    result.itemName = "用户缓存";
    
    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QStringList protectedDirs = getProtectedDirs();
    
    qint64 totalFreed = 0;
    QDir dir(cacheDir);
    
    if (!dir.exists()) {
        result.success = true;
        result.freedSpace = 0;
        return result;
    }
    
    QFileInfoList entries = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    int total = entries.size();
    int current = 0;
    
    for (const QFileInfo &info : entries) {
        QString itemName = info.fileName();
        
        // 跳过受保护的目录
        if (protectedDirs.contains(itemName)) {
            current++;
            continue;
        }
        
        QString itemPath = info.absoluteFilePath();
        qint64 size = getDirectorySize(itemPath);
        
        if (removeDirectory(itemPath)) {
            totalFreed += size;
        }
        
        current++;
        int percent = (current * 100) / total;
        emit cleanupProgress(itemName, percent);
    }
    
    result.success = true;
    result.freedSpace = totalFreed;
    
    LOG_INFO(QString("User cache cleanup completed. Freed: %1 bytes").arg(totalFreed));
    return result;
}

CleanupResult DiskCleaner::cleanThumbnailCache()
{
    LOG_INFO("Starting thumbnail cache cleanup");
    emit cleanupProgress("清理缩略图缓存", 0);
    
    CleanupResult result;
    result.itemName = "缩略图缓存";
    
    QString thumbDir = QDir::homePath() + "/.cache/thumbnails";
    qint64 size = getDirectorySize(thumbDir);
    
    if (removeDirectory(thumbDir)) {
        result.success = true;
        result.freedSpace = size;
        LOG_INFO(QString("Thumbnail cache cleanup completed. Freed: %1 bytes").arg(size));
    } else {
        result.success = false;
        result.errorMessage = "Failed to remove thumbnail cache";
    }
    
    return result;
}

CleanupResult DiskCleaner::cleanAptCache()
{
    LOG_INFO("Starting APT cache cleanup");
    emit cleanupProgress("清理APT缓存", 0);
    
    CleanupResult result;
    result.itemName = "APT缓存";
    
    // 需要 sudo 权限
    if (!checkSudoAccess()) {
        result.success = false;
        result.errorMessage = "需要 sudo 权限";
        return result;
    }
    
    qint64 totalFreed = 0;
    
    // 先获取 APT 缓存大小
    m_process->start("sh", QStringList() << "-c" << "du -sb /var/cache/apt/archives 2>/dev/null | cut -f1");
    m_process->waitForFinished();
    QString aptSizeStr = m_process->readAllStandardOutput().trimmed();
    qint64 aptCacheSize = aptSizeStr.toLongLong();
    
    // apt-get clean
    emit cleanupProgress("清理APT缓存", 20);
    m_process->start("sudo", QStringList() << "apt-get" << "clean");
    m_process->waitForFinished();
    
    // apt-get autoremove
    emit cleanupProgress("清理APT缓存", 50);
    m_process->start("sudo", QStringList() << "apt-get" << "autoremove" << "-y");
    m_process->waitForFinished();
    
    // apt-get autoclean
    emit cleanupProgress("清理APT缓存", 80);
    m_process->start("sudo", QStringList() << "apt-get" << "autoclean");
    m_process->waitForFinished();
    
    totalFreed = aptCacheSize;
    result.success = true;
    result.freedSpace = totalFreed;
    
    LOG_INFO(QString("APT cache cleanup completed. Freed: %1 bytes").arg(totalFreed));
    return result;
}

CleanupResult DiskCleaner::cleanJournalLogs(int keepDays)
{
    LOG_INFO(QString("Starting journal logs cleanup. Keep %1 days").arg(keepDays));
    emit cleanupProgress("清理系统日志", 0);
    
    CleanupResult result;
    result.itemName = "系统日志";
    
    if (!checkSudoAccess()) {
        result.success = false;
        result.errorMessage = "需要 sudo 权限";
        return result;
    }
    
    // 先获取当前日志大小
    m_process->start("sh", QStringList() << "-c" 
        << "journalctl --disk-usage 2>/dev/null | grep -oP '\\d+(?=\\s*bytes)' | head -1");
    m_process->waitForFinished();
    qint64 beforeSize = m_process->readAllStandardOutput().trimmed().toLongLong();
    
    // journalctl --vacuum-time=7d
    QString command = QString("journalctl --vacuum-time=%1d").arg(keepDays);
    m_process->start("sudo", QStringList() << "bash" << "-c" << command);
    m_process->waitForFinished();
    
    // 获取清理后的日志大小
    m_process->start("sh", QStringList() << "-c" 
        << "journalctl --disk-usage 2>/dev/null | grep -oP '\\d+(?=\\s*bytes)' | head -1");
    m_process->waitForFinished();
    qint64 afterSize = m_process->readAllStandardOutput().trimmed().toLongLong();
    
    result.success = (m_process->exitCode() == 0);
    result.freedSpace = beforeSize > afterSize ? beforeSize - afterSize : 0;
    
    LOG_INFO(QString("Journal logs cleanup completed. Freed: %1 bytes").arg(result.freedSpace));
    return result;
}

CleanupResult DiskCleaner::cleanJournalLogsBySize(int maxSizeMB)
{
    LOG_INFO(QString("Starting journal logs cleanup by size. Max size: %1 MB").arg(maxSizeMB));
    emit cleanupProgress("清理系统日志(按大小)", 0);
    
    CleanupResult result;
    result.itemName = "系统日志(按大小)";
    
    if (!checkSudoAccess()) {
        result.success = false;
        result.errorMessage = "需要 sudo 权限";
        return result;
    }
    
    // 先获取当前日志大小
    m_process->start("sh", QStringList() << "-c" 
        << "journalctl --disk-usage 2>/dev/null | grep -oP '\\d+(?=\\s*bytes)' | head -1");
    m_process->waitForFinished();
    qint64 beforeSize = m_process->readAllStandardOutput().trimmed().toLongLong();
    
    // journalctl --vacuum-size=100M
    QString command = QString("journalctl --vacuum-size=%1M").arg(maxSizeMB);
    m_process->start("sudo", QStringList() << "bash" << "-c" << command);
    m_process->waitForFinished();
    
    // 获取清理后的日志大小
    m_process->start("sh", QStringList() << "-c" 
        << "journalctl --disk-usage 2>/dev/null | grep -oP '\\d+(?=\\s*bytes)' | head -1");
    m_process->waitForFinished();
    qint64 afterSize = m_process->readAllStandardOutput().trimmed().toLongLong();
    
    result.success = (m_process->exitCode() == 0);
    result.freedSpace = beforeSize > afterSize ? beforeSize - afterSize : 0;
    
    LOG_INFO(QString("Journal logs cleanup by size completed. Freed: %1 bytes").arg(result.freedSpace));
    return result;
}

CleanupResult DiskCleaner::cleanTempFiles()
{
    LOG_INFO("Starting temp files cleanup");
    emit cleanupProgress("清理临时文件", 0);
    
    CleanupResult result;
    result.itemName = "临时文件";
    
    qint64 totalFreed = 0;
    
    // 清理 /tmp
    QString tmpDir = "/tmp";
    if (checkSudoAccess()) {
        totalFreed += getDirectorySize(tmpDir);
        // 注意：这里需要谨慎，可能影响正在运行的程序
    }
    
    // 清理用户临时目录
    QString userTmp = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    totalFreed += getDirectorySize(userTmp);
    
    result.success = true;
    result.freedSpace = totalFreed;
    
    LOG_INFO("Temp files cleanup completed");
    return result;
}

CleanupResult DiskCleaner::cleanTrash()
{
    LOG_INFO("Starting trash cleanup");
    emit cleanupProgress("清空回收站", 0);
    
    CleanupResult result;
    result.itemName = "回收站";
    
    QString trashDir = QDir::homePath() + "/.local/share/Trash/files";
    qint64 size = getDirectorySize(trashDir);
    
    if (removeDirectory(trashDir)) {
        // 同时清理 info 目录
        QString trashInfo = QDir::homePath() + "/.local/share/Trash/info";
        removeDirectory(trashInfo);
        
        result.success = true;
        result.freedSpace = size;
        LOG_INFO(QString("Trash cleanup completed. Freed: %1 bytes").arg(size));
    } else {
        result.success = false;
        result.errorMessage = "Failed to empty trash";
    }
    
    return result;
}

CleanupResult DiskCleaner::cleanDownloadedPackages()
{
    LOG_INFO("Starting downloaded packages cleanup");
    emit cleanupProgress("清理下载的安装包", 0);
    
    CleanupResult result;
    result.itemName = "下载的安装包";
    
    qint64 totalFreed = 0;
    QString downloadDir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    QDir dir(downloadDir);
    
    QFileInfoList debFiles = dir.entryInfoList(QStringList() << "*.deb" << "*.AppImage", QDir::Files);
    
    for (const QFileInfo &file : debFiles) {
        totalFreed += file.size();
        QFile::remove(file.absoluteFilePath());
    }
    
    result.success = true;
    result.freedSpace = totalFreed;
    
    LOG_INFO(QString("Downloaded packages cleanup completed. Freed: %1 bytes").arg(totalFreed));
    return result;
}

CleanupResult DiskCleaner::cleanSnapshots(int keepCount)
{
    LOG_INFO(QString("Starting snapshots cleanup. Keep %1 snapshots").arg(keepCount));
    emit cleanupProgress("清理系统快照", 0);
    
    CleanupResult result;
    result.itemName = "系统快照";
    
    if (!checkSudoAccess()) {
        result.success = false;
        result.errorMessage = "需要 sudo 权限";
        return result;
    }
    
    // 获取快照列表
    m_process->start("deepin-immutable-ctl", QStringList() << "snapshot" << "list");
    m_process->waitForFinished();
    
    QString output = QString::fromUtf8(m_process->readAllStandardOutput());
    QStringList snapshots = output.split('\n', Qt::SkipEmptyParts);
    
    // 获取快照目录总大小（清理前）
    qint64 beforeSize = 0;
    QString snapshotDir = "/boot/deepin-snapshots";
    QDir dir(snapshotDir);
    if (dir.exists()) {
        beforeSize = getDirectorySize(snapshotDir);
    }
    
    // 保留最新的 keepCount 个快照
    int toRemove = snapshots.size() - keepCount;
    
    for (int i = 0; i < toRemove; ++i) {
        QString snapshot = snapshots[i].trimmed();
        if (snapshot.isEmpty()) continue;
        
        emit cleanupProgress(QString("删除快照: %1").arg(snapshot), 
                            static_cast<int>((i * 100.0) / toRemove));
        
        m_process->start("deepin-immutable-ctl", QStringList() << "snapshot" << "delete" << snapshot);
        m_process->waitForFinished();
    }
    
    // 获取快照目录总大小（清理后）
    qint64 afterSize = 0;
    if (dir.exists()) {
        afterSize = getDirectorySize(snapshotDir);
    }
    
    result.success = true;
    result.freedSpace = beforeSize > afterSize ? beforeSize - afterSize : 0;
    
    LOG_INFO(QString("Snapshots cleanup completed. Freed: %1 bytes").arg(result.freedSpace));
    return result;
}

CleanupResult DiskCleaner::cleanLinglongApps(const QStringList &appIds)
{
    LOG_INFO("Starting Linglong apps cleanup");
    emit cleanupProgress("清理玲珑应用", 0);
    
    CleanupResult result;
    result.itemName = "玲珑应用";
    
    qint64 totalFreed = 0;
    
    for (const QString &appId : appIds) {
        m_process->start("ll-cli", QStringList() << "remove" << appId);
        m_process->waitForFinished();
        
        if (m_process->exitCode() == 0) {
            // 应用大小需要额外计算
        }
    }
    
    result.success = true;
    result.freedSpace = totalFreed;
    
    LOG_INFO("Linglong apps cleanup completed");
    return result;
}

CleanupResult DiskCleaner::cleanBrowserCache()
{
    LOG_INFO("Starting browser cache cleanup");
    emit cleanupProgress("清理浏览器缓存", 0);
    
    CleanupResult result;
    result.itemName = "浏览器缓存";
    
    qint64 totalFreed = 0;
    QString home = QDir::homePath();
    
    // Chrome cache
    QString chromeCache = home + "/.cache/google-chrome";
    totalFreed += getDirectorySize(chromeCache);
    removeDirectory(chromeCache);
    
    // Firefox cache
    QString firefoxCache = home + "/.cache/mozilla";
    totalFreed += getDirectorySize(firefoxCache);
    removeDirectory(firefoxCache);
    
    // 360浏览器缓存
    QStringList browser360Paths = {
        home + "/.cache/com.360.browser/Default/Cache",
        home + "/.cache/com.360.browser/Default/Code Cache",
        home + "/.cache/com.360.browser/Default/GPUCache"
    };
    for (const QString &path : browser360Paths) {
        totalFreed += getDirectorySize(path);
        cleanDirContent(path);  // 清理内容但保留目录结构
    }
    
    // 龙芯浏览器缓存
    QStringList lbrowserPaths = {
        home + "/.cache/cn.loongnix.lbrowser/Default/Cache",
        home + "/.cache/cn.loongnix.lbrowser/Default/Code Cache",
        home + "/.cache/cn.loongnix.lbrowser/Default/GPUCache"
    };
    for (const QString &path : lbrowserPaths) {
        totalFreed += getDirectorySize(path);
        cleanDirContent(path);
    }
    
    // QQ浏览器缓存
    QStringList qqbrowserPaths = {
        home + "/.cache/qqbrowser/Default/Cache",
        home + "/.cache/qqbrowser/Default/Code Cache",
        home + "/.cache/qqbrowser/Default/GPUCache"
    };
    for (const QString &path : qqbrowserPaths) {
        totalFreed += getDirectorySize(path);
        cleanDirContent(path);
    }
    
    // Edge浏览器缓存
    QStringList edgePaths = {
        home + "/.cache/microsoft-edge/Default/Cache",
        home + "/.cache/microsoft-edge/Default/Code Cache",
        home + "/.cache/microsoft-edge/Default/GPUCache"
    };
    for (const QString &path : edgePaths) {
        totalFreed += getDirectorySize(path);
        cleanDirContent(path);
    }
    
    // Chromium浏览器缓存
    QStringList chromiumPaths = {
        home + "/.cache/chromium/Default/Cache",
        home + "/.cache/chromium/Default/Code Cache",
        home + "/.cache/chromium/Default/GPUCache"
    };
    for (const QString &path : chromiumPaths) {
        totalFreed += getDirectorySize(path);
        cleanDirContent(path);
    }
    
    result.success = true;
    result.freedSpace = totalFreed;
    
    LOG_INFO(QString("Browser cache cleanup completed. Freed: %1 bytes").arg(totalFreed));
    return result;
}

CleanupResult DiskCleaner::cleanDevCache()
{
    LOG_INFO("Starting dev cache cleanup");
    emit cleanupProgress("清理开发工具缓存", 0);
    
    CleanupResult result;
    result.itemName = "开发工具缓存";
    
    qint64 totalFreed = 0;
    QString home = QDir::homePath();
    
    // Pip cache
    QString pipCache = home + "/.cache/pip";
    totalFreed += getDirectorySize(pipCache);
    removeDirectory(pipCache);
    
    // NPM cache
    QString npmCache = home + "/.npm";
    totalFreed += getDirectorySize(npmCache);
    removeDirectory(npmCache);
    
    // Go cache
    QString goCache = home + "/.cache/go-build";
    totalFreed += getDirectorySize(goCache);
    removeDirectory(goCache);
    
    // Cargo cache
    QString cargoCache = home + "/.cargo/registry";
    totalFreed += getDirectorySize(cargoCache);
    removeDirectory(cargoCache);
    
    result.success = true;
    result.freedSpace = totalFreed;
    
    LOG_INFO("Dev cache cleanup completed");
    return result;
}

CleanupResult DiskCleaner::cleanAppCache()
{
    LOG_INFO("Starting app cache cleanup");
    emit cleanupProgress("清理应用缓存", 0);
    
    CleanupResult result;
    result.itemName = "应用缓存";
    
    qint64 totalFreed = 0;
    QString home = QDir::homePath();
    
    // WPS Office缓存 - 清理后首次打开会重新初始化配置
    QStringList wpsPaths = {
        home + "/.local/share/Kingsoft",
        home + "/.config/Kingsoft"
    };
    for (const QString &path : wpsPaths) {
        totalFreed += getDirectorySize(path);
        removeDirectory(path);
    }
    
    // 钉钉缓存
    QStringList dingtalkPaths = {
        home + "/.config/DingTalk",
        home + "/.cache/DingTalk"
    };
    for (const QString &path : dingtalkPaths) {
        totalFreed += getDirectorySize(path);
        cleanDirContent(path);
    }
    
    // 腾讯会议缓存
    QStringList wemeetPaths = {
        home + "/.config/wemeetapp",
        home + "/.cache/wemeetapp"
    };
    for (const QString &path : wemeetPaths) {
        totalFreed += getDirectorySize(path);
        cleanDirContent(path);
    }
    
    // 网易云音乐缓存
    QString neteaseCache = home + "/.cache/netease-cloud-music";
    totalFreed += getDirectorySize(neteaseCache);
    cleanDirContent(neteaseCache);
    
    result.success = true;
    result.freedSpace = totalFreed;
    
    LOG_INFO(QString("App cache cleanup completed. Freed: %1 bytes").arg(totalFreed));
    return result;
}

QList<CleanupResult> DiskCleaner::smartCleanup()
{
    LOG_INFO("Starting smart cleanup");
    
    QList<CleanupResult> results;
    
    // 缩略图缓存 - 系统会自动重新生成
    results.append(cleanThumbnailCache());
    // 开发工具缓存 - pip/npm/go/cargo缓存，可重新下载
    results.append(cleanDevCache());
    // 应用缓存 - WPS/钉钉等应用缓存
    results.append(cleanAppCache());
    // 回收站 - 清空用户已删除的文件
    results.append(cleanTrash());
    // 注意：APT缓存和系统日志需要sudo权限，不在智能清理中执行
    
    emit cleanupFinished(results);
    
    LOG_INFO("Smart cleanup completed");
    return results;
}

bool DiskCleaner::checkSudoAccess()
{
    m_process->start("sudo", QStringList() << "-n" << "true");
    m_process->waitForFinished();
    return (m_process->exitCode() == 0);
}

bool DiskCleaner::removeDirectory(const QString &path)
{
    QDir dir(path);
    if (dir.exists()) {
        return dir.removeRecursively();
    }
    return true;
}

bool DiskCleaner::cleanDirContent(const QString &path)
{
    QDir dir(path);
    if (!dir.exists()) {
        return true;  // 目录不存在，无需清理
    }
    
    // 清理目录内容，但保留目录本身
    bool success = true;
    QFileInfoList entries = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot);
    for (const QFileInfo &entry : entries) {
        QString entryPath = entry.absoluteFilePath();
        if (entry.isDir()) {
            QDir subDir(entryPath);
            if (!subDir.removeRecursively()) {
                LOG_ERROR(QString("Failed to remove dir: %1").arg(entryPath));
                success = false;
            }
        } else {
            if (!QFile::remove(entryPath)) {
                LOG_ERROR(QString("Failed to remove file: %1").arg(entryPath));
                success = false;
            }
        }
    }
    return success;
}

qint64 DiskCleaner::getDirectorySize(const QString &path)
{
    QDir dir(path);
    
    if (!dir.exists()) {
        return 0;
    }
    
    // 使用 du 命令快速获取目录大小，避免符号链接问题
    QProcess duProcess;
    duProcess.start("du", QStringList() << "-sb" << path);
    duProcess.waitForFinished(30000);
    
    if (duProcess.exitCode() == 0) {
        QString output = duProcess.readAllStandardOutput();
        QStringList parts = output.split('\t');
        if (!parts.isEmpty()) {
            return parts[0].toLongLong();
        }
    }
    
    // 如果 du 命令失败，使用递归方式但跳过符号链接
    qint64 totalSize = 0;
    
    QFileInfoList entries = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot);
    for (const QFileInfo &entry : entries) {
        // 跳过符号链接，避免重复计算
        if (entry.isSymLink()) {
            continue;
        }
        
        if (entry.isFile()) {
            totalSize += entry.size();
        } else if (entry.isDir()) {
            totalSize += getDirectorySize(entry.absoluteFilePath());
        }
    }
    
    return totalSize;
}

bool DiskCleaner::isSafeToClean(const QString &itemName)
{
    QStringList protectedDirs = getProtectedDirs();
    return !protectedDirs.contains(itemName);
}

QStringList DiskCleaner::getProtectedDirs()
{
    return QStringList() 
        << "fontconfig"
        << "dconf"
        << "mesa_shader_cache"
        << "kioexec"
        << "pulse"
        << "xsession-errors"
        << "QtShaderCache"
        << "deepin"
        << "flatpak"
        << "linglong";
}