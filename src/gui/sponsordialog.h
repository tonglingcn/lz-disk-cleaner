/*
 * Sponsor Dialog - Header
 * 赞助对话框 - 头文件
 * 
 * 提供赞助支持功能的对话框
 */

#ifndef SPONSORDIALOG_H
#define SPONSORDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>

class SponsorDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SponsorDialog(QWidget *parent = nullptr);
    ~SponsorDialog();

private:
    void initUI();
    void applyTheme();
    bool isDarkTheme();

private:
    QLabel *m_descLabel;
    QLabel *m_alipayLabel;
    QLabel *m_wechatLabel;
    QLabel *m_tipLabel;
    QPushButton *m_closeBtn;
};

#endif // SPONSORDIALOG_H
