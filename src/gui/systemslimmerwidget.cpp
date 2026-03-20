/*
 * System Slimmer Widget - Implementation
 * 系统瘦身组件 - 实现文件
 */

#include "systemslimmerwidget.h"
#include "cleanuphistorywidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QApplication>
#include <QPalette>
#include <QMouseEvent>
#include <QCheckBox>
#include <QGroupBox>
#include <QScrollArea>
#include <QStackedWidget>
#include <QDebug>
#include <QMenu>
#include <QDesktopServices>
#include <QUrl>
#include <QClipboard>

// ==================== FeatureCard ====================

FeatureCard::FeatureCard(const QString &icon, const QString &title, 
                         const QString &desc, QWidget *parent)
    : QFrame(parent)
    , m_checked(false)
{
    setFixedSize(240, 240);
    setCursor(Qt::PointingHandCursor);
    
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(12);
    layout->setContentsMargins(20, 18, 20, 18);
    
    // 检测深色主题
    bool darkMode = false;
    if (qApp) {
        QPalette palette = qApp->palette();
        QColor windowColor = palette.color(QPalette::Window);
        int brightness = (windowColor.red() * 299 + windowColor.green() * 587 + windowColor.blue() * 114) / 1000;
        darkMode = brightness < 128;
    }
    
    QString titleColor = darkMode ? "#e0e0e0" : "#2C3E50";
    QString descColor = darkMode ? "#a0a0a0" : "#7F8C8D";
    
    // 顶部勾选标记区域
    QHBoxLayout *topLayout = new QHBoxLayout();
    topLayout->addStretch();
    m_checkLabel = new QLabel("✓", this);
    m_checkLabel->setFixedSize(22, 22);
    m_checkLabel->setAlignment(Qt::AlignCenter);
    m_checkLabel->setStyleSheet(
        "QLabel { "
        "   font-size: 13px; "
        "   font-weight: bold; "
        "   color: white; "
        "   background-color: #27AE60; "
        "   border-radius: 11px; "
        "}"
    );
    m_checkLabel->setVisible(false);
    topLayout->addWidget(m_checkLabel);
    layout->addLayout(topLayout);
    
    // 图标
    m_iconLabel = new QLabel(icon, this);
    m_iconLabel->setStyleSheet("font-size: 56px; background: transparent;");
    m_iconLabel->setAlignment(Qt::AlignCenter);
    m_iconLabel->setMinimumHeight(65);
    layout->addWidget(m_iconLabel, 0, Qt::AlignCenter);
    
    // 标题
    m_titleLabel = new QLabel(title, this);
    m_titleLabel->setStyleSheet(
        QString("font-size: 16px; "
        "font-weight: bold; "
        "color: %1; "
        "background: transparent;").arg(titleColor)
    );
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_titleLabel->setMinimumHeight(24);
    layout->addWidget(m_titleLabel);
    
    // 描述
    QLabel *descLabel = new QLabel(desc, this);
    descLabel->setStyleSheet(
        QString("font-size: 12px; "
        "color: %1; "
        "background: transparent;").arg(descColor)
    );
    descLabel->setAlignment(Qt::AlignCenter);
    descLabel->setWordWrap(true);
    descLabel->setMinimumHeight(20);
    layout->addWidget(descLabel);
    
    layout->addStretch();
    
    updateStyle();
}

void FeatureCard::setChecked(bool checked)
{
    m_checked = checked;
    m_checkLabel->setVisible(checked);
    updateStyle();
}

bool FeatureCard::isChecked() const
{
    return m_checked;
}

void FeatureCard::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        setChecked(!m_checked);
        emit clicked();
    }
    QFrame::mousePressEvent(event);
}

void FeatureCard::enterEvent(QEnterEvent *event)
{
    QFrame::enterEvent(event);
}

void FeatureCard::leaveEvent(QEvent *event)
{
    updateStyle();
    QFrame::leaveEvent(event);
}

void FeatureCard::updateStyle()
{
    // 使用固定的尺寸策略，防止选中时压缩
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    
    // 检测深色主题
    bool darkMode = false;
    if (qApp) {
        QPalette palette = qApp->palette();
        QColor windowColor = palette.color(QPalette::Window);
        int brightness = (windowColor.red() * 299 + windowColor.green() * 587 + windowColor.blue() * 114) / 1000;
        darkMode = brightness < 128;
    }
    
    QString normalBg = darkMode ? "#3d3d3d" : "white";
    QString normalBorder = darkMode ? "#555555" : "#E0E0E0";
    QString hoverBg = darkMode ? "#4a4a4a" : "#F5F7FA";
    QString hoverBorder = darkMode ? "#666666" : "#BDC3C7";
    QString checkedBg = darkMode ? "#2d4a6d" : "#E8F4FD";
    QString checkedBorder = "#3498DB";
    
    if (m_checked) {
        setStyleSheet(
            QString("FeatureCard { "
            "   background-color: %1; "
            "   border: 2px solid %2; "
            "   border-radius: 12px; "
            "}").arg(checkedBg, checkedBorder)
        );
    } else {
        setStyleSheet(
            QString("FeatureCard { "
            "   background-color: %1; "
            "   border: 2px solid %2; "
            "   border-radius: 12px; "
            "}"
            "FeatureCard:hover { "
            "   background-color: %3; "
            "   border-color: %4; "
            "}").arg(normalBg, normalBorder, hoverBg, hoverBorder)
        );
    }
}

// ==================== SystemSlimmerWidget ====================

SystemSlimmerWidget::SystemSlimmerWidget(QWidget *parent)
    : QWidget(parent)
    , m_slimmer(new SystemSlimmer(this))
    , m_scanLargeFiles(false)
    , m_scanDuplicates(false)
    , m_selectedLargeFilesTotalSize(0)
    , m_selectedDuplicateTotalSize(0)
{
    initUI();
    setupConnections();
    applyTheme();
}

SystemSlimmerWidget::~SystemSlimmerWidget()
{
}

void SystemSlimmerWidget::initUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    
    // 堆叠窗口
    m_stackWidget = new QStackedWidget(this);
    mainLayout->addWidget(m_stackWidget);
    
    // 初始化三个页面
    initMainPage();
    initScanningPage();
    initResultPage();
    
    // 添加到堆叠窗口
    m_stackWidget->addWidget(m_mainPage);
    m_stackWidget->addWidget(m_scanningPage);
    m_stackWidget->addWidget(m_resultPage);
}

