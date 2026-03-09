/*
 * Main Window - Implementation
 * 主窗口 - 实现
 */

#include "mainwindow.h"
#include "analyzewidget.h"
#include "../utils/logger.h"
#include "../utils/config.h"
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QApplication>
#include <QIcon>
#include <QSize>
#include <QProcess>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_tabWidget(nullptr)
    , m_dashboardWidget(nullptr)
    , m_analyzeWidget(nullptr)
    , m_resourcesWidget(nullptr)
    , m_fileShredderWidget(nullptr)
    , m_analyzeButton(nullptr)
    , m_smartCleanupButton(nullptr)
    , m_customCleanupButton(nullptr)
    , m_statusLabel(nullptr)
    , m_progressBar(nullptr)
    , m_analyzer(nullptr)
    , m_cleaner(nullptr)
    , m_cleanupDialog(nullptr)
    , m_progressDialog(nullptr)
{
    LOG_INFO("Initializing main window");
    
    initUI();
    connectSignals();
    applyTheme();
    
    LOG_INFO("Main window initialized");
}

MainWindow::~MainWindow()
{
    LOG_INFO("Destroying main window");
}

void MainWindow::initUI()
{
    // 设置窗口属性
    setWindowTitle(tr("LZ磁盘清理工具"));
    setMinimumSize(900, 700);
    resize(1100, 800);
    
    // 创建中心部件
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    // 主布局
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);
    
    // 创建菜单栏
    createMenuBar();
    
    // 不创建工具栏，保持界面简洁
    
    // 创建标签页
    m_tabWidget = new QTabWidget(this);
    
    // 仪表盘页面
    m_dashboardWidget = new DashboardWidget(this);
    m_tabWidget->addTab(m_dashboardWidget, tr("仪表盘"));
    
    // 磁盘分析页面
    m_analyzeWidget = new AnalyzeWidget(this);
    m_tabWidget->addTab(m_analyzeWidget, tr("磁盘分析"));
    
    // 系统资源监控页面
    m_resourcesWidget = new ResourcesWidget(this);
    m_tabWidget->addTab(m_resourcesWidget, tr("资源监控"));
    
    // 文件粉碎页面
    m_fileShredderWidget = new FileShredderWidget(this);
    m_tabWidget->addTab(m_fileShredderWidget, tr("文件粉碎"));
    
    // 系统瘦身页面
    m_systemSlimmerWidget = new SystemSlimmerWidget(this);
    m_tabWidget->addTab(m_systemSlimmerWidget, tr("系统瘦身"));
    
    mainLayout->addWidget(m_tabWidget, 1);
    
    // 创建按钮区域
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(10);
    
    // 仪表盘按钮 - 浅蓝色
    QPushButton *dashboardButton = new QPushButton(this);
    dashboardButton->setIcon(QIcon(":/icons/dashboard.svg"));
    dashboardButton->setIconSize(QSize(20, 20));
    dashboardButton->setText(tr("仪表盘"));
    dashboardButton->setMinimumHeight(45);
    dashboardButton->setStyleSheet(
        "QPushButton { "
        "   background-color: #5DADE2; "
        "   color: white; "
        "   border: none; "
        "   border-radius: 6px; "
        "   font-size: 14px; "
        "   font-weight: 500; "
        "   padding: 10px 20px; "
        "} "
        "QPushButton:hover { "
        "   background-color: #3498DB; "
        "} "
        "QPushButton:pressed { "
        "   background-color: #2E86C1; "
        "}"
    );
    connect(dashboardButton, &QPushButton::clicked, this, [this]() {
        m_tabWidget->setCurrentIndex(0);
    });
    
    // 分析磁盘按钮 - 青色
    m_analyzeButton = new QPushButton(this);
    m_analyzeButton->setIcon(QIcon(":/icons/analyze.svg"));
    m_analyzeButton->setIconSize(QSize(20, 20));
    m_analyzeButton->setText(tr("分析磁盘"));
    m_analyzeButton->setMinimumHeight(45);
    m_analyzeButton->setStyleSheet(
        "QPushButton { "
        "   background-color: #48C9B0; "
        "   color: white; "
        "   border: none; "
        "   border-radius: 6px; "
        "   font-size: 14px; "
        "   font-weight: 500; "
        "   padding: 10px 20px; "
        "} "
        "QPushButton:hover { "
        "   background-color: #1ABC9C; "
        "} "
        "QPushButton:pressed { "
        "   background-color: #16A085; "
        "}"
    );
    
    // 一键智能清理按钮 - 绿色
    m_smartCleanupButton = new QPushButton(this);
    m_smartCleanupButton->setIcon(QIcon(":/icons/cleanup.svg"));
    m_smartCleanupButton->setIconSize(QSize(20, 20));
    m_smartCleanupButton->setText(tr("一键智能清理"));
    m_smartCleanupButton->setMinimumHeight(45);
    m_smartCleanupButton->setStyleSheet(
        "QPushButton { "
        "   background-color: #58D68D; "
        "   color: white; "
        "   border: none; "
        "   border-radius: 6px; "
        "   font-size: 14px; "
        "   font-weight: 500; "
        "   padding: 10px 20px; "
        "} "
        "QPushButton:hover { "
        "   background-color: #2ECC71; "
        "} "
        "QPushButton:pressed { "
        "   background-color: #27AE60; "
        "}"
    );
    
    // 自定义清理按钮 - 橙色
    m_customCleanupButton = new QPushButton(this);
    m_customCleanupButton->setIcon(QIcon(":/icons/settings.svg"));
    m_customCleanupButton->setIconSize(QSize(20, 20));
    m_customCleanupButton->setText(tr("自定义清理"));
    m_customCleanupButton->setMinimumHeight(45);
    m_customCleanupButton->setStyleSheet(
        "QPushButton { "
        "   background-color: #F5B041; "
        "   color: white; "
        "   border: none; "
        "   border-radius: 6px; "
        "   font-size: 14px; "
        "   font-weight: 500; "
        "   padding: 10px 20px; "
        "} "
        "QPushButton:hover { "
        "   background-color: #F39C12; "
        "} "
        "QPushButton:pressed { "
        "   background-color: #D68910; "
        "}"
    );
    
    buttonLayout->addWidget(dashboardButton);
    buttonLayout->addWidget(m_analyzeButton);
    buttonLayout->addWidget(m_smartCleanupButton);
    buttonLayout->addWidget(m_customCleanupButton);
    
    mainLayout->addLayout(buttonLayout);
    
    // 创建状态栏
    createStatusBar();
    
    // 初始化核心组件
    m_analyzer = new DiskAnalyzer(this);
    m_cleaner = new DiskCleaner(this);
}

