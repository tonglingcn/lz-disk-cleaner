/*
 * Main Window - Implementation
 * 主窗口 - 实现
 */

#include "mainwindow.h"
#include "analyzewidget.h"
#include "sponsordialog.h"
#include "../core/systeminfo.h"
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
#include <QShowEvent>
#include <QWindowStateChangeEvent>
#include <QTimer>
#include <QCursor>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QTime>
#include <QScreen>
#include <QGuiApplication>
#include <QElapsedTimer>
#include <QRandomGenerator>

#ifdef HAVE_XCB
#include <xcb/xcb.h>
#endif

#ifdef HAVE_X11
// X11头文件定义了一些与Qt冲突的宏，需要先取消定义
#ifdef KeyPress
#undef KeyPress
#endif
#ifdef KeyRelease
#undef KeyRelease
#endif
#ifdef FocusIn
#undef FocusIn
#endif
#ifdef FocusOut
#undef FocusOut
#endif
#include <X11/Xlib.h>
#include <X11/Xatom.h>
// X11头文件包含后再次取消定义，防止后续Qt代码冲突
#ifdef KeyPress
#undef KeyPress
#endif
#ifdef KeyRelease
#undef KeyRelease
#endif
#ifdef FocusIn
#undef FocusIn
#endif
#ifdef FocusOut
#undef FocusOut
#endif
#endif

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_tabWidget(nullptr)
    , m_dashboardWidget(nullptr)
    , m_analyzeWidget(nullptr)
    , m_resourcesWidget(nullptr)
    , m_fileShredderWidget(nullptr)
    , m_systemSlimmerWidget(nullptr)
    , m_startupAppsWidget(nullptr)
    , m_aptSourceManagerWidget(nullptr)
    , m_cleanupHistoryWidget(nullptr)
    , m_analyzeButton(nullptr)
    , m_smartCleanupButton(nullptr)
    , m_customCleanupButton(nullptr)
    , m_statusLabel(nullptr)
    , m_progressBar(nullptr)
    , m_analyzer(nullptr)
    , m_cleaner(nullptr)
    , m_cleanupDialog(nullptr)
    , m_progressDialog(nullptr)
    , m_wasMaximized(false)
    , m_stateCheckTimer(nullptr)
    , m_lastX11MaximizedState(false)
    , m_lastGeometry(0, 0, 0, 0)
    , m_restoreHoverTimer(nullptr)
    , m_restoreHoverTriggered(false)
    , m_sponsorTimer(nullptr)
    , m_sponsorShowCount(0)
    , m_trayIcon(nullptr)
    , m_trayMenu(nullptr)
    , m_canClose(false)
{
    LOG_INFO("Initializing main window");
    
    initUI();
    connectSignals();
    applyTheme();
    createTrayIcon();  // 创建系统托盘
    
    LOG_INFO("Main window initialized");
}

MainWindow::~MainWindow()
{
    LOG_INFO("Destroying main window");
}

