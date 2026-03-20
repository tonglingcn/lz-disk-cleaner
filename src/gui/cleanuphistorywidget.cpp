/*
 * Cleanup History Widget - Implementation
 * 清理历史组件 - 实现
 */

#include "cleanuphistorywidget.h"
#include "../utils/config.h"
#include "../utils/logger.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QFileDialog>
#include <QTextStream>
#include <QFile>
#include <QApplication>
#include <QPalette>
#include <QUuid>
#include <QCheckBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

// ==================== CleanupHistoryItem ====================

QVariant CleanupHistoryItem::toVariant() const
{
    QJsonObject obj;
    obj["id"] = id;
    obj["timestamp"] = timestamp.toString(Qt::ISODate);
    obj["cleanupType"] = cleanupType;
    obj["totalFreed"] = totalFreed;
    obj["successCount"] = successCount;
    obj["failCount"] = failCount;
    obj["details"] = details;
    
    QJsonArray itemsArray;
    for (const QString &item : detailItems) {
        itemsArray.append(item);
    }
    obj["detailItems"] = itemsArray;
    
    return obj;
}

CleanupHistoryItem CleanupHistoryItem::fromVariant(const QVariant &variant)
{
    CleanupHistoryItem item;
    QJsonObject obj = variant.toJsonObject();
    
    item.id = obj["id"].toString();
    item.timestamp = QDateTime::fromString(obj["timestamp"].toString(), Qt::ISODate);
    item.cleanupType = obj["cleanupType"].toString();
    item.totalFreed = obj["totalFreed"].toVariant().toLongLong();
    item.successCount = obj["successCount"].toInt();
    item.failCount = obj["failCount"].toInt();
    item.details = obj["details"].toString();
    
    QJsonArray itemsArray = obj["detailItems"].toArray();
    for (const QJsonValue &value : itemsArray) {
        item.detailItems.append(value.toString());
    }
    
    return item;
}

// ==================== CleanupHistoryWidget ====================

CleanupHistoryWidget::CleanupHistoryWidget(QWidget *parent)
    : QWidget(parent)
    , m_historyTable(nullptr)
    , m_totalLabel(nullptr)
    , m_countLabel(nullptr)
    , m_clearSelectedBtn(nullptr)
    , m_clearAllBtn(nullptr)
    , m_exportBtn(nullptr)
    , m_filterCombo(nullptr)
    , m_currentFilter(0)
{
    initUI();
    setupConnections();
    loadHistory();
    applyTheme();
}

CleanupHistoryWidget::~CleanupHistoryWidget()
{
}