void MainWindow::createMenuBar()
{
    QMenuBar *menuBar = this->menuBar();
    
    // 文件菜单
    QMenu *fileMenu = menuBar->addMenu(tr("文件(&F)"));
    QAction *exitAction = fileMenu->addAction(tr("退出(&X)"), this, &QWidget::close, QKeySequence::Quit);
    exitAction->setIcon(QIcon(":/icons/about.svg"));
    
    // 工具菜单
    QMenu *toolsMenu = menuBar->addMenu(tr("工具(&T)"));
    QAction *analyzeAction = toolsMenu->addAction(tr("分析磁盘(&A)"), this, &MainWindow::onAnalyzeClicked);
    analyzeAction->setIcon(QIcon(":/icons/analyze.svg"));
    QAction *smartCleanupAction = toolsMenu->addAction(tr("智能清理(&S)"), this, &MainWindow::onSmartCleanupClicked);
    smartCleanupAction->setIcon(QIcon(":/icons/cleanup.svg"));
    QAction *customCleanupAction = toolsMenu->addAction(tr("自定义清理(&C)"), this, &MainWindow::onCustomCleanupClicked);
    customCleanupAction->setIcon(QIcon(":/icons/settings.svg"));
    
    // 设置菜单
    QMenu *settingsMenu = menuBar->addMenu(tr("设置(&S)"));
    QAction *prefsAction = settingsMenu->addAction(tr("首选项(&P)"), this, &MainWindow::onSettingsClicked);
    prefsAction->setIcon(QIcon(":/icons/settings.svg"));
    
    // 帮助菜单
    QMenu *helpMenu = menuBar->addMenu(tr("帮助(&H)"));
    QAction *aboutAction = helpMenu->addAction(tr("关于(&A)"), this, &MainWindow::onAboutClicked);
    aboutAction->setIcon(QIcon(":/icons/about.svg"));
}

