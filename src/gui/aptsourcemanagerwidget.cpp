/*
 * APT Source Manager Widget - Implementation
 * APT 源管理组件 - 实现文件
 */

#include "aptsourcemanagerwidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QFile>
#include <QDir>
#include <QRegularExpression>
#include <QHeaderView>
#include <QProcess>
#include <QApplication>

// ==================== APTSourceItem ====================

APTSourceItem::APTSourceItem(const APTSourceInfo &info, QWidget *parent)
    : QWidget(parent)
    , m_info(info)
{
    initUI();
    applyTheme();
}

void APTSourceItem::initUI()
{
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(10, 6, 10, 6);
    mainLayout->setSpacing(10);

    // 类型标签
    m_typeLabel = new QLabel(m_info.isSource ? "deb-src" : "deb", this);
    m_typeLabel->setStyleSheet(QString("font-weight: bold; color: %1; min-width: 60px;")
                               .arg(m_info.isSource ? "#e74c3c" : "#27ae60"));
    mainLayout->addWidget(m_typeLabel);

    // URI
    m_uriLabel = new QLabel(m_info.uri, this);
    m_uriLabel->setStyleSheet("font-size: 12px;");
    m_uriLabel->setWordWrap(true);
    mainLayout->addWidget(m_uriLabel, 1);

    // 发行版
    m_distLabel = new QLabel(m_info.distribution, this);
    m_distLabel->setStyleSheet("font-size: 12px; color: #666;");
    m_distLabel->setMinimumWidth(80);
    mainLayout->addWidget(m_distLabel);

    // 启用复选框
    m_enabledCheck = new QCheckBox(tr("启用"), this);
    m_enabledCheck->setChecked(m_info.isActive);
    connect(m_enabledCheck, &QCheckBox::toggled, this, &APTSourceItem::onCheckBoxToggled);
    mainLayout->addWidget(m_enabledCheck);

    // 编辑按钮
    m_editBtn = new QPushButton(this);
    m_editBtn->setIcon(QIcon::fromTheme("document-edit"));
    m_editBtn->setFixedSize(28, 28);
    m_editBtn->setToolTip(tr("编辑"));
    m_editBtn->setFlat(true);
    connect(m_editBtn, &QPushButton::clicked, this, &APTSourceItem::onEditClicked);
    mainLayout->addWidget(m_editBtn);

    // 删除按钮
    m_deleteBtn = new QPushButton(this);
    m_deleteBtn->setIcon(QIcon::fromTheme("edit-delete"));
    m_deleteBtn->setFixedSize(28, 28);
    m_deleteBtn->setToolTip(tr("删除"));
    m_deleteBtn->setFlat(true);
    connect(m_deleteBtn, &QPushButton::clicked, this, &APTSourceItem::onDeleteClicked);
    mainLayout->addWidget(m_deleteBtn);

    setFixedHeight(50);
}

void APTSourceItem::applyTheme()
{
    QString bgColor = m_info.isActive ? 
        (qApp->palette().window().color().lightness() > 128 ? "#ffffff" : "#3d3d3d") :
        (qApp->palette().window().color().lightness() > 128 ? "#f0f0f0" : "#2d2d2d");
    
    setStyleSheet(QString("APTSourceItem { background-color: %1; border-radius: 6px; }")
                  .arg(bgColor));
}

void APTSourceItem::onCheckBoxToggled(bool checked)
{
    emit statusChanged();
}

void APTSourceItem::onEditClicked()
{
    emit editRequested();
}

void APTSourceItem::onDeleteClicked()
{
    emit deleteRequested();
}

// ==================== APTSourceEditDialog ====================

APTSourceEditDialog::APTSourceEditDialog(const APTSourceInfo &info, QWidget *parent)
    : QDialog(parent)
    , m_originalInfo(info)
{
    setWindowTitle(info.filePath.isEmpty() ? tr("添加 APT 源") : tr("编辑 APT 源"));
    setMinimumWidth(500);
    initUI();
    
    if (!info.filePath.isEmpty()) {
        m_uriEdit->setText(info.uri);
        m_distEdit->setText(info.distribution);
        m_componentsEdit->setText(info.components);
        m_optionsEdit->setText(info.options);
        m_sourceCheck->setChecked(info.isSource);
    }
    
    updatePreview();
}