void SystemSlimmerWidget::initMainPage()
{
    m_mainPage = new QWidget(this);
    
    // 检测深色主题
    bool darkMode = isDarkTheme();
    QString hintColor = darkMode ? "#c0c0c0" : "#5D6D7E";
    QString startBtnBg = "#3498DB";
    QString startBtnHoverBg = "#2980B9";
    QString startBtnPressedBg = "#21618C";
    QString startBtnDisabledBg = darkMode ? "#4a4a4a" : "#DCDEE0";
    QString startBtnDisabledColor = darkMode ? "#808080" : "#8C9196";
    
    QVBoxLayout *layout = new QVBoxLayout(m_mainPage);
    layout->setSpacing(18);
    layout->setContentsMargins(30, 25, 30, 25);
    
    // ========== 顶部标题区域 ==========
    QFrame *headerFrame = new QFrame(m_mainPage);
    headerFrame->setStyleSheet(
        "QFrame { "
        "   background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #667eea, stop:1 #764ba2); "
        "   border-radius: 10px; "
        "}"
    );
    QHBoxLayout *headerLayout = new QHBoxLayout(headerFrame);
    headerLayout->setContentsMargins(20, 15, 20, 15);
    
    QLabel *titleLabel = new QLabel(tr("系统瘦身"), headerFrame);
    titleLabel->setStyleSheet("font-size: 20px; font-weight: bold; color: white;");
    headerLayout->addWidget(titleLabel);
    
    QLabel *descLabel = new QLabel(tr("查找并清理大文件及重复文件，释放磁盘空间"), headerFrame);
    descLabel->setStyleSheet("font-size: 12px; color: rgba(255,255,255,0.9);");
    headerLayout->addStretch();
    headerLayout->addWidget(descLabel);
    
    layout->addWidget(headerFrame);
    
    // ========== 功能选择区域 ==========
    QLabel *selectHint = new QLabel(tr("请选择扫描类型（单选）:"), m_mainPage);
    selectHint->setStyleSheet(QString("font-size: 13px; color: %1; font-weight: bold;").arg(hintColor));
    layout->addWidget(selectHint);
    
    // 卡片容器 - 使用固定宽度容器居中
    QHBoxLayout *cardsOuterLayout = new QHBoxLayout();
    cardsOuterLayout->addStretch();
    
    QWidget *cardsContainer = new QWidget(m_mainPage);
    cardsContainer->setFixedWidth(520);  // 240*2 + 40间距
    QHBoxLayout *cardsLayout = new QHBoxLayout(cardsContainer);
    cardsLayout->setSpacing(40);
    cardsLayout->setContentsMargins(0, 0, 0, 0);
    cardsLayout->setSpacing(40);
    
    // 查找大文件卡片
    m_largeFileCard = new FeatureCard("🔍", tr("查找大文件"), 
                                       tr("扫描大于100M以上文件"), cardsContainer);
    cardsLayout->addWidget(m_largeFileCard);
    
    // 查找重复文件卡片
    m_duplicateCard = new FeatureCard("📑", tr("查找重复文件"), 
                                       tr("找出内容相同文件"), cardsContainer);
    cardsLayout->addWidget(m_duplicateCard);
    
    cardsOuterLayout->addWidget(cardsContainer);
    cardsOuterLayout->addStretch();
    layout->addLayout(cardsOuterLayout);
    
    layout->addStretch();
    
    // ========== 开始扫描按钮 ==========
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    
    m_startScanBtn = new QPushButton(tr("🔍 开始扫描"), m_mainPage);
    m_startScanBtn->setMinimumSize(200, 50);
    m_startScanBtn->setEnabled(false);
    m_startScanBtn->setCursor(Qt::PointingHandCursor);
    m_startScanBtn->setStyleSheet(
        QString("QPushButton { "
        "   background-color: %1; "
        "   color: white; "
        "   border: none; "
        "   border-radius: 8px; "
        "   font-size: 15px; "
        "   font-weight: bold; "
        "}"
        "QPushButton:hover { background-color: %2; }"
        "QPushButton:pressed { background-color: %3; }"
        "QPushButton:disabled { background-color: %4; color: %5; }")
        .arg(startBtnBg, startBtnHoverBg, startBtnPressedBg, startBtnDisabledBg, startBtnDisabledColor)
    );
    btnLayout->addWidget(m_startScanBtn);
    btnLayout->addStretch();
    
    layout->addLayout(btnLayout);
}

