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
#include "../core/diskanalyzer.h"
#include "../core/diskcleaner.h"
#include "dashboardwidget.h"
#include "analyzewidget.h"
#include "resourceswidget.h"
#include "fileshredderwidget.h"
#include "systemslimmerwidget.h"
#include "cleanupdialog.h"
#include "progressdialog.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onAnalyzeClicked();
    void onSmartCleanupClicked();
    void onCustomCleanupClicked();
    void onSettingsClicked();
    void onAboutClicked();
    
    void onAnalysisFinished();
    void onCleanupProgress(const QString &itemName, int percent);
    void onCleanupFinished(const QList<CleanupResult> &results);
    void onCleanupError(const QString &error);

private:
    void initUI();
    QString formatSize(qint64 bytes);
    void createMenuBar();
    void createToolBar();
    void createStatusBar();
    void connectSignals();
    void applyTheme();
    void performAnalyzeCleanup(const QList<ScanResult> &items);
    
    // UI 组件
    QTabWidget *m_tabWidget;
    DashboardWidget *m_dashboardWidget;
    AnalyzeWidget *m_analyzeWidget;
    ResourcesWidget *m_resourcesWidget;
    FileShredderWidget *m_fileShredderWidget;
    SystemSlimmerWidget *m_systemSlimmerWidget;
    
    QPushButton *m_analyzeButton;
    QPushButton *m_smartCleanupButton;
    QPushButton *m_customCleanupButton;
    
    QLabel *m_statusLabel;
    QProgressBar *m_progressBar;
    
    // 核心组件
    DiskAnalyzer *m_analyzer;
    DiskCleaner *m_cleaner;
    
    // 对话框
    CleanupDialog *m_cleanupDialog;
    ProgressDialog *m_progressDialog;
};

#endif // MAINWINDOW_H