void APTSourceEditDialog::initUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // URI
    QHBoxLayout *uriLayout = new QHBoxLayout();
    uriLayout->addWidget(new QLabel(tr("URI:")));
    m_uriEdit = new QLineEdit(this);
    m_uriEdit->setPlaceholderText(tr("例如: http://archive.ubuntu.com/ubuntu"));
    connect(m_uriEdit, &QLineEdit::textChanged, this, &APTSourceEditDialog::updatePreview);
    uriLayout->addWidget(m_uriEdit);
    mainLayout->addLayout(uriLayout);

    // 发行版
    QHBoxLayout *distLayout = new QHBoxLayout();
    distLayout->addWidget(new QLabel(tr("发行版:")));
    m_distEdit = new QLineEdit(this);
    m_distEdit->setPlaceholderText(tr("例如: focal, stable, bullseye"));
    connect(m_distEdit, &QLineEdit::textChanged, this, &APTSourceEditDialog::updatePreview);
    distLayout->addWidget(m_distEdit);
    mainLayout->addLayout(distLayout);

    // 组件
    QHBoxLayout *compLayout = new QHBoxLayout();
    compLayout->addWidget(new QLabel(tr("组件:")));
    m_componentsEdit = new QLineEdit(this);
    m_componentsEdit->setPlaceholderText(tr("例如: main restricted universe multiverse"));
    connect(m_componentsEdit, &QLineEdit::textChanged, this, &APTSourceEditDialog::updatePreview);
    compLayout->addWidget(m_componentsEdit);
    mainLayout->addLayout(compLayout);

    // 选项
    QHBoxLayout *optLayout = new QHBoxLayout();
    optLayout->addWidget(new QLabel(tr("选项:")));
    m_optionsEdit = new QLineEdit(this);
    m_optionsEdit->setPlaceholderText(tr("例如: [arch=amd64] (可选)"));
    connect(m_optionsEdit, &QLineEdit::textChanged, this, &APTSourceEditDialog::updatePreview);
    optLayout->addWidget(m_optionsEdit);
    mainLayout->addLayout(optLayout);

    // 源码源
    m_sourceCheck = new QCheckBox(tr("源码源 (deb-src)"), this);
    connect(m_sourceCheck, &QCheckBox::toggled, this, &APTSourceEditDialog::updatePreview);
    mainLayout->addWidget(m_sourceCheck);

    // 预览
    mainLayout->addWidget(new QLabel(tr("预览:")));
    m_previewEdit = new QTextEdit(this);
    m_previewEdit->setReadOnly(true);
    m_previewEdit->setMaximumHeight(80);
    // 根据主题设置预览框样式
    bool darkMode = qApp->palette().window().color().lightness() < 128;
    if (darkMode) {
        m_previewEdit->setStyleSheet("background-color: #2d2d2d; color: #e0e0e0; font-family: monospace; border: 1px solid #555; border-radius: 4px;");
    } else {
        m_previewEdit->setStyleSheet("background-color: #f5f5f5; color: #333333; font-family: monospace; border: 1px solid #ddd; border-radius: 4px;");
    }
    mainLayout->addWidget(m_previewEdit);

    // 按钮
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    
    QPushButton *cancelBtn = new QPushButton(tr("取消"), this);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(cancelBtn);
    
    m_saveBtn = new QPushButton(tr("保存"), this);
    m_saveBtn->setEnabled(false);
    connect(m_saveBtn, &QPushButton::clicked, this, &APTSourceEditDialog::onSave);
    btnLayout->addWidget(m_saveBtn);
    mainLayout->addLayout(btnLayout);

    // 连接验证
    connect(m_uriEdit, &QLineEdit::textChanged, this, &APTSourceEditDialog::validateInput);
    connect(m_distEdit, &QLineEdit::textChanged, this, &APTSourceEditDialog::validateInput);
}

