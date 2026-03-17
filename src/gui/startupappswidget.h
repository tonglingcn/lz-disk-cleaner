/*
 * Startup Apps Widget - Header
 * 自启动管理组件 - 头文件
 */

#ifndef STARTUPAPPSWIDGET_H
#define STARTUPAPPSWIDGET_H

#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QCheckBox>
#include <QFileSystemWatcher>
#include <QDialog>

// 自启动应用信息结构
struct StartupAppInfo {
    QString name;           // 应用名称
    QString filePath;       // .desktop 文件路径
    QString exec;           // 执行命令
    QString icon;           // 图标名称
    bool enabled;           // 是否启用
    bool isSystem;          // 是否系统应用
};

// 单个自启动应用项控件
class StartupAppItem : public QWidget {
    Q_OBJECT

public:
    explicit StartupAppItem(const StartupAppInfo &info, QWidget *parent = nullptr);
    QString getName() const { return m_info.name; }
    QString getFilePath() const { return m_info.filePath; }
    bool isEnabled() const { return m_info.enabled; }

signals:
    void statusChanged();
    void deleteRequested();
    void editRequested();

private slots:
    void onCheckBoxToggled(bool checked);
    void onDeleteClicked();
    void onEditClicked();

private:
    void initUI();
    void applyTheme();

    StartupAppInfo m_info;
    QLabel *m_iconLabel;
    QLabel *m_nameLabel;
    QLabel *m_execLabel;
    QCheckBox *m_enabledCheck;
    QPushButton *m_editBtn;
    QPushButton *m_deleteBtn;
};

// 编辑/添加自启动应用对话框
class StartupAppEditDialog : public QDialog {
    Q_OBJECT

public:
    explicit StartupAppEditDialog(const QString &filePath = QString(), QWidget *parent = nullptr);
    StartupAppInfo getAppInfo() const;

signals:
    void saved();

private slots:
    void onBrowseExec();
    void onSave();
    void validateInput();

private:
    void initUI();
    void loadFromFile(const QString &filePath);

    QString m_filePath;
    QLineEdit *m_nameEdit;
    QLineEdit *m_execEdit;
    QLineEdit *m_commentEdit;
    QPushButton *m_saveBtn;
};

// 自启动管理主页面
class StartupAppsWidget : public QWidget {
    Q_OBJECT

public:
    explicit StartupAppsWidget(QWidget *parent = nullptr);
    ~StartupAppsWidget();

public slots:
    void refreshList();

private slots:
    void onAddClicked();
    void onDirectoryChanged();
    void onAppStatusChanged();

private:
    void initUI();
    void loadApps();
    void applyTheme();
    bool isDarkTheme();
    QString getDesktopValue(const QString &key, const QStringList &lines);
    void checkIfDisabled();

    // UI 组件
    QLabel *m_titleLabel;
    QLabel *m_countLabel;
    QLineEdit *m_searchEdit;
    QListWidget *m_appList;
    QPushButton *m_addBtn;
    QPushButton *m_refreshBtn;
    QLabel *m_emptyLabel;

    // 数据
    QString m_autostartPath;
    QFileSystemWatcher m_watcher;
    QList<StartupAppItem*> m_items;
};

#endif // STARTUPAPPSWIDGET_H
