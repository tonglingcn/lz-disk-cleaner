/*
 * Config - Implementation
 * 配置管理 - 实现
 */

#include "config.h"
#include <QStandardPaths>
#include <QDir>

Config* Config::s_instance = nullptr;

Config::Config(QObject *parent)
    : QObject(parent)
    , m_settings(nullptr)
{
}

Config::~Config()
{
    if (m_settings) {
        delete m_settings;
    }
}

Config* Config::instance()
{
    if (!s_instance) {
        s_instance = new Config();
        s_instance->init();
    }
    return s_instance;
}

void Config::init()
{
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(configDir);
    
    QString configPath = configDir + "/deepin-disk-cleaner.conf";
    m_settings = new QSettings(configPath, QSettings::IniFormat);
    
    load();
}

QString Config::getLanguage() const
{
    return m_settings->value("General/language", "zh_CN").toString();
}

void Config::setLanguage(const QString &language)
{
    m_settings->setValue("General/language", language);
}

bool Config::getAutoUpdate() const
{
    return m_settings->value("General/autoUpdate", false).toBool();
}

void Config::setAutoUpdate(bool enabled)
{
    m_settings->setValue("General/autoUpdate", enabled);
}

bool Config::getShowNotifications() const
{
    return m_settings->value("General/showNotifications", true).toBool();
}

void Config::setShowNotifications(bool enabled)
{
    m_settings->setValue("General/showNotifications", enabled);
}

bool Config::getAutoCleanCache() const
{
    return m_settings->value("Cleanup/autoCleanCache", false).toBool();
}

void Config::setAutoCleanCache(bool enabled)
{
    m_settings->setValue("Cleanup/autoCleanCache", enabled);
}

int Config::getJournalKeepDays() const
{
    return m_settings->value("Cleanup/journalKeepDays", 7).toInt();
}

void Config::setJournalKeepDays(int days)
{
    m_settings->setValue("Cleanup/journalKeepDays", days);
}

int Config::getSnapshotKeepCount() const
{
    return m_settings->value("Cleanup/snapshotKeepCount", 3).toInt();
}

void Config::setSnapshotKeepCount(int count)
{
    m_settings->setValue("Cleanup/snapshotKeepCount", count);
}

bool Config::getConfirmBeforeCleanup() const
{
    return m_settings->value("Cleanup/confirmBeforeCleanup", true).toBool();
}

void Config::setConfirmBeforeCleanup(bool enabled)
{
    m_settings->setValue("Cleanup/confirmBeforeCleanup", enabled);
}

bool Config::getDarkMode() const
{
    return m_settings->value("Interface/darkMode", false).toBool();
}

void Config::setDarkMode(bool enabled)
{
    m_settings->setValue("Interface/darkMode", enabled);
}

int Config::getRefreshInterval() const
{
    return m_settings->value("Interface/refreshInterval", 30).toInt();
}

void Config::setRefreshInterval(int seconds)
{
    m_settings->setValue("Interface/refreshInterval", seconds);
}

void Config::save()
{
    m_settings->sync();
}

void Config::load()
{
    m_settings->sync();
}

void Config::reset()
{
    m_settings->clear();
    save();
}