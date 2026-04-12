/*
 * Settings Dialog - Implementation
 * 设置对话框 - 实现
 */

#include "settingsdialog.h"
#include "../utils/config.h"
#include "../utils/logger.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QMessageBox>
#include <QInputDialog>
#include <QCoreApplication>
#include <QDir>

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
    , m_keepEmptyDirList(nullptr)
    , m_fullProtectList(nullptr)
    , m_filePatternList(nullptr)
    , m_systemKeepEmptyDirList(nullptr)
    , m_systemFullProtectList(nullptr)
    , m_systemFilePatternList(nullptr)
    , m_journalKeepDaysSpin(nullptr)
    , m_journalMaxSizeSpin(nullptr)
    , m_snapshotKeepCountSpin(nullptr)
    , m_confirmBeforeCleanupCheck(nullptr)
    , m_closeActionGroup(nullptr)
    , m_closeQuitRadio(nullptr)
    , m_closeMinimizeRadio(nullptr)

{
    initUI();
    loadSettings();
    
    setWindowTitle(tr("设置"));
    setMinimumSize(750, 550);
}

SettingsDialog::~SettingsDialog()
{
}

void SettingsDialog::initUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(15, 15, 15, 15);
    
    // 创建标签页
    QTabWidget *tabWidget = new QTabWidget(this);
    
    // 创建各标签页内容
    QWidget *generalTab = new QWidget();
    QVBoxLayout *generalLayout = new QVBoxLayout(generalTab);
    createGeneralTab(generalLayout);
    tabWidget->addTab(generalTab, tr("常规设置"));
    
    QWidget *keepEmptyDirTab = new QWidget();
    QHBoxLayout *keepEmptyDirLayout = new QHBoxLayout(keepEmptyDirTab);
    createKeepEmptyDirTab(keepEmptyDirLayout);
    tabWidget->addTab(keepEmptyDirTab, tr("保留空目录"));
    
    QWidget *fullProtectTab = new QWidget();
    QHBoxLayout *fullProtectLayout = new QHBoxLayout(fullProtectTab);
    createFullProtectTab(fullProtectLayout);
    tabWidget->addTab(fullProtectTab, tr("完全保护"));
    
    QWidget *filePatternTab = new QWidget();
    QHBoxLayout *filePatternLayout = new QHBoxLayout(filePatternTab);
    createFilePatternTab(filePatternLayout);
    tabWidget->addTab(filePatternTab, tr("保护文件"));
    
    mainLayout->addWidget(tabWidget, 1);
    
    // 底部按钮
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(10);
    
    QPushButton *importBtn = new QPushButton(tr("导入配置"), this);
    QPushButton *exportBtn = new QPushButton(tr("导出配置"), this);
    QPushButton *resetBtn = new QPushButton(tr("恢复默认"), this);
    QPushButton *okBtn = new QPushButton(tr("确定"), this);
    QPushButton *cancelBtn = new QPushButton(tr("取消"), this);
    
    okBtn->setDefault(true);
    
    buttonLayout->addWidget(importBtn);
    buttonLayout->addWidget(exportBtn);
    buttonLayout->addStretch();
    buttonLayout->addWidget(resetBtn);
    buttonLayout->addSpacing(20);
    buttonLayout->addWidget(okBtn);
    buttonLayout->addWidget(cancelBtn);
    
    mainLayout->addLayout(buttonLayout);
    
    // 连接信号
    connect(okBtn, &QPushButton::clicked, this, &SettingsDialog::onAccepted);
    connect(cancelBtn, &QPushButton::clicked, this, &SettingsDialog::onRejected);
    connect(resetBtn, &QPushButton::clicked, this, &SettingsDialog::onResetDefaults);
    connect(importBtn, &QPushButton::clicked, this, &SettingsDialog::onImportConfig);
    connect(exportBtn, &QPushButton::clicked, this, &SettingsDialog::onExportConfig);
}

