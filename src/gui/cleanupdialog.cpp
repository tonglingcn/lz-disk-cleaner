/*
 * Cleanup Dialog - Implementation
 * 自定义清理对话框 - 实现
 */

#include "cleanupdialog.h"
#include "../utils/logger.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QGroupBox>
#include <QMessageBox>
#include <QInputDialog>
#include <QDir>
#include <QFileInfo>
#include <QFileIconProvider>
#include <QApplication>
#include <QPalette>
#include <QRegularExpression>
#include <QProcess>
#include <algorithm>

// ============== PartitionScanThread 实现 ==============

PartitionScanThread::PartitionScanThread(QObject *parent)
    : QThread(parent)
    , m_maxDepth(3)
    , m_stopped(false)
{
}

void PartitionScanThread::setScanPath(const QString &path, int maxDepth)
{
    m_scanPath = path;
    m_maxDepth = maxDepth;
    m_stopped = false;
}

void PartitionScanThread::stop()
{
    m_stopped = true;
}

void PartitionScanThread::run()
{
    LOG_INFO(QString("Starting partition scan: %1").arg(m_scanPath));
    
    QList<ScanItem> items = scanDirectory(m_scanPath, 0);
    
    if (!m_stopped) {
        emit scanFinished(items);
    }
    
    LOG_INFO(QString("Partition scan finished, found %1 items").arg(items.size()));
}

// ==================== CleanupThread 实现 ====================

CleanupThread::CleanupThread(QObject *parent)
    : QThread(parent)
{
}

void CleanupThread::setPathsToDelete(const QStringList &paths)
{
    m_paths = paths;
}

void CleanupThread::run()
{
    int successCount = 0;
    int failCount = 0;
    qint64 freedSize = 0;
    int total = m_paths.size();
    
    for (int i = 0; i < total; ++i) {
        const QString &path = m_paths[i];
        emit cleanupProgress(i + 1, total, path);
        
        QProcess rmProc;
        rmProc.start("rm", QStringList() << "-rf" << path);
        bool finished = rmProc.waitForFinished(120000); // 2分钟超时
        
        // 检查是否真的被删除了（rm 可能因为权限等原因静默失败）
        bool deleted = !QFile::exists(path);
        
        if (deleted || (finished && rmProc.exitCode() == 0)) {
            successCount++;
            // 尝试从已保存的大小数据中获取（如果有的话）
            // 这里无法直接访问外部大小数据，由调用方汇总
        } else {
            failCount++;
            LOG_ERROR(QString("CleanupThread: failed to delete %1").arg(path));
        }
        
        // 短暂休眠，避免CPU占用过高
        if (i < total - 1) {
            msleep(10);
        }
    }
    
    emit cleanupFinished(successCount, failCount, freedSize);
}

QList<ScanItem> PartitionScanThread::scanDirectory(const QString &path, int depth)
{
    QList<ScanItem> items;
    
    if (m_stopped || depth > m_maxDepth) {
        return items;
    }
    
    QDir dir(path);
    if (!dir.exists()) {
        return items;
    }
    
    // 设置过滤器，跳过一些特殊目录
    dir.setFilter(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot | QDir::NoSymLinks);
    
    QFileInfoList entries = dir.entryInfoList();
    
    for (const QFileInfo &info : entries) {
        if (m_stopped) break;
        
        // 跳过系统特殊目录（只在根分区扫描时跳过）
        if (info.isDir()) {
            QString absolutePath = info.absoluteFilePath();
            // 只在根目录级别跳过系统目录
            if (depth == 0) {
                if (absolutePath == "/proc" ||
                    absolutePath == "/sys" ||
                    absolutePath == "/dev" ||
                    absolutePath == "/run" ||
                    absolutePath == "/tmp" ||
                    absolutePath == "/cdrom" ||
                    absolutePath == "/media" ||
                    absolutePath == "/mnt" ||
                    absolutePath == "/snap") {
                    continue;
                }
            }
        }
        
        ScanItem item;
        item.path = info.absoluteFilePath();
        item.name = info.fileName();
        item.isDir = info.isDir();
        
        if (info.isDir()) {
            int fileCount = 0, dirCount = 0;
            item.size = getDirectorySize(info.absoluteFilePath(), &fileCount, &dirCount);
            item.fileCount = fileCount;
            item.dirCount = dirCount;
        } else {
            item.size = info.size();
            item.fileCount = 1;
            item.dirCount = 0;
            item.mimeType = "file";
        }
        
        items.append(item);
        emit itemFound(item);
        
        // 递归扫描子目录
        // 降低阈值：只要不是极小目录（<4KB且非空）就递归，确保能扫描到所有内容
        if (info.isDir() && depth < m_maxDepth && item.size >= 4 * 1024) {
            QList<ScanItem> subItems = scanDirectory(info.absoluteFilePath(), depth + 1);
            items.append(subItems);
        }
    }
    
    return items;
}

qint64 PartitionScanThread::getDirectorySize(const QString &path, int *fileCount, int *dirCount)
{
    qint64 totalSize = 0;
    int files = 0;
    int dirs = 0;

    if (m_stopped) {
        if (fileCount) *fileCount = 0;
        if (dirCount) *dirCount = 0;
        return 0;
    }

    // 策略：先尝试 du -s（快速获取块级别估算），如果失败再尝试 du -sb（精确字节）
    // 对于大目录（如 ostree），du -sb 需要遍历整个树，可能因权限问题超时

    // 先用 du -s（以块为单位，不需要遍历每个文件，速度快很多）
    QProcess process;
    process.start("du", QStringList() << "-s" << path);

    int elapsed = 0;
    const int timeout = 30000; // 30秒超时（du -s 通常很快）
    const int checkInterval = 100;

    while (!process.waitForFinished(checkInterval)) {
        elapsed += checkInterval;
        if (m_stopped || elapsed >= timeout) {
            process.kill();
            process.waitForFinished();
            break;
        }
    }

    if (process.exitStatus() == QProcess::NormalExit && !m_stopped) {
        // 注意：即使 exitCode != 0（如权限不足导致 du 返回 1），
        // 标准输出中仍然包含有效的大小数据，所以不检查退出码
        QString output = process.readAllStandardOutput();
        // 取最后一行有效数据（du 可能输出多行错误信息）
        for (const QString &line : output.split('\n')) {
            QStringList lineParts = line.trimmed().split('\t');
            if (lineParts.size() >= 2 && !lineParts[0].isEmpty()) {
                bool ok = false;
                qint64 val = lineParts[0].toLongLong(&ok);
                if (ok && val > 0) {
                    totalSize = val * 1024; // du -s 输出 KB 单位
                    break;
                }
            }
        }
    } else if (!m_stopped) {
        // du -s 失败，尝试 du -sb 作为后备（更慢但更精确）
        QProcess process2;
        process2.start("du", QStringList() << "-sb" << path);

        elapsed = 0;
        const int timeout2 = 60000; // du -sb 给60秒

        while (!process2.waitForFinished(checkInterval)) {
            elapsed += checkInterval;
            if (m_stopped || elapsed >= timeout2) {
                process2.kill();
                process2.waitForFinished();
                break;
            }
        }

        if (process2.exitStatus() == QProcess::NormalExit && !m_stopped) {
            // 同样不检查退出码，du 可能因权限问题返回非零但输出有效
            QString output = process2.readAllStandardOutput();
            for (const QString &line : output.split('\n')) {
                QStringList lineParts = line.trimmed().split('\t');
                if (lineParts.size() >= 2) {
                    bool ok = false;
                    qint64 val = lineParts[0].toLongLong(&ok);
                    if (ok && val > 0) {
                        totalSize = val; // du -sb 输出字节单位
                        break;
                    }
                }
            }
        }
    }

    // 获取文件和目录数量
    QDir dir(path);
    if (dir.exists()) {
        files = dir.entryList(QDir::Files).count();
        dirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot).count();
    }

    if (fileCount) *fileCount = files;
    if (dirCount) *dirCount = dirs;

    return totalSize;
}

