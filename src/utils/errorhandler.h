/*
 * Error Handler - Header
 * 错误处理器 - 头文件
 * 
 * 统一的错误处理和报告机制
 */

#ifndef ERRORHANDLER_H
#define ERRORHANDLER_H

#include <QString>
#include <QObject>

// 错误级别
enum class ErrorLevel {
    Info,       // 信息
    Warning,    // 警告
    Error,      // 错误
    Critical    // 严重错误
};

// 错误代码
enum class ErrorCode {
    Success = 0,
    
    // 文件系统错误 (1000-1999)
    FileNotFound = 1000,
    FileAccessDenied = 1001,
    DirectoryNotFound = 1002,
    DirectoryAccessDenied = 1003,
    DiskFull = 1004,
    
    // 权限错误 (2000-2999)
    PermissionDenied = 2000,
    SudoRequired = 2001,
    
    // 进程错误 (3000-3999)
    ProcessTimeout = 3000,
    ProcessFailed = 3001,
    ProcessCrashed = 3002,
    
    // 系统错误 (4000-4999)
    SystemCommandNotFound = 4000,
    SystemNotSupported = 4001,
    
    // 未知错误
    Unknown = 9999
};

// 错误信息结构
struct ErrorInfo {
    ErrorCode code;
    ErrorLevel level;
    QString message;
    QString details;
    QString context;  // 错误发生的上下文
    
    ErrorInfo() 
        : code(ErrorCode::Success)
        , level(ErrorLevel::Info)
    {}
    
    ErrorInfo(ErrorCode c, ErrorLevel l, const QString &msg, const QString &det = QString())
        : code(c)
        , level(l)
        , message(msg)
        , details(det)
    {}
    
    bool isSuccess() const { return code == ErrorCode::Success; }
    bool isError() const { return level == ErrorLevel::Error || level == ErrorLevel::Critical; }
};

// 操作结果模板
template<typename T>
struct Result {
    T value;
    ErrorInfo error;
    
    Result() : error() {}
    Result(const T &val) : value(val), error() {}
    Result(const ErrorInfo &err) : error(err) {}
    
    bool isSuccess() const { return error.isSuccess(); }
    bool isError() const { return error.isError(); }
    
    operator bool() const { return isSuccess(); }
};

// 错误处理器类
class ErrorHandler : public QObject
{
    Q_OBJECT
    
public:
    static ErrorHandler* instance();
    
    // 报告错误
    void reportError(const ErrorInfo &error);
    void reportError(ErrorCode code, const QString &message, const QString &details = QString());
    
    // 获取错误描述
    static QString getErrorMessage(ErrorCode code);
    static QString getLevelString(ErrorLevel level);
    
signals:
    void errorOccurred(const ErrorInfo &error);
    
private:
    explicit ErrorHandler(QObject *parent = nullptr);
    ~ErrorHandler();
    
    static ErrorHandler *s_instance;
};

// 便捷宏
#define REPORT_ERROR(code, msg) ErrorHandler::instance()->reportError(code, msg)
#define REPORT_ERROR_DETAIL(code, msg, detail) ErrorHandler::instance()->reportError(code, msg, detail)

#endif // ERRORHANDLER_H
