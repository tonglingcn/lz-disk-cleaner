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

// 检查路径是否在白名单中（精确匹配或前缀匹配）
bool isPathInWhitelist(const QString &path, const QStringList &whitelist)
{
    for (const QString &wPath : whitelist) {
        if (path == wPath || path.startsWith(wPath + "/")) {
            return true;
        }
    }
    return false;
}

// 获取系统内置的日志目录保护白名单（兜底保护，不依赖GUI传递）
QStringList getBuiltinLogWhitelist()
{
    return QStringList()
        << "/var/log/supervisor"
        << "/var/log/nginx"
        << "/var/log/apache2"
        << "/var/log/mysql"
        << "/var/log/postgresql"
        << "/var/log/redis"
        << "/var/log/mongodb"
        << "/var/log/docker"
        << "/var/log/apt"
        << "/var/log/cups"
        << "/var/log/samba"
        << "/var/log/lightdm"
        << "/var/log/gdm"
        << "/var/log/Xorg"
        << "/var/log/tomcat"
        << "/var/log/elasticsearch"
        << "/var/log/kafka"
        << "/var/log/zookeeper";
}

// 清理系统日志（保留最近的，白名单目录只清内容不删目录）
// keepEmptyDirs: 需要保留空目录的路径列表（系统级+用户级白名单合并）
bool cleanSystemLogs(int keepDays, const QStringList &keepEmptyDirs)
{
    QDir logDir("/var/log");
    if (!logDir.exists()) {
        return true;
    }

    // 合并GUI传递的白名单和内置兜底白名单，确保关键目录始终受保护
    QStringList mergedWhitelist = keepEmptyDirs;
    QStringList builtinList = getBuiltinLogWhitelist();
    for (const QString &builtinPath : builtinList) {
        if (!mergedWhitelist.contains(builtinPath)) {
            mergedWhitelist.append(builtinPath);
        }
    }
    std::cout << "[LOG_CLEAN] Merged whitelist size: " << mergedWhitelist.size()
              << " (GUI: " << keepEmptyDirs.size() << ", builtin: " << builtinList.size() << ")" << std::endl;

    QDateTime cutoffDate = QDateTime::currentDateTime().addDays(-keepDays);
    bool success = true;

    // === 第一阶段：处理子目录（应用白名单规则）===
    QFileInfoList dirEntries = logDir.entryInfoList(
        QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    for (const QFileInfo &entry : dirEntries) {
        QString absPath = entry.absoluteFilePath();

        // 跳过 journal 目录（它由 journalctl --vacuum-time 单独处理，不要直接操作）
        if (entry.fileName() == "journal") {
            std::cout << "Skipping journal directory (handled by journalctl)" << std::endl;
            continue;
        }

        if (isPathInWhitelist(absPath, mergedWhitelist)) {
            // 白名单目录：只清空内容，保留目录本身
            std::cout << "[KeepEmpty] Clearing content of: " << absPath.toStdString() << std::endl;
            if (!clearDirectoryContents(absPath)) {
                std::cerr << "Failed to clear directory content: " << absPath.toStdString() << std::endl;
                success = false;
            } else {
                std::cout << "[OK] Directory content cleared (dir kept): " << absPath.toStdString() << std::endl;
            }
        } else {
            // 非白名单目录：递归删除整个目录
            std::cout << "[Remove] Removing: " << absPath.toStdString() << std::endl;
            if (!removeDirectory(absPath)) {
                std::cerr << "Failed to remove directory: " << absPath.toStdString() << std::endl;
                success = false;
            } else {
                std::cout << "[OK] Directory removed: " << absPath.toStdString() << std::endl;
            }
        }
    }

    // === 第二阶段：处理文件（原有逻辑保持不变）===
    QFileInfoList fileEntries = logDir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo &entry : fileEntries) {
        QString fileName = entry.fileName();
        // 保留关键日志文件
        if (fileName == "wtmp" || fileName == "btmp" || fileName == "lastlog" ||
            fileName == "syslog" || fileName == "kern.log" || fileName == "auth.log" ||
            fileName == "dpkg.log" || fileName == "alternatives.log" ||
            fileName == "bootstrap.log") {
            continue;
        }

        // 根据修改时间判断是否删除旧轮转日志
        if (entry.lastModified() < cutoffDate) {
            if (fileName.endsWith(".gz") || fileName.endsWith(".1") ||
                fileName.endsWith(".old") || fileName.contains(".log.") ||
                fileName.contains(".log-")) {
                if (QFile::remove(entry.absoluteFilePath())) {
                    std::cout << "Removed old log file: " << entry.fileName().toStdString() << std::endl;
                } else {
                    std::cerr << "Failed to remove: " << entry.fileName().toStdString() << std::endl;
                    success = false;
                }
            }
        }
    }

    // === 第三阶段：清理 journald 日志（通过 journalctl）===
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

// 卸载旧版本部署（undeploy）
bool undeployDeployment(int deployIndex)
{
    std::cout << "Undeploying deployment #" << deployIndex << "..." << std::endl;
    
    QProcess process;
    process.start("deepin-immutable-ctl", QStringList() << "admin" << "undeploy" 
                 << QString::number(deployIndex));
    
    if (!process.waitForFinished(60000)) {
        std::cerr << "Timeout waiting for undeploy command" << std::endl;
        return false;
    }
    
    QString stdoutOutput = QString::fromUtf8(process.readAllStandardOutput());
    QString stderrOutput = QString::fromUtf8(process.readAllStandardError());
    
    if (!stdoutOutput.isEmpty()) {
        std::cout << stdoutOutput.toStdString() << std::endl;
    }
    if (!stderrOutput.isEmpty()) {
        std::cerr << stderrOutput.toStdString() << std::endl;
    }
    
    if (process.exitCode() != 0) {
        std::cerr << "Failed to undeploy deployment #" << deployIndex 
                  << ", exit code: " << process.exitCode() << std::endl;
        return false;
    }
    
    std::cout << "Successfully undeployed deployment #" << deployIndex << std::endl;
    return true;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName("lz-disk-cleaner-helper");
    app.setApplicationVersion("1.3.0");
    
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
    QCommandLineOption undeployOption(
        QStringList() << "u" << "undeploy",
        "Undeploy old deployment by index (can be specified multiple times)",
        "index");
    QCommandLineOption deleteOption(
        QStringList() << "d" << "delete",
        "Delete specific files/directories (paths as positional arguments)");
    QCommandLineOption clearDirOption(
        QStringList() << "c" << "clear-dir",
        "Clear directory contents without removing the directory itself");
    QCommandLineOption keepEmptyDirsOption(
        QStringList() << "k" << "keep-empty-dirs",
        "Directories to keep empty (only clear content, comma-separated)");
    
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
    parser.addOption(undeployOption);
    parser.addOption(deleteOption);
    parser.addOption(clearDirOption);
    parser.addOption(keepEmptyDirsOption);
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
        // 解析保留空目录白名单
        QStringList keepEmptyDirs;
        if (parser.isSet(keepEmptyDirsOption)) {
            keepEmptyDirs = parser.value(keepEmptyDirsOption).split(",", Qt::SkipEmptyParts);
        }
        // 输出GUI传递的白名单详情（调试用）
        std::cout << "[LOG_CLEAN] GUI-provided whitelist (" << keepEmptyDirs.size() << " items):";
        for (const QString &p : keepEmptyDirs) {
            std::cout << " " << p.toStdString();
        }
        std::cout << std::endl;

        std::cout << "Cleaning system logs (keep " << keepDays << " days, "
                  << keepEmptyDirs.size() << " dirs from GUI)..." << std::endl;
        if (cleanSystemLogs(keepDays, keepEmptyDirs)) {
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
    
    // 处理部署卸载（undeploy）
    if (parser.isSet(undeployOption)) {
        QStringList deployIndices = parser.values(undeployOption);
        for (const QString &indexStr : deployIndices) {
            bool ok = false;
            int idx = indexStr.toInt(&ok);
            if (ok && idx >= 0) {
                std::cout << "Undeploying deployment #" << idx << "..." << std::endl;
                if (!undeployDeployment(idx)) {
                    success = false;
                }
            } else {
                std::cerr << "Invalid deployment index: " << indexStr.toStdString() << std::endl;
                success = false;
            }
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
