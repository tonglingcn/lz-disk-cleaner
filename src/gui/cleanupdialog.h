/*
 * Cleanup Dialog - Header
 * 自定义清理对话框 - 头文件
 * 
 * 功能：
 * 1. 显示系统所有硬盘分区
 * 2. 支持勾选指定分区进行扫描
 * 3. 扫描显示文件夹和文件的容量大小
 * 4. 支持深色主题
 * 5. 内置文件浏览器，双击进入文件夹
 */

#ifndef CLEANUPDIALOG_H
#define CLEANUPDIALOG_H

#include <QDialog>
#include <QTreeWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QProgressBar>
#include <QStackedWidget>
#include <QThread>
#include <QProcess>
#include <QTimer>
#include <QLineEdit>
#include <QStack>

// 分区信息结构
struct PartitionInfo {
    QString device;        // 设备名 /dev/sda1
    QString filesystem;    // 文件系统类型 ext4, ntfs等
    QString mountpoint;    // 挂载点
    qint64 total;          // 总容量
    qint64 used;           // 已用
    qint64 available;      // 可用
    double percent;        // 使用百分比
    bool isSystemPartition; // 是否为系统分区
};

// 文件/文件夹扫描结果
struct ScanItem {
    QString path;          // 完整路径
    QString name;          // 名称
    qint64 size;           // 大小
    int fileCount;         // 文件数量
    int dirCount;          // 文件夹数量
    bool isDir;            // 是否为目录
    QString mimeType;      // MIME类型（文件）
};

// 扫描线程
class PartitionScanThread : public QThread
{
    Q_OBJECT
    
public:
    explicit PartitionScanThread(QObject *parent = nullptr);
    void setScanPath(const QString &path, int maxDepth = 3);
    void stop();
    
signals:
    void scanProgress(const QString &currentPath, int scanned);
    void scanFinished(const QList<ScanItem> &items);
    void itemFound(const ScanItem &item);
    
protected:
    void run() override;
    
private:
    QString m_scanPath;
    int m_maxDepth;
    bool m_stopped;
    QList<ScanItem> scanDirectory(const QString &path, int depth);
    qint64 getDirectorySize(const QString &path, int *fileCount, int *dirCount);
};

// 异步清理线程 —— 使用 rm -rf 替代 Qt API，速度提升 5-10 倍
class CleanupThread : public QThread
{
    Q_OBJECT
    
public:
    explicit CleanupThread(QObject *parent = nullptr);
    void setPathsToDelete(const QStringList &paths);
    
signals:
    void cleanupProgress(int current, int total, const QString &currentPath);
    void cleanupFinished(int successCount, int failCount, qint64 freedSize);
    
protected:
    void run() override;
    
private:
    QStringList m_paths;
};

class CleanupDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CleanupDialog(QWidget *parent = nullptr);
    ~CleanupDialog();
    
    QStringList getSelectedItems() const;
    qint64 getTotalSelectedSize() const;
    QList<PartitionInfo> getSelectedPartitions() const;
    
private slots:
    void onStartScanClicked();
    void onScanProgress(const QString &currentPath, int scanned);
    void onScanFinished(const QList<ScanItem> &items);
    void onItemFound(const ScanItem &item);
    void onStopScanClicked();
    void onRefreshPartitions();
    void onItemDoubleClicked(QTreeWidgetItem *item, int column);
    void onBackToPartitionsClicked();
    void onCleanupClicked();
    void onSelectAllPartitions();
    void onDeselectAllPartitions();
    void onGoUpClicked();           // 返回上一级
    void onGoHomeClicked();         // 返回初始扫描目录
    void onPathEditReturnPressed(); // 路径输入框回车
    void onSelectAllItems();        // 全选项目
    void onDeselectAllItems();      // 取消全选项目
    void onCleanupProgress(int current, int total, const QString &currentPath);  // 清理进度
    void onCleanupFinished(int successCount, int failCount, qint64 freedSize);   // 清理完成
    
private:
    void initUI();
    void applyTheme();
    bool isDarkTheme();
    void loadPartitions();
    void createPartitionPage();
    void createScanResultPage();
    void updatePartitionTable();
    void updateScanResults(const QList<ScanItem> &items);
    QString formatSize(qint64 bytes);
    QString formatNumber(int num);
    PartitionInfo parsePartitionLine(const QString &line);
    bool isSystemPartition(const QString &mountpoint);
    QIcon getFileIcon(const QString &path, bool isDir);
    
    // 文件浏览器功能
    void navigateToPath(const QString &path);
    void updatePathDisplay();
    void browseDirectory(const QString &path);
    void updateSelectedInfo();  // 更新选中信息
    
    // UI组件 - 分区选择页
    QWidget *m_partitionPage;
    QTableWidget *m_partitionTable;
    QLabel *m_partitionInfoLabel;
    QPushButton *m_refreshButton;
    QPushButton *m_scanButton;
    
    // UI组件 - 扫描结果页
    QWidget *m_scanResultPage;
    QTreeWidget *m_resultTree;
    QLabel *m_scanInfoLabel;
    QProgressBar *m_progressBar;
    QPushButton *m_stopScanButton;
    QPushButton *m_backButton;
    QPushButton *m_cleanupButton;
    QPushButton *m_selectAllBtn;        // 全选按钮
    QPushButton *m_deselectAllBtn;      // 取消全选按钮
    
    // 路径导航
    QLineEdit *m_pathEdit;           // 路径输入框
    QPushButton *m_goUpButton;       // 返回上一级
    QPushButton *m_goHomeButton;     // 返回初始目录
    QLabel *m_currentPathLabel;      // 当前路径显示
    
    // 主布局
    QStackedWidget *m_stackedWidget;
    
    // 数据
    QList<PartitionInfo> m_partitions;
    QList<ScanItem> m_scanResults;
    QMap<QString, qint64> m_itemSizes;
    PartitionScanThread *m_scanThread;
    CleanupThread *m_cleanupThread;   // 异步清理线程
    QString m_currentScanPath;       // 初始扫描路径
    QString m_currentBrowsePath;     // 当前浏览路径
    QStack<QString> m_navigationHistory; // 导航历史
    int m_scannedCount;
    bool m_scanning;
};

#endif // CLEANUPDIALOG_H