void MainWindow::createToolBar()
{
    QToolBar *toolBar = addToolBar(tr("主工具栏"));
    toolBar->setMovable(false);
    toolBar->setIconSize(QSize(24, 24));
    
    QAction *dashboardAction = toolBar->addAction(QIcon(":/icons/dashboard.svg"), tr("仪表盘"));
    dashboardAction->setToolTip(tr("查看系统仪表盘"));
    connect(dashboardAction, &QAction::triggered, this, [this]() {
        m_tabWidget->setCurrentIndex(0);
    });
    
    QAction *analyzeAction = toolBar->addAction(QIcon(":/icons/analyze.svg"), tr("分析"));
    analyzeAction->setToolTip(tr("分析磁盘使用情况"));
    connect(analyzeAction, &QAction::triggered, this, &MainWindow::onAnalyzeClicked);
    
    QAction *cleanupAction = toolBar->addAction(QIcon(":/icons/cleanup.svg"), tr("清理"));
    cleanupAction->setToolTip(tr("执行磁盘清理"));
    connect(cleanupAction, &QAction::triggered, this, &MainWindow::onSmartCleanupClicked);
    
    toolBar->addSeparator();
    
    QAction *settingsAction = toolBar->addAction(QIcon(":/icons/settings.svg"), tr("设置"));
    settingsAction->setToolTip(tr("打开设置"));
    connect(settingsAction, &QAction::triggered, this, &MainWindow::onSettingsClicked);
}

void MainWindow::createStatusBar()
{
    QStatusBar *statusBar = this->statusBar();
    
    m_statusLabel = new QLabel(tr("🚀 智能清理引擎运行中..."), this);
    statusBar->addWidget(m_statusLabel, 1);
    
    m_progressBar = new QProgressBar(this);
    m_progressBar->setMaximumWidth(200);
    m_progressBar->setVisible(false);
    statusBar->addPermanentWidget(m_progressBar);
}

void MainWindow::connectSignals()
{
    // 按钮信号
    connect(m_analyzeButton, &QPushButton::clicked, this, &MainWindow::onAnalyzeClicked);
    connect(m_smartCleanupButton, &QPushButton::clicked, this, &MainWindow::onSmartCleanupClicked);
    connect(m_customCleanupButton, &QPushButton::clicked, this, &MainWindow::onCustomCleanupClicked);
    
    // 分析器信号
    connect(m_analyzer, &DiskAnalyzer::analysisFinished, this, &MainWindow::onAnalysisFinished);
    connect(m_analyzer, &DiskAnalyzer::errorOccurred, this, &MainWindow::onCleanupError);
    
    // 清理器信号
    connect(m_cleaner, &DiskCleaner::cleanupProgress, this, &MainWindow::onCleanupProgress);
    connect(m_cleaner, &DiskCleaner::cleanupFinished, this, &MainWindow::onCleanupFinished);
    connect(m_cleaner, &DiskCleaner::cleanupError, this, &MainWindow::onCleanupError);
    
    // 分析组件信号
    connect(m_analyzeWidget, &AnalyzeWidget::scanFinished, this, [this](qint64 totalSize) {
        m_statusLabel->setText(tr("扫描完成，共发现 %1 可清理空间")
            .arg(formatSize(totalSize)));
    });
    connect(m_analyzeWidget, &AnalyzeWidget::cleanupRequested, this, [this](const QList<ScanResult> &items) {
        // 执行清理操作
        m_statusLabel->setText(tr("正在清理..."));
        m_progressBar->setVisible(true);
        m_progressBar->setRange(0, 100);
        
        // 显示进度对话框
        m_progressDialog = new ProgressDialog(this);
        m_progressDialog->show();
        
        // 执行清理
        performAnalyzeCleanup(items);
    });
}

