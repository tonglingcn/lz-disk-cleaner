/*
 * Startup Apps Widget - Implementation
 * 自启动管理组件 - 实现文件
 */

#include "startupappswidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QFileDialog>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QApplication>
#include <QStyle>
#include <QIcon>
#include <QTimer>
#include <QPainter>

// ==================== StartupAppItem ====================

StartupAppItem::StartupAppItem(const StartupAppInfo &info, QWidget *parent)
    : QWidget(parent)
    , m_info(info)
{
    initUI();
    applyTheme();
}

void StartupAppItem::initUI()
{
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(10, 8, 10, 8);
    mainLayout->setSpacing(10);

    // 图标
    m_iconLabel = new QLabel(this);
    m_iconLabel->setFixedSize(32, 32);
    m_iconLabel->setAlignment(Qt::AlignCenter);
    
    // 尝试加载图标
    QIcon icon;
    if (!m_info.icon.isEmpty()) {
        icon = QIcon::fromTheme(m_info.icon);
    }
    if (icon.isNull()) {
        icon = QIcon::fromTheme("application-x-executable");
    }
    if (icon.isNull()) {
        // 使用默认应用图标
        QPixmap pixmap(32, 32);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setPen(QPen(QColor("#666"), 2));
        painter.drawRect(4, 4, 24, 24);
        painter.end();
        icon = QIcon(pixmap);
    }
    m_iconLabel->setPixmap(icon.pixmap(32, 32));
    mainLayout->addWidget(m_iconLabel);

    // 名称和命令
    QVBoxLayout *infoLayout = new QVBoxLayout();
    infoLayout->setSpacing(2);

    m_nameLabel = new QLabel(m_info.name, this);
    m_nameLabel->setStyleSheet("font-weight: bold; font-size: 13px;");
    infoLayout->addWidget(m_nameLabel);

    m_execLabel = new QLabel(m_info.exec, this);
    m_execLabel->setStyleSheet("font-size: 11px; color: #888;");
    m_execLabel->setWordWrap(true);
    infoLayout->addWidget(m_execLabel);

    mainLayout->addLayout(infoLayout, 1);

    // 启用复选框
    m_enabledCheck = new QCheckBox(tr("启用"), this);
    m_enabledCheck->setChecked(m_info.enabled);
    connect(m_enabledCheck, &QCheckBox::toggled, this, &StartupAppItem::onCheckBoxToggled);
    mainLayout->addWidget(m_enabledCheck);

    // 编辑按钮
    m_editBtn = new QPushButton(this);
    m_editBtn->setIcon(QIcon::fromTheme("document-edit"));
    m_editBtn->setFixedSize(32, 32);
    m_editBtn->setToolTip(tr("编辑"));
    m_editBtn->setFlat(true);
    connect(m_editBtn, &QPushButton::clicked, this, &StartupAppItem::onEditClicked);
    mainLayout->addWidget(m_editBtn);

    // 删除按钮
    m_deleteBtn = new QPushButton(this);
    m_deleteBtn->setIcon(QIcon::fromTheme("edit-delete"));
    m_deleteBtn->setFixedSize(32, 32);
    m_deleteBtn->setToolTip(tr("删除"));
    m_deleteBtn->setFlat(true);
    connect(m_deleteBtn, &QPushButton::clicked, this, &StartupAppItem::onDeleteClicked);
    mainLayout->addWidget(m_deleteBtn);

    setFixedHeight(60);
}

void StartupAppItem::applyTheme()
{
    QString bgColor = m_info.enabled ? 
        (qApp->palette().window().color().lightness() > 128 ? "#ffffff" : "#3d3d3d") :
        (qApp->palette().window().color().lightness() > 128 ? "#f5f5f5" : "#2d2d2d");
    
    setStyleSheet(QString("StartupAppItem { background-color: %1; border-radius: 8px; }")
                  .arg(bgColor));
}