void MainWindow::initUI()
{
    // 获取发行版名称，设置动态标题
    SystemInfo sysInfo;
    QString distroName = sysInfo.getDistroName();
    setWindowTitle(tr("磁盘清理工具-%1版").arg(distroName));
    setMinimumSize(900, 700);
    
    // 根据屏幕分辨率智能设置窗口大小
    QScreen *screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect screenGeometry = screen->availableGeometry();
        int screenWidth = screenGeometry.width();
        int screenHeight = screenGeometry.height();
        
        LOG_INFO(QString("Screen available size: %1x%2").arg(screenWidth).arg(screenHeight));
        
        // 如果屏幕较小（宽度 <= 1366 或高度 <= 768），使用最大化
        if (screenWidth <= 1366 || screenHeight <= 768) {
            LOG_INFO("Small screen detected, will maximize on show");
            // 先设置一个合理的初始大小，稍后在showEvent中最大化
            resize(screenWidth - 100, screenHeight - 100);
        } else {
            // 屏幕较大，使用固定大小 1100x780
            resize(1100, 780);
            LOG_INFO("Large screen detected, using default size: 1100x780");
        }
    } else {
        // 无法获取屏幕信息，使用默认大小
        resize(1100, 780);
        LOG_WARNING("Could not get screen info, using default size");
    }
    
    // 创建菜单栏
    createMenuBar();
    
    // 创建中心部件
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    // 主布局
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);
    
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
    
    // 自启动管理页面
    m_startupAppsWidget = new StartupAppsWidget(this);
    m_tabWidget->addTab(m_startupAppsWidget, tr("自启动"));
    
    // APT 源管理页面
    m_aptSourceManagerWidget = new APTSourceManagerWidget(this);
    m_tabWidget->addTab(m_aptSourceManagerWidget, tr("源管理"));
    
    // 清理历史页面
    m_cleanupHistoryWidget = new CleanupHistoryWidget(this);
    m_tabWidget->addTab(m_cleanupHistoryWidget, tr("清理历史"));
    
    // 连接各组件的历史变化信号，自动刷新清理历史
    connect(m_fileShredderWidget, &FileShredderWidget::historyChanged, 
            m_cleanupHistoryWidget, &CleanupHistoryWidget::refresh);
    connect(m_systemSlimmerWidget, &SystemSlimmerWidget::historyChanged, 
            m_cleanupHistoryWidget, &CleanupHistoryWidget::refresh);
    connect(m_startupAppsWidget, &StartupAppsWidget::historyChanged, 
            m_cleanupHistoryWidget, &CleanupHistoryWidget::refresh);
    
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
    QAction *exitAction = fileMenu->addAction(tr("退出(&X)"), QKeySequence::Quit, this, &QWidget::close);
    exitAction->setIcon(QIcon(":/icons/about.svg"));
    
    // 查看菜单 - 添加窗口控制选项
    QMenu *viewMenu = menuBar->addMenu(tr("查看(&V)"));
    QAction *maximizeAction = viewMenu->addAction(tr("最大化(&M)"), this, [this]() {
        if (isMaximized()) {
            showNormal();
            
            // 还原到固定的初始大小并居中
            QScreen *screen = QGuiApplication::primaryScreen();
            if (screen) {
                QRect screenGeometry = screen->availableGeometry();
                
                // 固定的窗口大小
                int windowWidth = 1100;
                int windowHeight = 780;
                
                // 计算居中位置
                int x = (screenGeometry.width() - windowWidth) / 2 + screenGeometry.x();
                int y = (screenGeometry.height() - windowHeight) / 2 + screenGeometry.y();
                
                // 设置固定的几何信息
                setGeometry(x, y, windowWidth, windowHeight);
                
                // 更新保存的正常几何信息
                m_normalGeometry = geometry();
            } else if (!m_normalGeometry.isNull()) {
                setGeometry(m_normalGeometry);
            }
        } else {
            showMaximized();
        }
    });
    maximizeAction->setShortcut(QKeySequence(Qt::Key_F11));
    
    QAction *restoreAction = viewMenu->addAction(tr("还原窗口(&R)"), this, [this]() {
        LOG_INFO("Manual restore triggered");
        showNormal();
        
        // 还原到固定的初始大小并居中
        QScreen *screen = QGuiApplication::primaryScreen();
        if (screen) {
            QRect screenGeometry = screen->availableGeometry();
            
            // 固定的窗口大小
            int windowWidth = 1100;
            int windowHeight = 780;
            
            // 计算居中位置
            int x = (screenGeometry.width() - windowWidth) / 2 + screenGeometry.x();
            int y = (screenGeometry.height() - windowHeight) / 2 + screenGeometry.y();
            
            // 设置固定的几何信息
            setGeometry(x, y, windowWidth, windowHeight);
            
            // 更新保存的正常几何信息
            m_normalGeometry = geometry();
            
            LOG_INFO(QString("Manually restored to fixed size: x=%1, y=%2, w=%3, h=%4")
                .arg(x).arg(y).arg(windowWidth).arg(windowHeight));
        } else {
            // 无法获取屏幕信息，使用默认位置
            resize(1100, 780);
            if (!m_normalGeometry.isNull()) {
                setGeometry(m_normalGeometry);
            }
        }
    });
    restoreAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
    
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
    
    // 添加赞助按钮到状态栏右下角（初始隐藏）
    m_sponsorButton = new QPushButton(tr("💝 赞助支持"), this);
    m_sponsorButton->setFlat(true);
    m_sponsorButton->setCursor(Qt::PointingHandCursor);
    m_sponsorButton->setStyleSheet(
        "QPushButton {"
        "  color: #ff6b6b;"
        "  padding: 2px 8px;"
        "  border-radius: 3px;"
        "}"
        "QPushButton:hover {"
        "  background-color: rgba(255, 107, 107, 0.1);"
        "}"
    );
    connect(m_sponsorButton, &QPushButton::clicked, this, &MainWindow::onSponsorClicked);
    m_sponsorButton->setVisible(false);  // 初始隐藏
    statusBar->addPermanentWidget(m_sponsorButton);
    
    // 设置赞助按钮随机显示定时器
    setupSponsorTimer();
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
    // 只隐藏 TabWidget 左边的边框线，保留上边框和标签样式
    QString tabWidgetStyle = R"(
        QTabWidget::pane {
            border-left: none;
        }
    )";
    
    m_tabWidget->setStyleSheet(tabWidgetStyle);
    
    // 使用系统默认主题
    qApp->setStyleSheet("");
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
        // 获取用户选中的项目
        QStringList selectedItems = m_cleanupDialog->getSelectedItems();
        qint64 totalSize = m_cleanupDialog->getTotalSelectedSize();
        
        if (selectedItems.isEmpty()) {
            QMessageBox::information(this, tr("提示"), tr("未选择任何项目"));
            return;
        }
        
        // 确认清理
        QString message = tr("确定要清理选中的 %1 个项目吗？\n预计释放空间: %2")
            .arg(selectedItems.size())
            .arg(formatSize(totalSize));
        
        int ret = QMessageBox::warning(this, tr("确认清理"), message,
                                      QMessageBox::Yes | QMessageBox::No);
        
        if (ret != QMessageBox::Yes) {
            return;
        }
        
        // 创建进度对话框
        m_progressDialog = new ProgressDialog(this);
        m_progressDialog->setWindowTitle(tr("正在清理"));
        m_progressDialog->show();
        
        // 执行清理
        qint64 freedSpace = 0;
        int successCount = 0;
        int failedCount = 0;
        
        for (int i = 0; i < selectedItems.size(); ++i) {
            const QString &itemPath = selectedItems.at(i);
            
            // 更新进度
            int percent = (i * 100) / selectedItems.size();
            m_progressDialog->updateProgress(tr("正在清理: %1").arg(itemPath), percent);
            QApplication::processEvents();
            
            // 获取大小
            QFileInfo info(itemPath);
            qint64 itemSize = 0;
            
            if (info.isDir()) {
                // 目录：使用 du 命令获取大小
                QProcess duProcess;
                duProcess.start("du", QStringList() << "-sb" << itemPath);
                duProcess.waitForFinished(10000);
                if (duProcess.exitCode() == 0) {
                    QString output = QString::fromUtf8(duProcess.readAllStandardOutput());
                    itemSize = output.split('\t').first().toLongLong();
                }
            } else {
                itemSize = info.size();
            }
            
            // 执行删除
            bool success = false;
            if (info.isDir()) {
                QDir dir(itemPath);
                success = dir.removeRecursively();
            } else {
                success = QFile::remove(itemPath);
            }
            
            if (success) {
                freedSpace += itemSize;
                successCount++;
                LOG_INFO(QString("Cleaned: %1 (%2 bytes)").arg(itemPath).arg(itemSize));
            } else {
                failedCount++;
                LOG_ERROR(QString("Failed to clean: %1").arg(itemPath));
            }
        }
        
        m_progressDialog->close();
        delete m_progressDialog;
        m_progressDialog = nullptr;
        
        // 显示结果
        QString resultMessage = tr("清理完成！\n"
                                  "成功: %1 项\n"
                                  "失败: %2 项\n"
                                  "释放空间: %3")
            .arg(successCount)
            .arg(failedCount)
            .arg(formatSize(freedSpace));
        
        QMessageBox::information(this, tr("清理结果"), resultMessage);
        
        LOG_INFO(QString("Custom cleanup finished. Freed: %1 bytes, Success: %2, Failed: %3")
                .arg(freedSpace).arg(successCount).arg(failedCount));
    }
}