void CleanupHistoryWidget::initUI()
{
    bool darkMode = isDarkTheme();
    QString titleColor = darkMode ? "#e0e0e0" : "#2C3E50";
    QString countColor = darkMode ? "#a0a0a0" : "#7F8C8D";
    QString tableBg = darkMode ? "#2d2d2d" : "white";
    QString tableBorder = darkMode ? "#555555" : "#E0E0E0";
    QString tableGrid = darkMode ? "#3d3d3d" : "#ECF0F1";
    QString tableSelectedBg = darkMode ? "#3d5a7d" : "#D6EAF8";
    QString headerBg = darkMode ? "#3d3d3d" : "#F8F9FA";
    QString headerColor = darkMode ? "#e0e0e0" : "#2C3E50";
    QString btnBg = darkMode ? "#4a4a4a" : "#ECF0F1";
    QString btnColor = darkMode ? "#e0e0e0" : "#2C3E50";
    QString btnHoverBg = darkMode ? "#5a5a5a" : "#D5DBDB";
    QString dangerBtnBg = "#E74C3C";
    QString dangerBtnHoverBg = "#C0392B";
    
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    
    // 顶部标题栏
    QHBoxLayout *headerLayout = new QHBoxLayout();
    
    QLabel *titleLabel = new QLabel(tr("清理历史"), this);
    titleLabel->setStyleSheet(QString("font-size: 20px; font-weight: bold; color: %1;").arg(titleColor));
    headerLayout->addWidget(titleLabel);
    
    headerLayout->addStretch();
    
    // 筛选下拉框
    QLabel *filterLabel = new QLabel(tr("筛选:"), this);
    filterLabel->setStyleSheet(QString("font-size: 13px; color: %1;").arg(countColor));
    headerLayout->addWidget(filterLabel);
    
    m_filterCombo = new QComboBox(this);
    m_filterCombo->addItem(tr("全部"));
    m_filterCombo->addItem(tr("今天"));
    m_filterCombo->addItem(tr("本周"));
    m_filterCombo->addItem(tr("本月"));
    m_filterCombo->setStyleSheet(
        QString("QComboBox { "
        "   background-color: %1; "
        "   color: %2; "
        "   border: 1px solid %3; "
        "   border-radius: 4px; "
        "   padding: 5px 10px; "
        "   min-width: 100px; "
        "}"
        "QComboBox:hover { background-color: %4; }"
        "QComboBox::drop-down { border: none; }"
        "QComboBox QAbstractItemView { "
        "   background-color: %1; "
        "   color: %2; "
        "   selection-background-color: %5; "
        "}")
        .arg(btnBg, btnColor, tableBorder, btnHoverBg, tableSelectedBg)
    );
    headerLayout->addWidget(m_filterCombo);
    
    mainLayout->addLayout(headerLayout);
    
    // 统计信息行
    QHBoxLayout *statsLayout = new QHBoxLayout();
    
    m_countLabel = new QLabel(tr("共 0 条记录"), this);
    m_countLabel->setStyleSheet(QString("font-size: 13px; color: %1;").arg(countColor));
    statsLayout->addWidget(m_countLabel);
    
    statsLayout->addStretch();
    
    m_totalLabel = new QLabel(tr("累计释放: 0 B"), this);
    m_totalLabel->setStyleSheet("font-size: 13px; color: #27AE60; font-weight: bold;");
    statsLayout->addWidget(m_totalLabel);
    
    mainLayout->addLayout(statsLayout);
    
    // 历史记录表格
    m_historyTable = new QTableWidget(this);
    m_historyTable->setColumnCount(6);
    m_historyTable->setHorizontalHeaderLabels(
        QStringList() << tr("序号") << tr("选择") << tr("清理时间") << tr("清理类型") << tr("释放空间") << tr("状态"));
    
    // 设置列宽
    m_historyTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_historyTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_historyTable->setColumnWidth(0, 60);
    m_historyTable->setColumnWidth(1, 60);
    m_historyTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    m_historyTable->setColumnWidth(2, 180);
    m_historyTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_historyTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
    m_historyTable->setColumnWidth(4, 100);
    m_historyTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Fixed);
    m_historyTable->setColumnWidth(5, 150);
    
    m_historyTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_historyTable->setAlternatingRowColors(true);
    m_historyTable->verticalHeader()->setDefaultSectionSize(36);
    m_historyTable->verticalHeader()->setVisible(false);
    
    m_historyTable->setStyleSheet(
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
        "QCheckBox:focus { outline: none; }")
        .arg(tableBorder, tableBg, tableGrid, titleColor, tableSelectedBg, titleColor, headerBg, tableBorder, headerColor)
    );
    
    mainLayout->addWidget(m_historyTable, 1);
    
    // 底部按钮行
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    
    m_exportBtn = new QPushButton(tr("导出历史"), this);
    m_exportBtn->setFixedSize(100, 36);
    m_exportBtn->setStyleSheet(
        QString("QPushButton { "
        "   background-color: #3498DB; "
        "   color: white; "
        "   border: none; "
        "   border-radius: 6px; "
        "   font-size: 12px; "
        "}"
        "QPushButton:hover { background-color: #2980B9; }")
    );
    buttonLayout->addWidget(m_exportBtn);
    
    m_clearSelectedBtn = new QPushButton(tr("删除选中"), this);
    m_clearSelectedBtn->setFixedSize(100, 36);
    m_clearSelectedBtn->setStyleSheet(
        QString("QPushButton { "
        "   background-color: %1; "
        "   color: %2; "
        "   border: none; "
        "   border-radius: 6px; "
        "   font-size: 12px; "
        "}"
        "QPushButton:hover { background-color: %3; }")
        .arg(btnBg, btnColor, btnHoverBg)
    );
    buttonLayout->addWidget(m_clearSelectedBtn);
    
    m_clearAllBtn = new QPushButton(tr("清空历史"), this);
    m_clearAllBtn->setFixedSize(100, 36);
    m_clearAllBtn->setStyleSheet(
        QString("QPushButton { "
        "   background-color: %1; "
        "   color: white; "
        "   border: none; "
        "   border-radius: 6px; "
        "   font-size: 12px; "
        "}"
        "QPushButton:hover { background-color: %2; }")
        .arg(dangerBtnBg, dangerBtnHoverBg)
    );
    buttonLayout->addWidget(m_clearAllBtn);
    
    mainLayout->addLayout(buttonLayout);
}