void StartupAppItem::onCheckBoxToggled(bool checked)
{
    // 读取文件内容
    QFile file(m_info.filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("错误"), tr("无法读取文件: %1").arg(m_info.filePath));
        m_enabledCheck->setChecked(!checked);
        return;
    }
    
    QStringList lines;
    while (!file.atEnd()) {
        lines << QString::fromUtf8(file.readLine()).trimmed();
    }
    file.close();

    // 修改 Hidden 或 X-GNOME-Autostart-enabled 字段
    bool found = false;
    QString statusStr = checked ? "false" : "true"; // Hidden=true 表示禁用
    
    for (int i = 0; i < lines.size(); ++i) {
        if (lines[i].startsWith("Hidden=", Qt::CaseInsensitive)) {
            lines[i] = QString("Hidden=%1").arg(statusStr);
            found = true;
            break;
        }
    }
    
    if (!found) {
        for (int i = 0; i < lines.size(); ++i) {
            if (lines[i].startsWith("X-GNOME-Autostart-enabled=", Qt::CaseInsensitive)) {
                lines[i] = QString("X-GNOME-Autostart-enabled=%1").arg(checked ? "true" : "false");
                found = true;
                break;
            }
        }
    }
    
    if (!found) {
        lines.append(QString("Hidden=%1").arg(statusStr));
    }

    // 写回文件
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QMessageBox::warning(this, tr("错误"), tr("无法写入文件: %1").arg(m_info.filePath));
        m_enabledCheck->setChecked(!checked);
        return;
    }
    
    file.write(lines.join("\n").toUtf8());
    file.close();
    
    m_info.enabled = checked;
    applyTheme();
    emit statusChanged();
}

void StartupAppItem::onDeleteClicked()
{
    auto reply = QMessageBox::question(this, tr("确认删除"),
        tr("确定要删除自启动应用 \"%1\" 吗？").arg(m_info.name),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        if (QFile::remove(m_info.filePath)) {
            emit deleteRequested();
        } else {
            QMessageBox::warning(this, tr("删除失败"), tr("无法删除文件: %1").arg(m_info.filePath));
        }
    }
}

void StartupAppItem::onEditClicked()
{
    emit editRequested();
}

// ==================== StartupAppEditDialog ====================

StartupAppEditDialog::StartupAppEditDialog(const QString &filePath, QWidget *parent)
    : QDialog(parent)
    , m_filePath(filePath)
{
    setWindowTitle(filePath.isEmpty() ? tr("添加自启动应用") : tr("编辑自启动应用"));
    setMinimumWidth(400);
    initUI();
    
    if (!filePath.isEmpty()) {
        loadFromFile(filePath);
    }
}

void StartupAppEditDialog::initUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // 名称
    QHBoxLayout *nameLayout = new QHBoxLayout();
    nameLayout->addWidget(new QLabel(tr("名称:")));
    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText(tr("应用显示名称"));
    nameLayout->addWidget(m_nameEdit);
    mainLayout->addLayout(nameLayout);

    // 命令
    QHBoxLayout *execLayout = new QHBoxLayout();
    execLayout->addWidget(new QLabel(tr("命令:")));
    m_execEdit = new QLineEdit(this);
    m_execEdit->setPlaceholderText(tr("执行的命令或程序路径"));
    execLayout->addWidget(m_execEdit);
    
    QPushButton *browseBtn = new QPushButton(tr("浏览"), this);
    connect(browseBtn, &QPushButton::clicked, this, &StartupAppEditDialog::onBrowseExec);
    execLayout->addWidget(browseBtn);
    mainLayout->addLayout(execLayout);

    // 描述
    QHBoxLayout *commentLayout = new QHBoxLayout();
    commentLayout->addWidget(new QLabel(tr("描述:")));
    m_commentEdit = new QLineEdit(this);
    m_commentEdit->setPlaceholderText(tr("可选描述信息"));
    commentLayout->addWidget(m_commentEdit);
    mainLayout->addLayout(commentLayout);

    // 按钮
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    
    QPushButton *cancelBtn = new QPushButton(tr("取消"), this);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(cancelBtn);
    
    m_saveBtn = new QPushButton(tr("保存"), this);
    m_saveBtn->setEnabled(false);
    connect(m_saveBtn, &QPushButton::clicked, this, &StartupAppEditDialog::onSave);
    btnLayout->addWidget(m_saveBtn);
    mainLayout->addLayout(btnLayout);

    // 连接验证
    connect(m_nameEdit, &QLineEdit::textChanged, this, &StartupAppEditDialog::validateInput);
    connect(m_execEdit, &QLineEdit::textChanged, this, &StartupAppEditDialog::validateInput);
}

void StartupAppEditDialog::loadFromFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }
    
    QString content = QString::fromUtf8(file.readAll());
    file.close();
    
    // 解析 .desktop 文件
    QRegularExpression nameRe("^Name\\s*=\\s*(.+)$", QRegularExpression::CaseInsensitiveOption);
    QRegularExpression execRe("^Exec\\s*=\\s*(.+)$", QRegularExpression::CaseInsensitiveOption);
    QRegularExpression commentRe("^Comment\\s*=\\s*(.+)$", QRegularExpression::CaseInsensitiveOption);
    
    for (const QString &line : content.split('\n')) {
        QRegularExpressionMatch match;
        
        match = nameRe.match(line);
        if (match.hasMatch()) {
            m_nameEdit->setText(match.captured(1).trimmed());
            continue;
        }
        
        match = execRe.match(line);
        if (match.hasMatch()) {
            m_execEdit->setText(match.captured(1).trimmed());
            continue;
        }
        
        match = commentRe.match(line);
        if (match.hasMatch()) {
            m_commentEdit->setText(match.captured(1).trimmed());
        }
    }
}

