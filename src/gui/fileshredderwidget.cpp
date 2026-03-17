/*
 * File Shredder Widget - Implementation
 * 文件粉碎组件 - 实现
 */

#include "fileshredderwidget.h"
#include "../utils/logger.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QUrl>
#include <QFileInfo>
#include <QApplication>
#include <QThread>
#include <QFrame>
#include <QInputDialog>

// ============================================================================
// ShredWorker Implementation
// ============================================================================

ShredWorker::ShredWorker(const QStringList &files, int passes, bool usePrivilege, QObject *parent)
    : QObject(parent)
    , m_files(files)
    , m_passes(passes)
    , m_usePrivilege(usePrivilege)
{
}

void ShredWorker::doWork()
{
    FileShredder shredder;
    shredder.setPasses(m_passes);
    shredder.setUsePrivilege(m_usePrivilege);

    connect(&shredder, &FileShredder::progress, this, &ShredWorker::progress);

    QList<ShredResult> results = shredder.shredFiles(m_files);
    emit finished(results);
}

// ============================================================================
// FileShredderWidget Implementation
// ============================================================================

FileShredderWidget::FileShredderWidget(QWidget *parent)
    : QWidget(parent)
    , m_shredder(nullptr)
    , m_workerThread(nullptr)
    , m_totalSize(0)
    , m_isProcessing(false)
{
    LOG_INFO("Initializing file shredder widget");
    initUI();
    applyTheme();
    setAcceptDrops(true);
}

FileShredderWidget::~FileShredderWidget()
{
    if (m_workerThread && m_workerThread->isRunning()) {
        m_workerThread->quit();
        m_workerThread->wait();
    }
}

