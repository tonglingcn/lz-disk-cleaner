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
#include "gui/mainwindow.h"
#include "utils/logger.h"

int main(int argc, char *argv[])
{
    // 高DPI支持 - 必须在创建QApplication之前设置
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#endif
    
    QApplication app(argc, argv);
    
    // 设置应用程序信息
    app.setApplicationName("LZ Disk Cleaner");
    app.setApplicationVersion("1.1.0");
    app.setOrganizationName("LZ");
    app.setOrganizationDomain("github.com/tonglingcn");
    
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
    
    // 运行应用
    int result = app.exec();
    
    LOG_INFO("LZ Disk Cleaner exited");
    return result;
}