void MainWindow::applyTheme()
{
    Config *config = Config::instance();
    
    if (config->getDarkMode()) {
        qApp->setStyleSheet(
            "QMainWindow { background-color: #2b2b2b; } "
            "QWidget { color: #ffffff; } "
            "QTabWidget::pane { border: 1px solid #444; } "
            "QTabBar::tab { background-color: #3b3b3b; color: #aaa; padding: 8px; } "
            "QTabBar::tab:selected { background-color: #2b2b2b; color: #fff; }"
        );
    } else {
        qApp->setStyleSheet("");
    }
}

void MainWindow::onAnalyzeClicked()
{
    LOG_INFO("Analyze button clicked");
    
    // 切换到磁盘分析标签页
    m_tabWidget->setCurrentWidget(m_analyzeWidget);
    
    // 开始扫描
    m_analyzeWidget->startScan();
    m_statusLabel->setText(tr("正在扫描磁盘..."));
}

void MainWindow::onSmartCleanupClicked()
{
    LOG_INFO("Smart cleanup button clicked");
    
    // 先扫描各项目大小
    m_statusLabel->setText(tr("正在计算可清理空间..."));
    QApplication::processEvents();
    
    // 计算各项目大小
    QString home = QDir::homePath();
    
    // 缩略图缓存
    QProcess duThumb;
    duThumb.start("du", QStringList() << "-sb" << home + "/.cache/thumbnails");
    duThumb.waitForFinished(5000);
    qint64 thumbSize = duThumb.readAllStandardOutput().split('\t')[0].toLongLong();
    
    // 开发工具缓存
    qint64 devSize = 0;
    QStringList devPaths = {
        home + "/.cache/pip",
        home + "/.npm",
        home + "/.cache/go-build",
        home + "/.cargo/registry"
    };
    for (const QString &path : devPaths) {
        QProcess duDev;
        duDev.start("du", QStringList() << "-sb" << path);
        duDev.waitForFinished(5000);
        devSize += duDev.readAllStandardOutput().split('\t')[0].toLongLong();
    }
    
    // 回收站
    QProcess duTrash;
    duTrash.start("du", QStringList() << "-sb" << home + "/.local/share/Trash/files");
    duTrash.waitForFinished(5000);
    qint64 trashSize = duTrash.readAllStandardOutput().split('\t')[0].toLongLong();
    
    qint64 totalSize = thumbSize + devSize + trashSize;
    QString totalText = formatSize(totalSize);
    
    QString message = tr("智能清理扫描结果：\n\n"
        "• 缩略图缓存: %1\n"
        "• 开发工具缓存: %2\n"
        "• 回收站: %3\n\n"
        "可释放空间: %4\n\n"
        "这些项目清理后可自动恢复，不会影响用户数据。\n"
        "确定要执行清理吗？")
        .arg(formatSize(thumbSize))
        .arg(formatSize(devSize))
        .arg(formatSize(trashSize))
        .arg(totalText);
    
    // 确认对话框
    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        tr("智能清理确认"),
        message,
        QMessageBox::Yes | QMessageBox::No
    );
    
    if (reply == QMessageBox::Yes) {
        m_statusLabel->setText(tr("正在清理..."));
        m_progressBar->setVisible(true);
        m_progressBar->setRange(0, 100);
        
        // 显示进度对话框
        m_progressDialog = new ProgressDialog(this);
        m_progressDialog->show();
        
        // 执行智能清理
        m_cleaner->smartCleanup();
    } else {
        m_statusLabel->setText(tr("已取消清理"));
    }
}

void MainWindow::onCustomCleanupClicked()
{
    LOG_INFO("Custom cleanup button clicked");
    
    // 显示自定义清理对话框
    m_cleanupDialog = new CleanupDialog(this);
    
    if (m_cleanupDialog->exec() == QDialog::Accepted) {
        // 执行选中的清理任务
        // TODO: 实现自定义清理逻辑
    }
}