void FileShredderWidget::initUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(20);
    mainLayout->setContentsMargins(25, 25, 25, 25);

    // ========== 标题卡片 ==========
    QFrame *headerCard = new QFrame(this);
    headerCard->setObjectName("headerCard");
    headerCard->setStyleSheet(
        "QFrame#headerCard { "
        "   background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #E74C3C, stop:1 #27AE60); "
        "   border-radius: 12px; "
        "   padding: 0px; "
        "}"
    );

    QHBoxLayout *headerLayout = new QHBoxLayout(headerCard);
    headerLayout->setContentsMargins(20, 15, 20, 15);
    headerLayout->setSpacing(10);

    // 左侧：图标+ 标题
    QLabel *iconLabel = new QLabel("🗑️", this);
    iconLabel->setStyleSheet("font-size: 24px; background: transparent;");
    headerLayout->addWidget(iconLabel);

    m_titleLabel = new QLabel(tr("文件粉碎"), this);
    m_titleLabel->setStyleSheet("font-size: 20px; font-weight: bold; color: white; background: transparent;");
    headerLayout->addWidget(m_titleLabel);

    headerLayout->addStretch();

    // 右侧：描述文字
    QLabel *descLabel = new QLabel(tr("安全彻底删除文件，多次覆写确保数据永久不可恢复"), this);
    descLabel->setStyleSheet("font-size: 12px; color: rgba(255,255,255,0.9); background: transparent;");
    headerLayout->addWidget(descLabel);

    mainLayout->addWidget(headerCard);

    // ========== 拖放区域（紧凑布局）==========
    QFrame *dropCard = new QFrame(this);
    dropCard->setObjectName("dropCard");
    dropCard->setMaximumHeight(80);
    dropCard->setStyleSheet(
        "QFrame#dropCard { "
        "   background-color: #F8F9FA; "
        "   border: 2px dashed #BDC3C7; "
        "   border-radius: 8px; "
        "}"
        "QFrame#dropCard:hover { "
        "   border-color: #0081FF; "
        "   background-color: #E8F4FD; "
        "}"
    );

    QHBoxLayout *dropLayout = new QHBoxLayout(dropCard);
    dropLayout->setAlignment(Qt::AlignCenter);
    dropLayout->setSpacing(10);
    dropLayout->setContentsMargins(10, 5, 10, 5);

    // 拖放图标
    QLabel *dropIcon = new QLabel("📁", this);
    dropIcon->setStyleSheet("font-size: 28px; background: transparent;");
    dropLayout->addWidget(dropIcon);

    // 拖放提示文字
    QVBoxLayout *dropTextLayout = new QVBoxLayout();
    dropTextLayout->setSpacing(2);
    dropTextLayout->setContentsMargins(0, 0, 0, 0);

    m_dropAreaLabel = new QLabel(tr("拖拽文件或文件夹到此处"), dropCard);
    m_dropAreaLabel->setStyleSheet("font-size: 14px; color: #5D6D7E; font-weight: 500; background: transparent;");
    dropTextLayout->addWidget(m_dropAreaLabel);

    QLabel *dropHint = new QLabel(tr("或点击下方按钮添加"), dropCard);
    dropHint->setStyleSheet("font-size: 11px; color: #95A5A6; background: transparent;");
    dropTextLayout->addWidget(dropHint);

    dropLayout->addLayout(dropTextLayout);
    mainLayout->addWidget(dropCard);

    // ========== 操作按钮区域（紧凑版）==========
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(8);

    m_addFilesBtn = new QPushButton("➕ " + tr("添加文件"), this);
    m_addFilesBtn->setMinimumHeight(32);
    m_addFilesBtn->setMaximumWidth(120);
    m_addFilesBtn->setCursor(Qt::PointingHandCursor);
    m_addFilesBtn->setStyleSheet(
        "QPushButton { "
        "   background-color: #0081FF; "
        "   color: white; "
        "   border: none; "
        "   border-radius: 6px; "
        "   font-size: 12px; "
        "   font-weight: 500; "
        "   padding: 6px 12px; "
        "   outline: none; "
        "} "
        "QPushButton:hover { background-color: #0066CC; border: none; outline: none; }"
        "QPushButton:pressed { background-color: #0052A3; border: none; outline: none; }"
        "QPushButton:focus { outline: none; border: none; }"
    );
    connect(m_addFilesBtn, &QPushButton::clicked, this, &FileShredderWidget::onAddFilesClicked);
    buttonLayout->addWidget(m_addFilesBtn);

    m_addFolderBtn = new QPushButton("📂 " + tr("添加文件夹"), this);
    m_addFolderBtn->setMinimumHeight(32);
    m_addFolderBtn->setMaximumWidth(130);
    m_addFolderBtn->setCursor(Qt::PointingHandCursor);
    m_addFolderBtn->setStyleSheet(
        "QPushButton { "
        "   background-color: #00C6FF; "
        "   color: white; "
        "   border: none; "
        "   border-radius: 6px; "
        "   font-size: 12px; "
        "   font-weight: 500; "
        "   padding: 6px 12px; "
        "} "
        "QPushButton:hover { background-color: #00A8D6; }"
        "QPushButton:pressed { background-color: #008FB3; }"
    );
    connect(m_addFolderBtn, &QPushButton::clicked, this, &FileShredderWidget::onAddFolderClicked);
    buttonLayout->addWidget(m_addFolderBtn);

    // 中间间距
    buttonLayout->addSpacing(40);

    m_removeSelectedBtn = new QPushButton("❌ " + tr("移除选中"), this);
    m_removeSelectedBtn->setMinimumHeight(32);
    m_removeSelectedBtn->setMaximumWidth(110);
    m_removeSelectedBtn->setCursor(Qt::PointingHandCursor);
    m_removeSelectedBtn->setStyleSheet(
        "QPushButton { "
        "   background-color: #5DADE2; "
        "   color: white; "
        "   border: none; "
        "   border-radius: 6px; "
        "   font-size: 12px; "
        "   font-weight: 500; "
        "   padding: 6px 12px; "
        "} "
        "QPushButton:hover { background-color: #3498DB; }"
        "QPushButton:pressed { background-color: #2E86C1; }"
        "QPushButton:disabled { background-color: #BDC3C7; color: #7F8C8D; }"
    );
    connect(m_removeSelectedBtn, &QPushButton::clicked, this, &FileShredderWidget::onRemoveSelectedClicked);
    buttonLayout->addWidget(m_removeSelectedBtn);

    m_clearListBtn = new QPushButton("🗑️ " + tr("清空"), this);
    m_clearListBtn->setMinimumHeight(32);
    m_clearListBtn->setMaximumWidth(80);
    m_clearListBtn->setCursor(Qt::PointingHandCursor);
    m_clearListBtn->setStyleSheet(
        "QPushButton { "
        "   background-color: #AED6F1; "
        "   color: #2C3E50; "
        "   border: none; "
        "   border-radius: 6px; "
        "   font-size: 12px; "
        "   font-weight: 500; "
        "   padding: 6px 12px; "
        "} "
        "QPushButton:hover { background-color: #85C1E9; }"
        "QPushButton:pressed { background-color: #5DADE2; }"
        "QPushButton:disabled { background-color: #BDC3C7; color: #7F8C8D; }"
    );
    connect(m_clearListBtn, &QPushButton::clicked, this, &FileShredderWidget::onClearListClicked);
    buttonLayout->addWidget(m_clearListBtn);

    // 使按钮布局居中
    QHBoxLayout *centeredButtonLayout = new QHBoxLayout();
    centeredButtonLayout->addStretch();
    centeredButtonLayout->addLayout(buttonLayout);
    centeredButtonLayout->addStretch();

    mainLayout->addLayout(centeredButtonLayout);

    // ========== 文件列表卡片 ==========
    QFrame *listCard = new QFrame(this);
    listCard->setObjectName("listCard");
    listCard->setStyleSheet(
        "QFrame#listCard { "
        "   background-color: white; "
        "   border: 1px solid #E5E8E8; "
        "   border-radius: 12px; "
        "}"
    );
    
    QVBoxLayout *listCardLayout = new QVBoxLayout(listCard);
    listCardLayout->setContentsMargins(15, 15, 15, 15);
    listCardLayout->setSpacing(12);
    
    // 列表头部
    QHBoxLayout *listHeaderLayout = new QHBoxLayout();
    QLabel *listLabel = new QLabel("📋 " + tr("待粉碎文件列表"), this);
    listLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: #2C3E50; background: transparent;");
    listHeaderLayout->addWidget(listLabel);
    listHeaderLayout->addStretch();
    
    // 总大小显示
    m_totalSizeLabel = new QLabel(tr("总大小: 0 B | 0 个项目"), this);
    m_totalSizeLabel->setStyleSheet("font-size: 12px; color: #7F8C8D; background: transparent;");
    listHeaderLayout->addWidget(m_totalSizeLabel);
    listCardLayout->addLayout(listHeaderLayout);
    
    // 文件列表 - 占据更多空间
    m_fileList = new QListWidget(this);
    m_fileList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_fileList->setMinimumHeight(200);
    m_fileList->setFrameStyle(QFrame::NoFrame);
    m_fileList->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_fileList->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_fileList->setStyleSheet(
        "QListWidget { "
        "   background-color: transparent; "
        "   border: none; "
        "   padding: 0px; "
        "} "
        "QListWidget::item { "
        "   padding: 10px 12px; "
        "   margin: 3px 0px; "
        "   background-color: #F8F9FA; "
        "   border-radius: 6px; "
        "   border-left: 4px solid #0081FF; "
        "   font-size: 13px; "
        "} "
        "QListWidget::item:hover { "
        "   background-color: #EBF5FB; "
        "} "
        "QListWidget::item:selected { "
        "   background-color: #D4E6F1; "
        "   color: #2C3E50; "
        "   border-left: 4px solid #0052A3; "
        "}"
        "QScrollBar:vertical { "
        "   background-color: #F0F0F0; "
        "   width: 10px; "
        "   border-radius: 5px; "
        "} "
        "QScrollBar::handle:vertical { "
        "   background-color: #BDC3C7; "
        "   border-radius: 5px; "
        "   min-height: 20px; "
        "} "
        "QScrollBar::handle:vertical:hover { "
        "   background-color: #95A5A6; "
        "} "
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { "
        "   height: 0px; "
        "}"
    );
    listCardLayout->addWidget(m_fileList, 1);

    // 进度条
    m_progressBar = new QProgressBar(this);
    m_progressBar->setTextVisible(true);
    m_progressBar->setFormat("%p%");
    m_progressBar->setStyleSheet(
        "QProgressBar { "
        "   border: none; "
        "   border-radius: 6px; "
        "   background-color: #ECF0F1; "
        "   text-align: center; "
        "   font-size: 12px; "
        "   font-weight: bold; "
        "   height: 20px; "
        "} "
        "QProgressBar::chunk { "
        "   background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #3498DB, stop:1 #2980B9); "
        "   border-radius: 6px; "
        "}"
    );
    m_progressBar->setVisible(false);
    listCardLayout->addWidget(m_progressBar);
    
    mainLayout->addWidget(listCard, 1);

    // ========== 粉碎按钮和警告 ==========
    // 粉碎按钮
    m_shredBtn = new QPushButton("🔥 " + tr("开始粉碎"), this);
    m_shredBtn->setMinimumHeight(55);
    m_shredBtn->setCursor(Qt::PointingHandCursor);
    m_shredBtn->setStyleSheet(
        "QPushButton { "
        "   background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #E74C3C, stop:1 #C0392B); "
        "   color: white; "
        "   border: none; "
        "   border-radius: 10px; "
        "   font-size: 16px; "
        "   font-weight: bold; "
        "   padding: 14px 28px; "
        "} "
        "QPushButton:hover { "
        "   background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #C0392B, stop:1 #A93226); "
        "} "
        "QPushButton:pressed { "
        "   background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #A93226, stop:1 #922B21); "
        "} "
        "QPushButton:disabled { "
        "   background-color: #BDC3C7; "
        "}"
    );
    connect(m_shredBtn, &QPushButton::clicked, this, &FileShredderWidget::onShredClicked);
    mainLayout->addWidget(m_shredBtn);
    
    // 权限提升选项 - 使用卡片式开关
    QFrame *privilegeCard = new QFrame(this);
    privilegeCard->setObjectName("privilegeCard");
    privilegeCard->setStyleSheet(
        "QFrame#privilegeCard { "
        "   background-color: #F8F9FA; "
        "   border: 1px solid #E5E8E8; "
        "   border-radius: 8px; "
        "}"
    );
    
    QHBoxLayout *privilegeLayout = new QHBoxLayout(privilegeCard);
    privilegeLayout->setContentsMargins(15, 10, 20, 10);  // 增加右边距
    privilegeLayout->setSpacing(10);
    
    // 图标
    QLabel *privilegeIcon = new QLabel("🔐", this);
    privilegeIcon->setStyleSheet("font-size: 20px; background: transparent;");
    privilegeIcon->setFixedWidth(28);
    privilegeLayout->addWidget(privilegeIcon);

    // 标题（包含描述文字）
    QLabel *privilegeTitle = new QLabel(tr("权限提升（处理只读、root权限、immutable属性等顽固文件）"), this);
    privilegeTitle->setStyleSheet("font-size: 13px; font-weight: bold; color: #2C3E50; background: transparent;");
    privilegeLayout->addWidget(privilegeTitle);

    privilegeLayout->addStretch();
    privilegeLayout->addStretch();
    
    // 右侧容器：状态标签 + 开关
    QHBoxLayout *rightLayout = new QHBoxLayout();
    rightLayout->setSpacing(8);
    
    // 状态标签
    QLabel *statusLabel = new QLabel(tr("已开启"), this);
    statusLabel->setObjectName("statusLabel");
    statusLabel->setStyleSheet("font-size: 12px; color: #0081FF; font-weight: bold; background: transparent;");
    statusLabel->setFixedWidth(50);  // 固定宽度避免抖动
    statusLabel->setAlignment(Qt::AlignCenter);
    rightLayout->addWidget(statusLabel);
    
    // 开关按钮 - 现代风格
    m_privilegeCheckBox = new QCheckBox(this);
    m_privilegeCheckBox->setChecked(true);  // 默认启用
    m_privilegeCheckBox->setFixedSize(44, 22);
    m_privilegeCheckBox->setCursor(Qt::PointingHandCursor);
    m_privilegeCheckBox->setStyleSheet(
        "QCheckBox { "
        "   background: transparent; "
        "   spacing: 0px; "
        "} "
        "QCheckBox::indicator { "
        "   width: 44px; "
        "   height: 22px; "
        "   border-radius: 11px; "
        "   border: 2px solid transparent; "
        "} "
        "QCheckBox::indicator:unchecked { "
        "   background-color: #E0E0E0; "
        "   border: 2px solid #BDBDBD; "
        "} "
        "QCheckBox::indicator:unchecked:hover { "
        "   background-color: #BDBDBD; "
        "   border: 2px solid #9E9E9E; "
        "} "
        "QCheckBox::indicator:checked { "
        "   background-color: #0081FF; "
        "   border: 2px solid #0066CC; "
        "} "
        "QCheckBox::indicator:checked:hover { "
        "   background-color: #0066CC; "
        "   border: 2px solid #0052A3; "
        "}"
    );
    rightLayout->addWidget(m_privilegeCheckBox);
    
    // 连接状态变化信号更新文字和样式
    connect(m_privilegeCheckBox, &QCheckBox::checkStateChanged, this, [statusLabel](Qt::CheckState state) {
        if (state == Qt::Checked) {
            statusLabel->setText(QObject::tr("已开启"));
            statusLabel->setStyleSheet("font-size: 12px; color: #0081FF; font-weight: bold; background: transparent;");
        } else {
            statusLabel->setText(QObject::tr("已关闭"));
            statusLabel->setStyleSheet("font-size: 12px; color: #95A5A6; font-weight: bold; background: transparent;");
        }
    });
    
    privilegeLayout->addLayout(rightLayout);
    
    mainLayout->addWidget(privilegeCard);

    // 警告标签
    m_warningFrame = new QFrame(this);
    m_warningFrame->setObjectName("warningFrame");

    QHBoxLayout *warningLayout = new QHBoxLayout(m_warningFrame);
    warningLayout->setContentsMargins(15, 6, 15, 6);

    warningLayout->addStretch();

    QLabel *warningIcon = new QLabel("⚠️", this);
    warningIcon->setObjectName("warningIcon");
    warningIcon->setStyleSheet("font-size: 16px; background: transparent;");
    warningLayout->addWidget(warningIcon);

    m_warningLabel = new QLabel(
        tr("警告：文件粉碎后无法恢复！此操作不可逆，请谨慎操作！"), this);
    m_warningLabel->setObjectName("warningLabel");
    warningLayout->addWidget(m_warningLabel);

    warningLayout->addStretch();

    mainLayout->addWidget(m_warningFrame);
}

void FileShredderWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
        m_dropAreaLabel->setText(tr("📂 释放鼠标添加文件"));
        m_dropAreaLabel->setStyleSheet("font-size: 16px; color: #27ae60; font-weight: bold;");
    }
}

void FileShredderWidget::dragMoveEvent(QDragMoveEvent *event)
{
    event->acceptProposedAction();
}

void FileShredderWidget::dropEvent(QDropEvent *event)
{
    m_dropAreaLabel->setText(tr("📂 拖拽文件或文件夹到此处"));
    m_dropAreaLabel->setStyleSheet("font-size: 16px; color: #666;");

    QList<QUrl> urls = event->mimeData()->urls();
    QStringList files;

    for (const QUrl &url : urls) {
        QString path = url.toLocalFile();
        if (!path.isEmpty()) {
            files.append(path);
        }
    }

    if (!files.isEmpty()) {
        addFilesToList(files);
    }

    event->acceptProposedAction();
}

void FileShredderWidget::onAddFilesClicked()
{
    QStringList files = QFileDialog::getOpenFileNames(
        this, tr("选择要粉碎的文件"), QDir::homePath(), tr("所有文件 (*.*)"));

    if (!files.isEmpty()) {
        addFilesToList(files);
    }
}

void FileShredderWidget::onAddFolderClicked()
{
    QString folder = QFileDialog::getExistingDirectory(
        this, tr("选择要粉碎的文件夹"), QDir::homePath());

    if (!folder.isEmpty()) {
        addFilesToList(QStringList() << folder);
    }
}

