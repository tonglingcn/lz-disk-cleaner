/*
 * LZ Disk Cleaner - Main Entry Point
 * LZ 磁盘清理工具 - 主程序入口
 * 
 * Copyright (C) 2025-2026 tonglingcn
 * License: GPL-3.0
 */

#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include <QStyleFactory>
#include <QGuiApplication>
#include <QSharedMemory>
#include <QLocalSocket>
#include <QLocalServer>
#include "gui/mainwindow.h"
#include "utils/logger.h"

// 单实例服务器名称
static const QString SINGLE_INSTANCE_SERVER = "lz-disk-cleaner-single-instance";

int main(int argc, char *argv[])
{
    // 高DPI支持 - 必须在创建QApplication之前设置
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#endif
    
    QApplication app(argc, argv);
    
    // 设置应用程序信息
    app.setApplicationName("LZ Disk Cleaner");
    app.setApplicationVersion("1.2.0");
    app.setOrganizationName("LZ");
    app.setOrganizationDomain("github.com/tonglingcn");
    
    // 单实例检测 - 尝试连接已存在的实例
    QLocalSocket socket;
    socket.connectToServer(SINGLE_INSTANCE_SERVER);
    if (socket.waitForConnected(500)) {
        // 已有实例运行，发送激活命令
        LOG_INFO("Another instance is running, sending activation request");
        socket.write("SHOW");
        socket.waitForBytesWritten();
        socket.disconnectFromServer();
        return 0;  // 退出当前实例
    }
    
    // 创建本地服务器用于单实例通信
    QLocalServer server;
    // 清理可能残留的服务器文件
    QLocalServer::removeServer(SINGLE_INSTANCE_SERVER);
    if (!server.listen(SINGLE_INSTANCE_SERVER)) {
        LOG_ERROR("Failed to create single instance server");
    }
    
    // 设置应用图标
    app.setWindowIcon(QIcon(":/icons/app_icon.svg"));
    
    // 加载翻译文件
    QTranslator translator;
    const QString locale = QLocale::system().name();
    if (translator.load(":/translations/lz-disk-cleaner_" + locale + ".qm")) {
        app.installTranslator(&translator);
    }
    
    // 初始化日志系统
    Logger::instance()->init();
    LOG_INFO("LZ Disk Cleaner started");
    
    // 创建并显示主窗口
    MainWindow window;
    window.show();
    
    // 监听其他实例的连接请求
    QObject::connect(&server, &QLocalServer::newConnection, [&window, &server]() {
        QLocalSocket *clientSocket = server.nextPendingConnection();
        if (clientSocket) {
            clientSocket->waitForReadyRead(1000);
            QByteArray data = clientSocket->readAll();
            if (data == "SHOW") {
                LOG_INFO("Received activation request from another instance");
                window.show();
                window.activateWindow();
                window.raise();
                // 如果是最小化状态，恢复窗口
                if (window.isMinimized()) {
                    window.showNormal();
                }
            }
            clientSocket->disconnectFromServer();
            clientSocket->deleteLater();
        }
    });
    
    // 运行应用
    int result = app.exec();
    
    LOG_INFO("LZ Disk Cleaner exited");
    return result;
}