void SettingsDialog::createGeneralTab(QVBoxLayout *layout)
{
    layout->setSpacing(15);
    
    // 清理设置组
    QGroupBox *cleanupGroup = new QGroupBox(tr("清理设置"));
    QGridLayout *cleanupLayout = new QGridLayout(cleanupGroup);
    cleanupLayout->setSpacing(10);
    
    // 日志保留天数
    QLabel *journalKeepLabel = new QLabel(tr("系统日志保留天数:"));
    m_journalKeepDaysSpin = new QSpinBox();
    m_journalKeepDaysSpin->setRange(1, 365);
    m_journalKeepDaysSpin->setValue(7);
    m_journalKeepDaysSpin->setToolTip(tr("清理系统日志时，保留最近N天的日志"));
    cleanupLayout->addWidget(journalKeepLabel, 0, 0);
    cleanupLayout->addWidget(m_journalKeepDaysSpin, 0, 1);
    
    // 日志最大大小
    QLabel *journalSizeLabel = new QLabel(tr("日志最大大小(MB):"));
    m_journalMaxSizeSpin = new QSpinBox();
    m_journalMaxSizeSpin->setRange(10, 10000);
    m_journalMaxSizeSpin->setValue(100);
    m_journalMaxSizeSpin->setToolTip(tr("系统日志最大占用空间"));
    cleanupLayout->addWidget(journalSizeLabel, 1, 0);
    cleanupLayout->addWidget(m_journalMaxSizeSpin, 1, 1);
    
    // 快照保留数量
    QLabel *snapshotLabel = new QLabel(tr("系统快照保留数量:"));
    m_snapshotKeepCountSpin = new QSpinBox();
    m_snapshotKeepCountSpin->setRange(1, 20);
    m_snapshotKeepCountSpin->setValue(3);
    m_snapshotKeepCountSpin->setToolTip(tr("清理系统快照时，保留最新的N个快照"));
    cleanupLayout->addWidget(snapshotLabel, 2, 0);
    cleanupLayout->addWidget(m_snapshotKeepCountSpin, 2, 1);
    
    cleanupLayout->setColumnStretch(2, 1);
    
    layout->addWidget(cleanupGroup);
    
    // 行为设置组
    QGroupBox *behaviorGroup = new QGroupBox(tr("行为设置"));
    QVBoxLayout *behaviorLayout = new QVBoxLayout(behaviorGroup);
    
    m_confirmBeforeCleanupCheck = new QCheckBox(tr("清理前显示确认对话框"));
    m_confirmBeforeCleanupCheck->setChecked(true);
    behaviorLayout->addWidget(m_confirmBeforeCleanupCheck);
    
    // 关闭窗口行为
    QLabel *closeActionLabel = new QLabel(tr("关闭窗口时:"));
    closeActionLabel->setStyleSheet("font-weight: bold; margin-top: 8px;");
    behaviorLayout->addWidget(closeActionLabel);
    
    m_closeActionGroup = new QButtonGroup(this);
    m_closeQuitRadio = new QRadioButton(tr("直接退出程序"));
    m_closeMinimizeRadio = new QRadioButton(tr("最小化到系统托盘"));
    m_closeQuitRadio->setChecked(true);
    m_closeActionGroup->addButton(m_closeQuitRadio, 0);
    m_closeActionGroup->addButton(m_closeMinimizeRadio, 1);
    behaviorLayout->addWidget(m_closeQuitRadio);
    behaviorLayout->addWidget(m_closeMinimizeRadio);
    
    layout->addWidget(behaviorGroup);
    layout->addStretch();
}

void SettingsDialog::createKeepEmptyDirTab(QHBoxLayout *layout)
{
    layout->setSpacing(15);
    
    // 用户级白名单
    QGroupBox *userGroup = new QGroupBox(tr("用户级白名单"));
    userGroup->setToolTip(tr("清理时保留目录结构，只清空目录内容。\n这些是您自定义的规则。"));
    QVBoxLayout *userLayout = new QVBoxLayout(userGroup);
    
    QLabel *userHint = new QLabel(tr("清理时保留这些目录结构，只清空内容"));
    userHint->setStyleSheet("color: #666; font-size: 11px;");
    userLayout->addWidget(userHint);
    
    m_keepEmptyDirList = new QListWidget();
    m_keepEmptyDirList->setSelectionMode(QAbstractItemView::SingleSelection);
    userLayout->addWidget(m_keepEmptyDirList);
    
    QHBoxLayout *userBtnLayout = new QHBoxLayout();
    m_addKeepEmptyDirBtn = new QPushButton(tr("添加"));
    m_removeKeepEmptyDirBtn = new QPushButton(tr("删除"));
    userBtnLayout->addStretch();
    userBtnLayout->addWidget(m_addKeepEmptyDirBtn);
    userBtnLayout->addWidget(m_removeKeepEmptyDirBtn);
    userLayout->addLayout(userBtnLayout);
    
    layout->addWidget(userGroup, 1);
    
    // 系统级白名单
    QGroupBox *systemGroup = new QGroupBox(tr("系统级白名单（内置）"));
    systemGroup->setToolTip(tr("这些是内置保护目录，不可修改。\n如 supervisor、nginx 等服务日志目录。"));
    QVBoxLayout *systemLayout = new QVBoxLayout(systemGroup);
    
    QLabel *systemHint = new QLabel(tr("内置保护目录（不可修改）"));
    systemHint->setStyleSheet("color: #888; font-size: 11px;");
    systemLayout->addWidget(systemHint);
    
    m_systemKeepEmptyDirList = new QListWidget();
    m_systemKeepEmptyDirList->setEnabled(false);
    m_systemKeepEmptyDirList->setStyleSheet("QListWidget { background-color: #f0f0f0; color: #666; }");
    
    // 加载系统级白名单
    Config *config = Config::instance();
    for (const QString &path : config->getSystemKeepEmptyDirs()) {
        m_systemKeepEmptyDirList->addItem(path);
    }
    
    systemLayout->addWidget(m_systemKeepEmptyDirList);
    layout->addWidget(systemGroup, 1);
    
    // 连接信号
    connect(m_addKeepEmptyDirBtn, &QPushButton::clicked, this, &SettingsDialog::onAddKeepEmptyDir);
    connect(m_removeKeepEmptyDirBtn, &QPushButton::clicked, this, &SettingsDialog::onRemoveKeepEmptyDir);
}