void FileShredderWidget::onClearListClicked()
{
    m_fileList->clear();
    m_totalSize = 0;
    updateTotalSize();
}

void FileShredderWidget::onRemoveSelectedClicked()
{
    QList<QListWidgetItem *> selectedItems = m_fileList->selectedItems();
    for (QListWidgetItem *item : selectedItems) {
        delete m_fileList->takeItem(m_fileList->row(item));
    }
    updateTotalSize();
}

void FileShredderWidget::addFilesToList(const QStringList &files)
{
    FileShredder shredder;
    int addedCount = 0;

    for (const QString &file : files) {
        QFileInfo info(file);
        if (!info.exists()) {
            continue;
        }

        // 检查是否受保护
        QString reason;
        if (!shredder.canShred(file, reason)) {
            QMessageBox::warning(this, tr("无法添加"),
                tr("无法粉碎 %1:\n%2").arg(file).arg(reason));
            continue;
        }

        // 检查是否已存在
        bool exists = false;
        for (int i = 0; i < m_fileList->count(); i++) {
            if (m_fileList->item(i)->data(Qt::UserRole).toString() == file) {
                exists = true;
                break;
            }
        }
        if (exists) {
            continue;
        }

        // 添加到列表
        QString displayText;
        QString tooltipText;
        qint64 size = 0;

        if (info.isDir()) {
            displayText = QString("📁 %1").arg(info.fileName());
            tooltipText = QString("📁 %1\n路径: %2\n类型: 文件夹").arg(info.fileName()).arg(file);
            size = 0;
        } else {
            displayText = QString("📄 %1 (%2)").arg(info.fileName()).arg(formatSize(info.size()));
            tooltipText = QString("📄 %1\n路径: %2\n大小: %3\n类型: %4")
                .arg(info.fileName())
                .arg(file)
                .arg(formatSize(info.size()))
                .arg(info.suffix().toUpper());
            size = info.size();
        }

        QListWidgetItem *item = new QListWidgetItem(displayText, m_fileList);
        item->setData(Qt::UserRole, file);
        item->setData(Qt::UserRole + 1, size);
        item->setToolTip(tooltipText);
        m_fileList->addItem(item);

        m_totalSize += size;
        addedCount++;
    }

    updateTotalSize();
}