void MainWindow::onSettingsClicked()
{
    LOG_INFO("Settings button clicked");
    // TODO: 实现设置对话框
    QMessageBox::information(this, tr("设置"), tr("设置功能开发中..."));
}

void MainWindow::onAboutClicked()
{
    LOG_INFO("About button clicked");
    QMessageBox::about(
        this,
        tr("关于 LZ磁盘清理工具"),
        tr("<h3>LZ磁盘清理工具 v1.0.0</h3>"
           "<p>一个专为 Deepin/Debian 系统设计的磁盘清理工具。</p>"
           "<p><b>功能特性：</b></p>"
           "<ul>"
           "<li>磁盘使用分析</li>"
           "<li>磐石系统快照管理</li>"
           "<li>玲珑应用管理</li>"
           "<li>智能清理</li>"
           "<li>自定义清理</li>"
           "<li>文件粉碎</li>"
           "<li>系统瘦身</li>"
           "</ul>"
           "<p><b>技术栈：</b> C++17 + Qt6</p>"
           "<p>Copyright © 2025-2026 tonglingcn</p>")
    );
}

void MainWindow::onAnalysisFinished()
{
    LOG_INFO("Analysis finished");
    m_statusLabel->setText(tr("分析完成"));
    m_progressBar->setVisible(false);
}

void MainWindow::onCleanupProgress(const QString &itemName, int percent)
{
    LOG_INFO(QString("Cleanup progress: %1 - %2%").arg(itemName).arg(percent));
    m_statusLabel->setText(tr("正在清理: %1").arg(itemName));
    m_progressBar->setValue(percent);
    
    if (m_progressDialog) {
        m_progressDialog->updateProgress(itemName, percent);
    }
}

void MainWindow::onCleanupFinished(const QList<CleanupResult> &results)
{
    LOG_INFO("Cleanup finished");
    
    qint64 totalFreed = 0;
    QString detailText;
    
    for (const CleanupResult &result : results) {
        totalFreed += result.freedSpace;
        QString statusIcon = result.success ? "✓" : "✗";
        QString sizeText = result.success ? formatSize(result.freedSpace) : result.errorMessage;
        detailText += QString("%1 %2: %3\n").arg(statusIcon, result.itemName, sizeText);
    }
    
    QString freedText = formatSize(totalFreed);
    
    m_statusLabel->setText(tr("清理完成，释放空间: %1").arg(freedText));
    m_progressBar->setVisible(false);
    
    if (m_progressDialog) {
        m_progressDialog->close();
        delete m_progressDialog;
        m_progressDialog = nullptr;
    }
    
    QMessageBox::information(
        this,
        tr("清理完成"),
        tr("清理完成！\n\n释放空间: %1\n\n详细:\n%2").arg(freedText, detailText.trimmed())
    );
}

void MainWindow::onCleanupError(const QString &error)
{
    LOG_ERROR(QString("Cleanup error: %1").arg(error));
    m_statusLabel->setText(tr("清理出错: %1").arg(error));
    m_progressBar->setVisible(false);
    
    if (m_progressDialog) {
        m_progressDialog->close();
        delete m_progressDialog;
        m_progressDialog = nullptr;
    }
    
    QMessageBox::critical(
        this,
        tr("清理错误"),
        tr("清理过程中发生错误：\n%1").arg(error)
    );
}

