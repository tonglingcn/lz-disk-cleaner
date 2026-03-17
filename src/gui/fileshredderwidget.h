/*
 * File Shredder Widget - Header
 * 文件粉碎组件 - 头文件
 * 
 * 支持顽固文件粉碎：
 * - 权限提升选项
 * - 进程占用检测
 */

#ifndef FILESHREDDERWIDGET_H
#define FILESHREDDERWIDGET_H

#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QThread>
#include <QCheckBox>
#include <QFrame>
#include "../core/fileshredder.h"

class ShredWorker : public QObject
{
    Q_OBJECT

public:
    explicit ShredWorker(const QStringList &files, int passes, bool usePrivilege, QObject *parent = nullptr);

public slots:
    void doWork();

signals:
    void progress(const QString &file, int current, int total, int percent);
    void finished(const QList<ShredResult> &results);

private:
    QStringList m_files;
    int m_passes;
    bool m_usePrivilege;
};

class FileShredderWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FileShredderWidget(QWidget *parent = nullptr);
    ~FileShredderWidget();

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;

private slots:
    void onAddFilesClicked();
    void onAddFolderClicked();
    void onClearListClicked();
    void onRemoveSelectedClicked();
    void onShredClicked();
    void onShredProgress(const QString &file, int current, int total, int percent);
    void onShredFinished(const QList<ShredResult> &results);

private:
    void initUI();
    void addFilesToList(const QStringList &files);
    void updateTotalSize();
    QString formatSize(qint64 bytes) const;
    bool isDarkTheme();
    void applyTheme();

    // UI 组件
    QLabel *m_titleLabel;
    QLabel *m_dropAreaLabel;
    QListWidget *m_fileList;
    QPushButton *m_addFilesBtn;
    QPushButton *m_addFolderBtn;
    QPushButton *m_removeSelectedBtn;
    QPushButton *m_clearListBtn;
    QPushButton *m_shredBtn;
    QProgressBar *m_progressBar;
    QLabel *m_totalSizeLabel;
    QCheckBox *m_privilegeCheckBox;  // 权限提升选项
    QFrame *m_warningFrame;          // 警告框
    QLabel *m_warningLabel;          // 警告文字

    // 数据
    FileShredder *m_shredder;
    QThread *m_workerThread;
    qint64 m_totalSize;
    bool m_isProcessing;
};

#endif // FILESHREDDERWIDGET_H