void MainWindow::onSettingsClicked()
{
    LOG_INFO("Settings button clicked");
    
    SettingsDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        // 应用主题变更
        applyTheme();
        LOG_INFO("Settings applied");
    }
}

void MainWindow::onSponsorClicked()
{
    LOG_INFO("Sponsor button clicked");
    SponsorDialog dialog(this);
    dialog.exec();
    
    // 点击后隐藏按钮，重新启动随机定时器
    m_sponsorButton->setVisible(false);
    m_sponsorShowCount++;
    setupSponsorTimer();
}

void MainWindow::setupSponsorTimer()
{
    // 删除旧定时器
    if (m_sponsorTimer) {
        m_sponsorTimer->stop();
        delete m_sponsorTimer;
        m_sponsorTimer = nullptr;
    }
    
    m_sponsorTimer = new QTimer(this);
    m_sponsorTimer->setSingleShot(true);
    
    connect(m_sponsorTimer, &QTimer::timeout, this, [this]() {
        // 显示赞助按钮
        m_sponsorButton->setVisible(true);
        m_sponsorShowCount++;
        
        // 显示60秒后自动隐藏
        QTimer::singleShot(60000, this, [this]() {
            if (m_sponsorButton->isVisible()) {
                m_sponsorButton->setVisible(false);
                // 重新启动随机定时器
                setupSponsorTimer();
            }
        });
    });
    
    // 计算延迟时间
    int delayMs;
    if (m_sponsorShowCount == 0) {
        // 首次：启动30秒后显示
        delayMs = 30000;
    } else {
        // 之后：随机5-15分钟显示
        delayMs = (300 + QRandomGenerator::global()->bounded(600)) * 1000;
    }
    
    m_sponsorTimer->start(delayMs);
}