void CleanupHistoryWidget::setupConnections()
{
    connect(m_filterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &CleanupHistoryWidget::onFilterChanged);
    connect(m_historyTable, &QTableWidget::cellDoubleClicked, 
            this, &CleanupHistoryWidget::onItemDoubleClicked);
    connect(m_clearSelectedBtn, &QPushButton::clicked, 
            this, &CleanupHistoryWidget::onClearSelected);
    connect(m_clearAllBtn, &QPushButton::clicked, 
            this, &CleanupHistoryWidget::onClearAll);
    connect(m_exportBtn, &QPushButton::clicked, 
            this, &CleanupHistoryWidget::onExportHistory);
}

void CleanupHistoryWidget::addHistory(const QString &cleanupType, qint64 totalFreed, 
                                       int successCount, int failCount, 
                                       const QStringList &detailItems)
{
    CleanupHistoryItem item;
    item.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    item.timestamp = QDateTime::currentDateTime();
    item.cleanupType = cleanupType;
    item.totalFreed = totalFreed;
    item.successCount = successCount;
    item.failCount = failCount;
    item.detailItems = detailItems;
    item.details = detailItems.join("\n");
    
    // 从配置中读取现有历史
    QSettings *settings = Config::instance()->getSettings();
    if (!settings) return;
    
    QList<QVariant> historyVariants = settings->value("History/cleanupHistory").toList();
    historyVariants.prepend(item.toVariant());
    
    // 限制历史记录数量（最多保留100条）
    while (historyVariants.size() > 100) {
        historyVariants.removeLast();
    }
    
    settings->setValue("History/cleanupHistory", historyVariants);
    settings->sync();
    
    LOG_INFO(QString("Added cleanup history: %1, freed %2 bytes")
        .arg(cleanupType).arg(totalFreed));
}

QList<CleanupHistoryItem> CleanupHistoryWidget::getAllHistory()
{
    QSettings *settings = Config::instance()->getSettings();
    if (!settings) return QList<CleanupHistoryItem>();
    
    QList<CleanupHistoryItem> historyList;
    QList<QVariant> historyVariants = settings->value("History/cleanupHistory").toList();
    
    for (const QVariant &variant : historyVariants) {
        historyList.append(CleanupHistoryItem::fromVariant(variant));
    }
    
    return historyList;
}

void CleanupHistoryWidget::clearAllHistory()
{
    QSettings *settings = Config::instance()->getSettings();
    if (!settings) return;
    
    settings->remove("History/cleanupHistory");
    settings->sync();
    
    LOG_INFO("Cleared all cleanup history");
}