void FileShredderWidget::updateTotalSize()
{
    m_totalSize = 0;
    for (int i = 0; i < m_fileList->count(); i++) {
        m_totalSize += m_fileList->item(i)->data(Qt::UserRole + 1).toLongLong();
    }
    m_totalSizeLabel->setText(tr("总大小: %1 (%2 个项目)")
        .arg(formatSize(m_totalSize))
        .arg(m_fileList->count()));
}

void FileShredderWidget::onShredClicked()
{
    if (m_fileList->count() == 0) {
        QMessageBox::information(this, tr("提示"), tr("请先添加要粉碎的文件"));
        return;
    }

    // 获取覆写次数
    bool ok;
    int passes = QInputDialog::getInt(this, tr("安全设置"),
        tr("覆写次数 (1-35):"), 3, 1, 35, 1, &ok);

    if (!ok) {
        return;
    }

    // 获取权限提升选项
    bool usePrivilege = m_privilegeCheckBox->isChecked();

    // 确认对话框
    QString fileListText;
    int count = qMin(m_fileList->count(), 10);
    for (int i = 0; i < count; i++) {
        fileListText += QString("• %1\n")
            .arg(m_fileList->item(i)->data(Qt::UserRole).toString());
    }
    if (m_fileList->count() > 10) {
        fileListText += tr("... 还有 %1 个项目\n").arg(m_fileList->count() - 10);
    }

    QString privilegeHint = usePrivilege ? 
        tr("\n💡 权限提升已启用，可处理顽固文件") : 
        tr("\n⚠️ 权限提升未启用，顽固文件可能无法删除");

    QMessageBox confirmBox(this);
    confirmBox.setIcon(QMessageBox::Warning);
    confirmBox.setWindowTitle(tr("确认粉碎"));
    confirmBox.setText(tr("即将粉碎以下 %1 个文件/文件夹:\n\n%2")
        .arg(m_fileList->count()).arg(fileListText));
    confirmBox.setInformativeText(tr("⚠️ 此操作不可逆！文件将无法恢复！%1\n是否继续？").arg(privilegeHint));
    confirmBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    confirmBox.setDefaultButton(QMessageBox::No);
    confirmBox.button(QMessageBox::Yes)->setText(tr("确认粉碎"));
    confirmBox.button(QMessageBox::No)->setText(tr("取消"));

    if (confirmBox.exec() != QMessageBox::Yes) {
        return;
    }

    // 收集文件列表
    QStringList files;
    for (int i = 0; i < m_fileList->count(); i++) {
        files.append(m_fileList->item(i)->data(Qt::UserRole).toString());
    }

    // 开始粉碎
    m_isProcessing = true;
    m_shredBtn->setEnabled(false);
    m_addFilesBtn->setEnabled(false);
    m_addFolderBtn->setEnabled(false);
    m_clearListBtn->setEnabled(false);
    m_removeSelectedBtn->setEnabled(false);
    m_privilegeCheckBox->setEnabled(false);
    m_progressBar->setVisible(true);
    m_progressBar->setRange(0, files.size());
    m_progressBar->setValue(0);

    // 在后台线程中执行
    m_workerThread = new QThread(this);
    ShredWorker *worker = new ShredWorker(files, passes, usePrivilege);
    worker->moveToThread(m_workerThread);

    connect(m_workerThread, &QThread::started, worker, &ShredWorker::doWork);
    connect(worker, &ShredWorker::progress, this, &FileShredderWidget::onShredProgress);
    connect(worker, &ShredWorker::finished, this, &FileShredderWidget::onShredFinished);
    connect(worker, &ShredWorker::finished, m_workerThread, &QThread::quit);
    connect(m_workerThread, &QThread::finished, worker, &QObject::deleteLater);

    m_workerThread->start();
}