// ============== CleanupDialog 实现 ==============

CleanupDialog::CleanupDialog(QWidget *parent)
    : QDialog(parent)
    , m_partitionPage(nullptr)
    , m_partitionTable(nullptr)
    , m_partitionInfoLabel(nullptr)
    , m_refreshButton(nullptr)
    , m_scanButton(nullptr)
    , m_scanResultPage(nullptr)
    , m_resultTree(nullptr)
    , m_scanInfoLabel(nullptr)
    , m_progressBar(nullptr)
    , m_stopScanButton(nullptr)
    , m_backButton(nullptr)
    , m_cleanupButton(nullptr)
    , m_pathEdit(nullptr)
    , m_goUpButton(nullptr)
    , m_goHomeButton(nullptr)
    , m_currentPathLabel(nullptr)
    , m_stackedWidget(nullptr)
    , m_scanThread(nullptr)
    , m_cleanupThread(nullptr)
    , m_scannedCount(0)
    , m_scanning(false)
{
    LOG_INFO("Initializing custom cleanup dialog");
    
    setWindowTitle(tr("自定义清理"));
    setMinimumSize(900, 650);
    resize(1000, 700);
    
    initUI();
    applyTheme();
    
    // 初始化扫描线程
    m_scanThread = new PartitionScanThread(this);
    connect(m_scanThread, &QThread::finished, m_scanThread, &QObject::deleteLater);
    connect(m_scanThread, &PartitionScanThread::scanProgress, this, &CleanupDialog::onScanProgress);
    connect(m_scanThread, &PartitionScanThread::scanFinished, this, &CleanupDialog::onScanFinished);
    connect(m_scanThread, &PartitionScanThread::itemFound, this, &CleanupDialog::onItemFound);
    
    // 初始化清理线程
    m_cleanupThread = new CleanupThread(this);
    connect(m_cleanupThread, &CleanupThread::cleanupProgress,
            this, &CleanupDialog::onCleanupProgress);
    connect(m_cleanupThread, &CleanupThread::cleanupFinished,
            this, &CleanupDialog::onCleanupFinished);
    
    loadPartitions();
    updatePartitionTable();
}

CleanupDialog::~CleanupDialog()
{
    if (m_scanThread && m_scanThread->isRunning()) {
        m_scanThread->stop();
        m_scanThread->wait();
    }
}

void CleanupDialog::initUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    
    // 创建堆叠窗口
    m_stackedWidget = new QStackedWidget(this);
    
    // 创建分区选择页
    createPartitionPage();
    
    // 创建扫描结果页
    createScanResultPage();
    
    mainLayout->addWidget(m_stackedWidget, 1);
}

void CleanupDialog::createPartitionPage()
{
    m_partitionPage = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(m_partitionPage);
    layout->setSpacing(15);
    
    // 标题
    QLabel *titleLabel = new QLabel(tr("选择要扫描的分区"), this);
    titleLabel->setStyleSheet("font-size: 18px; font-weight: bold;");
    layout->addWidget(titleLabel);
    
    // 分区信息提示
    m_partitionInfoLabel = new QLabel(tr("请勾选要扫描的分区，然后点击\"开始扫描\""), this);
    m_partitionInfoLabel->setStyleSheet("color: #666;");
    layout->addWidget(m_partitionInfoLabel);
    
    // 分区表格
    QGroupBox *group = new QGroupBox(tr("系统分区列表"), this);
    QVBoxLayout *groupLayout = new QVBoxLayout(group);
    
    m_partitionTable = new QTableWidget(this);
    m_partitionTable->setColumnCount(7);
    m_partitionTable->setHorizontalHeaderLabels(
        QStringList() 
            << tr("选择") 
            << tr("设备") 
            << tr("挂载点") 
            << tr("文件系统") 
            << tr("总容量") 
            << tr("已用") 
            << tr("使用率")
    );
    
    // 设置列宽
    m_partitionTable->setColumnWidth(0, 60);
    m_partitionTable->setColumnWidth(1, 120);
    m_partitionTable->setColumnWidth(2, 150);
    m_partitionTable->setColumnWidth(3, 80);
    m_partitionTable->setColumnWidth(4, 100);
    m_partitionTable->setColumnWidth(5, 100);
    m_partitionTable->horizontalHeader()->setStretchLastSection(true);
    
    m_partitionTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_partitionTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_partitionTable->setAlternatingRowColors(true);
    m_partitionTable->verticalHeader()->setVisible(false);
    
    groupLayout->addWidget(m_partitionTable);
    layout->addWidget(group, 1);
    
    // 按钮区域
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(10);
    
    QPushButton *selectAllBtn = new QPushButton(tr("全选"), this);
    QPushButton *deselectAllBtn = new QPushButton(tr("取消全选"), this);
    m_refreshButton = new QPushButton(tr("刷新分区"), this);
    m_scanButton = new QPushButton(tr("开始扫描"), this);
    
    m_scanButton->setStyleSheet(
        "QPushButton { "
        "   background-color: #4CAF50; "
        "   color: white; "
        "   border-radius: 5px; "
        "   padding: 8px 20px; "
        "   font-weight: bold; "
        "} "
        "QPushButton:hover { background-color: #45a049; }"
    );
    
    buttonLayout->addWidget(selectAllBtn);
    buttonLayout->addWidget(deselectAllBtn);
    buttonLayout->addWidget(m_refreshButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_scanButton);
    
    layout->addLayout(buttonLayout);
    
    // 连接信号
    connect(m_refreshButton, &QPushButton::clicked, this, &CleanupDialog::onRefreshPartitions);
    connect(m_scanButton, &QPushButton::clicked, this, &CleanupDialog::onStartScanClicked);
    connect(selectAllBtn, &QPushButton::clicked, this, &CleanupDialog::onSelectAllPartitions);
    connect(deselectAllBtn, &QPushButton::clicked, this, &CleanupDialog::onDeselectAllPartitions);
    
    m_stackedWidget->addWidget(m_partitionPage);
}