void CleanupHistoryWidget::clearHistoryBefore(const QDateTime &dateTime)
{
    QSettings *settings = Config::instance()->getSettings();
    if (!settings) return;
    
    QList<CleanupHistoryItem> historyList = getAllHistory();
    QList<QVariant> newHistory;
    
    for (const CleanupHistoryItem &item : historyList) {
        if (item.timestamp >= dateTime) {
            newHistory.append(item.toVariant());
        }
    }
    
    settings->setValue("History/cleanupHistory", newHistory);
    settings->sync();
    
    LOG_INFO(QString("Cleared cleanup history before %1").arg(dateTime.toString()));
}

void CleanupHistoryWidget::loadHistory()
{
    m_historyList = getAllHistory();
    updateTable();
}

void CleanupHistoryWidget::refresh()
{
    loadHistory();
}

void CleanupHistoryWidget::updateTable()
{
    // 应用筛选
    QList<CleanupHistoryItem> filteredList;
    QDateTime now = QDateTime::currentDateTime();
    
    for (const CleanupHistoryItem &item : m_historyList) {
        bool include = false;
        switch (m_currentFilter) {
        case 0: // 全部
            include = true;
            break;
        case 1: // 今天
            include = item.timestamp.date() == now.date();
            break;
        case 2: // 本周
            include = item.timestamp.date().weekNumber() == now.date().weekNumber() &&
                     item.timestamp.date().year() == now.date().year();
            break;
        case 3: // 本月
            include = item.timestamp.date().month() == now.date().month() &&
                     item.timestamp.date().year() == now.date().year();
            break;
        }
        if (include) {
            filteredList.append(item);
        }
    }
    
    // 更新表格
    m_historyTable->clearContents();
    m_historyTable->setRowCount(filteredList.size());
    
    qint64 totalFreed = 0;
    bool darkMode = isDarkTheme();
    QString checkBoxUncheckedBorder = darkMode ? "#666666" : "#95a5a6";
    QString checkBoxUncheckedBg = darkMode ? "#3d3d3d" : "white";
    
    for (int i = 0; i < filteredList.size(); ++i) {
        const CleanupHistoryItem &item = filteredList.at(i);
        totalFreed += item.totalFreed;
        
        // 序号
        QTableWidgetItem *indexItem = new QTableWidgetItem(QString::number(i + 1));
        indexItem->setTextAlignment(Qt::AlignCenter);
        indexItem->setFlags(indexItem->flags() & ~Qt::ItemIsEditable);
        m_historyTable->setItem(i, 0, indexItem);
        
        // 复选框
        QCheckBox *checkBox = new QCheckBox();
        checkBox->setChecked(false);
        checkBox->setStyleSheet(
            QString("QCheckBox { spacing: 0px; }"
            "QCheckBox::indicator {"
            "   width: 18px;"
            "   height: 18px;"
            "}"
            "QCheckBox::indicator:unchecked {"
            "   border: 2px solid %1;"
            "   background: %2;"
            "   border-radius: 4px;"
            "}"
            "QCheckBox::indicator:checked {"
            "   border: 2px solid #27AE60;"
            "   background: #27AE60;"
            "   border-radius: 4px;"
            "   image: url(:/icons/check.svg);"
            "}").arg(checkBoxUncheckedBorder, checkBoxUncheckedBg)
        );
        QWidget *checkWidget = new QWidget();
        QHBoxLayout *checkLayout = new QHBoxLayout(checkWidget);
        checkLayout->addWidget(checkBox);
        checkLayout->setAlignment(Qt::AlignCenter);
        checkLayout->setContentsMargins(0, 0, 0, 0);
        checkWidget->setLayout(checkLayout);
        m_historyTable->setCellWidget(i, 1, checkWidget);
        
        // 清理时间
        QTableWidgetItem *timeItem = new QTableWidgetItem(formatDateTime(item.timestamp));
        timeItem->setFlags(timeItem->flags() & ~Qt::ItemIsEditable);
        m_historyTable->setItem(i, 2, timeItem);
        
        // 清理类型
        QTableWidgetItem *typeItem = new QTableWidgetItem(item.cleanupType);
        typeItem->setFlags(typeItem->flags() & ~Qt::ItemIsEditable);
        typeItem->setData(Qt::UserRole, item.id);  // 存储ID
        m_historyTable->setItem(i, 3, typeItem);
        
        // 释放空间
        QTableWidgetItem *sizeItem = new QTableWidgetItem(formatFileSize(item.totalFreed));
        sizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        sizeItem->setFlags(sizeItem->flags() & ~Qt::ItemIsEditable);
        m_historyTable->setItem(i, 4, sizeItem);
        
        // 状态
        QString status = tr("成功 %1 / 失败 %2").arg(item.successCount).arg(item.failCount);
        QTableWidgetItem *statusItem = new QTableWidgetItem(status);
        statusItem->setFlags(statusItem->flags() & ~Qt::ItemIsEditable);
        if (item.failCount > 0) {
            statusItem->setForeground(QColor("#E74C3C"));
        } else {
            statusItem->setForeground(QColor("#27AE60"));
        }
        m_historyTable->setItem(i, 5, statusItem);
    }
    
    // 更新统计信息
    m_countLabel->setText(tr("共 %1 条记录").arg(filteredList.size()));
    m_totalLabel->setText(tr("累计释放: %1").arg(formatFileSize(totalFreed)));
}