void MainWindow::performAnalyzeCleanup(const QList<ScanResult> &items)
{
    LOG_INFO(QString("Performing analyze cleanup for %1 items").arg(items.size()));
    
    qint64 totalFreed = 0;
    int successCount = 0;
    int failCount = 0;
    int total = items.size();
    int current = 0;
    
    // 收集成功清理的项目
    QList<ScanResult> cleanedItems;
    
    // 分离需要权限和不需要权限的操作
    QStringList normalDeletePaths;      // 用户权限可直接删除的路径
    QList<ScanResult> normalItems;      // 对应的扫描结果（用于计算大小）
    QStringList privilegedDeletePaths;  // 需要 root 权限删除的路径
    QList<ScanResult> privilegedItems;  // 对应的扫描结果
    QList<ScanResult> linglongItems;    // 玲珑应用（需要通过 pkexec 卸载）
    bool needCleanAptCache = false;
    bool needCleanSystemLogs = false;
    
    for (const ScanResult &item : items) {
        switch (item.category) {
        case ScanCategory::APT_CACHE:
            needCleanAptCache = true;
            privilegedItems.append(item);
            break;
        case ScanCategory::SYSTEM_LOGS:
        case ScanCategory::JOURNAL_LOGS:
        case ScanCategory::CRASH_REPORTS:
            privilegedDeletePaths.append(item.path);
            privilegedItems.append(item);
            break;
        case ScanCategory::LINGLONG_APPS:
            // 玲珑应用需要通过 pkexec 执行 ll-cli uninstall
            linglongItems.append(item);
            break;
        case ScanCategory::TRASH:
        case ScanCategory::THUMBNAIL_CACHE:
        case ScanCategory::USER_CACHE:
        case ScanCategory::BROWSER_CACHE:
        case ScanCategory::DEV_CACHE:
        case ScanCategory::TEMP_FILES:
        default:
            normalDeletePaths.append(item.path);
            normalItems.append(item);
            break;
        }
    }
    
    // 第一步：执行用户权限内的删除操作
    for (const ScanResult &item : normalItems) {
        current++;
        int percent = (current * 100) / total;
        QString progressText = tr("正在清理: %1").arg(item.name);
        m_statusLabel->setText(progressText);
        m_progressBar->setValue(percent);
        if (m_progressDialog) {
            m_progressDialog->updateProgress(item.name, percent);
        }
        QApplication::processEvents();
        
        bool success = false;
        qint64 freedSize = 0;
        
        switch (item.category) {
        case ScanCategory::TRASH: {
            // 回收站清理 - 清空 files 和 info 子目录内容
            LOG_INFO(QString("Cleaning trash: %1").arg(item.path));
            QString filesPath = item.path + "/files";
            QString infoPath = item.path + "/info";
            
            freedSize = item.size;
            success = true;
            
            // 清空 files 目录内容
            QDir filesDir(filesPath);
            if (filesDir.exists()) {
                QFileInfoList entries = filesDir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot);
                for (const QFileInfo &entry : entries) {
                    QString entryPath = entry.absoluteFilePath();
                    if (entry.isDir()) {
                        QDir subDir(entryPath);
                        if (!subDir.removeRecursively()) {
                            LOG_ERROR(QString("Failed to remove trash dir: %1").arg(entryPath));
                            success = false;
                        }
                    } else {
                        if (!QFile::remove(entryPath)) {
                            LOG_ERROR(QString("Failed to remove trash file: %1").arg(entryPath));
                            success = false;
                        }
                    }
                }
            }
            
            // 清空 info 目录内容
            QDir infoDir(infoPath);
            if (infoDir.exists()) {
                QFileInfoList entries = infoDir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot);
                for (const QFileInfo &entry : entries) {
                    QString entryPath = entry.absoluteFilePath();
                    if (entry.isDir()) {
                        QDir subDir(entryPath);
                        if (!subDir.removeRecursively()) {
                            LOG_ERROR(QString("Failed to remove info dir: %1").arg(entryPath));
                            success = false;
                        }
                    } else {
                        if (!QFile::remove(entryPath)) {
                            LOG_ERROR(QString("Failed to remove info file: %1").arg(entryPath));
                            success = false;
                        }
                    }
                }
            }
            
            if (success) {
                LOG_INFO(QString("Trash cleaned successfully"));
            }
            break;
        }
        case ScanCategory::THUMBNAIL_CACHE:
        case ScanCategory::USER_CACHE:
        case ScanCategory::BROWSER_CACHE:
        case ScanCategory::DEV_CACHE: {
            // 缓存清理 - 清空目录内容而不是删除目录本身
            LOG_INFO(QString("Cleaning cache: %1").arg(item.path));
            QDir dir(item.path);
            if (dir.exists()) {
                freedSize = item.size;
                success = true;
                QFileInfoList entries = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot);
                for (const QFileInfo &entry : entries) {
                    QString entryPath = entry.absoluteFilePath();
                    if (entry.isDir()) {
                        QDir subDir(entryPath);
                        if (!subDir.removeRecursively()) {
                            LOG_ERROR(QString("Failed to remove cache dir: %1").arg(entryPath));
                            success = false;
                        }
                    } else {
                        if (!QFile::remove(entryPath)) {
                            LOG_ERROR(QString("Failed to remove cache file: %1").arg(entryPath));
                            success = false;
                        }
                    }
                }
            } else {
                success = true;
            }
            break;
        }
        case ScanCategory::TEMP_FILES: {
            // 临时文件清理 - 清空内容
            LOG_INFO(QString("Cleaning temp files: %1").arg(item.path));
            QDir dir(item.path);
            if (dir.exists()) {
                freedSize = item.size;
                success = true;
                QFileInfoList entries = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot);
                for (const QFileInfo &entry : entries) {
                    QString entryPath = entry.absoluteFilePath();
                    if (entry.isDir()) {
                        QDir subDir(entryPath);
                        if (!subDir.removeRecursively()) {
                            LOG_ERROR(QString("Failed to remove temp dir: %1").arg(entryPath));
                            success = false;
                        }
                    } else {
                        if (!QFile::remove(entryPath)) {
                            LOG_ERROR(QString("Failed to remove temp file: %1").arg(entryPath));
                            success = false;
                        }
                    }
                }
            } else {
                success = true;
            }
            break;
        }
        default: {
            // 默认：尝试删除路径
            LOG_INFO(QString("Cleaning: %1").arg(item.path));
            QFileInfo info(item.path);
            if (info.isDir()) {
                QDir dir(item.path);
                freedSize = item.size;
                success = dir.removeRecursively();
            } else if (info.isFile()) {
                freedSize = item.size;
                success = QFile::remove(item.path);
            } else {
                success = true;
            }
            break;
        }
        }
        
        if (success) {
            totalFreed += freedSize;
            successCount++;
            cleanedItems.append(item);  // 记录成功清理的项目
        } else {
            failCount++;
        }
    }
    
    // 第二步：执行需要 root 权限的操作
    if (!privilegedItems.isEmpty()) {
        // 检查 helper 程序是否存在
        QString helperPath = QStandardPaths::findExecutable("lz-disk-cleaner-helper");
        if (helperPath.isEmpty()) {
            // 尝试构建目录
            helperPath = QCoreApplication::applicationDirPath() + "/lz-disk-cleaner-helper";
        }
        
        m_statusLabel->setText(tr("正在执行需要权限的清理..."));
        if (m_progressDialog) {
            m_progressDialog->updateProgress(tr("需要权限的清理操作"), 90);
        }
        QApplication::processEvents();
        
        QStringList pkexecArgs;
        pkexecArgs << helperPath;
        
        if (needCleanAptCache) {
            pkexecArgs << "--apt-cache";
        }
        if (needCleanSystemLogs) {
            pkexecArgs << "--system-logs";
        }
        if (!privilegedDeletePaths.isEmpty()) {
            pkexecArgs << "--delete" << privilegedDeletePaths;
        }
        
        LOG_INFO(QString("Executing privileged cleanup: pkexec %1").arg(pkexecArgs.join(" ")));
        
        QProcess process;
        process.start("pkexec", pkexecArgs);
        process.waitForFinished(120000);  // 2分钟超时
        
        if (process.exitCode() == 0) {
            // 计算释放空间并记录成功清理的项目
            for (const ScanResult &item : privilegedItems) {
                totalFreed += item.size;
                successCount++;
                cleanedItems.append(item);  // 记录成功清理的项目
            }
            LOG_INFO("Privileged cleanup completed successfully");
        } else {
            QString error = process.readAllStandardError();
            LOG_ERROR(QString("Privileged cleanup failed: %1").arg(error));
            for (const ScanResult &item : privilegedItems) {
                Q_UNUSED(item);
                failCount++;
            }
        }
    }
    
    // 第三步：卸载玲珑应用（需要通过 pkexec 执行 ll-cli）
    if (!linglongItems.isEmpty()) {
        m_statusLabel->setText(tr("正在卸载玲珑应用..."));
        if (m_progressDialog) {
            m_progressDialog->updateProgress(tr("卸载玲珑应用"), 95);
        }
        QApplication::processEvents();
        
        int linglongIndex = 0;
        for (const ScanResult &item : linglongItems) {
            linglongIndex++;
            QString appId = item.appId.isEmpty() ? item.name : item.appId;
            QString progressMsg = tr("卸载玲珑应用 (%1/%2): %3")
                .arg(linglongIndex).arg(linglongItems.size()).arg(item.name);
            m_statusLabel->setText(progressMsg);
            if (m_progressDialog) {
                m_progressDialog->updateProgress(item.name, 
                    95 + (linglongIndex * 5 / linglongItems.size()));
            }
            QApplication::processEvents();
            
            LOG_INFO(QString("Uninstalling linglong app: %1 (appId: %2)")
                .arg(item.name, appId));
            
            // 使用 pkexec 执行 ll-cli uninstall
            QProcess llProcess;
            llProcess.start("pkexec", QStringList() << "ll-cli" << "uninstall" << appId);
            llProcess.waitForFinished(120000);  // 2分钟超时
            
            QString stdOutput = llProcess.readAllStandardOutput();
            QString errOutput = llProcess.readAllStandardError();
            
            LOG_INFO(QString("ll-cli uninstall output: %1").arg(stdOutput));
            if (!errOutput.isEmpty()) {
                LOG_ERROR(QString("ll-cli uninstall error: %1").arg(errOutput));
            }
            
            if (llProcess.exitCode() == 0) {
                totalFreed += item.size;
                successCount++;
                cleanedItems.append(item);
                LOG_INFO(QString("Linglong app uninstalled successfully: %1").arg(appId));
            } else {
                failCount++;
                LOG_ERROR(QString("Failed to uninstall linglong app: %1, exit code: %2")
                    .arg(appId).arg(llProcess.exitCode()));
            }
        }
    }
    
    // 清理完成
    m_progressBar->setValue(100);
    
    if (m_progressDialog) {
        m_progressDialog->close();
        delete m_progressDialog;
        m_progressDialog = nullptr;
    }
    
    // 从列表中移除已清理的项目
    if (!cleanedItems.isEmpty()) {
        m_analyzeWidget->removeCleanedItems(cleanedItems);
    }
    
    QString resultText = tr("清理完成！\n\n成功: %1 项\n失败: %2 项\n释放空间: %3")
        .arg(successCount)
        .arg(failCount)
        .arg(formatSize(totalFreed));
    
    m_statusLabel->setText(tr("清理完成，释放空间: %1").arg(formatSize(totalFreed)));
    m_progressBar->setVisible(false);
    
    LOG_INFO(QString("Analyze cleanup completed. Success: %1, Failed: %2, Freed: %3")
        .arg(successCount).arg(failCount).arg(totalFreed));
    
    QMessageBox::information(this, tr("清理完成"), resultText);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    LOG_INFO("Main window closing");
    
    // 保存配置
    Config::instance()->save();
    
    QMainWindow::closeEvent(event);
}

QString MainWindow::formatSize(qint64 bytes)
{
    const qint64 KB = 1024;
    const qint64 MB = 1024 * KB;
    const qint64 GB = 1024 * MB;
    const qint64 TB = 1024 * GB;
    
    if (bytes >= TB) {
        return QString("%1 TB").arg(bytes / (double)TB, 0, 'f', 2);
    } else if (bytes >= GB) {
        return QString("%1 GB").arg(bytes / (double)GB, 0, 'f', 2);
    } else if (bytes >= MB) {
        return QString("%1 MB").arg(bytes / (double)MB, 0, 'f', 2);
    } else if (bytes >= KB) {
        return QString("%1 KB").arg(bytes / (double)KB, 0, 'f', 2);
    } else {
        return QString("%1 B").arg(bytes);
    }
}