void SystemSlimmerWidget::initScanningPage()
{
    m_scanningPage = new QWidget(this);
    
    // 检测深色主题
    bool darkMode = isDarkTheme();
    QString scanningLabelColor = darkMode ? "#e0e0e0" : "#2C3E50";
    QString pathLabelColor = darkMode ? "#a0a0a0" : "#95A5A6";
    QString statsLabelColor = darkMode ? "#a0a0a0" : "#7F8C8D";
    QString progressBg = darkMode ? "#3d3d3d" : "#ECF0F1";
    QString cancelBtnBg = darkMode ? "#4a4a4a" : "#E5E8E8";
    QString cancelBtnColor = darkMode ? "#e0e0e0" : "#5D6D7E";
    QString cancelBtnHoverBg = darkMode ? "#5a5a5a" : "#D5DBDB";
    
    QVBoxLayout *layout = new QVBoxLayout(m_scanningPage);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(20);
    layout->setContentsMargins(60, 60, 60, 60);
    
    // 扫描图标/动画
    QLabel *iconLabel = new QLabel("🔍", m_scanningPage);
    iconLabel->setStyleSheet("font-size: 64px;");
    iconLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(iconLabel);
    
    // 扫描状态文字
    m_scanningLabel = new QLabel(tr("正在扫描磁盘中..."), m_scanningPage);
    m_scanningLabel->setStyleSheet(QString("font-size: 18px; font-weight: bold; color: %1;").arg(scanningLabelColor));
    m_scanningLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_scanningLabel);
    
    layout->addSpacing(10);
    
    // 进度条
    m_progressBar = new QProgressBar(m_scanningPage);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    m_progressBar->setFormat("%p%");
    m_progressBar->setFixedHeight(24);
    m_progressBar->setStyleSheet(
        QString("QProgressBar { "
        "   border: none; "
        "   border-radius: 12px; "
        "   background-color: %1; "
        "   text-align: center; "
        "   font-size: 12px; "
        "}"
        "QProgressBar::chunk { "
        "   background-color: #3498DB; "
        "   border-radius: 12px; "
        "}").arg(progressBg)
    );
    layout->addWidget(m_progressBar);
    
    // 当前扫描路径
    m_currentPathLabel = new QLabel("", m_scanningPage);
    m_currentPathLabel->setStyleSheet(QString("font-size: 11px; color: %1;").arg(pathLabelColor));
    m_currentPathLabel->setAlignment(Qt::AlignCenter);
    m_currentPathLabel->setWordWrap(true);
    layout->addWidget(m_currentPathLabel);
    
    // 统计信息
    m_statsLabel = new QLabel(tr("已扫描: 0 个文件 | 0 个目录"), m_scanningPage);
    m_statsLabel->setStyleSheet(QString("font-size: 12px; color: %1;").arg(statsLabelColor));
    m_statsLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_statsLabel);
    
    layout->addSpacing(30);
    
    // 取消按钮
    m_cancelScanBtn = new QPushButton(tr("取消扫描"), m_scanningPage);
    m_cancelScanBtn->setFixedSize(120, 36);
    m_cancelScanBtn->setCursor(Qt::PointingHandCursor);
    m_cancelScanBtn->setStyleSheet(
        QString("QPushButton { "
        "   background-color: %1; "
        "   color: %2; "
        "   border: none; "
        "   border-radius: 6px; "
        "   font-size: 13px; "
        "}"
        "QPushButton:hover { background-color: %3; }")
        .arg(cancelBtnBg, cancelBtnColor, cancelBtnHoverBg)
    );
    layout->addWidget(m_cancelScanBtn, 0, Qt::AlignCenter);
    
    layout->addStretch();
}