void APTSourceEditDialog::updatePreview()
{
    QString type = m_sourceCheck->isChecked() ? "deb-src" : "deb";
    QString options = m_optionsEdit->text().trimmed();
    if (!options.isEmpty() && !options.startsWith("[")) {
        options = "[" + options;
    }
    if (!options.isEmpty() && !options.endsWith("]")) {
        options = options + "]";
    }
    
    QString line = QString("%1 %2 %3 %4 %5")
        .arg(type)
        .arg(options.isEmpty() ? "" : options)
        .arg(m_uriEdit->text().trimmed())
        .arg(m_distEdit->text().trimmed())
        .arg(m_componentsEdit->text().trimmed());
    
    // 清理多余空格
    line = line.replace(QRegularExpression("\\s+"), " ").trimmed();
    
    m_previewEdit->setPlainText(line);
}

void APTSourceEditDialog::validateInput()
{
    m_saveBtn->setEnabled(!m_uriEdit->text().trimmed().isEmpty() && 
                          !m_distEdit->text().trimmed().isEmpty());
}

void APTSourceEditDialog::onSave()
{
    emit saved();
    accept();
}

APTSourceInfo APTSourceEditDialog::getSourceInfo() const
{
    APTSourceInfo info;
    info.isSource = m_sourceCheck->isChecked();
    info.uri = m_uriEdit->text().trimmed();
    info.distribution = m_distEdit->text().trimmed();
    info.components = m_componentsEdit->text().trimmed();
    info.options = m_optionsEdit->text().trimmed();
    info.isActive = true;
    
    // 构建完整行
    QString type = info.isSource ? "deb-src" : "deb";
    QString options = info.options;
    if (!options.isEmpty()) {
        if (!options.startsWith("[")) options = "[" + options;
        if (!options.endsWith("]")) options = options + "]";
    }
    info.fullLine = QString("%1 %2 %3 %4 %5")
        .arg(type)
        .arg(options)
        .arg(info.uri)
        .arg(info.distribution)
        .arg(info.components);
    info.fullLine = info.fullLine.replace(QRegularExpression("\\s+"), " ").trimmed();
    
    return info;
}

// ==================== APTSourceManagerWidget ====================

APTSourceManagerWidget::APTSourceManagerWidget(QWidget *parent)
    : QWidget(parent)
    , m_selectedRow(-1)
{
    initUI();
    loadSources();
}

APTSourceManagerWidget::~APTSourceManagerWidget()
{
}

void APTSourceManagerWidget::initUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // 标题栏
    QHBoxLayout *headerLayout = new QHBoxLayout();
    
    m_titleLabel = new QLabel(tr("APT 软件源管理"), this);
    m_titleLabel->setStyleSheet("font-size: 18px; font-weight: bold;");
    headerLayout->addWidget(m_titleLabel);
    
    m_countLabel = new QLabel(this);
    m_countLabel->setStyleSheet("font-size: 14px; color: #666;");
    headerLayout->addWidget(m_countLabel);
    
    headerLayout->addStretch();
    
    // 刷新按钮
    m_refreshBtn = new QPushButton(tr("刷新"), this);
    m_refreshBtn->setIcon(QIcon::fromTheme("view-refresh"));
    connect(m_refreshBtn, &QPushButton::clicked, this, &APTSourceManagerWidget::refreshList);
    headerLayout->addWidget(m_refreshBtn);
    
    mainLayout->addLayout(headerLayout);

    // 提示信息
    m_tipLabel = new QLabel(tr("提示: 修改软件源需要管理员权限，部分操作可能需要输入密码。"), this);
    m_tipLabel->setStyleSheet("font-size: 12px; color: #888; padding: 5px;");
    mainLayout->addWidget(m_tipLabel);

    // 搜索框
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("搜索软件源..."));
    m_searchEdit->setClearButtonEnabled(true);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &APTSourceManagerWidget::onSearchChanged);
    mainLayout->addWidget(m_searchEdit);

    // 源列表
    m_sourceTable = new QTableWidget(this);
    m_sourceTable->setColumnCount(5);
    m_sourceTable->setHorizontalHeaderLabels({tr("类型"), tr("URI"), tr("发行版"), tr("状态"), tr("操作")});
    m_sourceTable->horizontalHeader()->setStretchLastSection(true);
    m_sourceTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        m_sourceTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_sourceTable->setSelectionMode(QAbstractItemView::SingleSelection);
        m_sourceTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_sourceTable->verticalHeader()->setVisible(false);
        m_sourceTable->setColumnWidth(4, 140);  // 固定操作列宽度（3个按钮）
        connect(m_sourceTable, &QTableWidget::itemSelectionChanged, this, &APTSourceManagerWidget::onItemSelectionChanged);
        mainLayout->addWidget(m_sourceTable);

    // 空状态提示
    m_emptyLabel = new QLabel(tr("暂无软件源"), this);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setStyleSheet("font-size: 14px; color: #999; padding: 50px;");
    m_emptyLabel->hide();
    mainLayout->addWidget(m_emptyLabel);

    // 操作按钮
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    
    m_addBtn = new QPushButton(tr("添加"), this);
    m_addBtn->setIcon(QIcon::fromTheme("list-add"));
    connect(m_addBtn, &QPushButton::clicked, this, &APTSourceManagerWidget::onAddClicked);
    btnLayout->addWidget(m_addBtn);
    
    m_editBtn = new QPushButton(tr("编辑"), this);
    m_editBtn->setIcon(QIcon::fromTheme("document-edit"));
    m_editBtn->setEnabled(false);
    connect(m_editBtn, &QPushButton::clicked, this, &APTSourceManagerWidget::onEditClicked);
    btnLayout->addWidget(m_editBtn);
    
    m_deleteBtn = new QPushButton(tr("删除"), this);
    m_deleteBtn->setIcon(QIcon::fromTheme("edit-delete"));
    m_deleteBtn->setEnabled(false);
    connect(m_deleteBtn, &QPushButton::clicked, this, &APTSourceManagerWidget::onDeleteClicked);
    btnLayout->addWidget(m_deleteBtn);
    
    mainLayout->addLayout(btnLayout);

    applyTheme();
}

