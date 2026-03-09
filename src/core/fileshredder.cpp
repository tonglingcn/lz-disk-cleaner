/*
 * File Shredder - Implementation
 * 文件粉碎器 - 实现
 * 
 * 支持顽固文件粉碎：
 * - 自动处理只读权限
 * - 移除 immutable 属性
 * - 强制删除被占用文件
 * - 支持 root 权限提升
 */

#include "fileshredder.h"
#include <QFile>
#include <QDir>
#include <QRandomGenerator>
#include <QDateTime>
#include <QDebug>
#include <QProcess>
#include <QRegularExpression>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

// 前向声明：计算目录大小
static qint64 dirSize(const QString &path);

FileShredder::FileShredder(QObject *parent)
    : QObject(parent)
    , m_passes(3)
    , m_usePrivilege(true)
{
    // 设置受保护的系统路径
    m_protectedPaths << "/bin" << "/sbin" << "/usr" << "/lib" << "/lib64"
                     << "/etc" << "/boot" << "/dev" << "/proc" << "/sys"
                     << "/root" << "/var" << "/opt";
}

FileShredder::~FileShredder()
{
}

void FileShredder::setPasses(int passes)
{
    m_passes = qBound(1, passes, 35); // 1-35次覆写
}

bool FileShredder::isProtectedPath(const QString &path) const
{
    QString canonicalPath = QFileInfo(path).canonicalFilePath();

    // 如果文件不存在，检查绝对路径
    if (canonicalPath.isEmpty()) {
        canonicalPath = QFileInfo(path).absoluteFilePath();
    }

    for (const QString &protectedPath : m_protectedPaths) {
        // 检查是否是受保护路径或其子目录
        if (canonicalPath == protectedPath ||
            canonicalPath.startsWith(protectedPath + "/")) {
            return true;
        }
    }

    return false;
}

bool FileShredder::canShred(const QString &path, QString &reason) const
{
    QFileInfo info(path);

    // 检查文件是否存在
    if (!info.exists()) {
        reason = tr("文件或目录不存在");
        return false;
    }

    // 检查是否是受保护路径
    if (isProtectedPath(path)) {
        reason = tr("系统关键目录，禁止操作");
        return false;
    }

    // 注意：不再简单检查写权限，因为可以通过权限提升解决
    return true;
}

StubbornFileType FileShredder::detectStubbornType(const QString &path) const
{
    QFileInfo info(path);
    
    if (!info.exists()) {
        return StubbornFileType::Unknown;
    }
    
    // 检查是否是只读文件
    if (!info.isWritable()) {
        // 检查是否是权限问题
        if (info.ownerId() != getuid()) {
            return StubbornFileType::PermissionDenied;
        }
        return StubbornFileType::ReadOnly;
    }
    
    // 检查 immutable 属性
    QProcess lsattr;
    lsattr.start("lsattr", QStringList() << "-d" << path);
    if (lsattr.waitForFinished(2000)) {
        QString output = QString::fromUtf8(lsattr.readAllStandardOutput());
        if (output.contains('i') && !output.startsWith("lsattr:")) {
            return StubbornFileType::Immutable;
        }
    }
    
    // 检查是否被进程占用
    if (isFileLocked(path)) {
        return StubbornFileType::LockedByProcess;
    }
    
    return StubbornFileType::Normal;
}

bool FileShredder::isFileLocked(const QString &path) const
{
    QProcess lsof;
    lsof.start("lsof", QStringList() << "-t" << path);
    if (lsof.waitForFinished(2000)) {
        QString output = QString::fromUtf8(lsof.readAllStandardOutput()).trimmed();
        return !output.isEmpty();
    }
    return false;
}

QStringList FileShredder::getLockingProcesses(const QString &path) const
{
    QStringList processes;
    QProcess lsof;
    lsof.start("lsof", QStringList() << path);
    if (lsof.waitForFinished(3000)) {
        QString output = QString::fromUtf8(lsof.readAllStandardOutput());
        QStringList lines = output.split('\n');
        for (int i = 1; i < lines.size(); ++i) {  // 跳过标题行
            QStringList parts = lines[i].split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
            if (parts.size() >= 1) {
                processes.append(parts[0]);
            }
        }
    }
    return processes;
}

bool FileShredder::removeReadOnlyAttribute(const QString &path, QString &errorMessage)
{
    // 使用 chmod 添加写权限
    QProcess chmod;
    chmod.start("chmod", QStringList() << "u+w" << path);
    if (!chmod.waitForFinished(3000) || chmod.exitCode() != 0) {
        // 尝试使用 root 权限
        if (m_usePrivilege) {
            chmod.start("pkexec", QStringList() << "chmod" << "u+w" << path);
            if (!chmod.waitForFinished(10000) || chmod.exitCode() != 0) {
                errorMessage = tr("无法修改文件权限");
                return false;
            }
        } else {
            errorMessage = tr("无法修改文件权限: %1").arg(QString::fromUtf8(chmod.readAllStandardError()));
            return false;
        }
    }
    return true;
}