void SystemSlimmerWidget::initResultPage()
{
    m_resultPage = new QWidget(this);
    
    // 检测深色主题
    bool darkMode = isDarkTheme();
    QString titleColor = darkMode ? "#e0e0e0" : "#2C3E50";
    QString countLabelColor = darkMode ? "#c0c0c0" : "#5D6D7E";
    QString tableBg = darkMode ? "#2d2d2d" : "white";
    QString tableBorder = darkMode ? "#555555" : "#E5E8E8";
    QString tableGrid = darkMode ? "#3d3d3d" : "#ECF0F1";
    QString tableSelectedBg = darkMode ? "#3d5a7d" : "#D6EAF8";
    QString tableSelectedColor = darkMode ? "#e0e0e0" : "#2C3E50";
    QString headerBg = darkMode ? "#3d3d3d" : "#F8F9FA";
    QString headerBorder = darkMode ? "#555555" : "#E5E8E8";
    QString headerColor = darkMode ? "#e0e0e0" : "#2C3E50";
    QString checkBoxUncheckedBorder = darkMode ? "#666666" : "#95a5a6";
    QString checkBoxUncheckedBg = darkMode ? "#3d3d3d" : "white";
    
    QVBoxLayout *mainLayout = new QVBoxLayout(m_resultPage);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    
    // 结果标题
    QHBoxLayout *headerLayout = new QHBoxLayout();
    
    QLabel *resultTitle = new QLabel(tr("扫描结果"), m_resultPage);
    resultTitle->setStyleSheet(QString("font-size: 20px; font-weight: bold; color: %1;").arg(titleColor));
    headerLayout->addWidget(resultTitle);
    
    headerLayout->addStretch();
    
    // 重新扫描按钮
    m_rescanBtn = new QPushButton(tr("重新扫描"), m_resultPage);
    m_rescanBtn->setFixedSize(100, 32);
    m_rescanBtn->setStyleSheet(
        "QPushButton { "
        "   background-color: #3498DB; "
        "   color: white; "
        "   border: none; "
        "   border-radius: 4px; "
        "   font-size: 12px; "
        "}"
        "QPushButton:hover { background-color: #2980B9; }"
    );
    headerLayout->addWidget(m_rescanBtn);
    
    // 返回按钮
    m_backBtn = new QPushButton(tr("返回"), m_resultPage);
    m_backBtn->setFixedSize(80, 32);
    m_backBtn->setStyleSheet(
        "QPushButton { "
        "   background-color: #E74C3C; "
        "   color: white; "
        "   border: none; "
        "   border-radius: 4px; "
        "   font-size: 13px; "
        "   font-weight: bold; "
        "}"
        "QPushButton:hover { background-color: #C0392B; }"
    );
    headerLayout->addWidget(m_backBtn);
    
    mainLayout->addLayout(headerLayout);
    
    // 结果堆叠窗口
    m_resultStack = new QStackedWidget(m_resultPage);
    mainLayout->addWidget(m_resultStack, 1);
    
    // ========== 大文件结果页 ==========
    m_largeFileResultPage = new QWidget();
    QVBoxLayout *largeFileLayout = new QVBoxLayout(m_largeFileResultPage);
    largeFileLayout->setSpacing(10);
    largeFileLayout->setContentsMargins(0, 0, 0, 0);
    
    // 统计标签
    m_largeFileCountLabel = new QLabel(tr("找到 0 个大文件"), m_largeFileResultPage);
    m_largeFileCountLabel->setStyleSheet(QString("font-size: 13px; color: %1;").arg(countLabelColor));
    largeFileLayout->addWidget(m_largeFileCountLabel);
    
    // 大文件表格
    m_largeFileTable = new QTableWidget(m_largeFileResultPage);
    m_largeFileTable->setColumnCount(5);
    m_largeFileTable->setHorizontalHeaderLabels(
        QStringList() << tr("序号") << tr("选择") << tr("文件名") << tr("大小") << tr("修改时间"));
    m_largeFileTable->horizontalHeader()->setStretchLastSection(true);
    m_largeFileTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_largeFileTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_largeFileTable->setColumnWidth(0, 60);   // 序号列宽度
    m_largeFileTable->setColumnWidth(1, 60);   // 选择列宽度
    m_largeFileTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_largeFileTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_largeFileTable->setAlternatingRowColors(true);
    m_largeFileTable->verticalHeader()->setDefaultSectionSize(36);  // 行高
    m_largeFileTable->verticalHeader()->setVisible(false);  // 隐藏行号列
    
    // 添加右键菜单支持
    m_largeFileTable->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_largeFileTable, &QTableWidget::customContextMenuRequested, this, &SystemSlimmerWidget::onLargeFileContextMenu);
    
    m_largeFileTable->setStyleSheet(
        QString("QTableWidget { "
        "   border: 1px solid %1; "
        "   border-radius: 6px; "
        "   background-color: %2; "
        "   gridline-color: %3; "
        "   outline: none; "
        "}"
        "QTableWidget::item { padding: 4px; color: %4; }"
        "QTableWidget::item:selected { "
        "   background-color: %5; "
        "   color: %6; "
        "   border: none; "
        "   outline: none; "
        "}"
        "QTableWidget::item:focus { border: none; outline: none; }"
        "QTableWidget::item:selected QCheckBox::indicator:checked { "
        "   border: 2px solid #27ae60; "
        "   background: #27ae60; "
        "   border-radius: 3px; "
        "}"
        "QHeaderView::section { "
        "   background-color: %7; "
        "   padding: 8px; "
        "   border: none; "
        "   border-bottom: 2px solid %8; "
        "   font-weight: bold; "
        "   color: %9; "
        "}"
        "QCheckBox:focus { outline: none; }"
        "QPushButton:focus { outline: none; }")
        .arg(tableBorder, tableBg, tableGrid, titleColor, tableSelectedBg, tableSelectedColor, headerBg, headerBorder, headerColor)
    );
    largeFileLayout->addWidget(m_largeFileTable);
    
    // 操作按钮
    QHBoxLayout *largeFileBtnLayout = new QHBoxLayout();
    largeFileBtnLayout->addStretch();
    
    m_trashLargeFileBtn = new QPushButton(tr("🗑️ 移到回收站"), m_largeFileResultPage);
    m_trashLargeFileBtn->setFixedSize(130, 36);
    m_trashLargeFileBtn->setStyleSheet(
        "QPushButton { "
        "   background-color: #F39C12; "
        "   color: white; "
        "   border: none; "
        "   border-radius: 6px; "
        "   font-size: 12px; "
        "}"
        "QPushButton:hover { background-color: #D68910; }"
    );
    largeFileBtnLayout->addWidget(m_trashLargeFileBtn);
    
    m_deleteLargeFileBtn = new QPushButton(tr("🗑️ 永久删除"), m_largeFileResultPage);
    m_deleteLargeFileBtn->setFixedSize(120, 36);
    m_deleteLargeFileBtn->setStyleSheet(
        "QPushButton { "
        "   background-color: #E74C3C; "
        "   color: white; "
        "   border: none; "
        "   border-radius: 6px; "
        "   font-size: 12px; "
        "}"
        "QPushButton:hover { background-color: #C0392B; }"
    );
    largeFileBtnLayout->addWidget(m_deleteLargeFileBtn);
    
    largeFileLayout->addLayout(largeFileBtnLayout);
    
    m_resultStack->addWidget(m_largeFileResultPage);
    
    // ========== 重复文件结果页 ==========
    m_duplicateResultPage = new QWidget();
    QVBoxLayout *dupLayout = new QVBoxLayout(m_duplicateResultPage);
    dupLayout->setSpacing(10);
    dupLayout->setContentsMargins(0, 0, 0, 0);
    
    // 统计信息
    QHBoxLayout *dupStatsLayout = new QHBoxLayout();
    m_duplicateCountLabel = new QLabel(tr("找到 0 组重复文件"), m_duplicateResultPage);
    m_duplicateCountLabel->setStyleSheet(QString("font-size: 13px; color: %1;").arg(countLabelColor));
    dupStatsLayout->addWidget(m_duplicateCountLabel);
    
    dupStatsLayout->addStretch();
    
    m_duplicateSizeLabel = new QLabel(tr("可释放空间: 0 B"), m_duplicateResultPage);
    m_duplicateSizeLabel->setStyleSheet("font-size: 13px; color: #27AE60; font-weight: bold;");
    dupStatsLayout->addWidget(m_duplicateSizeLabel);
    
    dupLayout->addLayout(dupStatsLayout);
    
    // 重复文件列表
    m_duplicateTable = new QTableWidget(m_duplicateResultPage);
    m_duplicateTable->setColumnCount(5);
    m_duplicateTable->setHorizontalHeaderLabels(
        QStringList() << tr("序号") << tr("选择") << tr("文件路径") << tr("大小") << tr("组"));
    m_duplicateTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_duplicateTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_duplicateTable->setColumnWidth(0, 60);   // 序号列宽度
    m_duplicateTable->setColumnWidth(1, 60);   // 选择列宽度
    m_duplicateTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_duplicateTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    m_duplicateTable->setColumnWidth(3, 100);  // 大小列宽度
    m_duplicateTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
    m_duplicateTable->setColumnWidth(4, 60);   // 组列宽度
    m_duplicateTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_duplicateTable->setAlternatingRowColors(true);
    m_duplicateTable->verticalHeader()->setDefaultSectionSize(36);  // 行高
    m_duplicateTable->verticalHeader()->setVisible(false);  // 隐藏行号列
    
    // 添加右键菜单支持
    m_duplicateTable->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_duplicateTable, &QTableWidget::customContextMenuRequested, this, &SystemSlimmerWidget::onDuplicateFileContextMenu);
    
    m_duplicateTable->setStyleSheet(
        QString("QTableWidget { "
        "   border: 1px solid %1; "
        "   border-radius: 6px; "
        "   background-color: %2; "
        "   gridline-color: %3; "
        "   outline: none; "
        "}"
        "QTableWidget::item { padding: 4px; color: %4; }"
        "QTableWidget::item:selected { "
        "   background-color: %5; "
        "   color: %6; "
        "   border: none; "
        "   outline: none; "
        "}"
        "QTableWidget::item:focus { border: none; outline: none; }"
        "QHeaderView::section { "
        "   background-color: %7; "
        "   padding: 8px; "
        "   border: none; "
        "   border-bottom: 2px solid %8; "
        "   font-weight: bold; "
        "   color: %9; "
        "}"
        "QCheckBox:focus { outline: none; }"
        "QPushButton:focus { outline: none; }")
        .arg(tableBorder, tableBg, tableGrid, titleColor, tableSelectedBg, tableSelectedColor, headerBg, headerBorder, headerColor)
    );
    dupLayout->addWidget(m_duplicateTable);
    
    // 操作按钮
    QHBoxLayout *dupBtnLayout = new QHBoxLayout();
    dupBtnLayout->addStretch();
    
    m_trashDuplicateBtn = new QPushButton(tr("🗑️ 移到回收站"), m_duplicateResultPage);
    m_trashDuplicateBtn->setFixedSize(130, 36);
    m_trashDuplicateBtn->setStyleSheet(
        "QPushButton { "
        "   background-color: #F39C12; "
        "   color: white; "
        "   border: none; "
        "   border-radius: 6px; "
        "   font-size: 12px; "
        "}"
        "QPushButton:hover { background-color: #D68910; }"
    );
    dupBtnLayout->addWidget(m_trashDuplicateBtn);
    
    m_deleteDuplicateBtn = new QPushButton(tr("🗑️ 永久删除"), m_duplicateResultPage);
    m_deleteDuplicateBtn->setFixedSize(120, 36);
    m_deleteDuplicateBtn->setStyleSheet(
        "QPushButton { "
        "   background-color: #E74C3C; "
        "   color: white; "
        "   border: none; "
        "   border-radius: 6px; "
        "   font-size: 12px; "
        "}"
        "QPushButton:hover { background-color: #C0392B; }"
    );
    dupBtnLayout->addWidget(m_deleteDuplicateBtn);
    
    dupLayout->addLayout(dupBtnLayout);
    
    m_resultStack->addWidget(m_duplicateResultPage);
}

