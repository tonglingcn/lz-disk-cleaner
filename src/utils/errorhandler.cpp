/*
 * Error Handler - Implementation
 * 错误处理器 - 实现
 */

#include "errorhandler.h"
#include "logger.h"

ErrorHandler* ErrorHandler::s_instance = nullptr;

ErrorHandler::ErrorHandler(QObject *parent)
    : QObject(parent)
{
}

ErrorHandler::~ErrorHandler()
{
}

ErrorHandler* ErrorHandler::instance()
{
    if (!s_instance) {
        s_instance = new ErrorHandler();
    }
    return s_instance;
}

void ErrorHandler::reportError(const ErrorInfo &error)
{
    // 记录日志
    QString logMessage = QString("[%1] %2").arg(getLevelString(error.level)).arg(error.message);
    if (!error.details.isEmpty()) {
        logMessage += QString(" - %1").arg(error.details);
    }
    if (!error.context.isEmpty()) {
        logMessage += QString(" (Context: %1)").arg(error.context);
    }
    
    switch (error.level) {
        case ErrorLevel::Info:
            LOG_INFO(logMessage);
            break;
        case ErrorLevel::Warning:
            LOG_WARNING(logMessage);
            break;
        case ErrorLevel::Error:
            LOG_ERROR(logMessage);
            break;
        case ErrorLevel::Critical:
            LOG_CRITICAL(logMessage);
            break;
    }
    
    // 发送信号
    emit errorOccurred(error);
}

void ErrorHandler::reportError(ErrorCode code, const QString &message, const QString &details)
{
    ErrorLevel level = ErrorLevel::Error;
    
    // 根据错误代码确定级别
    if (code == ErrorCode::Success) {
        level = ErrorLevel::Info;
    } else if (static_cast<int>(code) >= 4000) {
        level = ErrorLevel::Critical;
    } else if (static_cast<int>(code) >= 2000) {
        level = ErrorLevel::Warning;
    }
    
    ErrorInfo error(code, level, message, details);
    reportError(error);
}

QString ErrorHandler::getErrorMessage(ErrorCode code)
{
    switch (code) {
        case ErrorCode::Success:
            return tr("操作成功");
            
        // 文件系统错误
        case ErrorCode::FileNotFound:
            return tr("文件未找到");
        case ErrorCode::FileAccessDenied:
            return tr("文件访问被拒绝");
        case ErrorCode::DirectoryNotFound:
            return tr("目录未找到");
        case ErrorCode::DirectoryAccessDenied:
            return tr("目录访问被拒绝");
        case ErrorCode::DiskFull:
            return tr("磁盘空间不足");
            
        // 权限错误
        case ErrorCode::PermissionDenied:
            return tr("权限不足");
        case ErrorCode::SudoRequired:
            return tr("需要管理员权限");
            
        // 进程错误
        case ErrorCode::ProcessTimeout:
            return tr("进程执行超时");
        case ErrorCode::ProcessFailed:
            return tr("进程执行失败");
        case ErrorCode::ProcessCrashed:
            return tr("进程崩溃");
            
        // 系统错误
        case ErrorCode::SystemCommandNotFound:
            return tr("系统命令未找到");
        case ErrorCode::SystemNotSupported:
            return tr("系统不支持此操作");
            
        default:
            return tr("未知错误");
    }
}

QString ErrorHandler::getLevelString(ErrorLevel level)
{
    switch (level) {
        case ErrorLevel::Info:
            return "INFO";
        case ErrorLevel::Warning:
            return "WARNING";
        case ErrorLevel::Error:
            return "ERROR";
        case ErrorLevel::Critical:
            return "CRITICAL";
        default:
            return "UNKNOWN";
    }
}
