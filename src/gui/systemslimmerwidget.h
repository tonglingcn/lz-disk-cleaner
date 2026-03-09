/*
 * System Slimmer Widget - Header
 * 系统瘦身组件 - 头文件
 */

#ifndef SYSTEMSLIMMERWIDGET_H
#define SYSTEMSLIMMERWIDGET_H

#include <QWidget>
#include <QStackedWidget>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QListWidget>
#include <QTableWidget>
#include <QCheckBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include "../core/systemslimmer.h"

// 功能卡片按钮
class FeatureCard : public QFrame {
    Q_OBJECT

public:
    explicit FeatureCard(const QString &icon, const QString &title, 
                         const QString &desc, QWidget *parent = nullptr);
    void setChecked(bool checked);
    bool isChecked() const;

signals:
    void clicked();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    bool m_checked;
    QLabel *m_iconLabel;
    QLabel *m_titleLabel;
    QLabel *m_checkLabel;
    QString m_normalStyle;
    QString m_checkedStyle;
    void updateStyle();
};

class SystemSlimmerWidget : public QWidget {
    Q_OBJECT

public:
    explicit SystemSlimmerWidget(QWidget *parent = nullptr);
    ~SystemSlimmerWidget();

private slots:
    void onStartScanClicked();
    void onCancelScanClicked();
    void onScanProgress(const QString &currentPath, int percent, int fileCount, int largeFileCount);
    void onScanFinished(const SlimmerScanResult &result);
    void onScanError(const QString &error);
    void onLargeFileSelected();
    void onDuplicateGroupSelected();
    void onDeleteSelected();
    void onMoveToTrash();
    void onBackToMain();
    void onFeatureCardToggled();

private:
    void initUI();
    void initMainPage();
    void initScanningPage();
    void initResultPage();
    void setupConnections();
    QString formatFileSize(qint64 bytes);
    void updateResultPage();
    void applyTheme();
    
    // 页面切换
    void showMainPage();
    void showScanningPage();
    void showResultPage();

    // UI 组件
    QStackedWidget *m_stackWidget;
    
    // 主页面
    QWidget *m_mainPage;
    FeatureCard *m_largeFileCard;
    FeatureCard *m_duplicateCard;
    QPushButton *m_startScanBtn;
    
    // 扫描中页面
    QWidget *m_scanningPage;
    QLabel *m_scanningLabel;
    QProgressBar *m_progressBar;
    QLabel *m_currentPathLabel;
    QPushButton *m_cancelScanBtn;
    QLabel *m_statsLabel;
    
    // 结果页面
    QWidget *m_resultPage;
    QStackedWidget *m_resultStack;
    
    // 大文件结果页
    QWidget *m_largeFileResultPage;
    QTableWidget *m_largeFileTable;
    QLabel *m_largeFileCountLabel;
    QPushButton *m_deleteLargeFileBtn;
    QPushButton *m_trashLargeFileBtn;
    
    // 重复文件结果页
    QWidget *m_duplicateResultPage;
    QTableWidget *m_duplicateTable;
    QListWidget *m_duplicateGroupList;
    QLabel *m_duplicateCountLabel;
    QLabel *m_duplicateSizeLabel;
    QPushButton *m_deleteDuplicateBtn;
    QPushButton *m_trashDuplicateBtn;
    
    // 通用按钮
    QPushButton *m_backBtn;
    QPushButton *m_rescanBtn;
    
    // 核心组件
    SystemSlimmer *m_slimmer;
    SlimmerScanResult m_lastResult;
    SlimmerScanOptions m_currentOptions;
    bool m_scanLargeFiles;
    bool m_scanDuplicates;
    
    // 状态
    qint64 m_selectedLargeFilesTotalSize;
    qint64 m_selectedDuplicateTotalSize;
};

#endif // SYSTEMSLIMMERWIDGET_H