void CleanupDialog::createScanResultPage()
{
    m_scanResultPage = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(m_scanResultPage);
    layout->setSpacing(10);
    
    // 标题
    QLabel *titleLabel = new QLabel(tr("文件浏览器"), this);
    titleLabel->setStyleSheet("font-size: 18px; font-weight: bold;");
    layout->addWidget(titleLabel);
    
    // 路径导航栏
    QHBoxLayout *navLayout = new QHBoxLayout();
    navLayout->setSpacing(10);
    
    m_goUpButton = new QPushButton(tr("上一级"), this);
    m_goUpButton->setIcon(QIcon::fromTheme("go-up"));
    m_goUpButton->setToolTip(tr("返回上一级目录"));
    m_goUpButton->setMaximumWidth(100);
    
    m_goHomeButton = new QPushButton(tr("初始目录"), this);
    m_goHomeButton->setIcon(QIcon::fromTheme("go-home"));
    m_goHomeButton->setToolTip(tr("返回初始扫描目录"));
    m_goHomeButton->setMaximumWidth(100);
    
    QLabel *pathLabel = new QLabel(tr("当前路径:"), this);
    m_pathEdit = new QLineEdit(this);
    m_pathEdit->setReadOnly(false);
    m_pathEdit->setPlaceholderText(tr("输入路径后按回车跳转"));
    
    navLayout->addWidget(m_goUpButton);
    navLayout->addWidget(m_goHomeButton);
    navLayout->addWidget(pathLabel);
    navLayout->addWidget(m_pathEdit, 1);
    
    layout->addLayout(navLayout);
    
    // 进度条
    m_progressBar = new QProgressBar(this);
    m_progressBar->setTextVisible(true);
    m_progressBar->setFormat(tr("扫描中: %p%"));
    m_progressBar->setVisible(false);
    layout->addWidget(m_progressBar);
    
    // 扫描信息
    m_scanInfoLabel = new QLabel(this);
    m_scanInfoLabel->setStyleSheet("color: #666;");
    layout->addWidget(m_scanInfoLabel);
    
    // 结果树形视图
    QGroupBox *group = new QGroupBox(tr("文件和文件夹大小"), this);
    QVBoxLayout *groupLayout = new QVBoxLayout(group);
    
    m_resultTree = new QTreeWidget(this);
    m_resultTree->setHeaderLabels(
        QStringList() 
            << ""                        // 第0列：勾选框
            << tr("名称")                 // 第1列：名称+图标
            << tr("大小")                 // 第2列
            << tr("文件数")               // 第3列
            << tr("文件夹数")             // 第4列
            << tr("路径")                 // 第5列
    );
    
    m_resultTree->setColumnWidth(0, 36);   // 勾选框列（固定窄宽）
    m_resultTree->setColumnWidth(1, 260);  // 名称列
    m_resultTree->setColumnWidth(2, 100);  // 大小
    m_resultTree->setColumnWidth(3, 80);   // 文件数
    m_resultTree->setColumnWidth(4, 80);   // 文件夹数
    m_resultTree->header()->setStretchLastSection(true);
    m_resultTree->setSortingEnabled(false);
    m_resultTree->setAlternatingRowColors(true);
    m_resultTree->setAnimated(true);
    m_resultTree->setSelectionMode(QAbstractItemView::SingleSelection);
    
    groupLayout->addWidget(m_resultTree);
    layout->addWidget(group, 1);
    
    // 选择操作按钮
    QHBoxLayout *selectLayout = new QHBoxLayout();
    selectLayout->setSpacing(10);
    
    m_selectAllBtn = new QPushButton(tr("全选"), this);
    m_deselectAllBtn = new QPushButton(tr("取消全选"), this);
    QLabel *hintLabel = new QLabel(tr("提示：勾选要清理的项目，然后点击清理按钮"), this);
    hintLabel->setStyleSheet("color: #888;");
    
    selectLayout->addWidget(m_selectAllBtn);
    selectLayout->addWidget(m_deselectAllBtn);
    selectLayout->addStretch();
    selectLayout->addWidget(hintLabel);
    
    layout->addLayout(selectLayout);
    
    // 按钮区域
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(10);
    
    m_stopScanButton = new QPushButton(tr("停止扫描"), this);
    m_stopScanButton->setVisible(false);
    m_stopScanButton->setStyleSheet(
        "QPushButton { "
        "   background-color: #f44336; "
        "   color: white; "
        "   border-radius: 5px; "
        "   padding: 8px 20px; "
        "   outline: none; "
        "} "
        "QPushButton:hover { background-color: #d32f2f; }"
        "QPushButton:focus { outline: none; border: none; }"
    );
    
    m_backButton = new QPushButton(tr("返回分区选择"), this);
    m_cleanupButton = new QPushButton(tr("清理选中项目"), this);
    
    m_cleanupButton->setStyleSheet(
        "QPushButton { "
        "   background-color: #FF9800; "
        "   color: white; "
        "   border-radius: 5px; "
        "   padding: 8px 20px; "
        "   font-weight: bold; "
        "} "
        "QPushButton:hover { background-color: #F57C00; }"
    );
    
    buttonLayout->addWidget(m_stopScanButton);
    buttonLayout->addWidget(m_backButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_cleanupButton);
    
    layout->addLayout(buttonLayout);
    
    // 连接信号
    connect(m_stopScanButton, &QPushButton::clicked, this, &CleanupDialog::onStopScanClicked);
    connect(m_backButton, &QPushButton::clicked, this, &CleanupDialog::onBackToPartitionsClicked);
    connect(m_cleanupButton, &QPushButton::clicked, this, &CleanupDialog::onCleanupClicked);
    connect(m_resultTree, &QTreeWidget::itemDoubleClicked, this, &CleanupDialog::onItemDoubleClicked);
    connect(m_goUpButton, &QPushButton::clicked, this, &CleanupDialog::onGoUpClicked);
    connect(m_goHomeButton, &QPushButton::clicked, this, &CleanupDialog::onGoHomeClicked);
    connect(m_pathEdit, &QLineEdit::returnPressed, this, &CleanupDialog::onPathEditReturnPressed);
    connect(m_selectAllBtn, &QPushButton::clicked, this, &CleanupDialog::onSelectAllItems);
    connect(m_deselectAllBtn, &QPushButton::clicked, this, &CleanupDialog::onDeselectAllItems);
    
    m_stackedWidget->addWidget(m_scanResultPage);
}