void MainWindow::onAboutClicked()
{
    LOG_INFO("About button clicked");

    // 获取发行版名称
    SystemInfo sysInfo;
    QString distroName = sysInfo.getDistroName();

    QMessageBox::about(
        this,
        tr("关于"),
        tr("<h3>磁盘清理工具-%1版</h3>"
           "<p><b>版本号：</b>v1.1.1</p>"
           "<p>一个专为 %2 系统设计的磁盘清理工具。</p>"
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
           "<p align=\"center\">Copyright © 2026 克亮 UOS-AI</p>")
        .arg(distroName)
        .arg(distroName)
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
    int successCount = 0;
    int failCount = 0;
    QStringList detailItems;
    
    for (const CleanupResult &result : results) {
        totalFreed += result.freedSpace;
        if (result.success) {
            successCount++;
        } else {
            failCount++;
        }
        QString statusIcon = result.success ? "✓" : "✗";
        QString sizeText = result.success ? formatSize(result.freedSpace) : result.errorMessage;
        detailText += QString("%1 %2: %3\n").arg(statusIcon, result.itemName, sizeText);
        detailItems.append(QString("%1 %2: %3").arg(statusIcon, result.itemName, sizeText));
    }
    
    QString freedText = formatSize(totalFreed);
    
    // 记录清理历史
    CleanupHistoryWidget::addHistory(tr("智能清理"), totalFreed, successCount, failCount, detailItems);
    if (m_cleanupHistoryWidget) {
        m_cleanupHistoryWidget->refresh();
    }
    
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
        
        // 从配置获取参数
        Config *config = Config::instance();
        int keepDays = config->getJournalKeepDays();
        int keepCount = config->getSnapshotKeepCount();
        
        // 判断是否需要清理 journal 日志或系统快照
        bool needCleanJournal = false;
        bool needCleanSnapshot = false;
        for (const ScanResult &item : privilegedItems) {
            if (item.category == ScanCategory::JOURNAL_LOGS) {
                needCleanJournal = true;
            }
            if (item.category == ScanCategory::IMMUTABLE_SNAPSHOTS) {
                needCleanSnapshot = true;
            }
        }
        
        QStringList pkexecArgs;
        pkexecArgs << helperPath;
        
        // 传递日志保留天数参数
        pkexecArgs << "--keep-days" << QString::number(keepDays);
        pkexecArgs << "--keep-count" << QString::number(keepCount);
        
        if (needCleanAptCache) {
            pkexecArgs << "--apt-cache";
        }
        if (needCleanSystemLogs) {
            pkexecArgs << "--system-logs";
        }
        if (needCleanJournal) {
            pkexecArgs << "--journal";
        }
        if (needCleanSnapshot) {
            pkexecArgs << "--snapshot";
        }
        
        // 收集需要删除的路径（排除 journal 和 snapshot 类型，它们通过专用选项处理）
        QStringList deletePaths;
        for (const QString &path : privilegedDeletePaths) {
            deletePaths.append(path);
        }
        
        if (!deletePaths.isEmpty()) {
            pkexecArgs << "--delete" << deletePaths;
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
    
    // 收集详细清理项目
    QStringList detailItems;
    for (const ScanResult &item : cleanedItems) {
        QString statusIcon = "✓";
        detailItems.append(QString("%1 %2: %3").arg(statusIcon, item.name, formatSize(item.size)));
    }
    
    QString resultText = tr("清理完成！\n\n成功: %1 项\n失败: %2 项\n释放空间: %3")
        .arg(successCount)
        .arg(failCount)
        .arg(formatSize(totalFreed));
    
    // 记录清理历史
    if (successCount > 0 || failCount > 0) {
        CleanupHistoryWidget::addHistory(tr("磁盘分析清理"), totalFreed, successCount, failCount, detailItems);
        if (m_cleanupHistoryWidget) {
            m_cleanupHistoryWidget->refresh();
        }
    }
    
    m_statusLabel->setText(tr("清理完成，释放空间: %1").arg(formatSize(totalFreed)));
    m_progressBar->setVisible(false);
    
    LOG_INFO(QString("Analyze cleanup completed. Success: %1, Failed: %2, Freed: %3")
        .arg(successCount).arg(failCount).arg(totalFreed));
    
    QMessageBox::information(this, tr("清理完成"), resultText);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    LOG_INFO("Main window closing");
    
    // 如果允许关闭（从托盘退出），则正常关闭
    if (m_canClose) {
        // 保存配置
        Config::instance()->save();
        QMainWindow::closeEvent(event);
        return;
    }
    
    // 否则最小化到托盘
    event->ignore();
    hide();
    
    // 首次最小化时显示提示
    static bool firstTime = true;
    if (firstTime && m_trayIcon) {
        m_trayIcon->showMessage(
            tr("磁盘清理工具"),
            tr("程序已最小化到系统托盘，点击图标可恢复窗口"),
            QSystemTrayIcon::Information,
            2000
        );
        firstTime = false;
    }
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    
    // 首次显示时处理窗口位置和大小
    static bool firstShow = true;
    if (firstShow) {
        firstShow = false;
        
        // 获取屏幕几何信息
        QScreen *screen = QGuiApplication::primaryScreen();
        if (screen) {
            QRect screenGeometry = screen->availableGeometry();
            int screenWidth = screenGeometry.width();
            int screenHeight = screenGeometry.height();
            
            // 如果屏幕较小，最大化窗口
            if (screenWidth <= 1366 || screenHeight <= 768) {
                LOG_INFO("Maximizing window for small screen");
                showMaximized();
            } else {
                // 屏幕较大，居中显示
                int x = (screenGeometry.width() - width()) / 2 + screenGeometry.x();
                int y = (screenGeometry.height() - height()) / 2 + screenGeometry.y();
                move(x, y);
                
                LOG_INFO(QString("Window centered at: x=%1, y=%2").arg(x).arg(y));
            }
        }
        
        // 启动DDE窗口状态检测定时器
        // 这是解决DDE窗口最大化后无法还原问题的关键workaround
        m_stateCheckTimer = new QTimer(this);
        connect(m_stateCheckTimer, &QTimer::timeout, this, &MainWindow::onWindowStateCheck);
        m_stateCheckTimer->start(100);  // 每100ms检测一次
        LOG_INFO("DDE window state check timer started (100ms interval)");
        
        // 启动还原按钮悬停检测定时器
        // 当鼠标悬停在还原按钮区域1秒后，自动触发F11还原
        m_restoreHoverTimer = new QTimer(this);
        connect(m_restoreHoverTimer, &QTimer::timeout, this, &MainWindow::checkRestoreButtonHover);
        m_restoreHoverTimer->start(200);  // 每200ms检测一次鼠标位置
        LOG_INFO("Restore button hover detection timer started (200ms interval)");
    }
    
    // 保存初始几何信息
    if (m_normalGeometry.isNull() && !isMaximized()) {
        m_normalGeometry = geometry();
        LOG_INFO(QString("Saved initial geometry: x=%1, y=%2, w=%3, h=%4")
            .arg(m_normalGeometry.x()).arg(m_normalGeometry.y())
            .arg(m_normalGeometry.width()).arg(m_normalGeometry.height()));
    }
    
    // 在窗口显示后，确保 QWindow 设置正确支持最大化/还原
    if (windowHandle()) {
        // 设置窗口标志，确保支持最大化/还原
        Qt::WindowStates states = windowHandle()->windowStates();
        LOG_INFO(QString("Initial window states: %1").arg(static_cast<int>(states)));
        
        // 确保窗口管理器正确识别窗口类型
        windowHandle()->setFlags(windowHandle()->flags() | Qt::Window);
    }
}

void MainWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::WindowStateChange) {
        QWindowStateChangeEvent *stateEvent = static_cast<QWindowStateChangeEvent*>(event);
        Qt::WindowStates oldState = stateEvent->oldState();
        Qt::WindowStates newState = windowState();
        
        LOG_INFO(QString("Window state changed - isMaximized: %1, isMinimized: %2, isFullScreen: %3")
            .arg(isMaximized() ? "true" : "false")
            .arg(isMinimized() ? "true" : "false")
            .arg(isFullScreen() ? "true" : "false"));
        LOG_INFO(QString("State transition: %1 -> %2")
            .arg(static_cast<int>(oldState))
            .arg(static_cast<int>(newState)));
        
        // 保存正常状态的几何信息
        if (!(oldState & Qt::WindowMaximized) && !(oldState & Qt::WindowMinimized) && 
            !(oldState & Qt::WindowFullScreen)) {
            m_normalGeometry = geometry();
            LOG_INFO(QString("Saved normal geometry: x=%1, y=%2, w=%3, h=%4")
                .arg(m_normalGeometry.x()).arg(m_normalGeometry.y())
                .arg(m_normalGeometry.width()).arg(m_normalGeometry.height()));
        }
        
        // 从最大化状态切换到正常状态
        if ((oldState & Qt::WindowMaximized) && !(newState & Qt::WindowMaximized) && 
            !(newState & Qt::WindowMinimized)) {
            LOG_INFO("Restoring from maximized state");
            
            // 强制恢复到保存的几何信息
            if (!m_normalGeometry.isNull()) {
                QTimer::singleShot(0, this, [this]() {
                    setGeometry(m_normalGeometry);
                    LOG_INFO(QString("Restored to geometry: x=%1, y=%2, w=%3, h=%4")
                        .arg(m_normalGeometry.x()).arg(m_normalGeometry.y())
                        .arg(m_normalGeometry.width()).arg(m_normalGeometry.height()));
                });
            }
        }
        
        m_wasMaximized = isMaximized();
    }
    QMainWindow::changeEvent(event);
}

