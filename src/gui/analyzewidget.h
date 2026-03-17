/*
 * Analyze Widget - Header
 * 磁盘分析界面 - 头文件
 * 
 * 针对 Deepin V25 系统设计的磁盘分析组件
 */

#ifndef ANALYZEWIDGET_H
#define ANALYZEWIDGET_H

#include <QWidget>
#include <QTreeWidget>
#include <QStackedLayout>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QThread>
#include <QMutex>

// 扫描类别枚举 - 针对Deepin V25系统
enum class ScanCategory {
    USER_CACHE,           // 用户缓存 ~/.cache
    THUMBNAIL_CACHE,      // 缩略图缓存 ~/.cache/thumbnails
    APT_CACHE,            // APT包缓存 /var/cache/apt/archives
    SYSTEM_LOGS,          // 系统日志 /var/log
    JOURNAL_LOGS,         // Journald日志
    CRASH_REPORTS,        // 崩溃报告 /var/crash
    TEMP_FILES,           // 临时文件 /tmp
    TRASH,                // 回收站 ~/.local/share/Trash
    BROWSER_CACHE,        // 浏览器缓存
    DEV_CACHE,            // 开发工具缓存
    LINGLONG_APPS,        // 玲珑应用
    IMMUTABLE_SNAPSHOTS   // 磐石系统快照
};

// 扫描结果项
struct ScanResult {
    ScanCategory category;
    QString name;         // 显示名称
    QString path;         // 实际路径
    QString appId;        // 应用ID（用于玲珑应用卸载）
    qint64 size;          // 文件大小
    qint64 fileCount;     // 文件数量
    bool isDirectory;
    bool isDeletable;     // 是否可安全删除
    bool isDangerous;     // 是否危险操作
    QString description;  // 描述信息
    QString purpose;      // 目录用途（如"钉钉应用数据"）
    QList<ScanResult> children; // 子目录项（用于层级展示）
};

// 扫描线程
class ScanThread : public QThread
{
    Q_OBJECT

public:
    explicit ScanThread(QObject *parent = nullptr);
    void stop();
    void setScanCategories(const QList<ScanCategory> &categories);

signals:
    void scanProgress(const QString &currentItem, int percent);
    void categoryScanned(ScanCategory category, const QList<ScanResult> &results);
    void scanFinished(qint64 totalSize, int totalFiles);

protected:
    void run() override;

private:
    QList<ScanResult> scanCategory(ScanCategory category, const QString &categoryName,
                                    int progressStart, int progressEnd);
    QList<ScanResult> scanDirectory(const QString &path, ScanCategory category, 
                                     const QString &displayName, bool isDangerous = false);
    QList<ScanResult> scanDirectoryWithChildren(const QString &path, ScanCategory category,
                                                 const QString &displayName, int depth = 0,
                                                 int progressStart = 0, int progressEnd = 100,
                                                 int currentItem = 0, int totalItems = 1);
    qint64 getDirectorySize(const QString &path, int *fileCount = nullptr);
    qint64 calculateDirSize(const QString &path);
    QString formatSize(qint64 bytes);
    QString getDirectoryPurpose(const QString &path);
    QString getAppIcon(const QString &appName);

    QList<ScanCategory> m_categories;
    bool m_stopRequested;
    QMutex m_mutex;
    
    // 进度相关
    QString m_currentCategoryName;
    int m_progressStart;
    int m_progressEnd;
};

// 分析界面主组件
class AnalyzeWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AnalyzeWidget(QWidget *parent = nullptr);
    ~AnalyzeWidget();

    void startScan();
    void stopScan();
    bool isScanning() const;
    void removeCleanedItems(const QList<ScanResult> &cleanedItems);  // 移除已清理的项目

signals:
    void scanStarted();
    void scanFinished(qint64 totalSize);
    void cleanupRequested(const QList<ScanResult> &items);

private slots:
    void onStartScanClicked();
    void onStopScanClicked();
    void onCleanupClicked();
    void onScanProgress(const QString &currentItem, int percent);
    void onCategoryScanned(ScanCategory category, const QList<ScanResult> &results);
    void onScanFinished(qint64 totalSize, int totalFiles);
    void onTreeItemChanged(QTreeWidgetItem *item, int column);
    void onTreeContextMenu(const QPoint &pos);

private:
    void initUI();
    void createInfoPage();      // 信息页 - 显示扫描类型
    void createProgressPage();  // 进度页 - 扫描动画
    void createResultPage();    // 结果页 - 扫描结果

    void updateResultTree();
    void updateTotalSize();
    void addCategoryToTree(ScanCategory category, const QList<ScanResult> &results);
    QString getCategoryName(ScanCategory category) const;
    QString getCategoryIcon(ScanCategory category) const;
    QString formatSize(qint64 bytes) const;
    bool isDarkTheme();
    void applyTheme();

    // UI 组件
    QStackedLayout *m_stackedLayout;
    
    // 信息页
    QWidget *m_infoPage;
    QPushButton *m_startScanButton;
    
    // 进度页
    QWidget *m_progressPage;
    QLabel *m_progressLabel;
    QProgressBar *m_progressBar;
    QPushButton *m_stopScanButton;
    
    // 结果页
    QWidget *m_resultPage;
    QTreeWidget *m_resultTree;
    QLabel *m_totalSizeLabel;
    QPushButton *m_rescanButton;
    QPushButton *m_cleanupButton;

    // 扫描线程
    ScanThread *m_scanThread;
    
    // 数据
    QMap<ScanCategory, QList<ScanResult>> m_scanResults;
    qint64 m_totalSize;
    int m_totalFiles;
    bool m_isScanning;
};

#endif // ANALYZEWIDGET_H