void CleanupDialog::applyTheme()
{
    bool dark = isDarkTheme();

    QString tableStyle;
    QString treeStyle;
    QString groupStyle;

    // 基础样式 - 去除焦点边框
    QString baseTableStyle =
        "QTableWidget { outline: none; } "
        "QTableWidget::item { padding: 5px; outline: none; border: none; } "
        "QTableWidget::item:selected { outline: none; border: none; } "
        "QTableWidget::item:focus { outline: none; border: none; } "
        "QCheckBox:focus { outline: none; } "
        "QPushButton:focus { outline: none; } ";

    QString baseTreeStyle =
        "QTreeWidget { outline: none; } "
        "QTreeWidget::item { padding: 3px; outline: none; } "
        "QTreeWidget::item:selected { outline: none; border: none; } "
        "QTreeWidget::item:focus { outline: none; border: none; } "
        "QCheckBox:focus { outline: none; } ";

    if (dark) {
        tableStyle = baseTableStyle +
            "QTableWidget { "
            "   background-color: #2d2d2d; "
            "   color: #e0e0e0; "
            "   gridline-color: #444; "
            "   border: 1px solid #444; "
            "} "
            "QTableWidget::item:selected { background-color: #3d5a80; border: none; } "
            "QHeaderView::section { "
            "   background-color: #3d3d3d; "
            "   color: #e0e0e0; "
            "   padding: 5px; "
            "   border: 1px solid #444; "
            "}";

        treeStyle = baseTreeStyle +
            "QTreeWidget { "
            "   background-color: #2d2d2d; "
            "   color: #e0e0e0; "
            "   border: 1px solid #444; "
            "} "
            "QTreeWidget::item:selected { background-color: #3d5a80; border: none; } "
            "QHeaderView::section { "
            "   background-color: #3d3d3d; "
            "   color: #e0e0e0; "
            "   padding: 5px; "
            "   border: 1px solid #444; "
            "}";

        groupStyle =
            "QGroupBox { "
            "   color: #e0e0e0; "
            "   border: 1px solid #555; "
            "   border-radius: 5px; "
            "   margin-top: 10px; "
            "   padding-top: 10px; "
            "} "
            "QGroupBox::title { "
            "   subcontrol-origin: margin; "
            "   left: 10px; "
            "   padding: 0 5px; "
            "}";

        m_partitionInfoLabel->setStyleSheet("color: #aaa;");
        m_scanInfoLabel->setStyleSheet("color: #aaa;");
    } else {
        // 浅色主题也要添加去除焦点边框的样式，并确保选中时复选框可见
        tableStyle = baseTableStyle +
            "QTableWidget { "
            "   background-color: #ffffff; "
            "   color: #333333; "
            "   gridline-color: #e0e0e0; "
            "   border: 1px solid #ddd; "
            "} "
            "QTableWidget::item:selected { "
            "   background-color: #4a90d9; "
            "   color: #ffffff; "
            "   border: none; "
            "} "
            "QHeaderView::section { "
            "   background-color: #f5f5f5; "
            "   color: #333333; "
            "   padding: 5px; "
            "   border: 1px solid #ddd; "
            "   font-weight: bold; "
            "}";

        treeStyle = baseTreeStyle +
            "QTreeWidget { "
            "   background-color: #ffffff; "
            "   color: #333333; "
            "   border: 1px solid #ddd; "
            "} "
            "QTreeWidget::item:selected { "
            "   background-color: #4a90d9; "
            "   color: #ffffff; "
            "   border: none; "
            "} "
            "QHeaderView::section { "
            "   background-color: #f5f5f5; "
            "   color: #333333; "
            "   padding: 5px; "
            "   border: 1px solid #ddd; "
            "   font-weight: bold; "
            "}";
    }

    m_partitionTable->setStyleSheet(tableStyle);
    m_resultTree->setStyleSheet(treeStyle);

    // 应用到所有 GroupBox
    QList<QGroupBox*> groups = findChildren<QGroupBox*>();
    for (QGroupBox *group : groups) {
        group->setStyleSheet(groupStyle);
    }
}

bool CleanupDialog::isDarkTheme()
{
    QPalette palette = qApp->palette();
    QColor windowColor = palette.color(QPalette::Window);
    int brightness = (windowColor.red() * 299 + windowColor.green() * 587 + windowColor.blue() * 114) / 1000;
    return brightness < 128;
}

void CleanupDialog::loadPartitions()
{
    m_partitions.clear();
    
    // 使用 df 命令获取分区信息
    QProcess process;
    process.start("df", QStringList() << "-Th");
    process.waitForFinished();
    
    QString output = QString::fromUtf8(process.readAllStandardOutput());
    QStringList lines = output.split('\n');
    
    for (const QString &line : lines) {
        // 过滤表头行（支持英文和中文环境）
        if (line.startsWith("Filesystem") || 
            line.startsWith("文件系统") ||
            line.trimmed().isEmpty()) {
            continue;
        }
        
        PartitionInfo info = parsePartitionLine(line);
        
        // 过滤掉一些特殊文件系统
        if (info.filesystem == "tmpfs" || 
            info.filesystem == "devtmpfs" ||
            info.filesystem == "squashfs" ||
            info.filesystem == "overlay" ||
            info.filesystem == "udev" ||
            info.filesystem == "cgmfs" ||
            info.filesystem == "mqueue") {
            continue;
        }
        
        // 过滤掉一些特殊挂载点
        if (info.mountpoint.startsWith("/run/") ||
            info.mountpoint.startsWith("/sys/") ||
            info.mountpoint.startsWith("/dev/") ||
            info.mountpoint.startsWith("/snap/")) {
            continue;
        }
        
        // 只保留有实际挂载点的分区
        if (!info.mountpoint.isEmpty() && info.mountpoint != "none") {
            info.isSystemPartition = isSystemPartition(info.mountpoint);
            m_partitions.append(info);
        }
    }
    
    // 按挂载点排序
    std::sort(m_partitions.begin(), m_partitions.end(), 
        [](const PartitionInfo &a, const PartitionInfo &b) {
            // 根分区放在最前面
            if (a.mountpoint == "/") return true;
            if (b.mountpoint == "/") return false;
            // home 分区次之
            if (a.mountpoint == "/home") return true;
            if (b.mountpoint == "/home") return false;
            // 其他按挂载点名称排序
            return a.mountpoint < b.mountpoint;
        });
    
    LOG_INFO(QString("Loaded %1 partitions").arg(m_partitions.size()));
}

PartitionInfo CleanupDialog::parsePartitionLine(const QString &line)
{
    PartitionInfo info;
    
    // df -Th 输出格式:
    // Filesystem     Type      Size  Used Avail Use% Mounted on
    // /dev/sda1      ext4      100G   50G   50G  50% /
    
    QStringList parts = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    
    if (parts.size() >= 7) {
        info.device = parts[0];
        info.filesystem = parts[1];
        
        // 解析大小（处理 K, M, G, T 后缀，支持小数如 9.0G）
        auto parseSize = [](const QString &str) -> qint64 {
            QString s = str.trimmed();
            if (s == "-" || s.isEmpty()) {
                return 0;
            }
            
            qint64 multiplier = 1;
            
            if (s.endsWith('K') || s.endsWith('k')) {
                multiplier = 1024;
                s.chop(1);
            } else if (s.endsWith('M') || s.endsWith('m')) {
                multiplier = 1024 * 1024;
                s.chop(1);
            } else if (s.endsWith('G') || s.endsWith('g')) {
                multiplier = 1024 * 1024 * 1024;
                s.chop(1);
            } else if (s.endsWith('T') || s.endsWith('t')) {
                multiplier = 1024LL * 1024 * 1024 * 1024;
                s.chop(1);
            }
            
            // 使用 toDouble 支持小数（如 9.0G）
            bool ok;
            double value = s.toDouble(&ok);
            return ok ? static_cast<qint64>(value * multiplier) : 0;
        };
        
        info.total = parseSize(parts[2]);
        info.used = parseSize(parts[3]);
        info.available = parseSize(parts[4]);
        
        // 解析使用百分比
        QString percentStr = parts[5];
        percentStr.remove('%');
        info.percent = percentStr.toDouble();
        
        // 挂载点是第7个字段到行尾（可能包含空格）
        info.mountpoint = parts[6];
        for (int i = 7; i < parts.size(); ++i) {
            info.mountpoint += " " + parts[i];
        }
    }
    
    return info;
}