void SystemSlimmerWidget::setupConnections()
{
    // 功能卡片点击
    connect(m_largeFileCard, &FeatureCard::clicked, this, &SystemSlimmerWidget::onFeatureCardToggled);
    connect(m_duplicateCard, &FeatureCard::clicked, this, &SystemSlimmerWidget::onFeatureCardToggled);
    
    // 开始/取消扫描
    connect(m_startScanBtn, &QPushButton::clicked, this, &SystemSlimmerWidget::onStartScanClicked);
    connect(m_cancelScanBtn, &QPushButton::clicked, this, &SystemSlimmerWidget::onCancelScanClicked);
    
    // 扫描信号
    connect(m_slimmer, &SystemSlimmer::scanProgress, this, &SystemSlimmerWidget::onScanProgress);
    connect(m_slimmer, &SystemSlimmer::scanFinished, this, &SystemSlimmerWidget::onScanFinished);
    connect(m_slimmer, &SystemSlimmer::scanError, this, &SystemSlimmerWidget::onScanError);
    
    // 导航按钮
    connect(m_backBtn, &QPushButton::clicked, this, &SystemSlimmerWidget::onBackToMain);
    connect(m_rescanBtn, &QPushButton::clicked, this, &SystemSlimmerWidget::onStartScanClicked);
    
    // 删除操作
    connect(m_deleteLargeFileBtn, &QPushButton::clicked, this, &SystemSlimmerWidget::onDeleteSelected);
    connect(m_trashLargeFileBtn, &QPushButton::clicked, this, &SystemSlimmerWidget::onMoveToTrash);
    connect(m_deleteDuplicateBtn, &QPushButton::clicked, this, &SystemSlimmerWidget::onDeleteSelected);
    connect(m_trashDuplicateBtn, &QPushButton::clicked, this, &SystemSlimmerWidget::onMoveToTrash);
}

void SystemSlimmerWidget::onFeatureCardToggled()
{
    // 单选逻辑：检查哪个卡片被点击，取消另一个的选择
    QObject *sender = this->sender();
    
    if (sender == m_largeFileCard) {
        if (m_largeFileCard->isChecked()) {
            m_duplicateCard->setChecked(false);
        }
    } else if (sender == m_duplicateCard) {
        if (m_duplicateCard->isChecked()) {
            m_largeFileCard->setChecked(false);
        }
    }
    
    // 更新按钮状态
    bool hasSelection = m_largeFileCard->isChecked() || m_duplicateCard->isChecked();
    m_startScanBtn->setEnabled(hasSelection);
}

void SystemSlimmerWidget::onStartScanClicked()
{
    // 检查选择了哪些功能
    m_scanLargeFiles = m_largeFileCard->isChecked();
    m_scanDuplicates = m_duplicateCard->isChecked();
    
    if (!m_scanLargeFiles && !m_scanDuplicates) {
        return;
    }
    
    // 清理之前的扫描结果
    m_lastResult = SlimmerScanResult();
    m_largeFileTable->clearContents();
    m_largeFileTable->setRowCount(0);
    m_duplicateTable->clearContents();
    m_duplicateTable->setRowCount(0);
    
    // 设置扫描选项
    // 查找大文件最小限制为100MB
    m_currentOptions.minFileSize = 100LL * 1024 * 1024;
    // 默认扫描主目录
    m_currentOptions.searchPaths = QStringList() << QDir::homePath();
    m_currentOptions.excludePaths = QStringList();
    m_currentOptions.scanHiddenFiles = false;
    m_currentOptions.duplicateCompareMethod = 1; // 标准模式
    
    // 切换到扫描页面
    showScanningPage();
    
    // 开始扫描
    m_slimmer->startScan(m_currentOptions, m_scanLargeFiles, m_scanDuplicates);
}

void SystemSlimmerWidget::onCancelScanClicked()
{
    m_slimmer->stopScan();
    showMainPage();
}

void SystemSlimmerWidget::onScanProgress(const QString &currentPath, int percent, int fileCount, int largeFileCount)
{
    m_progressBar->setValue(percent);
    m_currentPathLabel->setText(currentPath);
    
    // 根据扫描模式显示不同的统计信息
    QString statsText;
    if (m_scanLargeFiles) {
        // 扫描大文件
        statsText = tr("已扫描 %1 个文件，发现 %2 个大文件")
            .arg(fileCount)
            .arg(largeFileCount);
    } else {
        // 扫描重复文件
        statsText = tr("已扫描 %1 个文件")
            .arg(fileCount);
    }
    
    m_statsLabel->setText(statsText);
}

