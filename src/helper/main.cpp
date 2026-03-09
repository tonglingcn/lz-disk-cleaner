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
bool cleanSystemLogs()
{
    QDir logDir("/var/log");
    if (!logDir.exists()) {
        return true;
    }
    
    QFileInfoList entries = logDir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    bool success = true;
    
    for (const QFileInfo &entry : entries) {
        QString fileName = entry.fileName();
        // 保留关键日志文件
        if (fileName == "wtmp" || fileName == "btmp" || fileName == "lastlog") {
            continue;
        }
        
        // 删除 .gz 和 .1 等旧日志
        if (fileName.endsWith(".gz") || fileName.endsWith(".1") || 
            fileName.endsWith(".old") || fileName.contains(".log.")) {
            if (!QFile::remove(entry.absoluteFilePath())) {
                success = false;
            }
        }
    }
    
    return success;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName("lz-disk-cleaner-helper");
    app.setApplicationVersion("1.0.0");
    
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
    QCommandLineOption deleteOption(
        QStringList() << "d" << "delete",
        "Delete specific files/directories (paths as positional arguments)");
    QCommandLineOption clearDirOption(
        QStringList() << "c" << "clear-dir",
        "Clear directory contents without removing the directory itself");
    
    parser.addOption(aptCacheOption);
    parser.addOption(systemLogsOption);
    parser.addOption(deleteOption);
    parser.addOption(clearDirOption);
    
    parser.process(app);
    
    bool success = true;
    
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
        std::cout << "Cleaning system logs..." << std::endl;
        if (cleanSystemLogs()) {
            std::cout << "System logs cleaned successfully" << std::endl;
        } else {
            std::cerr << "Failed to clean system logs" << std::endl;
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