bool CleanupDialog::isSystemPartition(const QString &mountpoint)
{
    return mountpoint == "/" || 
           mountpoint == "/home" || 
           mountpoint == "/usr" ||
           mountpoint == "/var";
}

void CleanupDialog::updatePartitionTable()
{
    // 暂时断开信号，避免在填充表格时触发递归
    m_partitionTable->blockSignals(true);
    
    m_partitionTable->setRowCount(m_partitions.size());
    
    bool dark = isDarkTheme();
    
    for (int i = 0; i < m_partitions.size(); ++i) {
        const PartitionInfo &partition = m_partitions[i];
        
        // 复选框
        QWidget *checkboxWidget = new QWidget();
        QHBoxLayout *checkboxLayout = new QHBoxLayout(checkboxWidget);
        checkboxLayout->setAlignment(Qt::AlignCenter);
        QCheckBox *checkbox = new QCheckBox();
        
        // 默认选中根分区和home分区
        checkbox->setChecked(partition.mountpoint == "/" || partition.mountpoint == "/home");
        
        // 连接复选框状态变化信号
        connect(checkbox, &QCheckBox::checkStateChanged, this, [this]() {
            // 更新信息标签
            int selectedCount = 0;
            qint64 totalSize = 0;
            for (int j = 0; j < m_partitionTable->rowCount(); ++j) {
                QWidget *w = m_partitionTable->cellWidget(j, 0);
                if (w) {
                    QCheckBox *cb = w->findChild<QCheckBox*>();
                    if (cb && cb->isChecked()) {
                        selectedCount++;
                        if (j < m_partitions.size()) {
                            totalSize += m_partitions[j].used;
                        }
                    }
                }
            }
            m_partitionInfoLabel->setText(tr("已选择 %1 个分区，共使用 %2")
                .arg(selectedCount).arg(formatSize(totalSize)));
        });
        
        checkboxLayout->addWidget(checkbox);
        checkboxLayout->setContentsMargins(0, 0, 0, 0);
        m_partitionTable->setCellWidget(i, 0, checkboxWidget);
        
        // 设备名
        QTableWidgetItem *deviceItem = new QTableWidgetItem(partition.device);
        m_partitionTable->setItem(i, 1, deviceItem);
        
        // 挂载点
        QTableWidgetItem *mountItem = new QTableWidgetItem(partition.mountpoint);
        if (partition.isSystemPartition) {
            QFont font = mountItem->font();
            font.setBold(true);
            mountItem->setFont(font);
        }
        m_partitionTable->setItem(i, 2, mountItem);
        
        // 文件系统
        QTableWidgetItem *fsItem = new QTableWidgetItem(partition.filesystem);
        m_partitionTable->setItem(i, 3, fsItem);
        
        // 总容量
        QTableWidgetItem *totalItem = new QTableWidgetItem(formatSize(partition.total));
        totalItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_partitionTable->setItem(i, 4, totalItem);
        
        // 已用
        QTableWidgetItem *usedItem = new QTableWidgetItem(formatSize(partition.used));
        usedItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_partitionTable->setItem(i, 5, usedItem);
        
        // 使用率 - 使用进度条样式
        QString percentText = QString("%1%").arg(partition.percent, 0, 'f', 1);
        QTableWidgetItem *percentItem = new QTableWidgetItem(percentText);
        percentItem->setTextAlignment(Qt::AlignCenter);
        
        // 根据使用率设置颜色
        if (partition.percent > 90) {
            percentItem->setForeground(QColor("#f44336")); // 红色
        } else if (partition.percent > 70) {
            percentItem->setForeground(QColor("#FF9800")); // 橙色
        } else {
            percentItem->setForeground(QColor("#4CAF50")); // 绿色
        }
        m_partitionTable->setItem(i, 6, percentItem);
    }
    
    // 更新信息标签 - 初始状态
    int selectedCount = 0;
    qint64 totalSize = 0;
    for (int i = 0; i < m_partitionTable->rowCount(); ++i) {
        QWidget *widget = m_partitionTable->cellWidget(i, 0);
        if (widget) {
            QCheckBox *checkbox = widget->findChild<QCheckBox*>();
            if (checkbox && checkbox->isChecked()) {
                selectedCount++;
                if (i < m_partitions.size()) {
                    totalSize += m_partitions[i].used;
                }
            }
        }
    }
    
    m_partitionInfoLabel->setText(tr("已选择 %1 个分区，共使用 %2")
        .arg(selectedCount).arg(formatSize(totalSize)));
    
    // 恢复信号
    m_partitionTable->blockSignals(false);
}

void CleanupDialog::onSelectAllPartitions()
{
    for (int i = 0; i < m_partitionTable->rowCount(); ++i) {
        QWidget *widget = m_partitionTable->cellWidget(i, 0);
        if (widget) {
            QCheckBox *checkbox = widget->findChild<QCheckBox*>();
            if (checkbox) {
                checkbox->setChecked(true);
            }
        }
    }
    // 只更新信息标签，不重建表格
    int selectedCount = m_partitionTable->rowCount();
    qint64 totalSize = 0;
    for (int i = 0; i < m_partitions.size(); ++i) {
        totalSize += m_partitions[i].used;
    }
    m_partitionInfoLabel->setText(tr("已选择 %1 个分区，共使用 %2")
        .arg(selectedCount).arg(formatSize(totalSize)));
}

void CleanupDialog::onDeselectAllPartitions()
{
    for (int i = 0; i < m_partitionTable->rowCount(); ++i) {
        QWidget *widget = m_partitionTable->cellWidget(i, 0);
        if (widget) {
            QCheckBox *checkbox = widget->findChild<QCheckBox*>();
            if (checkbox) {
                checkbox->setChecked(false);
            }
        }
    }
    m_partitionInfoLabel->setText(tr("已选择 0 个分区，共使用 0 B"));
}

void CleanupDialog::onStartScanClicked()
{
    // 获取选中的分区
    QList<PartitionInfo> selectedPartitions = getSelectedPartitions();
    
    if (selectedPartitions.isEmpty()) {
        QMessageBox::warning(this, tr("警告"), tr("请至少选择一个分区进行扫描"));
        return;
    }
    
    // 如果选择了多个分区，提示用户选择一个
    QString scanPath;
    if (selectedPartitions.size() == 1) {
        scanPath = selectedPartitions[0].mountpoint;
    } else {
        // 显示选择对话框
        QStringList mountPoints;
        for (const PartitionInfo &p : selectedPartitions) {
            mountPoints << QString("%1 (%2)").arg(p.mountpoint).arg(formatSize(p.used));
        }
        
        bool ok;
        QString selected = QInputDialog::getItem(this, tr("选择扫描路径"),
            tr("请选择要扫描的分区（目前只支持单个分区扫描）："), mountPoints, 0, false, &ok);
        
        if (!ok || selected.isEmpty()) {
            return;
        }
        
        // 解析选择的挂载点
        int spaceIndex = selected.indexOf(" (");
        if (spaceIndex > 0) {
            scanPath = selected.left(spaceIndex);
        } else {
            scanPath = selected;
        }
    }
    
    LOG_INFO(QString("Starting scan for: %1").arg(scanPath));
    
    // 设置初始路径
    m_currentScanPath = scanPath;
    m_currentBrowsePath = scanPath;
    m_navigationHistory.clear();
    
    // 切换到扫描结果页
    m_stackedWidget->setCurrentWidget(m_scanResultPage);
    m_scannedCount = 0;
    m_scanning = true;
    
    // 清空之前的结果
    m_resultTree->clear();
    m_scanResults.clear();
    
    // 显示进度条
    m_progressBar->setVisible(true);
    m_progressBar->setValue(0);
    m_stopScanButton->setVisible(true);
    m_backButton->setVisible(false);
    m_cleanupButton->setVisible(false);
    m_goUpButton->setVisible(false);
    m_goHomeButton->setVisible(false);
    m_pathEdit->setEnabled(false);
    
    m_scanInfoLabel->setText(tr("正在扫描: %1").arg(scanPath));
    
    // 更新路径显示
    updatePathDisplay();
    
    // 启动扫描线程
    m_scanThread->setScanPath(scanPath, 3);
    m_scanThread->start();
}

