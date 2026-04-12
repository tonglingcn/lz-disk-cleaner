/*
 * Settings Dialog - Header
 * 设置对话框 - 头文件
 */

#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QTabWidget>
#include <QListWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QLabel>
#include <QGroupBox>
#include <QFileDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    ~SettingsDialog();

private slots:
    void onAccepted();
    void onRejected();
    void onResetDefaults();
    
    // 白名单操作
    void onAddKeepEmptyDir();
    void onRemoveKeepEmptyDir();
    void onAddFullProtect();
    void onRemoveFullProtect();
    void onAddFilePattern();
    void onRemoveFilePattern();
    
    // 导入导出
    void onExportConfig();
    void onImportConfig();

private:
    void initUI();
    void loadSettings();
    void saveSettings();
    void createGeneralTab(QVBoxLayout *layout);
    void createKeepEmptyDirTab(QHBoxLayout *layout);
    void createFullProtectTab(QHBoxLayout *layout);
    void createFilePatternTab(QHBoxLayout *layout);
    
    // 白名单列表
    QListWidget *m_keepEmptyDirList;
    QListWidget *m_fullProtectList;
    QListWidget *m_filePatternList;
    QListWidget *m_systemKeepEmptyDirList;
    QListWidget *m_systemFullProtectList;
    QListWidget *m_systemFilePatternList;
    
    // 通用设置
    QSpinBox *m_journalKeepDaysSpin;
    QSpinBox *m_journalMaxSizeSpin;
    QSpinBox *m_snapshotKeepCountSpin;
    QCheckBox *m_confirmBeforeCleanupCheck;
    QButtonGroup *m_closeActionGroup;
    QRadioButton *m_closeQuitRadio;
    QRadioButton *m_closeMinimizeRadio;

    
    // 按钮
    QPushButton *m_addKeepEmptyDirBtn;
    QPushButton *m_removeKeepEmptyDirBtn;
    QPushButton *m_addFullProtectBtn;
    QPushButton *m_removeFullProtectBtn;
    QPushButton *m_addFilePatternBtn;
    QPushButton *m_removeFilePatternBtn;
};

#endif // SETTINGSDIALOG_H