void SystemSlimmerWidget::onScanFinished(const SlimmerScanResult &result)
{
    m_progressBar->setValue(100);
    m_lastResult = result;
    updateResultPage();
    showResultPage();
}

void SystemSlimmerWidget::onScanError(const QString &error)
{
    if (error != tr("扫描已取消")) {
        QMessageBox::warning(this, tr("扫描错误"), error);
    }
    showMainPage();
}

void SystemSlimmerWidget::updateResultPage()
{
    // 判断显示哪个结果页
    if (m_scanLargeFiles && !m_lastResult.largeFiles.isEmpty()) {
        m_resultStack->setCurrentWidget(m_largeFileResultPage);
        
        // 清理旧表格数据
        m_largeFileTable->clearContents();
        m_largeFileTable->setRowCount(0);
        
        // 填充大文件表格
        m_largeFileTable->setRowCount(m_lastResult.largeFiles.size());
        
        for (int i = 0; i < m_lastResult.largeFiles.size(); ++i) {
            const SlimmerLargeFileInfo &info = m_lastResult.largeFiles.at(i);
            
            // 序号
            QTableWidgetItem *indexItem = new QTableWidgetItem(QString::number(i + 1));
            indexItem->setTextAlignment(Qt::AlignCenter);
            indexItem->setFlags(indexItem->flags() & ~Qt::ItemIsEditable);
            m_largeFileTable->setItem(i, 0, indexItem);
            
            // 复选框（默认不选中）
            QCheckBox *checkBox = new QCheckBox();
            checkBox->setChecked(false);
            QString checkBoxUncheckedBorder = isDarkTheme() ? "#666666" : "#95a5a6";
            QString checkBoxUncheckedBg = isDarkTheme() ? "#3d3d3d" : "white";
            checkBox->setStyleSheet(
                QString("QCheckBox::indicator {"
                "   width: 16px;"
                "   height: 16px;"
                "}"
                "QCheckBox::indicator:unchecked {"
                "   border: 2px solid %1;"
                "   background: %2;"
                "   border-radius: 3px;"
                "}"
                "QCheckBox::indicator:checked {"
                "   border: 2px solid #3498db;"
                "   background: #3498db;"
                "   border-radius: 3px;"
                "   image: url(:/icons/check.svg);"
                "}").arg(checkBoxUncheckedBorder, checkBoxUncheckedBg)
            );
            QWidget *checkWidget = new QWidget();
            QHBoxLayout *checkLayout = new QHBoxLayout(checkWidget);
            checkLayout->addWidget(checkBox);
            checkLayout->setAlignment(Qt::AlignCenter);
            checkLayout->setContentsMargins(0, 0, 0, 0);
            checkWidget->setLayout(checkLayout);
            m_largeFileTable->setCellWidget(i, 1, checkWidget);
            
            // 文件名
            QTableWidgetItem *nameItem = new QTableWidgetItem(info.name);
            nameItem->setData(Qt::UserRole, info.path);
            nameItem->setToolTip(info.path);
            m_largeFileTable->setItem(i, 2, nameItem);
            
            // 大小
            QTableWidgetItem *sizeItem = new QTableWidgetItem(formatFileSize(info.size));
            sizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            m_largeFileTable->setItem(i, 3, sizeItem);
            
            // 修改时间
            m_largeFileTable->setItem(i, 4, new QTableWidgetItem(info.lastModified));
        }
        
        m_largeFileTable->resizeColumnsToContents();
        m_largeFileTable->setColumnWidth(0, 60);   // 保持序号列宽度
        m_largeFileTable->setColumnWidth(1, 60);   // 保持选择列宽度
        m_largeFileTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
        
        m_largeFileCountLabel->setText(tr("找到 %1 个大文件").arg(m_lastResult.largeFiles.size()));
        
    } else if (m_scanDuplicates && !m_lastResult.duplicateGroups.isEmpty()) {
        m_resultStack->setCurrentWidget(m_duplicateResultPage);
        
        // 清理旧表格数据
        m_duplicateTable->clearContents();
        m_duplicateTable->setRowCount(0);
        
        // 计算总行数和可释放空间
        int totalRows = 0;
        qint64 releasableSpace = 0;
        for (const auto &group : m_lastResult.duplicateGroups) {
            totalRows += group.paths.size();
            // 每组保留一个，其余可删除
            if (group.paths.size() > 1) {
                releasableSpace += group.size * (group.paths.size() - 1);
            }
        }
        
        m_duplicateTable->setRowCount(totalRows);
        
        int row = 0;
        int groupIndex = 1;
        for (const auto &group : m_lastResult.duplicateGroups) {
            for (int j = 0; j < group.paths.size(); ++j) {
                const QString &filePath = group.paths.at(j);
                QFileInfo fileInfo(filePath);
                
                // 序号
                QTableWidgetItem *indexItem = new QTableWidgetItem(QString::number(row + 1));
                indexItem->setTextAlignment(Qt::AlignCenter);
                indexItem->setFlags(indexItem->flags() & ~Qt::ItemIsEditable);
                m_duplicateTable->setItem(row, 0, indexItem);
                
                // 复选框（默认不选中，但每组第一个不勾选用于保留）
                QCheckBox *checkBox = new QCheckBox();
                checkBox->setChecked(j > 0);  // 每组第一个不选（保留），其余选中
                QString dupCheckBoxUncheckedBorder = isDarkTheme() ? "#666666" : "#95a5a6";
                QString dupCheckBoxUncheckedBg = isDarkTheme() ? "#3d3d3d" : "white";
                checkBox->setStyleSheet(
                    QString("QCheckBox::indicator {"
                    "   width: 16px;"
                    "   height: 16px;"
                    "}"
                    "QCheckBox::indicator:unchecked {"
                    "   border: 2px solid %1;"
                    "   background: %2;"
                    "   border-radius: 3px;"
                    "}"
                    "QCheckBox::indicator:checked {"
                    "   border: 2px solid #3498db;"
                    "   background: #3498db;"
                    "   border-radius: 3px;"
                    "   image: url(:/icons/check.svg);"
                    "}").arg(dupCheckBoxUncheckedBorder, dupCheckBoxUncheckedBg)
                );
                QWidget *checkWidget = new QWidget();
                QHBoxLayout *checkLayout = new QHBoxLayout(checkWidget);
                checkLayout->addWidget(checkBox);
                checkLayout->setAlignment(Qt::AlignCenter);
                checkLayout->setContentsMargins(0, 0, 0, 0);
                checkWidget->setLayout(checkLayout);
                m_duplicateTable->setCellWidget(row, 1, checkWidget);
                
                // 文件路径
                QTableWidgetItem *pathItem = new QTableWidgetItem(filePath);
                pathItem->setData(Qt::UserRole, filePath);  // 存储完整路径
                pathItem->setToolTip(filePath);
                m_duplicateTable->setItem(row, 2, pathItem);
                
                // 大小
                QTableWidgetItem *sizeItem = new QTableWidgetItem(formatFileSize(group.size));
                sizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                sizeItem->setData(Qt::UserRole, group.size);  // 存储字节大小
                m_duplicateTable->setItem(row, 3, sizeItem);
                
                // 组号
                QTableWidgetItem *groupItem = new QTableWidgetItem(QString::number(groupIndex));
                groupItem->setTextAlignment(Qt::AlignCenter);
                groupItem->setFlags(groupItem->flags() & ~Qt::ItemIsEditable);
                m_duplicateTable->setItem(row, 4, groupItem);
                
                row++;
            }
            groupIndex++;
        }
        
        m_duplicateTable->resizeColumnsToContents();
        m_duplicateTable->setColumnWidth(0, 60);
        m_duplicateTable->setColumnWidth(1, 60);
        m_duplicateTable->setColumnWidth(3, 100);
        m_duplicateTable->setColumnWidth(4, 60);
        m_duplicateTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
        
        m_duplicateCountLabel->setText(tr("找到 %1 组重复文件，共 %2 个文件")
            .arg(m_lastResult.duplicateGroups.size()).arg(totalRows));
        m_duplicateSizeLabel->setText(tr("可释放空间: %1").arg(formatFileSize(releasableSpace)));
        
    } else {
        // 没有结果
        QMessageBox::information(this, tr("扫描完成"), tr("未发现符合条件的文件。"));
        showMainPage();
    }
}

