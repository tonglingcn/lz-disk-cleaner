/*
 * Config - Implementation
 * 配置管理 - 实现
 */

#include "config.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QStringConverter>
#include <QDateTime>

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

int Config::getJournalMaxSizeMB() const
{
    return m_settings->value("Cleanup/journalMaxSizeMB", 100).toInt();
}

void Config::setJournalMaxSizeMB(int sizeMB)
{
    m_settings->setValue("Cleanup/journalMaxSizeMB", sizeMB);
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

int Config::getRefreshInterval() const
{
    return m_settings->value("Interface/refreshInterval", 30).toInt();
}

void Config::setRefreshInterval(int seconds)
{
    m_settings->setValue("Interface/refreshInterval", seconds);
}

// 白名单设置 - 保留空目录
QStringList Config::getKeepEmptyDirWhitelist() const
{
    return m_settings->value("Whitelist/keepEmptyDirs").toStringList();
}

void Config::setKeepEmptyDirWhitelist(const QStringList &list)
{
    m_settings->setValue("Whitelist/keepEmptyDirs", list);
}

void Config::addKeepEmptyDirPath(const QString &path)
{
    QStringList list = getKeepEmptyDirWhitelist();
    if (!list.contains(path)) {
        list.append(path);
        setKeepEmptyDirWhitelist(list);
    }
}

void Config::removeKeepEmptyDirPath(const QString &path)
{
    QStringList list = getKeepEmptyDirWhitelist();
    list.removeAll(path);
    setKeepEmptyDirWhitelist(list);
}

// 白名单设置 - 完全保护
QStringList Config::getFullProtectWhitelist() const
{
    return m_settings->value("Whitelist/fullProtect").toStringList();
}

void Config::setFullProtectWhitelist(const QStringList &list)
{
    m_settings->setValue("Whitelist/fullProtect", list);
}

void Config::addFullProtectPath(const QString &path)
{
    QStringList list = getFullProtectWhitelist();
    if (!list.contains(path)) {
        list.append(path);
        setFullProtectWhitelist(list);
    }
}

void Config::removeFullProtectPath(const QString &path)
{
    QStringList list = getFullProtectWhitelist();
    list.removeAll(path);
    setFullProtectWhitelist(list);
}

// 白名单设置 - 保护文件模式
QStringList Config::getFilePatternWhitelist() const
{
    return m_settings->value("Whitelist/filePatterns").toStringList();
}

void Config::setFilePatternWhitelist(const QStringList &list)
{
    m_settings->setValue("Whitelist/filePatterns", list);
}

void Config::addFilePattern(const QString &pattern)
{
    QStringList list = getFilePatternWhitelist();
    if (!list.contains(pattern)) {
        list.append(pattern);
        setFilePatternWhitelist(list);
    }
}

void Config::removeFilePattern(const QString &pattern)
{
    QStringList list = getFilePatternWhitelist();
    list.removeAll(pattern);
    setFilePatternWhitelist(list);
}

// 系统级白名单（内置保护，不可修改）
QStringList Config::getSystemKeepEmptyDirs() const
{
    // 这些目录在清理时会保留目录结构，只清理内容
    return QStringList()
        << "/var/log/supervisor"
        << "/var/log/nginx"
        << "/var/log/apache2"
        << "/var/log/mysql"
        << "/var/log/postgresql"
        << "/var/log/redis"
        << "/var/log/mongodb"
        << "/var/log/docker"
        << "/var/log/journal"
        << "/var/log/apt"
        << "/var/log/cups"
        << "/var/log/samba"
        << "/var/log/lightdm"
        << "/var/log/gdm"
        << "/var/log/Xorg";
}

QStringList Config::getSystemFullProtectDirs() const
{
    // 这些目录完全不参与扫描和清理
    return QStringList()
        << "/boot"
        << "/etc"
        << "/proc"
        << "/sys"
        << "/dev"
        << "/run"
        << "/bin"
        << "/sbin"
        << "/lib"
        << "/lib64"
        << "/usr/bin"
        << "/usr/sbin"
        << "/usr/lib"
        << "/usr/lib64"
        << "/usr/share"
        << "/root/.ssh"
        << "/root/.gnupg"
        << QDir::homePath() + "/.ssh"
        << QDir::homePath() + "/.gnupg"
        << QDir::homePath() + "/.config";
}

QStringList Config::getSystemFilePatterns() const
{
    // 这些文件模式不会被删除
    return QStringList()
        << "*.lock"
        << "*.pid"
        << "*.socket"
        << "*.sock"
        << "*.fifo"
        << ".XAuthority"
        << ".Xauthority"
        << ".bash_history"
        << ".zsh_history"
        << ".mysql_history"
        << ".viminfo"
        << "config.ini"
        << "settings.conf"
        << ".gitignore"
        << ".gitmodules";
}

// 导出配置
bool Config::exportConfig(const QString &filePath)
{
    // 导出用户配置到文件
    QStringList config;
    config << "# Deepin Disk Cleaner Configuration";
    config << QString("# Exported: %1").arg(QDateTime::currentDateTime().toString(Qt::ISODate));
    config << "";
    config << "[General]";
    config << QString("language=%1").arg(getLanguage());
    config << QString("autoUpdate=%1").arg(getAutoUpdate() ? "true" : "false");
    config << "";
    config << "[Cleanup]";
    config << QString("journalKeepDays=%1").arg(getJournalKeepDays());
    config << QString("journalMaxSizeMB=%1").arg(getJournalMaxSizeMB());
    config << QString("snapshotKeepCount=%1").arg(getSnapshotKeepCount());
    config << QString("confirmBeforeCleanup=%1").arg(getConfirmBeforeCleanup() ? "true" : "false");
    config << "";
    config << "[Interface]";
    config << QString("refreshInterval=%1").arg(getRefreshInterval());
    config << "";
    config << "[Whitelist.KeepEmptyDirs]";
    for (const QString &path : getKeepEmptyDirWhitelist()) {
        config << path;
    }
    config << "";
    config << "[Whitelist.FullProtect]";
    for (const QString &path : getFullProtectWhitelist()) {
        config << path;
    }
    config << "";
    config << "[Whitelist.FilePatterns]";
    for (const QString &pattern : getFilePatternWhitelist()) {
        config << pattern;
    }
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << config.join('\n') << '\n';
    file.close();
    return true;
}

// 导入配置
bool Config::importConfig(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    
    QTextStream in(&file);
    in.setEncoding(QStringConverter::Utf8);
    QString content = in.readAll();
    file.close();
    
    QStringList lines = content.split('\n');
    QString currentSection;
    QStringList keepEmptyDirs, fullProtect, filePatterns;
    
    for (QString line : lines) {
        line = line.trimmed();
        if (line.isEmpty() || line.startsWith('#')) continue;
        
        if (line.startsWith('[') && line.endsWith(']')) {
            currentSection = line.mid(1, line.length() - 2);
            continue;
        }
        
        if (currentSection == "General") {
            QStringList parts = line.split('=');
            if (parts.size() == 2) {
                if (parts[0] == "language") setLanguage(parts[1]);
                else if (parts[0] == "autoUpdate") setAutoUpdate(parts[1] == "true");
            }
        } else if (currentSection == "Cleanup") {
            QStringList parts = line.split('=');
            if (parts.size() == 2) {
                if (parts[0] == "journalKeepDays") setJournalKeepDays(parts[1].toInt());
                else if (parts[0] == "journalMaxSizeMB") setJournalMaxSizeMB(parts[1].toInt());
                else if (parts[0] == "snapshotKeepCount") setSnapshotKeepCount(parts[1].toInt());
                else if (parts[0] == "confirmBeforeCleanup") setConfirmBeforeCleanup(parts[1] == "true");
            }
        } else if (currentSection == "Interface") {
            QStringList parts = line.split('=');
            if (parts.size() == 2) {
                if (parts[0] == "refreshInterval") setRefreshInterval(parts[1].toInt());
            }
        } else if (currentSection == "Whitelist.KeepEmptyDirs") {
            if (!line.contains('=')) keepEmptyDirs.append(line);
        } else if (currentSection == "Whitelist.FullProtect") {
            if (!line.contains('=')) fullProtect.append(line);
        } else if (currentSection == "Whitelist.FilePatterns") {
            if (!line.contains('=')) filePatterns.append(line);
        }
    }
    
    setKeepEmptyDirWhitelist(keepEmptyDirs);
    setFullProtectWhitelist(fullProtect);
    setFilePatternWhitelist(filePatterns);
    
    save();
    return true;
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