void FileShredderWidget::onShredProgress(const QString &file, int current, int total, int percent)
{
    Q_UNUSED(percent);
    m_progressBar->setValue(current);
    m_progressBar->setFormat(tr("粉碎中 %1/%2").arg(current).arg(total) + " - %p%");
    QApplication::processEvents();
}

void FileShredderWidget::onShredFinished(const QList<ShredResult> &results)
{
    m_isProcessing = false;
    m_shredBtn->setEnabled(true);
    m_addFilesBtn->setEnabled(true);
    m_addFolderBtn->setEnabled(true);
    m_clearListBtn->setEnabled(true);
    m_removeSelectedBtn->setEnabled(true);
    m_privilegeCheckBox->setEnabled(true);
    m_progressBar->setVisible(false);

    int successCount = 0;
    int failCount = 0;
    int privilegeCount = 0;
    QStringList failedFiles;

    for (const ShredResult &result : results) {
        if (result.success) {
            successCount++;
            if (result.usedPrivilege) {
                privilegeCount++;
            }
            // 从列表中移除成功的项目
            for (int i = m_fileList->count() - 1; i >= 0; i--) {
                if (m_fileList->item(i)->data(Qt::UserRole).toString() == result.filePath) {
                    delete m_fileList->takeItem(i);
                }
            }
        } else {
            failCount++;
            failedFiles.append(QString("%1: %2").arg(result.filePath).arg(result.errorMessage));
        }
    }

    updateTotalSize();

    QString message = tr("粉碎完成！\n成功: %1 个\n失败: %2 个")
        .arg(successCount).arg(failCount);
    
    if (privilegeCount > 0) {
        message += tr("\n其中 %1 个通过权限提升删除").arg(privilegeCount);
    }

    if (failCount > 0) {
        QMessageBox::warning(this, tr("粉碎结果"),
            message + "\n\n" + tr("失败文件:\n") + failedFiles.join("\n").left(500));
    } else {
        QMessageBox::information(this, tr("粉碎完成"), message);
    }
}