bool FileShredder::removeImmutableAttribute(const QString &path, QString &errorMessage)
{
    // 使用 chattr 移除 immutable 属性
    QProcess chattr;
    
    if (m_usePrivilege) {
        chattr.start("pkexec", QStringList() << "chattr" << "-i" << path);
        if (!chattr.waitForFinished(10000)) {
            errorMessage = tr("移除 immutable 属性超时");
            return false;
        }
    } else {
        chattr.start("chattr", QStringList() << "-i" << path);
        if (!chattr.waitForFinished(5000)) {
            errorMessage = tr("移除 immutable 属性超时");
            return false;
        }
    }
    
    if (chattr.exitCode() != 0) {
        errorMessage = tr("无法移除 immutable 属性: %1")
            .arg(QString::fromUtf8(chattr.readAllStandardError()));
        return false;
    }
    
    return true;
}

bool FileShredder::deleteWithPrivilege(const QString &path, QString &errorMessage)
{
    QProcess rm;
    
    // 使用 pkexec rm -rf 删除
    rm.start("pkexec", QStringList() << "rm" << "-rf" << path);
    
    if (!rm.waitForFinished(30000)) {  // 30秒超时
        errorMessage = tr("删除操作超时");
        return false;
    }
    
    if (rm.exitCode() != 0) {
        errorMessage = tr("权限删除失败: %1")
            .arg(QString::fromUtf8(rm.readAllStandardError()));
        return false;
    }
    
    return true;
}

bool FileShredder::forceShred(const QString &path, QString &errorMessage)
{
    QFileInfo info(path);
    
    if (!info.exists()) {
        errorMessage = tr("文件不存在");
        return false;
    }
    
    // 检测顽固类型
    StubbornFileType type = detectStubbornType(path);
    
    switch (type) {
    case StubbornFileType::Immutable:
        // 先移除 immutable 属性
        if (!removeImmutableAttribute(path, errorMessage)) {
            return false;
        }
        // 继续正常粉碎
        break;
        
    case StubbornFileType::ReadOnly:
    case StubbornFileType::PermissionDenied:
        // 先移除只读属性
        if (!removeReadOnlyAttribute(path, errorMessage)) {
            return false;
        }
        // 继续正常粉碎
        break;
        
    case StubbornFileType::LockedByProcess:
        {
            // 获取占用进程并提示
            QStringList processes = getLockingProcesses(path);
            errorMessage = tr("文件被以下进程占用: %1\n请先关闭相关程序。")
                .arg(processes.join(", "));
            // 仍然尝试删除
            qWarning() << "File locked by processes:" << processes;
        }
        break;
        
    default:
        break;
    }
    
    // 尝试正常粉碎
    if (info.isDir()) {
        if (shredFolder(path, errorMessage)) {
            return true;
        }
    } else {
        if (shredFile(path, errorMessage)) {
            return true;
        }
    }
    
    // 如果正常粉碎失败，尝试使用权限提升强制删除
    if (m_usePrivilege) {
        qWarning() << "Normal shred failed, trying privileged delete for:" << path;
        return deleteWithPrivilege(path, errorMessage);
    }
    
    return false;
}

bool FileShredder::shredFile(const QString &filePath, QString &errorMessage)
{
    // 检查是否可以粉碎（不检查写权限，因为可以提升）
    QString reason;
    if (!canShred(filePath, reason)) {
        errorMessage = reason;
        return false;
    }

    QFile file(filePath);
    qint64 fileSize = file.size();

    // 如果是符号链接，直接删除
    QFileInfo info(filePath);
    if (info.isSymLink()) {
        if (!QFile::remove(filePath)) {
            // 尝试权限删除
            if (m_usePrivilege) {
                return deleteWithPrivilege(filePath, errorMessage);
            }
            errorMessage = tr("无法删除符号链接");
            return false;
        }
        return true;
    }

    // 尝试打开文件进行覆写
    if (!file.open(QIODevice::ReadWrite)) {
        // 可能是权限问题，尝试修复权限后再打开
        QString permError;
        if (removeReadOnlyAttribute(filePath, permError)) {
            // 再次尝试打开
            if (!file.open(QIODevice::ReadWrite)) {
                errorMessage = tr("无法打开文件: %1").arg(file.errorString());
                // 尝试直接权限删除
                if (m_usePrivilege) {
                    return deleteWithPrivilege(filePath, errorMessage);
                }
                return false;
            }
        } else {
            // 无法修复权限，尝试权限删除
            if (m_usePrivilege) {
                return deleteWithPrivilege(filePath, errorMessage);
            }
            errorMessage = tr("无法打开文件: %1").arg(file.errorString());
            return false;
        }
    }

    // 多次覆写
    for (int pass = 0; pass < m_passes; pass++) {
        // 创建覆写数据
        QByteArray data(fileSize, 0);

        if (pass == 0 || pass == 2) {
            // 随机数据
            for (qint64 i = 0; i < fileSize; i++) {
                data[i] = static_cast<char>(QRandomGenerator::global()->bounded(256));
            }
        } else if (pass == 1) {
            // 全0
            data.fill(0x00);
        } else {
            // 全1或随机交替
            if (pass % 2 == 0) {
                data.fill(0xFF);
            } else {
                for (qint64 i = 0; i < fileSize; i++) {
                    data[i] = static_cast<char>(QRandomGenerator::global()->bounded(256));
                }
            }
        }

        // 写入数据
        file.seek(0);
        if (file.write(data) != fileSize) {
            errorMessage = tr("写入文件失败");
            file.close();
            // 尝试权限删除
            if (m_usePrivilege) {
                return deleteWithPrivilege(filePath, errorMessage);
            }
            return false;
        }

        // 强制刷新到磁盘
        file.flush();
        fsync(file.handle());
    }

    file.close();

    // 重命名为随机名称
    QString randomName = generateRandomName(filePath);
    if (!file.rename(randomName)) {
        // 如果重命名失败，尝试直接删除
        qWarning() << "Failed to rename file, attempting direct delete";
    }

    // 删除文件
    if (!file.remove()) {
        // 尝试权限删除
        if (m_usePrivilege) {
            return deleteWithPrivilege(filePath, errorMessage);
        }
        errorMessage = tr("删除文件失败");
        return false;
    }

    return true;
}