void SettingsDialog::createFullProtectTab(QHBoxLayout *layout)
{
    layout->setSpacing(15);
    
    // 用户级白名单
    QGroupBox *userGroup = new QGroupBox(tr("用户级白名单"));
    userGroup->setToolTip(tr("这些目录/文件完全不参与扫描和清理。\n适用于重要数据保护。"));
    QVBoxLayout *userLayout = new QVBoxLayout(userGroup);
    
    QLabel *userHint = new QLabel(tr("这些路径不参与扫描和清理"));
    userHint->setStyleSheet("color: #666; font-size: 11px;");
    userLayout->addWidget(userHint);
    
    m_fullProtectList = new QListWidget();
    m_fullProtectList->setSelectionMode(QAbstractItemView::SingleSelection);
    userLayout->addWidget(m_fullProtectList);
    
    QHBoxLayout *userBtnLayout = new QHBoxLayout();
    m_addFullProtectBtn = new QPushButton(tr("添加"));
    m_removeFullProtectBtn = new QPushButton(tr("删除"));
    userBtnLayout->addStretch();
    userBtnLayout->addWidget(m_addFullProtectBtn);
    userBtnLayout->addWidget(m_removeFullProtectBtn);
    userLayout->addLayout(userBtnLayout);
    
    layout->addWidget(userGroup, 1);
    
    // 系统级白名单
    QGroupBox *systemGroup = new QGroupBox(tr("系统级白名单（内置）"));
    systemGroup->setToolTip(tr("这些是内置保护目录，不可修改。\n如 /etc、/boot、~/.ssh 等关键系统目录。"));
    QVBoxLayout *systemLayout = new QVBoxLayout(systemGroup);
    
    QLabel *systemHint = new QLabel(tr("内置保护目录（不可修改）"));
    systemHint->setStyleSheet("color: #888; font-size: 11px;");
    systemLayout->addWidget(systemHint);
    
    m_systemFullProtectList = new QListWidget();
    m_systemFullProtectList->setEnabled(false);
    m_systemFullProtectList->setStyleSheet("QListWidget { background-color: #f0f0f0; color: #666; }");
    
    // 加载系统级白名单
    Config *config = Config::instance();
    for (const QString &path : config->getSystemFullProtectDirs()) {
        m_systemFullProtectList->addItem(path);
    }
    
    systemLayout->addWidget(m_systemFullProtectList);
    layout->addWidget(systemGroup, 1);
    
    // 连接信号
    connect(m_addFullProtectBtn, &QPushButton::clicked, this, &SettingsDialog::onAddFullProtect);
    connect(m_removeFullProtectBtn, &QPushButton::clicked, this, &SettingsDialog::onRemoveFullProtect);
}