bool MainWindow::event(QEvent *event)
{
    // 捕获标题栏双击事件
    if (event->type() == QEvent::NonClientAreaMouseButtonDblClick) {
        LOG_INFO("Title bar double-clicked");
        
        // 切换最大化/正常状态
        if (isMaximized()) {
            LOG_INFO("Restoring from maximized via double-click");
            showNormal();
            if (!m_normalGeometry.isNull()) {
                setGeometry(m_normalGeometry);
            }
        } else {
            LOG_INFO("Maximizing via double-click");
            showMaximized();
        }
        return true;
    }
    
    // 捕获标题栏按钮点击
    if (event->type() == QEvent::NonClientAreaMouseButtonPress) {
        if (isMaximized()) {
            // 获取当前鼠标位置
            QPoint globalPos = QCursor::pos();
            QPoint localPos = mapFromGlobal(globalPos);
            
            LOG_INFO(QString("Non-client area clicked at: x=%1, y=%2 (window width=%3)")
                .arg(localPos.x()).arg(localPos.y()).arg(width()));
            
            // 检查是否点击了还原按钮区域
            // 标题栏在窗口上方（y < 0），还原按钮在右上角
            int rightEdge = width();
            bool clickedRestoreButton = (localPos.y() < 0 && localPos.y() > -40 && 
                                        localPos.x() > rightEdge - 90 && localPos.x() < rightEdge - 30);
            
            LOG_INFO(QString("Clicked restore button area: %1").arg(clickedRestoreButton ? "YES" : "NO"));
            
            if (clickedRestoreButton) {
                LOG_INFO("Restore button clicked, triggering F11 in 100ms");
                // 延迟触发，确保点击事件处理完成
                QTimer::singleShot(100, this, [this]() {
                    if (isMaximized()) {
                        LOG_INFO("Executing F11 key press");
                        QKeyEvent pressEvent(QEvent::KeyPress, Qt::Key_F11, Qt::NoModifier);
                        QApplication::sendEvent(this, &pressEvent);
                        QKeyEvent releaseEvent(QEvent::KeyRelease, Qt::Key_F11, Qt::NoModifier);
                        QApplication::sendEvent(this, &releaseEvent);
                    } else {
                        LOG_INFO("Window already restored, skipping F11");
                    }
                });
            }
        }
    }
    
    return QMainWindow::event(event);
}

bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
#ifdef HAVE_X11
    // 处理X11事件 - 这是解决DDE窗口最大化还原问题的关键
    // DDE窗口管理器的还原按钮点击不会触发Qt的WindowStateChange事件
    // 我们需要监听底层的X11属性变化事件
    if (eventType == "xcb_generic_event_t") {
        xcb_generic_event_t *xcbEvent = static_cast<xcb_generic_event_t*>(message);
        uint8_t responseType = xcbEvent->response_type & ~0x80;
        
        if (responseType == XCB_PROPERTY_NOTIFY) {
            xcb_property_notify_event_t *propEvent = 
                reinterpret_cast<xcb_property_notify_event_t*>(xcbEvent);
            
            // 获取当前窗口的X11窗口ID
            Window winIdX11 = static_cast<Window>(winId());
            
            // 检查是否是当前窗口的属性变化
            if (propEvent->window == winIdX11) {
                Display *display = XOpenDisplay(nullptr);
                if (display) {
                    // 获取 _NET_WM_STATE 原子
                    static Atom netWmState = None;
                    static Atom netWmStateMaxVert = None;
                    static Atom netWmStateMaxHorz = None;
                    static bool atomsInitialized = false;
                    
                    if (!atomsInitialized) {
                        atomsInitialized = true;
                        netWmState = XInternAtom(display, "_NET_WM_STATE", True);
                        netWmStateMaxVert = XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_VERT", True);
                        netWmStateMaxHorz = XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_HORZ", True);
                        LOG_INFO(QString("X11 atoms initialized: _NET_WM_STATE=%1, MAX_VERT=%2, MAX_HORZ=%3")
                            .arg(netWmState).arg(netWmStateMaxVert).arg(netWmStateMaxHorz));
                    }
                    
                    // 检查是否是 _NET_WM_STATE 属性变化
                    if (static_cast<Atom>(propEvent->atom) == netWmState) {
                        // 查询窗口属性
                        Atom actualType;
                        int actualFormat;
                        unsigned long numItems, bytesAfter;
                        unsigned char *propData = nullptr;
                        
                        bool hasMaxVert = false;
                        bool hasMaxHorz = false;
                        
                        if (XGetWindowProperty(display, winIdX11, netWmState, 0, 1024, 
                                               False, XA_ATOM, &actualType, &actualFormat,
                                               &numItems, &bytesAfter, &propData) == Success) {
                            if (actualType == XA_ATOM && propData) {
                                Atom *atoms = reinterpret_cast<Atom*>(propData);
                                for (unsigned long i = 0; i < numItems; i++) {
                                    if (atoms[i] == netWmStateMaxVert) hasMaxVert = true;
                                    if (atoms[i] == netWmStateMaxHorz) hasMaxHorz = true;
                                }
                                XFree(propData);
                            }
                        }
                        
                        bool currentlyMaximized = hasMaxVert && hasMaxHorz;
                        
                        LOG_INFO(QString("X11 state: hasMaxVert=%1, hasMaxHorz=%2, isMaximized=%3, wasMaximized=%4, QtMaximized=%5")
                            .arg(hasMaxVert ? "true" : "false")
                            .arg(hasMaxHorz ? "true" : "false")
                            .arg(currentlyMaximized ? "true" : "false")
                            .arg(m_wasMaximized ? "true" : "false")
                            .arg(isMaximized() ? "true" : "false"));
                        
                        // 检测从最大化到非最大化的转换
                        // 使用 Qt 的 isMaximized() 作为参考，因为它反映了当前窗口的实际状态
                        bool qtCurrentlyMaximized = isMaximized();
                        
                        if (m_wasMaximized && !currentlyMaximized && !qtCurrentlyMaximized) {
                            LOG_INFO("Window restore detected via X11! Applying fix.");
                            
                            // 延迟执行，确保窗口管理器完成操作
                            QTimer::singleShot(50, this, [this]() {
                                // 再次确认窗口状态
                                if (!isMaximized() && !m_normalGeometry.isNull()) {
                                    showNormal();
                                    setGeometry(m_normalGeometry);
                                    LOG_INFO(QString("X11 fix: Restored to x=%1, y=%2, w=%3, h=%4")
                                        .arg(m_normalGeometry.x()).arg(m_normalGeometry.y())
                                        .arg(m_normalGeometry.width()).arg(m_normalGeometry.height()));
                                }
                            });
                        }
                        
                        // 只有当状态真正改变时才更新
                        if (currentlyMaximized != m_wasMaximized) {
                            m_wasMaximized = currentlyMaximized;
                        }
                    }
                    
                    XCloseDisplay(display);
                }
            }
        }
    }
