/*
 * Logger - Header
 * 日志工具 - 头文件
 */

#ifndef LOGGER_H
#define LOGGER_H

#include <QObject>
#include <QString>
#include <QFile>
#include <QTextStream>
#include <QMutex>
#include <QDateTime>

enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    CRITICAL
};

class Logger : public QObject
{
    Q_OBJECT

public:
    static Logger* instance();
    
    void init(const QString &logPath = QString());
    void setLogLevel(LogLevel level);
    
    void debug(const QString &message);
    void info(const QString &message);
    void warning(const QString &message);
    void error(const QString &message);
    void critical(const QString &message);
    
private:
    explicit Logger(QObject *parent = nullptr);
    ~Logger();
    
    void writeLog(LogLevel level, const QString &message);
    QString levelToString(LogLevel level);
    QString levelToColor(LogLevel level);
    
    static Logger *s_instance;
    QFile m_logFile;
    QTextStream m_logStream;
    QMutex m_mutex;
    LogLevel m_logLevel;
    bool m_consoleOutput;
};

// 便捷宏定义
#define LOG_DEBUG(msg) Logger::instance()->debug(msg)
#define LOG_INFO(msg) Logger::instance()->info(msg)
#define LOG_WARNING(msg) Logger::instance()->warning(msg)
#define LOG_ERROR(msg) Logger::instance()->error(msg)
#define LOG_CRITICAL(msg) Logger::instance()->critical(msg)

#endif // LOGGER_H