QString FileShredderWidget::formatSize(qint64 bytes) const
{
    const qint64 KB = 1024;
    const qint64 MB = 1024 * KB;
    const qint64 GB = 1024 * MB;
    const qint64 TB = 1024 * GB;

    if (bytes >= TB) {
        return QString("%1 TiB").arg(bytes / (double)TB, 0, 'f', 2);
    } else if (bytes >= GB) {
        return QString("%1 GiB").arg(bytes / (double)GB, 0, 'f', 2);
    } else if (bytes >= MB) {
        return QString("%1 MiB").arg(bytes / (double)MB, 0, 'f', 2);
    } else if (bytes >= KB) {
        return QString("%1 KiB").arg(bytes / (double)KB, 0, 'f', 2);
    } else {
        return QString("%1 B").arg(bytes);
    }
}

bool FileShredderWidget::isDarkTheme()
{
    QPalette palette = qApp->palette();
    QColor windowColor = palette.color(QPalette::Window);
    int brightness = (windowColor.red() * 299 + windowColor.green() * 587 + windowColor.blue() * 114) / 1000;
    return brightness < 128;
}

void FileShredderWidget::applyTheme()
{
    bool darkMode = isDarkTheme();

    if (darkMode) {
        setStyleSheet(
            "FileShredderWidget { background-color: #2d2d2d; }"
            "QLabel { color: #e0e0e0; }"
            "QListWidget { background-color: #3d3d3d; color: #e0e0e0; border: 1px solid #555; }"
            "QListWidget::item:selected { background-color: #4a90d9; }"
        );
        // 深色主题下警告框样式
        m_warningFrame->setStyleSheet(
            "QFrame#warningFrame { "
            "   background-color: rgba(231, 76, 60, 0.25); "
            "   border: 2px solid #E74C3C; "
            "   border-radius: 8px; "
            "}"
        );
        m_warningLabel->setStyleSheet(
            "font-size: 13px; "
            "font-weight: bold; "
            "color: #FF6B6B; "
            "background: transparent; "
        );
    } else {
        setStyleSheet(
            "FileShredderWidget { background-color: #F5F5F5; }"
            "QLabel { color: #333; }"
        );
        // 浅色主题下警告框样式
        m_warningFrame->setStyleSheet(
            "QFrame#warningFrame { "
            "   background-color: rgba(231, 76, 60, 0.08); "
            "   border: 1px solid #E74C3C; "
            "   border-radius: 8px; "
            "}"
        );
        m_warningLabel->setStyleSheet(
            "font-size: 13px; "
            "font-weight: bold; "
            "color: #C0392B; "
            "background: transparent; "
        );
    }
}