void APTSourceManagerWidget::applyTheme()
{
    QString bgColor = isDarkTheme() ? "#2d2d2d" : "#f5f5f5";
    QString cardBg = isDarkTheme() ? "#3d3d3d" : "#ffffff";
    QString selectBg = isDarkTheme() ? "#4a90d9" : "#4a90d9";
    
    setStyleSheet(QString(
        "QLineEdit { "
        "   background-color: %1; "
        "   border: 1px solid #ccc; "
        "   border-radius: 6px; "
        "   padding: 8px; "
        "} "
        "QTableWidget { "
        "   background-color: %2; "
        "   border: 1px solid #ccc; "
        "   border-radius: 6px; "
        "   gridline-color: #e0e0e0; "
        "   outline: none; "
        "} "
        "QTableWidget::item { "
        "   background-color: transparent; "
        "   padding: 4px; "
        "} "
        "QTableWidget::item:selected { "
        "   background-color: %4; "
        "   color: %5; "
        "   border: none; "
        "} "
        "QTableWidget::item:focus { "
        "   border: none; "
        "   outline: none; "
        "} "
        "QTableWidget:focus { "
        "   border: 1px solid #ccc; "
        "   outline: none; "
        "} "
        "QCheckBox::indicator { "
        "   border: none; "
        "} "
        "QCheckBox:focus { "
        "   outline: none; "
        "} "
        "QPushButton:focus { "
        "   outline: none; "
        "} "
        "QHeaderView::section { "
        "   background-color: %1; "
        "   padding: 8px; "
        "   border: none; "
        "   border-bottom: 1px solid #ccc; "
        "} "
        "QPushButton { "
        "   background-color: %2; "
        "   border: 1px solid #ccc; "
        "   border-radius: 6px; "
        "   padding: 6px 12px; "
        "} "
        "QPushButton:hover { "
        "   background-color: %3; "
        "} "
        "QPushButton:disabled { "
        "   background-color: #e0e0e0; "
        "   color: #999; "
        "} "
    ).arg(bgColor).arg(cardBg).arg(isDarkTheme() ? "#4d4d4d" : "#e0e0e0").arg(selectBg).arg(isDarkTheme() ? "#ffffff" : "#333333"));
}

bool APTSourceManagerWidget::isDarkTheme()
{
    QPalette palette = qApp->palette();
    QColor windowColor = palette.color(QPalette::Window);
    int brightness = (windowColor.red() * 299 + windowColor.green() * 587 + windowColor.blue() * 114) / 1000;
    return brightness < 128;
}