void SettingsDialog::createFilePatternTab(QHBoxLayout *layout)
{
    layout->setSpacing(15);
    
    // 用户级白名单
    QGroupBox *userGroup = new QGroupBox(tr("用户级白名单"));
    userGroup->setToolTip(tr("匹配这些模式的文件不会被删除。\n支持通配符，如 *.lock, config.*"));
    QVBoxLayout *userLayout = new QVBoxLayout(userGroup);
    
    QLabel *userHint = new QLabel(tr("匹配这些模式的文件不会被删除"));
    userHint->setStyleSheet("color: #666; font-size: 11px;");
    userLayout->addWidget(userHint);
    
    m_filePatternList = new QListWidget();
    m_filePatternList->setSelectionMode(QAbstractItemView::SingleSelection);
    userLayout->addWidget(m_filePatternList);
    
    QHBoxLayout *userBtnLayout = new QHBoxLayout();
    m_addFilePatternBtn = new QPushButton(tr("添加"));
    m_removeFilePatternBtn = new QPushButton(tr("删除"));
    userBtnLayout->addStretch();
    userBtnLayout->addWidget(m_addFilePatternBtn);
    userBtnLayout->addWidget(m_removeFilePatternBtn);
    userLayout->addLayout(userBtnLayout);
    
    layout->addWidget(userGroup, 1);
    
    // 系统级白名单
    QGroupBox *systemGroup = new QGroupBox(tr("系统级白名单（内置）"));
    systemGroup->setToolTip(tr("这些是内置保护文件模式，不可修改。\n如 *.lock, *.pid 等系统文件。"));
    QVBoxLayout *systemLayout = new QVBoxLayout(systemGroup);
    
    QLabel *systemHint = new QLabel(tr("内置保护文件模式（不可修改）"));
    systemHint->setStyleSheet("color: #888; font-size: 11px;");
    systemLayout->addWidget(systemHint);
    
    m_systemFilePatternList = new QListWidget();
    m_systemFilePatternList->setEnabled(false);
    m_systemFilePatternList->setStyleSheet("QListWidget { background-color: #f0f0f0; color: #666; }");
    
    // 加载系统级白名单
    Config *config = Config::instance();
    for (const QString &pattern : config->getSystemFilePatterns()) {
        m_systemFilePatternList->addItem(pattern);
    }
    
    systemLayout->addWidget(m_systemFilePatternList);
    layout->addWidget(systemGroup, 1);
    
    // 连接信号
    connect(m_addFilePatternBtn, &QPushButton::clicked, this, &SettingsDialog::onAddFilePattern);
    connect(m_removeFilePatternBtn, &QPushButton::clicked, this, &SettingsDialog::onRemoveFilePattern);
}

void SettingsDialog::loadSettings()
{
    Config *config = Config::instance();
    
    // 加载常规设置
    m_journalKeepDaysSpin->setValue(config->getJournalKeepDays());
    m_journalMaxSizeSpin->setValue(config->getJournalMaxSizeMB());
    m_snapshotKeepCountSpin->setValue(config->getSnapshotKeepCount());
    m_confirmBeforeCleanupCheck->setChecked(config->getConfirmBeforeCleanup());
    
    // 加载关闭行为设置
    int closeAction = config->getCloseAction();
    if (closeAction == 1) {
        m_closeMinimizeRadio->setChecked(true);
    } else {
        m_closeQuitRadio->setChecked(true);
    }

    
    // 加载用户级白名单
    m_keepEmptyDirList->clear();
    for (const QString &path : config->getKeepEmptyDirWhitelist()) {
        m_keepEmptyDirList->addItem(path);
    }
    
    m_fullProtectList->clear();
    for (const QString &path : config->getFullProtectWhitelist()) {
        m_fullProtectList->addItem(path);
    }
    
    m_filePatternList->clear();
    for (const QString &pattern : config->getFilePatternWhitelist()) {
        m_filePatternList->addItem(pattern);
    }
}

void SettingsDialog::saveSettings()
{
    Config *config = Config::instance();
    
    // 保存常规设置
    config->setJournalKeepDays(m_journalKeepDaysSpin->value());
    config->setJournalMaxSizeMB(m_journalMaxSizeSpin->value());
    config->setSnapshotKeepCount(m_snapshotKeepCountSpin->value());
    config->setConfirmBeforeCleanup(m_confirmBeforeCleanupCheck->isChecked());
    config->setCloseAction(m_closeActionGroup->checkedId());

    
    // 保存用户级白名单
    QStringList keepEmptyDirs;
    for (int i = 0; i < m_keepEmptyDirList->count(); ++i) {
        keepEmptyDirs.append(m_keepEmptyDirList->item(i)->text());
    }
    config->setKeepEmptyDirWhitelist(keepEmptyDirs);
    
    QStringList fullProtect;
    for (int i = 0; i < m_fullProtectList->count(); ++i) {
        fullProtect.append(m_fullProtectList->item(i)->text());
    }
    config->setFullProtectWhitelist(fullProtect);
    
    QStringList filePatterns;
    for (int i = 0; i < m_filePatternList->count(); ++i) {
        filePatterns.append(m_filePatternList->item(i)->text());
    }
    config->setFilePatternWhitelist(filePatterns);
    
    config->save();
    LOG_INFO("Settings saved");
}