void CleanupHistoryWidget::onFilterChanged(int index)
{
    m_currentFilter = index;
    updateTable();
}

void CleanupHistoryWidget::onItemDoubleClicked(int row, int column)
{
    Q_UNUSED(column);
    
    QTableWidgetItem *typeItem = m_historyTable->item(row, 3);
    if (!typeItem) return;
    
    QString id = typeItem->data(Qt::UserRole).toString();
    
    // 查找对应的历史记录
    for (const CleanupHistoryItem &item : m_historyList) {
        if (item.id == id) {
            // 显示详细信息
            QString detailText = formatHistoryItem(item);
            QMessageBox::information(this, tr("清理详情"), detailText);
            break;
        }
    }
}

void CleanupHistoryWidget::onClearSelected()
{
    // 收集选中的记录ID
    QStringList idsToRemove;
    for (int i = 0; i < m_historyTable->rowCount(); ++i) {
        QWidget *checkWidget = m_historyTable->cellWidget(i, 1);
        QCheckBox *checkBox = checkWidget ? checkWidget->findChild<QCheckBox*>() : nullptr;
        if (checkBox && checkBox->isChecked()) {
            QTableWidgetItem *typeItem = m_historyTable->item(i, 3);
            if (typeItem) {
                idsToRemove.append(typeItem->data(Qt::UserRole).toString());
            }
        }
    }
    
    if (idsToRemove.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("请先选择要删除的记录。"));
        return;
    }
    
    int ret = QMessageBox::question(this, tr("确认删除"), 
        tr("确定要删除选中的 %1 条记录吗？").arg(idsToRemove.size()),
        QMessageBox::Yes | QMessageBox::No);
    
    if (ret != QMessageBox::Yes) {
        return;
    }
    
    // 从列表中移除
    QSettings *settings = Config::instance()->getSettings();
    if (!settings) return;
    
    QList<QVariant> historyVariants = settings->value("History/cleanupHistory").toList();
    QList<QVariant> newHistory;
    
    for (const QVariant &variant : historyVariants) {
        CleanupHistoryItem item = CleanupHistoryItem::fromVariant(variant);
        if (!idsToRemove.contains(item.id)) {
            newHistory.append(variant);
        }
    }
    
    settings->setValue("History/cleanupHistory", newHistory);
    settings->sync();
    
    loadHistory();
    emit historyChanged();
    
    QMessageBox::information(this, tr("删除完成"), 
        tr("已删除 %1 条记录。").arg(idsToRemove.size()));
}

