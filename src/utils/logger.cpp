/*
 * Logger - Implementation
 * 日志工具 - 实现
 */

#include "logger.h"
#include <QStandardPaths>
#include <QDir>
#include <QDebug>
#include <iostream>

Logger* Logger::s_instance = nullptr;

Logger::Logger(QObject *parent)
    : QObject(parent)
    , m_logLevel(LogLevel::INFO)
    , m_consoleOutput(true)
{
}

Logger::~Logger()
{
    if (m_logFile.isOpen()) {
        m_logStream.flush();
        m_logFile.close();
    }
}

Logger* Logger::instance()
{
    if (!s_instance) {
        s_instance = new Logger();
    }
    return s_instance;
}

void Logger::init(const QString &logPath)
{
    QMutexLocker locker(&m_mutex);
    
    QString path = logPath;
    if (path.isEmpty()) {
        QString logDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        QDir().mkpath(logDir);
        path = logDir + "/deepin-disk-cleaner.log";
    }
    
    if (m_logFile.isOpen()) {
        m_logStream.flush();
        m_logFile.close();
    }
    
    m_logFile.setFileName(path);
    if (m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        m_logStream.setDevice(&m_logFile);
        m_logStream.setEncoding(QStringConverter::Utf8);
    }
}

void Logger::setLogLevel(LogLevel level)
{
    QMutexLocker locker(&m_mutex);
    m_logLevel = level;
}

void Logger::debug(const QString &message)
{
    writeLog(LogLevel::DEBUG, message);
}

void Logger::info(const QString &message)
{
    writeLog(LogLevel::INFO, message);
}

void Logger::warning(const QString &message)
{
    writeLog(LogLevel::WARNING, message);
}

void Logger::error(const QString &message)
{
    writeLog(LogLevel::ERROR, message);
}

void Logger::critical(const QString &message)
{
    writeLog(LogLevel::CRITICAL, message);
}

void Logger::writeLog(LogLevel level, const QString &message)
{
    QMutexLocker locker(&m_mutex);
    
    if (level < m_logLevel) {
        return;
    }
    
    QDateTime now = QDateTime::currentDateTime();
    QString timestamp = now.toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString levelStr = levelToString(level);
    
    QString logMessage = QString("[%1] [%2] %3")
        .arg(timestamp)
        .arg(levelStr)
        .arg(message);
    
    // 写入文件
    if (m_logFile.isOpen()) {
        m_logStream << logMessage << Qt::endl;
        m_logStream.flush();
    }
    
    // 输出到控制台
    if (m_consoleOutput) {
        QString colorCode = levelToColor(level);
        std::cout << colorCode.toStdString() 
                  << logMessage.toStdString() 
                  << "\033[0m" << std::endl;
    }
}
QString Logger::levelToString(LogLevel level)
{
    switch (level) {
        case LogLevel::DEBUG:    return "DEBUG";
        case LogLevel::INFO:     return "INFO ";
        case LogLevel::WARNING:  return "WARN ";
        case LogLevel::ERROR:    return "ERROR";
        case LogLevel::CRITICAL: return "FATAL";
        default: return "UNKNOWN";
    }
}

QString Logger::levelToColor(LogLevel level)
{
    switch (level) {
        case LogLevel::DEBUG:    return "\033[36m";    // Cyan
        case LogLevel::INFO:     return "\033[32m";    // Green
        case LogLevel::WARNING:  return "\033[33m";    // Yellow
        case LogLevel::ERROR:    return "\033[31m";    // Red
        case LogLevel::CRITICAL: return "\033[35m";    // Magenta
        default: return "\033[0m";      // Reset
    }
}