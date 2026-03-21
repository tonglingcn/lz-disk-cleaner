/*
 * LZ Disk Cleaner Helper - 权限提升删除工具
 * 通过 PolicyKit (pkexec) 调用执行需要 root 权限的删除操作
 * 
 * Copyright (C) 2025-2026 tonglingcn
 */

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QDebug>
#include <QProcess>
#include <QDateTime>
#include <iostream>

// 递归删除目录
bool removeDirectory(const QString &path)
{
    QDir dir(path);
    if (!dir.exists()) {
        return true;  // 目录不存在也算成功
    }
    
    QFileInfoList entries = dir.entryInfoList(
        QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    
    for (const QFileInfo &entry : entries) {
        QString entryPath = entry.absoluteFilePath();
        if (entry.isDir()) {
            if (!removeDirectory(entryPath)) {
                return false;
            }
        } else {
            if (!QFile::remove(entryPath)) {
                std::cerr << "Failed to remove file: " << entryPath.toStdString() << std::endl;
                return false;
            }
        }
    }
    
    // 删除空目录本身
    if (!dir.rmdir(path)) {
        std::cerr << "Failed to remove directory: " << path.toStdString() << std::endl;
        return false;
    }
    
    return true;
}

// 清空目录内容（保留目录本身）
bool clearDirectoryContents(const QString &path)
{
    QDir dir(path);
    if (!dir.exists()) {
        return true;
    }
    
    QFileInfoList entries = dir.entryInfoList(
        QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    
    bool success = true;
    for (const QFileInfo &entry : entries) {
        QString entryPath = entry.absoluteFilePath();
        if (entry.isDir()) {
            if (!removeDirectory(entryPath)) {
                success = false;
            }
        } else {
            if (!QFile::remove(entryPath)) {
                std::cerr << "Failed to remove file: " << entryPath.toStdString() << std::endl;
                success = false;
            }
        }
    }
    
    return success;
}

// 清理 APT 缓存
bool cleanAptCache()
{
    QString aptCachePath = "/var/cache/apt/archives";
    QDir aptDir(aptCachePath);
    
    if (!aptDir.exists()) {
        return true;
    }
    
    QFileInfoList entries = aptDir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    bool success = true;
    
    for (const QFileInfo &entry : entries) {
        QString fileName = entry.fileName();
        // 保留 lock 文件
        if (fileName == "lock" || fileName == "lockfront") {
            continue;
        }
        if (!QFile::remove(entry.absoluteFilePath())) {
            success = false;
        }
    }
    
    return success;
}

// 清理系统日志（保留最近的）
bool cleanSystemLogs(int keepDays = 7)
{
    QDir logDir("/var/log");
    if (!logDir.exists()) {
        return true;
    }
    
    QFileInfoList entries = logDir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    bool success = true;
    QDateTime cutoffDate = QDateTime::currentDateTime().addDays(-keepDays);
    
    for (const QFileInfo &entry : entries) {
        QString fileName = entry.fileName();
        // 保留关键日志文件
        if (fileName == "wtmp" || fileName == "btmp" || fileName == "lastlog") {
            continue;
        }
        
        // 根据修改时间判断是否删除
        if (entry.lastModified() < cutoffDate) {
            // 删除 .gz 和 .1 等旧日志
            if (fileName.endsWith(".gz") || fileName.endsWith(".1") || 
                fileName.endsWith(".old") || fileName.contains(".log.")) {
                if (!QFile::remove(entry.absoluteFilePath())) {
                    success = false;
                }
            }
        }
    }
    
    // 清理 journal 日志
    QProcess journalProcess;
    QString vacuumCmd = QString("journalctl --vacuum-time=%1d").arg(keepDays);
    journalProcess.start("bash", QStringList() << "-c" << vacuumCmd);
    journalProcess.waitForFinished(30000);
    
    return success;
}

// 清理 journal 日志
bool cleanJournalLogs(int keepDays = 7)
{
    QProcess process;
    QString cmd = QString("journalctl --vacuum-time=%1d").arg(keepDays);
    process.start("bash", QStringList() << "-c" << cmd);
    process.waitForFinished(60000);
    
    return process.exitCode() == 0;
}

// 清理系统快照（保留指定数量）
bool cleanSnapshots(int keepCount = 3)
{
    QProcess process;
    process.start("deepin-immutable-ctl", QStringList() << "snapshot" << "list");
    process.waitForFinished(10000);
    
    if (process.exitCode() != 0) {
        // 命令不存在或执行失败，可能不是磐石系统
        return true;
    }
    
    QString output = process.readAllStandardOutput();
    QStringList snapshots = output.split('\n', Qt::SkipEmptyParts);
    
    // 保留最新的 keepCount 个快照
    int toRemove = snapshots.size() - keepCount;
    
    for (int i = 0; i < toRemove; ++i) {
        QString snapshot = snapshots[i].trimmed();
        if (snapshot.isEmpty()) continue;
        
        QProcess delProcess;
        delProcess.start("deepin-immutable-ctl", QStringList() << "snapshot" << "delete" << snapshot);
        delProcess.waitForFinished(30000);
    }
    
    return true;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName("lz-disk-cleaner-helper");
    app.setApplicationVersion("1.1.2");
    
    QCommandLineParser parser;
    parser.setApplicationDescription("LZ Disk Cleaner Helper - Execute privileged cleanup operations");
    parser.addHelpOption();
    parser.addVersionOption();
    
    // 添加选项
    QCommandLineOption aptCacheOption(
        QStringList() << "a" << "apt-cache",
        "Clean APT package cache");
    QCommandLineOption systemLogsOption(
        QStringList() << "l" << "system-logs",
        "Clean system logs");
    QCommandLineOption journalOption(
        QStringList() << "j" << "journal",
        "Clean journal logs");
    QCommandLineOption snapshotOption(
        QStringList() << "s" << "snapshot",
        "Clean system snapshots");
    QCommandLineOption deleteOption(
        QStringList() << "d" << "delete",
        "Delete specific files/directories (paths as positional arguments)");
    QCommandLineOption clearDirOption(
        QStringList() << "c" << "clear-dir",
        "Clear directory contents without removing the directory itself");
    
    // 参数选项
    QCommandLineOption keepDaysOption(
        QStringList() << "keep-days",
        "Keep logs for N days (default: 7)",
        "days", "7");
    QCommandLineOption keepCountOption(
        QStringList() << "keep-count",
        "Keep N snapshots (default: 3)",
        "count", "3");
    
    parser.addOption(aptCacheOption);
    parser.addOption(systemLogsOption);
    parser.addOption(journalOption);
    parser.addOption(snapshotOption);
    parser.addOption(deleteOption);
    parser.addOption(clearDirOption);
    parser.addOption(keepDaysOption);
    parser.addOption(keepCountOption);
    
    parser.process(app);
    
    bool success = true;
    int keepDays = parser.value(keepDaysOption).toInt();
    int keepCount = parser.value(keepCountOption).toInt();
    
    // 处理 APT 缓存清理
    if (parser.isSet(aptCacheOption)) {
        std::cout << "Cleaning APT cache..." << std::endl;
        if (cleanAptCache()) {
            std::cout << "APT cache cleaned successfully" << std::endl;
        } else {
            std::cerr << "Failed to clean APT cache" << std::endl;
            success = false;
        }
    }
    
    // 处理系统日志清理
    if (parser.isSet(systemLogsOption)) {
        std::cout << "Cleaning system logs (keep " << keepDays << " days)..." << std::endl;
        if (cleanSystemLogs(keepDays)) {
            std::cout << "System logs cleaned successfully" << std::endl;
        } else {
            std::cerr << "Failed to clean system logs" << std::endl;
            success = false;
        }
    }
    
    // 处理 journal 日志清理
    if (parser.isSet(journalOption)) {
        std::cout << "Cleaning journal logs (keep " << keepDays << " days)..." << std::endl;
        if (cleanJournalLogs(keepDays)) {
            std::cout << "Journal logs cleaned successfully" << std::endl;
        } else {
            std::cerr << "Failed to clean journal logs" << std::endl;
            success = false;
        }
    }
    
    // 处理系统快照清理
    if (parser.isSet(snapshotOption)) {
        std::cout << "Cleaning system snapshots (keep " << keepCount << ")..." << std::endl;
        if (cleanSnapshots(keepCount)) {
            std::cout << "System snapshots cleaned successfully" << std::endl;
        } else {
            std::cerr << "Failed to clean system snapshots" << std::endl;
            success = false;
        }
    }
    
    // 处理文件删除
    if (parser.isSet(deleteOption)) {
        QStringList paths = parser.positionalArguments();
        for (const QString &path : paths) {
            QFileInfo info(path);
            std::cout << "Deleting: " << path.toStdString() << std::endl;
            
            if (info.isDir()) {
                if (removeDirectory(path)) {
                    std::cout << "Directory removed: " << path.toStdString() << std::endl;
                } else {
                    std::cerr << "Failed to remove directory: " << path.toStdString() << std::endl;
                    success = false;
                }
            } else if (info.isFile()) {
                if (QFile::remove(path)) {
                    std::cout << "File removed: " << path.toStdString() << std::endl;
                } else {
                    std::cerr << "Failed to remove file: " << path.toStdString() << std::endl;
                    success = false;
                }
            }
        }
    }
    
    // 处理清空目录内容
    if (parser.isSet(clearDirOption)) {
        QStringList paths = parser.positionalArguments();
        for (const QString &path : paths) {
            std::cout << "Clearing directory: " << path.toStdString() << std::endl;
            if (clearDirectoryContents(path)) {
                std::cout << "Directory cleared: " << path.toStdString() << std::endl;
            } else {
                std::cerr << "Failed to clear directory: " << path.toStdString() << std::endl;
                success = false;
            }
        }
    }
    
    return success ? 0 : 1;
}