QList<APTSourceInfo> APTSourceManagerWidget::parseSourceFile(const QString &filePath)
{
    QList<APTSourceInfo> result;
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return result;
    }
    
    QString content = QString::fromUtf8(file.readAll());
    file.close();
    
    QStringList lines = content.split('\n');
    
    for (const QString &line : lines) {
        QString trimmed = line.trimmed();
        
        // 跳过空行和注释行（但保留被注释的 deb 行）
        if (trimmed.isEmpty() || trimmed.startsWith("#") && !trimmed.contains(QRegularExpression("^#\\s*deb"))) {
            continue;
        }
        
        // 移除前导 # 号（用于检测被禁用的源）
        bool isActive = !trimmed.startsWith("#");
        QString cleanLine = trimmed;
        cleanLine.remove(QRegularExpression("^#\\s*"));
        
        // 匹配 deb 或 deb-src 行
        QRegularExpression debRe("^(deb-src|deb)\\s+(\\[.*?\\]\\s+)?(\\S+)\\s+(\\S+)(?:\\s+(.*))?$");
        QRegularExpressionMatch match = debRe.match(cleanLine);
        
        if (match.hasMatch()) {
            APTSourceInfo info;
            info.filePath = filePath;
            info.isSource = (match.captured(1) == "deb-src");
            info.options = match.captured(2).trimmed();
            info.uri = match.captured(3);
            info.distribution = match.captured(4);
            info.components = match.captured(5).trimmed();
            info.fullLine = cleanLine;
            info.isActive = isActive;
            
            result.append(info);
        }
    }
    
    return result;
}

void APTSourceManagerWidget::loadSources()
{
    m_sources.clear();
    m_sourceTable->setRowCount(0);
    
    // 解析 sources.list
    m_sources.append(parseSourceFile("/etc/apt/sources.list"));
    
    // 解析 sources.list.d 目录
    QDir sourcesListD("/etc/apt/sources.list.d");
    for (const QFileInfo &fileInfo : sourcesListD.entryInfoList({"*.list"}, QDir::Files)) {
        m_sources.append(parseSourceFile(fileInfo.absoluteFilePath()));
    }
    
    // 填充表格
    m_sourceTable->setRowCount(m_sources.size());
    
    for (int i = 0; i < m_sources.size(); ++i) {
        const APTSourceInfo &info = m_sources[i];
        
        // 类型
        QTableWidgetItem *typeItem = new QTableWidgetItem(info.isSource ? "deb-src" : "deb");
        typeItem->setForeground(QBrush(info.isSource ? QColor("#e74c3c") : QColor("#27ae60")));
        m_sourceTable->setItem(i, 0, typeItem);
        
        // URI
        m_sourceTable->setItem(i, 1, new QTableWidgetItem(info.uri));
        
        // 发行版
        m_sourceTable->setItem(i, 2, new QTableWidgetItem(info.distribution));
        
        // 状态
        QTableWidgetItem *statusItem = new QTableWidgetItem(info.isActive ? tr("已启用") : tr("已禁用"));
        statusItem->setForeground(QBrush(info.isActive ? QColor("#27ae60") : QColor("#999")));
        m_sourceTable->setItem(i, 3, statusItem);
        
        // 操作按钮列 - 使用自定义widget避免被选中背景覆盖
        QWidget *btnWidget = new QWidget();
        btnWidget->setAttribute(Qt::WA_TransparentForMouseEvents, false);
        QHBoxLayout *btnLayout = new QHBoxLayout(btnWidget);
        btnLayout->setContentsMargins(4, 2, 4, 2);
        btnLayout->setSpacing(4);
        
        // 启用/禁用按钮
        QPushButton *toggleBtn = new QPushButton();
        toggleBtn->setFixedSize(24, 24);
        toggleBtn->setFlat(true);
        if (info.isActive) {
            toggleBtn->setIcon(QIcon::fromTheme("media-playback-pause", QIcon::fromTheme("process-stop")));
            toggleBtn->setToolTip(tr("禁用此源"));
        } else {
            toggleBtn->setIcon(QIcon::fromTheme("media-playback-start", QIcon::fromTheme("dialog-ok")));
            toggleBtn->setToolTip(tr("启用此源"));
        }
        connect(toggleBtn, &QPushButton::clicked, this, [this, i, info]() {
            bool newStatus = !info.isActive;
            if (changeSourceStatus(m_sources[i], newStatus)) {
                m_sources[i].isActive = newStatus;
                loadSources();
            }
        });
        btnLayout->addWidget(toggleBtn);
        
        // 编辑按钮
        QPushButton *editBtn = new QPushButton();
        editBtn->setIcon(QIcon::fromTheme("document-edit"));
        editBtn->setFixedSize(24, 24);
        editBtn->setFlat(true);
        connect(editBtn, &QPushButton::clicked, this, [this, i]() {
            m_selectedRow = i;
            onEditClicked();
        });
        btnLayout->addWidget(editBtn);
        
        // 删除按钮
        QPushButton *deleteBtn = new QPushButton();
        deleteBtn->setIcon(QIcon::fromTheme("edit-delete"));
        deleteBtn->setFixedSize(24, 24);
        deleteBtn->setFlat(true);
        connect(deleteBtn, &QPushButton::clicked, this, [this, i]() {
            m_selectedRow = i;
            onDeleteClicked();
        });
        btnLayout->addWidget(deleteBtn);
        
        btnLayout->addStretch();
        m_sourceTable->setCellWidget(i, 4, btnWidget);
        
        m_sourceTable->setRowHeight(i, 40);
    }
    
    // 更新计数
    int activeCount = 0;
    for (const APTSourceInfo &info : m_sources) {
        if (info.isActive) activeCount++;
    }
    m_countLabel->setText(tr("共 %1 个源，已启用 %2 个").arg(m_sources.size()).arg(activeCount));
    
    // 显示/隐藏空状态
    m_emptyLabel->setVisible(m_sources.isEmpty());
    m_sourceTable->setVisible(!m_sources.isEmpty());
}

