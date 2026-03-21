/*
 * Progress Dialog - Header
 * 进度对话框 - 头文件
 */

#ifndef PROGRESSDIALOG_H
#define PROGRESSDIALOG_H

#include <QDialog>
#include <QProgressBar>
#include <QLabel>
#include <QTextEdit>

class ProgressDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ProgressDialog(QWidget *parent = nullptr);
    ~ProgressDialog();
    
    void updateProgress(const QString &itemName, int percent);
    void addLog(const QString &message);
    
private:
    void initUI();
    
    // UI 组件
    QLabel *m_statusLabel;
    QProgressBar *m_progressBar;
    QLabel *m_percentLabel;
    QTextEdit *m_logTextEdit;
};

#endif // PROGRESSDIALOG_H