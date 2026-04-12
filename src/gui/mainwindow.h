/*
 * Main Window - Header
 * 主窗口 - 头文件
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTabWidget>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QEvent>
#include <QRect>
#include <QWindow>
#include <QTimer>
#include <QSystemTrayIcon>
#include <QMenu>
#include "../core/diskanalyzer.h"
#include "../core/diskcleaner.h"
#include "dashboardwidget.h"
#include "analyzewidget.h"
#include "resourceswidget.h"
#include "fileshredderwidget.h"
#include "systemslimmerwidget.h"
#include "startupappswidget.h"
#include "aptsourcemanagerwidget.h"
#include "cleanuphistorywidget.h"
#include "cleanupdialog.h"
#include "progressdialog.h"
#include "settingsdialog.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event) override;
    void showEvent(QShowEvent *event) override;
    bool event(QEvent *event) override;
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;

private slots:
    void onAnalyzeClicked();
    void onSmartCleanupClicked();
    void onCustomCleanupClicked();
    void onSettingsClicked();
    void onSponsorClicked();
    void onAboutClicked();
    
    void onAnalysisFinished();
    void onCleanupProgress(const QString &itemName, int percent);
    void onCleanupFinished(const QList<CleanupResult> &results);
    void onCleanupError(const QString &error);
    
    // DDE窗口状态检测定时器
    void onWindowStateCheck();
    
    // 系统托盘
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void onTrayShowWindow();
    void onTrayCleanup();
    void onTrayQuit();

private:
    void initUI();
    QString formatSize(qint64 bytes);
    qint64 parseSizeToBytes(const QString &sizeStr);
    void createMenuBar();
    void createToolBar();
    void createStatusBar();
    void connectSignals();
    void applyTheme();
    void performAnalyzeCleanup(const QList<ScanResult> &items);
    bool checkX11WindowState();
    
    // 白名单检查辅助函数
    bool isPathInWhitelist(const QString &path, const QStringList &whitelist);
    bool isFileMatchPattern(const QString &fileName, const QStringList &patterns);
    bool cleanDirContent(const QString &dirPath);
    
    // UI 组件
    QTabWidget *m_tabWidget;
    DashboardWidget *m_dashboardWidget;
    AnalyzeWidget *m_analyzeWidget;
    ResourcesWidget *m_resourcesWidget;
    FileShredderWidget *m_fileShredderWidget;
    SystemSlimmerWidget *m_systemSlimmerWidget;
    StartupAppsWidget *m_startupAppsWidget;
    APTSourceManagerWidget *m_aptSourceManagerWidget;
    CleanupHistoryWidget *m_cleanupHistoryWidget;
    
    QPushButton *m_analyzeButton;
    QPushButton *m_smartCleanupButton;
    QPushButton *m_customCleanupButton;
    
    QLabel *m_statusLabel;
    QProgressBar *m_progressBar;
    QPushButton *m_sponsorButton;
    
    // 核心组件
    DiskAnalyzer *m_analyzer;
    DiskCleaner *m_cleaner;
    
    // 对话框
    CleanupDialog *m_cleanupDialog;
    ProgressDialog *m_progressDialog;
    
    // 保存窗口几何信息
    QRect m_normalGeometry;
    bool m_wasMaximized;
    
    // DDE窗口状态检测定时器
    QTimer *m_stateCheckTimer;
    bool m_lastX11MaximizedState;
    QRect m_lastGeometry;  // 用于检测几何变化
    
    // DDE还原按钮悬停检测
    QTimer *m_restoreHoverTimer;
    bool m_restoreHoverTriggered;
    QPoint m_lastMousePos;
    
    // 赞助按钮随机显示定时器
    QTimer *m_sponsorTimer;
    int m_sponsorShowCount;  // 记录显示次数
    
    void checkRestoreButtonHover();
    void setupSponsorTimer();
    void createTrayIcon();
    
    // 系统托盘
    QSystemTrayIcon *m_trayIcon;
    QMenu *m_trayMenu;
    bool m_canClose;  // 是否允许关闭窗口
};

#endif // MAINWINDOW_H