void APTSourceManagerWidget::refreshList()
{
    loadSources();
}

void APTSourceManagerWidget::onAddClicked()
{
    APTSourceInfo emptyInfo;
    APTSourceEditDialog dialog(emptyInfo, this);
    connect(&dialog, &APTSourceEditDialog::saved, this, [this, &dialog]() {
        APTSourceInfo newInfo = dialog.getSourceInfo();
        if (addSource(newInfo)) {
            loadSources();
        }
    });
    dialog.exec();
}

void APTSourceManagerWidget::onEditClicked()
{
    if (m_selectedRow < 0 || m_selectedRow >= m_sources.size()) {
        return;
    }
    
    APTSourceInfo oldInfo = m_sources[m_selectedRow];
    APTSourceEditDialog dialog(oldInfo, this);
    connect(&dialog, &APTSourceEditDialog::saved, this, [this, oldInfo, &dialog]() {
        APTSourceInfo newInfo = dialog.getSourceInfo();
        newInfo.filePath = oldInfo.filePath;
        if (modifySource(oldInfo, newInfo)) {
            loadSources();
        }
    });
    dialog.exec();
}

void APTSourceManagerWidget::onDeleteClicked()
{
    if (m_selectedRow < 0 || m_selectedRow >= m_sources.size()) {
        return;
    }
    
    const APTSourceInfo &info = m_sources[m_selectedRow];
    
    auto reply = QMessageBox::question(this, tr("确认删除"),
        tr("确定要删除以下软件源吗？\n\n%1").arg(info.fullLine),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        if (deleteSource(info)) {
            loadSources();
        }
    }
}

void APTSourceManagerWidget::onSearchChanged(const QString &text)
{
    for (int i = 0; i < m_sourceTable->rowCount(); ++i) {
        bool match = false;
        for (int j = 0; j < m_sourceTable->columnCount() - 1; ++j) {
            QTableWidgetItem *item = m_sourceTable->item(i, j);
            if (item && item->text().contains(text, Qt::CaseInsensitive)) {
                match = true;
                break;
            }
        }
        m_sourceTable->setRowHidden(i, !match);
    }
}

void APTSourceManagerWidget::onAppStatusChanged()
{
    loadSources();
}

void APTSourceManagerWidget::onItemSelectionChanged()
{
    QList<QTableWidgetItem*> selected = m_sourceTable->selectedItems();
    m_selectedRow = selected.isEmpty() ? -1 : selected.first()->row();
    
    m_editBtn->setEnabled(m_selectedRow >= 0);
    m_deleteBtn->setEnabled(m_selectedRow >= 0);
}

