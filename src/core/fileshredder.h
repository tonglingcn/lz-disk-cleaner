/*
 * File Shredder - Header
 * 文件粉碎器 - 头文件
 * 
 * 支持顽固文件粉碎：
 * - 自动处理只读权限
 * - 移除 immutable 属性
 * - 强制删除被占用文件
 * - 支持 root 权限提升
 */

#ifndef FILESHREDDER_H
#define FILESHREDDER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QFileInfo>

// 粉碎结果
struct ShredResult {
    QString filePath;
    bool success;
    QString errorMessage;
    qint64 size;
    bool usedPrivilege;  // 是否使用了权限提升
};

// 顽固文件类型
enum class StubbornFileType {
    Normal,             // 普通文件
    ReadOnly,           // 只读文件
    Immutable,          // immutable 属性文件
    LockedByProcess,    // 被进程占用
    PermissionDenied,   // 权限不足
    Unknown             // 未知问题
};

class FileShredder : public QObject
{
    Q_OBJECT

public:
    explicit FileShredder(QObject *parent = nullptr);
    ~FileShredder();

    // 设置覆写次数（默认3次）
    void setPasses(int passes);
    int passes() const { return m_passes; }

    // 设置是否使用权限提升（pkexec）
    void setUsePrivilege(bool use) { m_usePrivilege = use; }
    bool usePrivilege() const { return m_usePrivilege; }

    // 检查路径是否受保护（系统关键目录）
    bool isProtectedPath(const QString &path) const;

    // 检查是否可以粉碎该文件
    bool canShred(const QString &path, QString &reason) const;

    // 检测顽固文件类型
    StubbornFileType detectStubbornType(const QString &path) const;

    // 粉碎单个文件
    bool shredFile(const QString &filePath, QString &errorMessage);

    // 粉碎文件夹
    bool shredFolder(const QString &folderPath, QString &errorMessage);

    // 批量粉碎
    QList<ShredResult> shredFiles(const QStringList &filePaths);

    // 强制粉碎（处理顽固文件）
    bool forceShred(const QString &path, QString &errorMessage);

signals:
    // 粉碎进度信号
    void progress(const QString &currentFile, int current, int total, int percent);
    // 单个文件粉碎完成
    void fileCompleted(const QString &filePath, bool success, const QString &message);

private:
    // 生成随机文件名
    QString generateRandomName(const QString &originalPath);

    // 安全覆写文件内容
    bool overwriteFile(const QString &filePath, QString &errorMessage);

    // 递归删除目录内容
    bool clearDirectory(const QString &dirPath, QString &errorMessage);

    // 移除只读属性
    bool removeReadOnlyAttribute(const QString &path, QString &errorMessage);

    // 移除 immutable 属性
    bool removeImmutableAttribute(const QString &path, QString &errorMessage);

    // 检查文件是否被进程占用
    bool isFileLocked(const QString &path) const;

    // 获取占用文件的进程列表
    QStringList getLockingProcesses(const QString &path) const;

    // 使用 root 权限删除
    bool deleteWithPrivilege(const QString &path, QString &errorMessage);

    int m_passes;
    bool m_usePrivilege;
    QStringList m_protectedPaths;
};

#endif // FILESHREDDER_H