bool FileShredder::shredFolder(const QString &folderPath, QString &errorMessage)
{
    // 检查是否可以粉碎
    QString reason;
    if (!canShred(folderPath, reason)) {
        errorMessage = reason;
        return false;
    }

    QDir dir(folderPath);

    // 先粉碎目录中的所有内容
    if (!clearDirectory(folderPath, errorMessage)) {
        // 尝试权限删除
        if (m_usePrivilege) {
            return deleteWithPrivilege(folderPath, errorMessage);
        }
        return false;
    }

    // 删除空目录
    if (!dir.rmdir(folderPath)) {
        // 尝试权限删除
        if (m_usePrivilege) {
            return deleteWithPrivilege(folderPath, errorMessage);
        }
        errorMessage = tr("删除目录失败");
        return false;
    }

    return true;
}

bool FileShredder::clearDirectory(const QString &dirPath, QString &errorMessage)
{
    QDir dir(dirPath);
    bool success = true;

    // 遍历目录内容
    QFileInfoList entries = dir.entryInfoList(
        QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);

    for (const QFileInfo &entry : entries) {
        QString path = entry.absoluteFilePath();

        if (entry.isDir() && !entry.isSymLink()) {
            // 递归处理子目录
            if (!clearDirectory(path, errorMessage)) {
                // 尝试权限删除
                if (m_usePrivilege) {
                    deleteWithPrivilege(path, errorMessage);
                } else {
                    success = false;
                }
            }
            // 删除空目录
            QDir().rmdir(path);
        } else {
            // 粉碎文件或符号链接
            QString err;
            if (!shredFile(path, err)) {
                errorMessage = err;
                success = false;
            }
        }
    }

    return success;
}

QList<ShredResult> FileShredder::shredFiles(const QStringList &filePaths)
{
    QList<ShredResult> results;
    int total = filePaths.size();

    for (int i = 0; i < total; i++) {
        const QString &path = filePaths[i];
        ShredResult result;
        result.filePath = path;
        result.size = 0;
        result.usedPrivilege = false;

        QFileInfo info(path);
        if (info.exists()) {
            if (info.isDir()) {
                result.size = dirSize(path);
            } else {
                result.size = info.size();
            }
        }

        QString errorMessage;

        // 先尝试正常粉碎
        if (info.isDir()) {
            result.success = shredFolder(path, errorMessage);
        } else {
            result.success = shredFile(path, errorMessage);
        }

        // 如果失败，尝试强制粉碎
        if (!result.success && m_usePrivilege) {
            qWarning() << "Normal shred failed, trying force shred for:" << path;
            result.success = forceShred(path, errorMessage);
            result.usedPrivilege = result.success;
        }

        result.errorMessage = errorMessage;
        results.append(result);

        // 发送进度信号
        int percent = static_cast<int>((i + 1) * 100.0 / total);
        emit progress(path, i + 1, total, percent);
        emit fileCompleted(path, result.success, errorMessage);
    }

    return results;
}

QString FileShredder::generateRandomName(const QString &originalPath)
{
    QString chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    QString randomName;

    for (int i = 0; i < 16; i++) {
        randomName += chars[QRandomGenerator::global()->bounded(chars.length())];
    }

    QFileInfo info(originalPath);
    return info.absolutePath() + "/" + randomName;
}

// 辅助函数：计算目录大小
static qint64 dirSize(const QString &path)
{
    QDir dir(path);
    qint64 size = 0;

    QFileInfoList entries = dir.entryInfoList(
        QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);

    for (const QFileInfo &entry : entries) {
        if (entry.isDir()) {
            size += dirSize(entry.absoluteFilePath());
        } else {
            size += entry.size();
        }
    }

    return size;
}