void CleanupDialog::onScanProgress(const QString &currentPath, int scanned)
{
    m_scannedCount = scanned;
    m_progressBar->setValue(scanned % 100);
    m_scanInfoLabel->setText(tr("正在扫描: %1\n已扫描 %2 项")
        .arg(currentPath).arg(scanned));
}

void CleanupDialog::onItemFound(const ScanItem &item)
{
    // 只添加较大的文件/文件夹到树中（大于4KB，避免显示过多微小文件）
    // 注意：size=0 表示无法获取大小（如权限不足导致du超时），这类目录也需要显示
    if (item.size > 0 && item.size < 4 * 1024) {
        return;
    }
    
    m_scanResults.append(item);
    
    // 创建树节点（扫描时不排序，避免性能问题）
    m_resultTree->setSortingEnabled(false);

    QTreeWidgetItem *treeItem = new QTreeWidgetItem(m_resultTree);
    treeItem->setFlags(treeItem->flags() | Qt::ItemIsUserCheckable);
    treeItem->setCheckState(0, Qt::Unchecked);
    // 第1列：名称 + 图标
    treeItem->setText(1, item.name);
    treeItem->setIcon(1, getFileIcon(item.path, item.isDir));
    
    // 如果大小为0，显示"未知"；否则正常显示
    if (item.size == 0 && item.isDir) {
        treeItem->setText(2, tr("未知"));
        treeItem->setForeground(2, QColor("#999999"));
    } else {
        treeItem->setText(2, formatSize(item.size));
        
        // 根据大小设置颜色
        if (item.size > 1024LL * 1024 * 1024) { // > 1GB
            treeItem->setForeground(2, QColor("#f44336"));
        } else if (item.size > 100 * 1024 * 1024) { // > 100MB
            treeItem->setForeground(2, QColor("#FF9800"));
        }
    }
    
    treeItem->setText(3, formatNumber(item.fileCount));
    treeItem->setText(4, formatNumber(item.dirCount));
    treeItem->setText(5, item.path);
    
    // 设置排序数据（未知大小的排到最后）
    treeItem->setData(2, Qt::UserRole, item.size);
    
    // 更新扫描信息
    m_scanInfoLabel->setText(tr("正在扫描: %1\n已发现 %2 个项目")
        .arg(m_currentScanPath).arg(m_scanResults.size()));
}

void CleanupDialog::onScanFinished(const QList<ScanItem> &items)
{
    Q_UNUSED(items);
    
    m_scanning = false;
    m_progressBar->setVisible(false);
    m_stopScanButton->setVisible(false);
    m_backButton->setVisible(true);
    m_cleanupButton->setVisible(true);
    m_goUpButton->setVisible(true);
    m_goHomeButton->setVisible(true);
    m_pathEdit->setEnabled(true);
    
    // 按大小排序扫描结果
    std::sort(m_scanResults.begin(), m_scanResults.end(), 
        [](const ScanItem &a, const ScanItem &b) {
            return a.size > b.size; // 降序
        });
    
    // 清空树并重新填充（确保正确排序）
    m_resultTree->clear();
    m_resultTree->setSortingEnabled(false);
    
    for (const ScanItem &item : m_scanResults) {
        QTreeWidgetItem *treeItem = new QTreeWidgetItem(m_resultTree);
        treeItem->setFlags(treeItem->flags() | Qt::ItemIsUserCheckable);
        treeItem->setCheckState(0, Qt::Unchecked);
        // 第1列：名称 + 图标
        treeItem->setText(1, item.name);
        treeItem->setIcon(1, getFileIcon(item.path, item.isDir));
        
        // 如果大小为0，显示"未知"；否则正常显示
        if (item.size == 0 && item.isDir) {
            treeItem->setText(2, tr("未知"));
            treeItem->setForeground(2, QColor("#999999"));
        } else {
            treeItem->setText(2, formatSize(item.size));
            
            // 根据大小设置颜色
            if (item.size > 1024LL * 1024 * 1024) { // > 1GB
                treeItem->setForeground(2, QColor("#f44336"));
            } else if (item.size > 100 * 1024 * 1024) { // > 100MB
                treeItem->setForeground(2, QColor("#FF9800"));
            }
        }

        treeItem->setText(3, formatNumber(item.fileCount));
        treeItem->setText(4, formatNumber(item.dirCount));
        treeItem->setText(5, item.path);

        // 设置排序数据（未知大小的排到最后）
        treeItem->setData(2, Qt::UserRole, item.size);
    }
    
    // 更新路径显示
    updatePathDisplay();
    
    // 计算总大小
    qint64 totalSize = 0;
    for (const ScanItem &item : m_scanResults) {
        totalSize += item.size;
    }
    
    m_scanInfoLabel->setText(tr("扫描完成！\n共发现 %1 个项目，总大小: %2")
        .arg(m_scanResults.size()).arg(formatSize(totalSize)));
    
    LOG_INFO(QString("Scan finished, %1 items found, total size: %2")
        .arg(m_scanResults.size()).arg(totalSize));
}

void CleanupDialog::onStopScanClicked()
{
    if (m_scanThread && m_scanThread->isRunning()) {
        m_scanThread->stop();
        m_scanThread->wait();
    }
    
    m_scanning = false;
    m_progressBar->setVisible(false);
    m_stopScanButton->setVisible(false);
    m_backButton->setVisible(true);
    m_cleanupButton->setVisible(true);
    m_goUpButton->setVisible(true);
    m_goHomeButton->setVisible(true);
    m_pathEdit->setEnabled(true);
    
    // 更新路径显示
    updatePathDisplay();
    
    m_scanInfoLabel->setText(tr("扫描已停止\n已发现 %1 个项目")
        .arg(m_scanResults.size()));
}

void CleanupDialog::onRefreshPartitions()
{
    loadPartitions();
    updatePartitionTable();
}

void CleanupDialog::onItemDoubleClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column);
    
    QString path = item->text(5);
    QFileInfo info(path);
    
    if (info.isDir()) {
        // 进入该目录
        navigateToPath(path);
    } else {
        // 如果是文件，在文件管理器中打开所在目录并选中
        QProcess::startDetached("xdg-open", QStringList() << info.absolutePath());
    }
}

void CleanupDialog::onBackToPartitionsClicked()
{
    m_stackedWidget->setCurrentWidget(m_partitionPage);
}