void SettingsDialog::onAccepted()
{
    saveSettings();
    accept();
}

void SettingsDialog::onRejected()
{
    reject();
}

void SettingsDialog::onResetDefaults()
{
    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        tr("恢复默认设置"),
        tr("确定要恢复默认设置吗？\n这将清除所有用户级白名单配置。"),
        QMessageBox::Yes | QMessageBox::No
    );
    
    if (reply == QMessageBox::Yes) {
        Config *config = Config::instance();
        config->reset();
        loadSettings();
        QMessageBox::information(this, tr("成功"), tr("已恢复默认设置"));
    }
}

void SettingsDialog::onAddKeepEmptyDir()
{
    QString dir = QFileDialog::getExistingDirectory(
        this,
        tr("选择目录"),
        QDir::rootPath(),
        QFileDialog::ShowDirsOnly
    );
    
    if (!dir.isEmpty()) {
        // 检查是否已存在
        for (int i = 0; i < m_keepEmptyDirList->count(); ++i) {
            if (m_keepEmptyDirList->item(i)->text() == dir) {
                QMessageBox::warning(this, tr("提示"), tr("该目录已在白名单中"));
                return;
            }
        }
        m_keepEmptyDirList->addItem(dir);
    }
}

void SettingsDialog::onRemoveKeepEmptyDir()
{
    QListWidgetItem *item = m_keepEmptyDirList->currentItem();
    if (item) {
        delete m_keepEmptyDirList->takeItem(m_keepEmptyDirList->row(item));
    }
}

void SettingsDialog::onAddFullProtect()
{
    QString path = QFileDialog::getExistingDirectory(
        this,
        tr("选择目录"),
        QDir::rootPath()
    );
    
    if (!path.isEmpty()) {
        // 检查是否已存在
        for (int i = 0; i < m_fullProtectList->count(); ++i) {
            if (m_fullProtectList->item(i)->text() == path) {
                QMessageBox::warning(this, tr("提示"), tr("该路径已在白名单中"));
                return;
            }
        }
        m_fullProtectList->addItem(path);
    }
}

void SettingsDialog::onRemoveFullProtect()
{
    QListWidgetItem *item = m_fullProtectList->currentItem();
    if (item) {
        delete m_fullProtectList->takeItem(m_fullProtectList->row(item));
    }
}

void SettingsDialog::onAddFilePattern()
{
    bool ok;
    QString pattern = QInputDialog::getText(
        this,
        tr("添加文件模式"),
        tr("输入文件模式（支持通配符）：\n例如: *.lock, config.*, *.bak"),
        QLineEdit::Normal,
        QString(),
        &ok
    );
    
    if (ok && !pattern.isEmpty()) {
        // 检查是否已存在
        for (int i = 0; i < m_filePatternList->count(); ++i) {
            if (m_filePatternList->item(i)->text() == pattern) {
                QMessageBox::warning(this, tr("提示"), tr("该模式已在白名单中"));
                return;
            }
        }
        m_filePatternList->addItem(pattern);
    }
}

void SettingsDialog::onRemoveFilePattern()
{
    QListWidgetItem *item = m_filePatternList->currentItem();
    if (item) {
        delete m_filePatternList->takeItem(m_filePatternList->row(item));
    }
}

void SettingsDialog::onExportConfig()
{
    QString filePath = QFileDialog::getSaveFileName(
        this,
        tr("导出配置"),
        QDir::homePath() + "/disk-cleaner-config.ini",
        tr("配置文件 (*.ini);;所有文件 (*)")
    );
    
    if (!filePath.isEmpty()) {
        if (Config::instance()->exportConfig(filePath)) {
            QMessageBox::information(this, tr("成功"), tr("配置已导出到:\n%1").arg(filePath));
        } else {
            QMessageBox::warning(this, tr("失败"), tr("导出配置失败"));
        }
    }
}

void SettingsDialog::onImportConfig()
{
    QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("导入配置"),
        QDir::homePath(),
        tr("配置文件 (*.ini);;所有文件 (*)")
    );
    
    if (!filePath.isEmpty()) {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this,
            tr("导入配置"),
            tr("导入配置将覆盖当前设置，是否继续？"),
            QMessageBox::Yes | QMessageBox::No
        );
        
        if (reply == QMessageBox::Yes) {
            if (Config::instance()->importConfig(filePath)) {
                loadSettings();
                QMessageBox::information(this, tr("成功"), tr("配置已导入"));
            } else {
                QMessageBox::warning(this, tr("失败"), tr("导入配置失败，请检查文件格式"));
            }
        }
    }
}