void SystemSlimmerWidget::onLargeFileSelected()
{
    // 计算选中文件总大小
    m_selectedLargeFilesTotalSize = 0;
    for (int i = 0; i < m_largeFileTable->rowCount(); ++i) {
        QWidget *checkWidget = m_largeFileTable->cellWidget(i, 1);
        QCheckBox *checkBox = checkWidget ? checkWidget->findChild<QCheckBox*>() : nullptr;
        if (checkBox && checkBox->isChecked()) {
            QString path = m_largeFileTable->item(i, 2)->data(Qt::UserRole).toString();
            QFileInfo info(path);
            m_selectedLargeFilesTotalSize += info.size();
        }
    }
}

void SystemSlimmerWidget::onDuplicateGroupSelected()
{
    // TODO: 处理重复文件组选择
}

void SystemSlimmerWidget::onDeleteSelected()
{
    int ret = QMessageBox::warning(this, tr("确认删除"), 
        tr("确定要永久删除选中的文件吗？此操作不可恢复！"),
        QMessageBox::Yes | QMessageBox::No);
    
    if (ret != QMessageBox::Yes) {
        return;
    }
    
    // 执行删除
    int deleted = 0;
    qint64 totalFreed = 0;
    QStringList failedFiles;
    QStringList deletedFiles;
    
    if (m_resultStack->currentWidget() == m_largeFileResultPage) {
        for (int i = m_largeFileTable->rowCount() - 1; i >= 0; --i) {
            QWidget *checkWidget = m_largeFileTable->cellWidget(i, 1);
            QCheckBox *checkBox = checkWidget ? checkWidget->findChild<QCheckBox*>() : nullptr;
            if (checkBox && checkBox->isChecked()) {
                QString path = m_largeFileTable->item(i, 2)->data(Qt::UserRole).toString();
                // 获取文件大小
                QFileInfo fileInfo(path);
                qint64 fileSize = fileInfo.exists() ? fileInfo.size() : 0;
                QString error;
                if (SystemSlimmer::deleteFile(path, &error)) {
                    m_largeFileTable->removeRow(i);
                    deleted++;
                    totalFreed += fileSize;
                    deletedFiles.append(path);
                } else {
                    failedFiles.append(path);
                }
            }
        }
    } else if (m_resultStack->currentWidget() == m_duplicateResultPage) {
        for (int i = m_duplicateTable->rowCount() - 1; i >= 0; --i) {
            QWidget *checkWidget = m_duplicateTable->cellWidget(i, 1);
            QCheckBox *checkBox = checkWidget ? checkWidget->findChild<QCheckBox*>() : nullptr;
            if (checkBox && checkBox->isChecked()) {
                QString path = m_duplicateTable->item(i, 2)->data(Qt::UserRole).toString();
                // 获取文件大小
                QFileInfo fileInfo(path);
                qint64 fileSize = fileInfo.exists() ? fileInfo.size() : 0;
                QString error;
                if (SystemSlimmer::deleteFile(path, &error)) {
                    m_duplicateTable->removeRow(i);
                    deleted++;
                    totalFreed += fileSize;
                    deletedFiles.append(path);
                } else {
                    failedFiles.append(path);
                }
            }
        }
    }
    
    // 记录清理历史
    if (deleted > 0 || !failedFiles.isEmpty()) {
        CleanupHistoryWidget::addHistory(tr("系统瘦身-删除文件"), totalFreed, deleted, failedFiles.size(), deletedFiles);
        emit historyChanged();
    }
    
    if (!failedFiles.isEmpty()) {
        QMessageBox::warning(this, tr("删除完成"), 
            tr("成功删除 %1 个文件，%2 个文件删除失败。").arg(deleted).arg(failedFiles.size()));
    } else {
        QMessageBox::information(this, tr("删除完成"), 
            tr("成功删除 %1 个文件。").arg(deleted));
    }
}