bool APTSourceManagerWidget::changeSourceStatus(const APTSourceInfo &info, bool enabled)
{
    // 使用 pkexec 或 gksudo 执行需要 root 权限的操作
    QFile file(info.filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("错误"), tr("无法读取文件: %1").arg(info.filePath));
        return false;
    }
    
    QStringList lines;
    while (!file.atEnd()) {
        lines << QString::fromUtf8(file.readLine());
    }
    file.close();
    
    // 查找并修改行
    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines[i].trimmed();
        QString cleanLine = line;
        cleanLine.remove(QRegularExpression("^#\\s*"));
        
        if (cleanLine == info.fullLine) {
            if (enabled) {
                lines[i] = cleanLine + "\n";
            } else {
                lines[i] = "# " + cleanLine + "\n";
            }
            break;
        }
    }
    
    // 使用 pkexec 写入文件
    QProcess process;
    process.start("pkexec", {"tee", info.filePath});
    process.write(lines.join("").toUtf8());
    process.closeWriteChannel();
    process.waitForFinished();
    
    if (process.exitCode() != 0) {
        QMessageBox::warning(this, tr("错误"), tr("修改失败: %1").arg(QString::fromUtf8(process.readAllStandardError())));
        return false;
    }
    
    return true;
}

bool APTSourceManagerWidget::deleteSource(const APTSourceInfo &info)
{
    QFile file(info.filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("错误"), tr("无法读取文件: %1").arg(info.filePath));
        return false;
    }
    
    QStringList lines;
    while (!file.atEnd()) {
        lines << QString::fromUtf8(file.readLine());
    }
    file.close();
    
    // 查找并删除行
    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines[i].trimmed();
        QString cleanLine = line;
        cleanLine.remove(QRegularExpression("^#\\s*"));
        
        if (cleanLine == info.fullLine) {
            lines.removeAt(i);
            break;
        }
    }
    
    // 使用 pkexec 写入文件
    QProcess process;
    process.start("pkexec", {"tee", info.filePath});
    process.write(lines.join("").toUtf8());
    process.closeWriteChannel();
    process.waitForFinished();
    
    if (process.exitCode() != 0) {
        QMessageBox::warning(this, tr("错误"), tr("删除失败: %1").arg(QString::fromUtf8(process.readAllStandardError())));
        return false;
    }
    
    return true;
}

bool APTSourceManagerWidget::addSource(const APTSourceInfo &info)
{
    // 将新源添加到 sources.list.d 目录
    QString filePath = "/etc/apt/sources.list.d/lz-disk-cleaner.list";
    
    // 使用 pkexec 写入文件
    QProcess process;
    process.start("pkexec", {"tee", "-a", filePath});
    process.write((info.fullLine + "\n").toUtf8());
    process.closeWriteChannel();
    process.waitForFinished();
    
    if (process.exitCode() != 0) {
        QMessageBox::warning(this, tr("错误"), tr("添加失败: %1").arg(QString::fromUtf8(process.readAllStandardError())));
        return false;
    }
    
    return true;
}

bool APTSourceManagerWidget::modifySource(const APTSourceInfo &oldInfo, const APTSourceInfo &newInfo)
{
    QFile file(oldInfo.filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("错误"), tr("无法读取文件: %1").arg(oldInfo.filePath));
        return false;
    }
    
    QStringList lines;
    while (!file.atEnd()) {
        lines << QString::fromUtf8(file.readLine());
    }
    file.close();
    
    // 查找并修改行
    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines[i].trimmed();
        QString cleanLine = line;
        cleanLine.remove(QRegularExpression("^#\\s*"));
        
        if (cleanLine == oldInfo.fullLine) {
            lines[i] = (oldInfo.isActive ? "" : "# ") + newInfo.fullLine + "\n";
            break;
        }
    }
    
    // 使用 pkexec 写入文件
    QProcess process;
    process.start("pkexec", {"tee", oldInfo.filePath});
    process.write(lines.join("").toUtf8());
    process.closeWriteChannel();
    process.waitForFinished();
    
    if (process.exitCode() != 0) {
        QMessageBox::warning(this, tr("错误"), tr("修改失败: %1").arg(QString::fromUtf8(process.readAllStandardError())));
        return false;
    }
    
    return true;
}
