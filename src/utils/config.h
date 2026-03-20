/*
 * Config - Header
 * 配置管理 - 头文件
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <QObject>
#include <QSettings>
#include <QString>
#include <QVariant>

class Config : public QObject
{
    Q_OBJECT

public:
    static Config* instance();
    
    void init();
    
    // 通用设置
    QString getLanguage() const;
    void setLanguage(const QString &language);
    
    bool getAutoUpdate() const;
    void setAutoUpdate(bool enabled);
    
    // 清理设置
    bool getAutoCleanCache() const;
    void setAutoCleanCache(bool enabled);
    
    int getJournalKeepDays() const;
    void setJournalKeepDays(int days);
    
    int getJournalMaxSizeMB() const;
    void setJournalMaxSizeMB(int sizeMB);
    
    int getSnapshotKeepCount() const;
    void setSnapshotKeepCount(int count);
    
    bool getConfirmBeforeCleanup() const;
    void setConfirmBeforeCleanup(bool enabled);
    
    int getRefreshInterval() const;
    void setRefreshInterval(int seconds);
    
    // 白名单设置 - 保留空目录（清理内容但保留目录结构）
    QStringList getKeepEmptyDirWhitelist() const;
    void setKeepEmptyDirWhitelist(const QStringList &list);
    void addKeepEmptyDirPath(const QString &path);
    void removeKeepEmptyDirPath(const QString &path);
    
    // 白名单设置 - 完全保护（不扫描不清理）
    QStringList getFullProtectWhitelist() const;
    void setFullProtectWhitelist(const QStringList &list);
    void addFullProtectPath(const QString &path);
    void removeFullProtectPath(const QString &path);
    
    // 白名单设置 - 保护文件模式（按模式匹配）
    QStringList getFilePatternWhitelist() const;
    void setFilePatternWhitelist(const QStringList &list);
    void addFilePattern(const QString &pattern);
    void removeFilePattern(const QString &pattern);
    
    // 系统级白名单（只读，内置保护）
    QStringList getSystemKeepEmptyDirs() const;
    QStringList getSystemFullProtectDirs() const;
    QStringList getSystemFilePatterns() const;
    
    // 导入导出配置
    bool exportConfig(const QString &filePath);
    bool importConfig(const QString &filePath);
    
    // 保存和加载
    void save();
    void load();
    void reset();
    
    // 获取 QSettings 对象（供其他模块使用）
    QSettings* getSettings() { return m_settings; }
    
private:
    explicit Config(QObject *parent = nullptr);
    ~Config();
    
    static Config *s_instance;
    QSettings *m_settings;
};

#endif // CONFIG_H