void SystemSlimmerWidget::onMoveToTrash()
{
    // 执行移动到回收站
    int moved = 0;
    qint64 totalFreed = 0;
    QStringList failedFiles;
    QStringList movedFiles;
    
    if (m_resultStack->currentWidget() == m_largeFileResultPage) {
        for (int i = m_largeFileTable->rowCount() - 1; i >= 0; --i) {
            QWidget *checkWidget = m_largeFileTable->cellWidget(i, 1);
            QCheckBox *checkBox = checkWidget ? checkWidget->findChild<QCheckBox*>() : nullptr;
            if (checkBox && checkBox->isChecked()) {
                QString path = m_largeFileTable->item(i, 2)->data(Qt::UserRole).toString();
                // 获取文件大小
                QFileInfo fileInfo(path);
                qint64 fileSize = fileInfo.exists() ? fileInfo.size() : 0;
                QString error;
                if (SystemSlimmer::moveToTrash(path, &error)) {
                    m_largeFileTable->removeRow(i);
                    moved++;
                    totalFreed += fileSize;
                    movedFiles.append(path);
                } else {
                    failedFiles.append(path);
                }
            }
        }
    } else if (m_resultStack->currentWidget() == m_duplicateResultPage) {
        for (int i = m_duplicateTable->rowCount() - 1; i >= 0; --i) {
            QWidget *checkWidget = m_duplicateTable->cellWidget(i, 1);
            QCheckBox *checkBox = checkWidget ? checkWidget->findChild<QCheckBox*>() : nullptr;
            if (checkBox && checkBox->isChecked()) {
                QString path = m_duplicateTable->item(i, 2)->data(Qt::UserRole).toString();
                // 获取文件大小
                QFileInfo fileInfo(path);
                qint64 fileSize = fileInfo.exists() ? fileInfo.size() : 0;
                QString error;
                if (SystemSlimmer::moveToTrash(path, &error)) {
                    m_duplicateTable->removeRow(i);
                    moved++;
                    totalFreed += fileSize;
                    movedFiles.append(path);
                } else {
                    failedFiles.append(path);
                }
            }
        }
    }
    
    // 记录清理历史
    if (moved > 0 || !failedFiles.isEmpty()) {
        CleanupHistoryWidget::addHistory(tr("系统瘦身-移至回收站"), totalFreed, moved, failedFiles.size(), movedFiles);
        emit historyChanged();
    }
    
    if (!failedFiles.isEmpty()) {
        QMessageBox::warning(this, tr("移动完成"), 
            tr("成功移动 %1 个文件到回收站，%2 个文件失败。").arg(moved).arg(failedFiles.size()));
    } else {
        QMessageBox::information(this, tr("移动完成"), 
            tr("成功移动 %1 个文件到回收站。").arg(moved));
    }
}

void SystemSlimmerWidget::onBackToMain()
{
    showMainPage();
}

void SystemSlimmerWidget::showMainPage()
{
    m_stackWidget->setCurrentWidget(m_mainPage);
}

void SystemSlimmerWidget::showScanningPage()
{
    m_progressBar->setValue(0);
    m_currentPathLabel->clear();
    m_stackWidget->setCurrentWidget(m_scanningPage);
}

void SystemSlimmerWidget::showResultPage()
{
    m_stackWidget->setCurrentWidget(m_resultPage);
}

QString SystemSlimmerWidget::formatFileSize(qint64 bytes)
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

void SystemSlimmerWidget::applyTheme()
{
    // 主题适配已在各控件样式中设置
}

bool SystemSlimmerWidget::isDarkTheme()
{
    QPalette palette = qApp->palette();
    QColor windowColor = palette.color(QPalette::Window);
    int brightness = (windowColor.red() * 299 + windowColor.green() * 587 + windowColor.blue() * 114) / 1000;
    return brightness < 128;
}

void SystemSlimmerWidget::onLargeFileContextMenu(const QPoint &pos)
{
    // 获取被右键点击的行
    QTableWidgetItem *item = m_largeFileTable->itemAt(pos);
    if (!item) {
        return;
    }
    
    int row = item->row();
    
    // 获取路径（存储在第2列的 UserRole 中）
    QTableWidgetItem *nameItem = m_largeFileTable->item(row, 2);
    if (!nameItem) {
        return;
    }
    
    QString path = nameItem->data(Qt::UserRole).toString();
    if (path.isEmpty()) {
        return;
    }
    
    // 创建右键菜单
    QMenu contextMenu(this);
    QAction *openFolderAction = contextMenu.addAction(tr("📂 打开所在文件夹"));
    QAction *copyPathAction = contextMenu.addAction(tr("📋 复制路径"));
    
    // 显示菜单并获取选中的动作
    QAction *selectedAction = contextMenu.exec(m_largeFileTable->viewport()->mapToGlobal(pos));
    
    if (selectedAction == openFolderAction) {
        // 打开文件夹
        QFileInfo fileInfo(path);
        QString dirPath = fileInfo.dir().absolutePath();
        
        QDesktopServices::openUrl(QUrl::fromLocalFile(dirPath));
    } else if (selectedAction == copyPathAction) {
        // 复制路径到剪贴板
        QApplication::clipboard()->setText(path);
    }
}

void SystemSlimmerWidget::onDuplicateFileContextMenu(const QPoint &pos)
{
    // 获取被右键点击的行
    QTableWidgetItem *item = m_duplicateTable->itemAt(pos);
    if (!item) {
        return;
    }
    
    int row = item->row();
    
    // 获取路径（存储在第2列的 UserRole 中）
    QTableWidgetItem *nameItem = m_duplicateTable->item(row, 2);
    if (!nameItem) {
        return;
    }
    
    QString path = nameItem->data(Qt::UserRole).toString();
    if (path.isEmpty()) {
        return;
    }
    
    // 创建右键菜单
    QMenu contextMenu(this);
    QAction *openFolderAction = contextMenu.addAction(tr("📂 打开所在文件夹"));
    QAction *copyPathAction = contextMenu.addAction(tr("📋 复制路径"));
    
    // 显示菜单并获取选中的动作
    QAction *selectedAction = contextMenu.exec(m_duplicateTable->viewport()->mapToGlobal(pos));
    
    if (selectedAction == openFolderAction) {
        // 打开文件夹
        QFileInfo fileInfo(path);
        QString dirPath = fileInfo.dir().absolutePath();
        
        QDesktopServices::openUrl(QUrl::fromLocalFile(dirPath));
    } else if (selectedAction == copyPathAction) {
        // 复制路径到剪贴板
        QApplication::clipboard()->setText(path);
    }
}