void CleanupDialog::onCleanupClicked()
{
    // 获取勾选的项目
    QList<QTreeWidgetItem*> checkedItems;
    for (int i = 0; i < m_resultTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = m_resultTree->topLevelItem(i);
        if (item && item->checkState(0) == Qt::Checked) {
            checkedItems.append(item);
        }
    }
    
    if (checkedItems.isEmpty()) {
        QMessageBox::information(this, tr("提示"), 
            tr("请先勾选要清理的文件或文件夹"));
        return;
    }
    
    // 确认删除（直接使用扫描时已缓存的大小，不再执行 du -sb）
    qint64 totalSize = 0;
    QStringList paths;
    for (QTreeWidgetItem *item : checkedItems) {
        paths << item->text(5);
        totalSize += item->data(2, Qt::UserRole).toLongLong();
    }
    
    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        tr("确认清理"),
        tr("确定要清理以下 %1 个项目吗？\n\n共释放空间: %2\n\n注意：删除后无法恢复！")
            .arg(paths.size()).arg(formatSize(totalSize)),
        QMessageBox::Yes | QMessageBox::No
    );
    
    if (reply == QMessageBox::Yes) {
        // 禁用按钮防止重复点击
        m_cleanupButton->setEnabled(false);
        m_selectAllBtn->setEnabled(false);
        m_deselectAllBtn->setEnabled(false);
        
        // 显示进度
        m_progressBar->setVisible(true);
        m_progressBar->setRange(0, paths.size());
        m_progressBar->setValue(0);
        m_scanInfoLabel->setText(tr("正在清理中...\n共 %1 个项目").arg(paths.size()));
        
        // 启动异步清理线程
        m_cleanupThread->setPathsToDelete(paths);
        m_cleanupThread->start();
    }
}

void CleanupDialog::onCleanupProgress(int current, int total, const QString &currentPath)
{
    m_progressBar->setValue(current);
    
    // 只显示文件名，避免路径太长
    QFileInfo info(currentPath);
    QString displayName = info.fileName().isEmpty() ? currentPath : info.fileName();
    m_scanInfoLabel->setText(tr("正在清理: %1\n进度: %2/%3")
        .arg(displayName).arg(current).arg(total));
}

void CleanupDialog::onCleanupFinished(int successCount, int failCount, qint64 freedSize)
{
    // 恢复按钮状态
    m_cleanupButton->setEnabled(true);
    m_selectAllBtn->setEnabled(true);
    m_deselectAllBtn->setEnabled(true);
    m_progressBar->setVisible(false);
    
    // 从树中移除已成功删除的项
    QList<QTreeWidgetItem*> toRemove;
    for (int i = 0; i < m_resultTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = m_resultTree->topLevelItem(i);
        if (item && item->checkState(0) == Qt::Checked) {
            toRemove.append(item);
        }
    }
    for (QTreeWidgetItem *item : toRemove) {
        delete item;
    }
    
    // 显示结果
    if (failCount == 0) {
        QMessageBox::information(this, tr("✓ 清理完成"),
            tr("成功清理 %1 个项目\n释放空间: %2")
                .arg(successCount).arg(formatSize(freedSize)));
    } else {
        QMessageBox::warning(this, tr("清理完成"),
            tr("成功: %1 个项目\n失败: %2 个项目\n释放空间: %3")
                .arg(successCount).arg(failCount).arg(formatSize(freedSize)));
    }
    
    LOG_INFO(QString("Cleanup finished: success=%1, fail=%2").arg(successCount).arg(failCount));
}

QStringList CleanupDialog::getSelectedItems() const
{
    QStringList selectedItems;
    
    for (int i = 0; i < m_partitionTable->rowCount(); ++i) {
        QWidget *widget = m_partitionTable->cellWidget(i, 0);
        if (widget) {
            QCheckBox *checkbox = widget->findChild<QCheckBox*>();
            if (checkbox && checkbox->isChecked()) {
                QTableWidgetItem *mountItem = m_partitionTable->item(i, 2);
                if (mountItem) {
                    selectedItems.append(mountItem->text());
                }
            }
        }
    }
    
    return selectedItems;
}

qint64 CleanupDialog::getTotalSelectedSize() const
{
    qint64 total = 0;
    
    for (int i = 0; i < m_partitionTable->rowCount(); ++i) {
        QWidget *widget = m_partitionTable->cellWidget(i, 0);
        if (widget) {
            QCheckBox *checkbox = widget->findChild<QCheckBox*>();
            if (checkbox && checkbox->isChecked() && i < m_partitions.size()) {
                total += m_partitions[i].used;
            }
        }
    }
    
    return total;
}

QList<PartitionInfo> CleanupDialog::getSelectedPartitions() const
{
    QList<PartitionInfo> selected;
    
    for (int i = 0; i < m_partitionTable->rowCount(); ++i) {
        QWidget *widget = m_partitionTable->cellWidget(i, 0);
        if (widget) {
            QCheckBox *checkbox = widget->findChild<QCheckBox*>();
            if (checkbox && checkbox->isChecked() && i < m_partitions.size()) {
                selected.append(m_partitions[i]);
            }
        }
    }
    
    return selected;
}

QString CleanupDialog::formatSize(qint64 bytes)
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

QString CleanupDialog::formatNumber(int num)
{
    if (num >= 10000) {
        return QString("%1万").arg(num / 10000);
    }
    return QString::number(num);
}

QIcon CleanupDialog::getFileIcon(const QString &path, bool isDir)
{
    QFileIconProvider provider;
    QFileInfo info(path);
    
    if (isDir) {
        return provider.icon(QFileIconProvider::Folder);
    } else {
        return provider.icon(info);
    }
}

void CleanupDialog::onGoUpClicked()
{
    if (m_currentBrowsePath.isEmpty() || m_currentBrowsePath == "/") {
        return;
    }
    
    QDir dir(m_currentBrowsePath);
    if (dir.cdUp()) {
        navigateToPath(dir.absolutePath());
    }
}

void CleanupDialog::onGoHomeClicked()
{
    if (!m_currentScanPath.isEmpty()) {
        navigateToPath(m_currentScanPath);
    }
}

void CleanupDialog::onPathEditReturnPressed()
{
    QString path = m_pathEdit->text().trimmed();
    
    if (path.isEmpty()) {
        return;
    }
    
    QFileInfo info(path);
    if (info.exists() && info.isDir()) {
        navigateToPath(path);
    } else {
        QMessageBox::warning(this, tr("警告"), tr("路径不存在或不是有效目录"));
        m_pathEdit->setText(m_currentBrowsePath);
    }
}

void CleanupDialog::navigateToPath(const QString &path)
{
    LOG_INFO(QString("Navigating to: %1").arg(path));
    
    // 保存当前路径到历史
    if (!m_currentBrowsePath.isEmpty()) {
        m_navigationHistory.push(m_currentBrowsePath);
    }
    
    m_currentBrowsePath = path;
    updatePathDisplay();
    browseDirectory(path);
}

void CleanupDialog::updatePathDisplay()
{
    m_pathEdit->setText(m_currentBrowsePath);
    
    // 更新按钮状态
    m_goUpButton->setEnabled(!m_currentBrowsePath.isEmpty() && m_currentBrowsePath != "/");
    m_goHomeButton->setEnabled(m_currentBrowsePath != m_currentScanPath);
}