#else
    Q_UNUSED(eventType);
    Q_UNUSED(message);
    Q_UNUSED(result);
#endif
    
    return QMainWindow::nativeEvent(eventType, message, result);
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

#ifdef HAVE_X11
// 检查X11窗口状态 - 直接查询_X11窗口属性
bool MainWindow::checkX11WindowState()
{
    Display *display = XOpenDisplay(nullptr);
    if (!display) return isMaximized();
    
    Window winIdX11 = static_cast<Window>(winId());
    
    // 获取原子
    static Atom netWmState = None;
    static Atom netWmStateMaxVert = None;
    static Atom netWmStateMaxHorz = None;
    static bool atomsInitialized = false;
    
    if (!atomsInitialized) {
        atomsInitialized = true;
        netWmState = XInternAtom(display, "_NET_WM_STATE", True);
        netWmStateMaxVert = XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_VERT", True);
        netWmStateMaxHorz = XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_HORZ", True);
    }
    
    bool hasMaxVert = false;
    bool hasMaxHorz = false;
    
    Atom actualType;
    int actualFormat;
    unsigned long numItems, bytesAfter;
    unsigned char *propData = nullptr;
    
    if (XGetWindowProperty(display, winIdX11, netWmState, 0, 1024, 
                           False, XA_ATOM, &actualType, &actualFormat,
                           &numItems, &bytesAfter, &propData) == Success) {
        if (actualType == XA_ATOM && propData) {
            Atom *atoms = reinterpret_cast<Atom*>(propData);
            for (unsigned long i = 0; i < numItems; i++) {
                if (atoms[i] == netWmStateMaxVert) hasMaxVert = true;
                if (atoms[i] == netWmStateMaxHorz) hasMaxHorz = true;
            }
            XFree(propData);
        }
    }
    
    XCloseDisplay(display);
    return hasMaxVert && hasMaxHorz;
}
#else
bool MainWindow::checkX11WindowState()
{
    return isMaximized();
}
#endif

// DDE窗口状态检测定时器回调
// 这是解决DDE窗口最大化后无法还原问题的关键workaround
void MainWindow::onWindowStateCheck()
{
#ifdef HAVE_X11
    // 通过X11直接查询窗口状态
    bool x11Maximized = checkX11WindowState();
    bool qtMaximized = isMaximized();
    QRect currentGeo = geometry();
    
    // 检测状态变化：从最大化变为非最大化
    if (m_lastX11MaximizedState && !x11Maximized) {
        LOG_INFO(QString("DDE TIMER: Restore detected via X11 state! X11=%1, Qt=%2")
            .arg(x11Maximized ? "max" : "normal")
            .arg(qtMaximized ? "max" : "normal"));
        
        // 触发还原
        QTimer::singleShot(10, this, [this]() {
            if (!m_normalGeometry.isNull()) {
                showNormal();
                setGeometry(m_normalGeometry);
                LOG_INFO(QString("DDE TIMER FIX: Restored to x=%1, y=%2, w=%3, h=%4")
                    .arg(m_normalGeometry.x()).arg(m_normalGeometry.y())
                    .arg(m_normalGeometry.width()).arg(m_normalGeometry.height()));
            }
        });
        
        m_wasMaximized = false;
    }
    // 方案2：检测几何变化 - 当窗口从最大化状态开始改变大小时
    else if (m_wasMaximized && !m_lastGeometry.isNull()) {
        // 如果几何信息发生变化，且不再是全屏大小
        if (currentGeo != m_lastGeometry && !x11Maximized) {
            QScreen *screen = QGuiApplication::primaryScreen();
            if (screen) {
                QRect screenGeo = screen->availableGeometry();
                // 如果当前几何不是屏幕大小，说明正在还原
                if (currentGeo.width() < screenGeo.width() - 10 || 
                    currentGeo.height() < screenGeo.height() - 10) {
                    LOG_INFO(QString("DDE TIMER: Restore detected via geometry change! geo=%1,%2 %3x%4")
                        .arg(currentGeo.x()).arg(currentGeo.y())
                        .arg(currentGeo.width()).arg(currentGeo.height()));
                    
                    // 强制完成还原
                    QTimer::singleShot(10, this, [this]() {
                        showNormal();
                        if (!m_normalGeometry.isNull()) {
                            setGeometry(m_normalGeometry);
                        }
                        m_wasMaximized = false;
                    });
                }
            }
        }
    }
    
    // 更新状态
    m_lastX11MaximizedState = x11Maximized;
    m_wasMaximized = x11Maximized;
    m_lastGeometry = currentGeo;
#else
    Q_UNUSED(this);
#endif
}

