/*
 * APT Source Manager Widget - Header
 * APT 源管理组件 - 头文件
 */

#ifndef APTSOURCEMANAGERWIDGET_H
#define APTSOURCEMANAGERWIDGET_H

#include <QWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QCheckBox>
#include <QDialog>
#include <QTextEdit>

// APT 源信息结构
struct APTSourceInfo {
    QString filePath;       // 源文件路径
    bool isSource;          // 是否为源码源 (deb-src)
    QString options;        // 选项如 [arch=amd64]
    QString uri;            // URI 地址
    QString distribution;   // 发行版代号
    QString components;     // 组件
    QString fullLine;       // 完整源行
    bool isActive;          // 是否启用
};

// 单个 APT 源项控件
class APTSourceItem : public QWidget {
    Q_OBJECT

public:
    explicit APTSourceItem(const APTSourceInfo &info, QWidget *parent = nullptr);
    const APTSourceInfo& getInfo() const { return m_info; }

signals:
    void statusChanged();
    void editRequested();
    void deleteRequested();

private slots:
    void onCheckBoxToggled(bool checked);
    void onEditClicked();
    void onDeleteClicked();

private:
    void initUI();
    void applyTheme();

    APTSourceInfo m_info;
    QLabel *m_typeLabel;
    QLabel *m_uriLabel;
    QLabel *m_distLabel;
    QCheckBox *m_enabledCheck;
    QPushButton *m_editBtn;
    QPushButton *m_deleteBtn;
};

// 编辑 APT 源对话框
class APTSourceEditDialog : public QDialog {
    Q_OBJECT

public:
    explicit APTSourceEditDialog(const APTSourceInfo &info = APTSourceInfo(), QWidget *parent = nullptr);
    APTSourceInfo getSourceInfo() const;

signals:
    void saved();

private slots:
    void onSave();
    void validateInput();

private:
    void initUI();

    APTSourceInfo m_originalInfo;
    QLineEdit *m_uriEdit;
    QLineEdit *m_distEdit;
    QLineEdit *m_componentsEdit;
    QLineEdit *m_optionsEdit;
    QCheckBox *m_sourceCheck;
    QTextEdit *m_previewEdit;
    QPushButton *m_saveBtn;
    
    void updatePreview();
};

// APT 源管理主页面
class APTSourceManagerWidget : public QWidget {
    Q_OBJECT

public:
    explicit APTSourceManagerWidget(QWidget *parent = nullptr);
    ~APTSourceManagerWidget();

public slots:
    void refreshList();

private slots:
    void onAddClicked();
    void onEditClicked();
    void onDeleteClicked();
    void onSearchChanged(const QString &text);
    void onAppStatusChanged();
    void onItemSelectionChanged();

private:
    void initUI();
    void loadSources();
    void applyTheme();
    bool isDarkTheme();
    QList<APTSourceInfo> parseSourceFile(const QString &filePath);
    bool changeSourceStatus(const APTSourceInfo &info, bool enabled);
    bool deleteSource(const APTSourceInfo &info);
    bool addSource(const APTSourceInfo &info);
    bool modifySource(const APTSourceInfo &oldInfo, const APTSourceInfo &newInfo);

    // UI 组件
    QLabel *m_titleLabel;
    QLabel *m_countLabel;
    QLineEdit *m_searchEdit;
    QTableWidget *m_sourceTable;
    QPushButton *m_addBtn;
    QPushButton *m_editBtn;
    QPushButton *m_deleteBtn;
    QPushButton *m_refreshBtn;
    QLabel *m_emptyLabel;
    
    // 提示信息
    QLabel *m_tipLabel;

    // 数据
    QList<APTSourceInfo> m_sources;
    int m_selectedRow;
};

#endif // APTSOURCEMANAGERWIDGET_H