void CleanupDialog::browseDirectory(const QString &path)
{
    // 清空之前的结果
    m_resultTree->clear();
    m_scanResults.clear();
    
    m_scanInfoLabel->setText(tr("正在加载 %1 ...").arg(path));
    m_scanInfoLabel->repaint();
    QApplication::processEvents(); // 立即更新UI
    
    // 直接扫描该目录
    QDir dir(path);
    if (!dir.exists()) {
        m_scanInfoLabel->setText(tr("目录不存在"));
        return;
    }
    
    dir.setFilter(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot | QDir::NoSymLinks);
    QFileInfoList entries = dir.entryInfoList();
    
    QList<ScanItem> items;
    int processedCount = 0;
    int totalCount = entries.size();
    
    for (const QFileInfo &info : entries) {
        processedCount++;
        
        // 每10个项目更新一次进度，避免UI阻塞
        if (processedCount % 10 == 0) {
            m_scanInfoLabel->setText(tr("正在扫描 %1/%2 ...").arg(processedCount).arg(totalCount));
            QApplication::processEvents();
        }
        
        ScanItem item;
        item.path = info.absoluteFilePath();
        item.name = info.fileName();
        item.isDir = info.isDir();
        
        if (info.isDir()) {
            // 使用 du 命令获取目录大小（先尝试 du -s 快速模式，再降级到 du -sb）
            QProcess process;
            process.start("du", QStringList() << "-s" << info.absoluteFilePath());
            
            int elapsed = 0;
            const int timeout = 15000;  // du -s 超时15秒
            const int checkInterval = 50;
            
            bool sizeObtained = false;
            
            while (!process.waitForFinished(checkInterval)) {
                elapsed += checkInterval;
                if (elapsed >= timeout) {
                    process.kill();
                    process.waitForFinished();
                    break;
                }
            }
            
            // du -s 即使退出码非零（权限不足返回1），输出仍可能包含有效大小数据
            if (process.exitStatus() == QProcess::NormalExit) {
                QString output = process.readAllStandardOutput();
                for (const QString &line : output.split('\n')) {
                    QStringList lineParts = line.trimmed().split('\t');
                    if (lineParts.size() >= 2) {
                        bool ok = false;
                        qint64 val = lineParts[0].toLongLong(&ok);
                        if (ok && val > 0) {
                            item.size = val * 1024; // du -s 输出KB单位
                            sizeObtained = true;
                            break;
                        }
                    }
                }
            }
            
            // 如果 du -s 失败，尝试 du -sb 作为后备
            if (!sizeObtained) {
                QProcess process2;
                process2.start("du", QStringList() << "-sb" << info.absoluteFilePath());
                
                elapsed = 0;
                const int timeout2 = 30000; // du -sb 给30秒
                
                while (!process2.waitForFinished(checkInterval)) {
                    elapsed += checkInterval;
                    if (elapsed >= timeout2) {
                        process2.kill();
                        process2.waitForFinished();
                        break;
                    }
                }
                
                if (process2.exitStatus() == QProcess::NormalExit) {
                    QString output = process2.readAllStandardOutput();
                    for (const QString &line : output.split('\n')) {
                        QStringList lineParts = line.trimmed().split('\t');
                        if (lineParts.size() >= 2) {
                            bool ok = false;
                            qint64 val = lineParts[0].toLongLong(&ok);
                            if (ok && val > 0) {
                                item.size = val; // du -sb 输出字节单位
                                break;
                            }
                        }
                    }
                }
            }
            
            // 获取文件和目录数量
            QDir subDir(info.absoluteFilePath());
            if (subDir.exists()) {
                item.fileCount = subDir.entryList(QDir::Files).count();
                item.dirCount = subDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot).count();
            }
        } else {
            item.size = info.size();
            item.fileCount = 1;
            item.dirCount = 0;
        }
        
        items.append(item);
    }
    
    // 按大小排序
    std::sort(items.begin(), items.end(), 
        [](const ScanItem &a, const ScanItem &b) {
            return a.size > b.size;
        });
    
    // 填充树形控件
    for (const ScanItem &item : items) {
        // 只显示大于1KB的项目（browseDirectory 用更宽松的过滤）
        if (item.size > 0 && item.size < 1024) {
            continue;
        }
        
        m_scanResults.append(item);
        
        QTreeWidgetItem *treeItem = new QTreeWidgetItem(m_resultTree);
        treeItem->setFlags(treeItem->flags() | Qt::ItemIsUserCheckable);
        treeItem->setCheckState(0, Qt::Unchecked);
        // 第1列：名称 + 图标
        treeItem->setText(1, item.name);
        treeItem->setIcon(1, getFileIcon(item.path, item.isDir));
        
        // 如果大小为0，显示"未知"；否则正常显示
        if (item.size == 0 && item.isDir) {
            treeItem->setText(2, tr("未知"));
            treeItem->setForeground(2, QColor("#999999"));
        } else {
            treeItem->setText(2, formatSize(item.size));
            
            // 根据大小设置颜色
            if (item.size > 1024LL * 1024 * 1024) {
                treeItem->setForeground(2, QColor("#f44336"));
            } else if (item.size > 100 * 1024 * 1024) {
                treeItem->setForeground(2, QColor("#FF9800"));
            }
        }

        treeItem->setText(3, formatNumber(item.fileCount));
        treeItem->setText(4, formatNumber(item.dirCount));
        treeItem->setText(5, item.path);

        // 设置排序数据
        treeItem->setData(2, Qt::UserRole, item.size);
    }
    
    // 计算总大小
    qint64 totalSize = 0;
    for (const ScanItem &item : m_scanResults) {
        totalSize += item.size;
    }
    
    m_scanInfoLabel->setText(tr("当前目录: %1\n共 %2 个项目，总大小: %3")
        .arg(path).arg(m_scanResults.size()).arg(formatSize(totalSize)));
}

void CleanupDialog::onSelectAllItems()
{
    m_resultTree->blockSignals(true);
    for (int i = 0; i < m_resultTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = m_resultTree->topLevelItem(i);
        if (item) {
            item->setCheckState(0, Qt::Checked);
        }
    }
    m_resultTree->blockSignals(false);
    
    // 更新选中信息
    updateSelectedInfo();
}

void CleanupDialog::onDeselectAllItems()
{
    m_resultTree->blockSignals(true);
    for (int i = 0; i < m_resultTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = m_resultTree->topLevelItem(i);
        if (item) {
            item->setCheckState(0, Qt::Unchecked);
        }
    }
    m_resultTree->blockSignals(false);
    
    m_scanInfoLabel->setText(tr("已取消选择所有项目"));
}

void CleanupDialog::updateSelectedInfo()
{
    int count = 0;
    qint64 totalSize = 0;
    
    for (int i = 0; i < m_resultTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = m_resultTree->topLevelItem(i);
        if (item && item->checkState(0) == Qt::Checked) {
            count++;
            totalSize += item->data(2, Qt::UserRole).toLongLong();
        }
    }
    
    m_scanInfoLabel->setText(tr("已选择 %1 个项目，共 %2")
        .arg(count).arg(formatSize(totalSize)));
}
