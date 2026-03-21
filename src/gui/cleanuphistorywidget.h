/*
 * Cleanup History Widget - Header
 * 清理历史组件 - 头文件
 */

#ifndef CLEANUPHISTORYWIDGET_H
#define CLEANUPHISTORYWIDGET_H

#include <QWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QDateTime>

// 清理历史记录项
struct CleanupHistoryItem {
    QString id;                     // 唯一标识
    QDateTime timestamp;            // 清理时间
    QString cleanupType;            // 清理类型：智能清理/自定义清理/磁盘分析清理
    qint64 totalFreed;              // 释放总空间（字节）
    int successCount;               // 成功项目数
    int failCount;                  // 失败项目数
    QStringList detailItems;        // 详细清理项目
    QString details;                // 详细信息（JSON或其他格式）
    
    QVariant toVariant() const;
    static CleanupHistoryItem fromVariant(const QVariant &variant);
};

class CleanupHistoryWidget : public QWidget {
    Q_OBJECT

public:
    explicit CleanupHistoryWidget(QWidget *parent = nullptr);
    ~CleanupHistoryWidget();
    
    // 添加清理历史记录
    static void addHistory(const QString &cleanupType, qint64 totalFreed, 
                          int successCount, int failCount, 
                          const QStringList &detailItems);
    
    // 获取所有历史记录
    static QList<CleanupHistoryItem> getAllHistory();
    
    // 清除所有历史记录
    static void clearAllHistory();
    
    // 清除指定日期之前的历史记录
    static void clearHistoryBefore(const QDateTime &dateTime);
    
    // 刷新历史记录显示
    void refresh();
    
signals:
    void historyChanged();
    
private slots:
    void onClearSelected();
    void onClearAll();
    void onExportHistory();
    void onFilterChanged(int index);
    void onItemDoubleClicked(int row, int column);
    
private:
    void initUI();
    void setupConnections();
    void loadHistory();
    void updateTable();
    void applyTheme();
    bool isDarkTheme();
    QString formatFileSize(qint64 bytes);
    QString formatDateTime(const QDateTime &dateTime);
    QString formatHistoryItem(const CleanupHistoryItem &item);
    
    QTableWidget *m_historyTable;
    QLabel *m_totalLabel;
    QLabel *m_countLabel;
    QPushButton *m_clearSelectedBtn;
    QPushButton *m_clearAllBtn;
    QPushButton *m_exportBtn;
    QComboBox *m_filterCombo;
    
    QList<CleanupHistoryItem> m_historyList;
    int m_currentFilter;  // 0:全部 1:今天 2:本周 3:本月
};

#endif // CLEANUPHISTORYWIDGET_H