void StartupAppEditDialog::onBrowseExec()
{
    QString path = QFileDialog::getOpenFileName(this, tr("选择程序"),
        "/usr/bin", tr("可执行文件 (*)"));
    if (!path.isEmpty()) {
        m_execEdit->setText(path);
    }
}

void StartupAppEditDialog::validateInput()
{
    m_saveBtn->setEnabled(!m_nameEdit->text().trimmed().isEmpty() && 
                          !m_execEdit->text().trimmed().isEmpty());
}

void StartupAppEditDialog::onSave()
{
    // 确定 autostart 目录
    QString autostartPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/autostart";
    QDir().mkpath(autostartPath);
    
    // 生成文件名
    QString fileName = m_nameEdit->text().trimmed();
    fileName.replace(QRegularExpression("[^a-zA-Z0-9_-]"), "-");
    QString filePath = autostartPath + "/" + fileName + ".desktop";
    
    // 生成 .desktop 文件内容
    QString content = QString(
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=%1\n"
        "Exec=%2\n"
        "Comment=%3\n"
        "X-GNOME-Autostart-enabled=true\n"
    ).arg(m_nameEdit->text().trimmed())
     .arg(m_execEdit->text().trimmed())
     .arg(m_commentEdit->text().trimmed());
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("保存失败"), tr("无法创建文件: %1").arg(filePath));
        return;
    }
    
    file.write(content.toUtf8());
    file.close();
    
    emit saved();
    accept();
}

StartupAppInfo StartupAppEditDialog::getAppInfo() const
{
    StartupAppInfo info;
    info.name = m_nameEdit->text().trimmed();
    info.exec = m_execEdit->text().trimmed();
    return info;
}

// ==================== StartupAppsWidget ====================

StartupAppsWidget::StartupAppsWidget(QWidget *parent)
    : QWidget(parent)
{
    m_autostartPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/autostart";
    
    // 确保目录存在
    QDir().mkpath(m_autostartPath);
    
    initUI();
    loadApps();
    
    // 监控目录变化
    m_watcher.addPath(m_autostartPath);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, &StartupAppsWidget::onDirectoryChanged);
}

StartupAppsWidget::~StartupAppsWidget()
{
}

void StartupAppsWidget::initUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // 标题栏
    QHBoxLayout *headerLayout = new QHBoxLayout();
    
    m_titleLabel = new QLabel(tr("开机自启动应用"), this);
    m_titleLabel->setStyleSheet("font-size: 18px; font-weight: bold;");
    headerLayout->addWidget(m_titleLabel);
    
    m_countLabel = new QLabel(this);
    m_countLabel->setStyleSheet("font-size: 14px; color: #666;");
    headerLayout->addWidget(m_countLabel);
    
    headerLayout->addStretch();
    
    // 刷新按钮
    m_refreshBtn = new QPushButton(tr("刷新"), this);
    m_refreshBtn->setIcon(QIcon::fromTheme("view-refresh"));
    connect(m_refreshBtn, &QPushButton::clicked, this, &StartupAppsWidget::refreshList);
    headerLayout->addWidget(m_refreshBtn);
    
    // 添加按钮
    m_addBtn = new QPushButton(tr("添加"), this);
    m_addBtn->setIcon(QIcon::fromTheme("list-add"));
    connect(m_addBtn, &QPushButton::clicked, this, &StartupAppsWidget::onAddClicked);
    headerLayout->addWidget(m_addBtn);
    
    mainLayout->addLayout(headerLayout);

    // 搜索框
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("搜索自启动应用..."));
    m_searchEdit->setClearButtonEnabled(true);
    connect(m_searchEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        for (int i = 0; i < m_appList->count(); ++i) {
            QListWidgetItem *item = m_appList->item(i);
            StartupAppItem *appItem = qobject_cast<StartupAppItem*>(m_appList->itemWidget(item));
            if (appItem) {
                item->setHidden(!appItem->getName().contains(text, Qt::CaseInsensitive));
            }
        }
    });
    mainLayout->addWidget(m_searchEdit);

    // 应用列表
    m_appList = new QListWidget(this);
    m_appList->setSpacing(5);
    m_appList->setSelectionMode(QAbstractItemView::NoSelection);
    mainLayout->addWidget(m_appList);

    // 空状态提示
    m_emptyLabel = new QLabel(tr("暂无自启动应用"), this);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setStyleSheet("font-size: 14px; color: #999; padding: 50px;");
    m_emptyLabel->hide();
    mainLayout->addWidget(m_emptyLabel);

    applyTheme();
}