void CleanupHistoryWidget::onClearAll()
{
    if (m_historyList.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("暂无历史记录。"));
        return;
    }
    
    int ret = QMessageBox::warning(this, tr("确认清空"), 
        tr("确定要清空所有清理历史记录吗？\n此操作不可恢复！"),
        QMessageBox::Yes | QMessageBox::No);
    
    if (ret != QMessageBox::Yes) {
        return;
    }
    
    clearAllHistory();
    loadHistory();
    emit historyChanged();
    
    QMessageBox::information(this, tr("清空完成"), tr("已清空所有清理历史记录。"));
}

void CleanupHistoryWidget::onExportHistory()
{
    if (m_historyList.isEmpty()) {
        QMessageBox::information(this, tr("提示"), tr("暂无历史记录可导出。"));
        return;
    }
    
    QString filePath = QFileDialog::getSaveFileName(this, tr("导出清理历史"),
        QString("cleanup_history_%1.csv").arg(QDateTime::currentDateTime().toString("yyyyMMdd")),
        tr("CSV 文件 (*.csv);;文本文件 (*.txt)"));
    
    if (filePath.isEmpty()) {
        return;
    }
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("导出失败"), tr("无法创建文件：") + filePath);
        return;
    }
    
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    
    // 写入表头
    out << tr("清理时间") << ","
        << tr("清理类型") << ","
        << tr("释放空间") << ","
        << tr("成功数") << ","
        << tr("失败数") << ","
        << tr("详细信息") << "\n";
    
    // 写入数据
    for (const CleanupHistoryItem &item : m_historyList) {
        QString escapedDetails = item.details;
        escapedDetails.replace("\"", "\"\"");
        out << item.timestamp.toString("yyyy-MM-dd hh:mm:ss") << ","
            << item.cleanupType << ","
            << formatFileSize(item.totalFreed) << ","
            << item.successCount << ","
            << item.failCount << ","
            << "\"" << escapedDetails << "\"" << "\n";
    }
    
    file.close();
    
    QMessageBox::information(this, tr("导出成功"), 
        tr("清理历史已导出到：\n%1").arg(filePath));
}

void CleanupHistoryWidget::applyTheme()
{
    // 主题在 initUI 中已处理
}

bool CleanupHistoryWidget::isDarkTheme()
{
    QPalette palette = qApp->palette();
    QColor windowColor = palette.color(QPalette::Window);
    int brightness = (windowColor.red() * 299 + windowColor.green() * 587 + windowColor.blue() * 114) / 1000;
    return brightness < 128;
}

QString CleanupHistoryWidget::formatFileSize(qint64 bytes)
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

QString CleanupHistoryWidget::formatDateTime(const QDateTime &dateTime)
{
    QDateTime now = QDateTime::currentDateTime();
    qint64 secsAgo = dateTime.secsTo(now);
    
    if (secsAgo < 60) {
        return tr("刚刚");
    } else if (secsAgo < 3600) {
        return tr("%1 分钟前").arg(secsAgo / 60);
    } else if (secsAgo < 86400) {
        return tr("%1 小时前").arg(secsAgo / 3600);
    } else if (dateTime.date() == now.date().addDays(-1)) {
        return tr("昨天 %1").arg(dateTime.time().toString("hh:mm"));
    } else if (secsAgo < 604800) {
        return tr("%1 天前").arg(secsAgo / 86400);
    } else {
        return dateTime.toString("yyyy-MM-dd hh:mm");
    }
}

QString CleanupHistoryWidget::formatHistoryItem(const CleanupHistoryItem &item)
{
    QString text;
    text += tr("清理时间: %1\n").arg(item.timestamp.toString("yyyy-MM-dd hh:mm:ss"));
    text += tr("清理类型: %1\n").arg(item.cleanupType);
    text += tr("释放空间: %1\n").arg(formatFileSize(item.totalFreed));
    text += tr("成功项目: %1\n").arg(item.successCount);
    text += tr("失败项目: %1\n").arg(item.failCount);
    text += tr("\n详细信息:\n");
    text += item.details;
    
    return text;
}
