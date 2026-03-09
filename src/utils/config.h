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
    
    bool getShowNotifications() const;
    void setShowNotifications(bool enabled);
    
    // 清理设置
    bool getAutoCleanCache() const;
    void setAutoCleanCache(bool enabled);
    
    int getJournalKeepDays() const;
    void setJournalKeepDays(int days);
    
    int getSnapshotKeepCount() const;
    void setSnapshotKeepCount(int count);
    
    bool getConfirmBeforeCleanup() const;
    void setConfirmBeforeCleanup(bool enabled);
    
    // 界面设置
    bool getDarkMode() const;
    void setDarkMode(bool enabled);
    
    int getRefreshInterval() const;
    void setRefreshInterval(int seconds);
    
    // 保存和加载
    void save();
    void load();
    void reset();
    
private:
    explicit Config(QObject *parent = nullptr);
    ~Config();
    
    static Config *s_instance;
    QSettings *m_settings;
};

#endif // CONFIG_H