// DDE还原按钮悬停检测
// 当鼠标悬停在标题栏还原按钮区域约1秒后，自动触发F11还原
void MainWindow::checkRestoreButtonHover()
{
    // 只在最大化状态下检测
    if (!isMaximized()) {
        m_restoreHoverTriggered = false;
        return;
    }
    
    // 获取全局鼠标位置
    QPoint globalMousePos = QCursor::pos();
    QPoint localMousePos = mapFromGlobal(globalMousePos);
    
    // 计算还原按钮区域
    // DDE标题栏还原按钮大约在窗口右上角，宽度约40-50像素，高度约30-40像素
    int windowWidth = width();
    
    // 还原按钮区域：右上角，x从width-90到width-30，y从-40到0（标题栏在窗口上方）
    int buttonLeft = windowWidth - 100;
    int buttonRight = windowWidth - 40;
    int buttonTop = -45;
    int buttonBottom = -5;
    
    bool inRestoreButtonArea = (localMousePos.x() >= buttonLeft && 
                                 localMousePos.x() <= buttonRight &&
                                 localMousePos.y() >= buttonTop && 
                                 localMousePos.y() <= buttonBottom);
    
    static QElapsedTimer hoverTimer;
    static bool hoverStarted = false;
    
    if (inRestoreButtonArea) {
        if (!hoverStarted) {
            hoverStarted = true;
            hoverTimer.start();
            LOG_INFO(QString("Mouse entered restore button area: localPos=(%1,%2)")
                .arg(localMousePos.x()).arg(localMousePos.y()));
        } else {
            // 检查是否悬停超过800ms
            if (hoverTimer.elapsed() > 800 && !m_restoreHoverTriggered) {
                m_restoreHoverTriggered = true;
                LOG_INFO("Restore button hover detected! Triggering F11 for restore.");
                
                // 触发F11按键事件
                QKeyEvent pressEvent(QEvent::KeyPress, Qt::Key_F11, Qt::NoModifier);
                QApplication::sendEvent(this, &pressEvent);
                QKeyEvent releaseEvent(QEvent::KeyRelease, Qt::Key_F11, Qt::NoModifier);
                QApplication::sendEvent(this, &releaseEvent);
                
                // 重置状态
                hoverStarted = false;
            }
        }
    } else {
        if (hoverStarted) {
            LOG_INFO("Mouse left restore button area, resetting hover timer");
        }
        hoverStarted = false;
        m_restoreHoverTriggered = false;
    }
    
    m_lastMousePos = globalMousePos;
}

// 系统托盘功能
void MainWindow::createTrayIcon()
{
    // 创建托盘图标
    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setIcon(QIcon(":/icons/app_icon.svg"));
    m_trayIcon->setToolTip(tr("磁盘清理工具"));
    
    // 创建托盘菜单
    m_trayMenu = new QMenu(this);
    
    QAction *showAction = m_trayMenu->addAction(tr("显示主窗口"));
    showAction->setIcon(QIcon(":/icons/dashboard.svg"));
    connect(showAction, &QAction::triggered, this, &MainWindow::onTrayShowWindow);
    
    QAction *cleanupAction = m_trayMenu->addAction(tr("一键智能清理"));
    cleanupAction->setIcon(QIcon(":/icons/cleanup.svg"));
    connect(cleanupAction, &QAction::triggered, this, &MainWindow::onTrayCleanup);
    
    m_trayMenu->addSeparator();
    
    QAction *quitAction = m_trayMenu->addAction(tr("退出"));
    quitAction->setIcon(QIcon::fromTheme("application-exit"));
    connect(quitAction, &QAction::triggered, this, &MainWindow::onTrayQuit);
    
    m_trayIcon->setContextMenu(m_trayMenu);
    
    // 连接托盘图标激活信号（单击/双击）
    connect(m_trayIcon, &QSystemTrayIcon::activated, this, &MainWindow::onTrayIconActivated);
    
    // 显示托盘图标
    m_trayIcon->show();
    
    LOG_INFO("System tray icon created");
}

void MainWindow::onTrayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason) {
    case QSystemTrayIcon::Trigger:
    case QSystemTrayIcon::DoubleClick:
        // 单击或双击显示窗口
        onTrayShowWindow();
        break;
    default:
        break;
    }
}

void MainWindow::onTrayShowWindow()
{
    show();
    activateWindow();
    raise();
    
    // 如果是最小化状态，恢复窗口
    if (isMinimized()) {
        showNormal();
    }
    
    LOG_INFO("Window restored from system tray");
}

void MainWindow::onTrayCleanup()
{
    // 显示窗口并执行智能清理
    onTrayShowWindow();
    onSmartCleanupClicked();
}

void MainWindow::onTrayQuit()
{
    LOG_INFO("Quitting from system tray");
    
    // 停止所有监控定时器
    if (m_resourcesWidget) {
        m_resourcesWidget->stopMonitoring();
    }
    
    // 停止DDE窗口状态检测定时器
    if (m_stateCheckTimer) {
        m_stateCheckTimer->stop();
    }
    
    // 停止还原按钮悬停检测定时器
    if (m_restoreHoverTimer) {
        m_restoreHoverTimer->stop();
    }
    
    // 停止赞助按钮定时器
    if (m_sponsorTimer) {
        m_sponsorTimer->stop();
    }
    
    // 设置允许关闭标志
    m_canClose = true;
    
    // 隐藏托盘图标
    if (m_trayIcon) {
        m_trayIcon->hide();
    }
    
    // 关闭窗口并退出应用
    close();
    QApplication::quit();
}