void StartupAppsWidget::applyTheme()
{
    QString bgColor = isDarkTheme() ? "#2d2d2d" : "#f5f5f5";
    QString cardBg = isDarkTheme() ? "#3d3d3d" : "#ffffff";
    
    setStyleSheet(QString(
        "QLineEdit { "
        "   background-color: %1; "
        "   border: 1px solid #ccc; "
        "   border-radius: 6px; "
        "   padding: 8px; "
        "} "
        "QListWidget { "
        "   background-color: transparent; "
        "   border: none; "
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
    ).arg(bgColor).arg(cardBg).arg(isDarkTheme() ? "#4d4d4d" : "#e0e0e0"));
}

bool StartupAppsWidget::isDarkTheme()
{
    QPalette palette = qApp->palette();
    QColor windowColor = palette.color(QPalette::Window);
    int brightness = (windowColor.red() * 299 + windowColor.green() * 587 + windowColor.blue() * 114) / 1000;
    return brightness < 128;
}

QString StartupAppsWidget::getDesktopValue(const QString &key, const QStringList &lines)
{
    QRegularExpression re(QString("^%1\\s*=\\s*(.+)$").arg(key), QRegularExpression::CaseInsensitiveOption);
    for (const QString &line : lines) {
        QRegularExpressionMatch match = re.match(line);
        if (match.hasMatch()) {
            return match.captured(1).trimmed();
        }
    }
    return QString();
}

void StartupAppsWidget::loadApps()
{
    m_appList->clear();
    m_items.clear();

    QDir autostartDir(m_autostartPath, "*.desktop");
    int enabledCount = 0;
    
    for (const QFileInfo &fileInfo : autostartDir.entryInfoList(QDir::Files, QDir::Name)) {
        QFile file(fileInfo.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }
        
        QStringList lines;
        while (!file.atEnd()) {
            lines << QString::fromUtf8(file.readLine()).trimmed();
        }
        file.close();
        
        // 解析 .desktop 文件
        StartupAppInfo info;
        info.filePath = fileInfo.absoluteFilePath();
        info.name = getDesktopValue("Name", lines);
        info.exec = getDesktopValue("Exec", lines);
        info.icon = getDesktopValue("Icon", lines);
        
        if (info.name.isEmpty()) {
            info.name = fileInfo.baseName();
        }
        
        // 检查是否启用
        QString hidden = getDesktopValue("Hidden", lines).toLower();
        QString gnomeEnabled = getDesktopValue("X-GNOME-Autostart-enabled", lines).toLower();
        
        if (!hidden.isEmpty()) {
            info.enabled = (hidden != "true");
        } else {
            info.enabled = (gnomeEnabled.isEmpty() || gnomeEnabled == "true");
        }
        
        if (info.enabled) {
            enabledCount++;
        }
        
        // 创建列表项
        QListWidgetItem *item = new QListWidgetItem(m_appList);
        StartupAppItem *appItem = new StartupAppItem(info, this);
        
        connect(appItem, &StartupAppItem::statusChanged, this, &StartupAppsWidget::onAppStatusChanged);
        connect(appItem, &StartupAppItem::deleteRequested, this, &StartupAppsWidget::onDirectoryChanged);
        connect(appItem, &StartupAppItem::editRequested, this, [this, fileInfo]() {
            StartupAppEditDialog dialog(fileInfo.absoluteFilePath(), this);
            connect(&dialog, &StartupAppEditDialog::saved, this, &StartupAppsWidget::onDirectoryChanged);
            dialog.exec();
        });
        
        item->setSizeHint(appItem->sizeHint());
        m_appList->setItemWidget(item, appItem);
        m_items.append(appItem);
    }
    
    // 更新计数
    m_countLabel->setText(tr("共 %1 项，已启用 %2 项").arg(m_items.size()).arg(enabledCount));
    
    // 显示/隐藏空状态
    m_emptyLabel->setVisible(m_items.isEmpty());
    m_appList->setVisible(!m_items.isEmpty());
}

void StartupAppsWidget::refreshList()
{
    loadApps();
}

void StartupAppsWidget::onAddClicked()
{
    StartupAppEditDialog dialog(QString(), this);
    connect(&dialog, &StartupAppEditDialog::saved, this, &StartupAppsWidget::onDirectoryChanged);
    dialog.exec();
}

void StartupAppsWidget::onDirectoryChanged()
{
    // 延迟刷新，避免频繁更新
    QTimer::singleShot(100, this, &StartupAppsWidget::loadApps);
}

void StartupAppsWidget::onAppStatusChanged()
{
    loadApps();
}
