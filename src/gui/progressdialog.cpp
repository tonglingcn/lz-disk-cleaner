/*
 * Progress Dialog - Implementation
 * 进度对话框 - 实现
 */

#include "progressdialog.h"
#include "../utils/logger.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QDateTime>

ProgressDialog::ProgressDialog(QWidget *parent)
    : QDialog(parent)
    , m_statusLabel(nullptr)
    , m_progressBar(nullptr)
    , m_percentLabel(nullptr)
    , m_logTextEdit(nullptr)
{
    LOG_INFO("Initializing progress dialog");
    
    setWindowTitle(tr("清理进度"));
    setMinimumSize(600, 400);
    resize(700, 500);
    
    initUI();
}

ProgressDialog::~ProgressDialog()
{
}

void ProgressDialog::initUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    
    // 标题
    QLabel *titleLabel = new QLabel(tr("磁盘清理进行中..."), this);
    titleLabel->setStyleSheet("font-size: 16px; font-weight: bold;");
    mainLayout->addWidget(titleLabel);
    
    // 状态标签
    m_statusLabel = new QLabel(tr("准备中..."), this);
    m_statusLabel->setStyleSheet("font-size: 14px; color: #666;");
    mainLayout->addWidget(m_statusLabel);
    
    // 进度条
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setMinimumHeight(25);
    m_progressBar->setStyleSheet(
        "QProgressBar { "
        "   border: 2px solid grey; "
        "   border-radius: 5px; "
        "   text-align: center; "
        "} "
        "QProgressBar::chunk { "
        "   background-color: #4CAF50; "
        "   width: 20px; "
        "}"
    );
    mainLayout->addWidget(m_progressBar);
    
    // 百分比标签
    QHBoxLayout *percentLayout = new QHBoxLayout();
    percentLayout->addStretch();
    m_percentLabel = new QLabel(tr("0%"), this);
    m_percentLabel->setStyleSheet("font-size: 24px; font-weight: bold; color: #4CAF50;");
    percentLayout->addWidget(m_percentLabel);
    percentLayout->addStretch();
    mainLayout->addLayout(percentLayout);
    
    // 日志区域
    QLabel *logLabel = new QLabel(tr("清理日志:"), this);
    logLabel->setStyleSheet("font-weight: bold;");
    mainLayout->addWidget(logLabel);
    
    m_logTextEdit = new QTextEdit(this);
    m_logTextEdit->setReadOnly(true);
    m_logTextEdit->setMinimumHeight(200);
    m_logTextEdit->setStyleSheet(
        "QTextEdit { "
        "   background-color: #f5f5f5; "
        "   border: 1px solid #ddd; "
        "   border-radius: 5px; "
        "   padding: 10px; "
        "   font-family: monospace; "
        "   font-size: 12px; "
        "}"
    );
    mainLayout->addWidget(m_logTextEdit, 1);
    
    // 提示信息
    QLabel *tipLabel = new QLabel(
        tr("提示: 清理过程中请勿关闭此窗口，否则可能导致数据损坏。"),
        this
    );
    tipLabel->setStyleSheet("color: #FF9800; font-style: italic;");
    tipLabel->setWordWrap(true);
    mainLayout->addWidget(tipLabel);
    
    // 取消按钮
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    
    QPushButton *cancelButton = new QPushButton(tr("取消"), this);
    cancelButton->setStyleSheet(
        "QPushButton { "
        "   background-color: #f44336; "
        "   color: white; "
        "   border-radius: 5px; "
        "   padding: 8px 16px; "
        "} "
        "QPushButton:hover { "
        "   background-color: #d32f2f; "
        "}"
    );
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    
    buttonLayout->addWidget(cancelButton);
    mainLayout->addLayout(buttonLayout);
}

void ProgressDialog::updateProgress(const QString &itemName, int percent)
{
    LOG_INFO(QString("Progress update: %1 - %2%").arg(itemName).arg(percent));
    
    // 更新状态标签
    m_statusLabel->setText(tr("正在清理: %1").arg(itemName));
    
    // 更新进度条
    m_progressBar->setValue(percent);
    
    // 更新百分比标签
    m_percentLabel->setText(QString("%1%").arg(percent));
    
    // 添加日志
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString logMessage = QString("[%1] %2 - %3%\n")
        .arg(timestamp)
        .arg(itemName)
        .arg(percent);
    
    m_logTextEdit->append(logMessage);
    
    // 自动滚动到底部
    QTextCursor cursor = m_logTextEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_logTextEdit->setTextCursor(cursor);
}

void ProgressDialog::addLog(const QString &message)
{
    LOG_INFO(QString("Add log: %1").arg(message));
    
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString logMessage = QString("[%1] %2\n").arg(timestamp).arg(message);
    
    m_logTextEdit->append(logMessage);
    
    // 自动滚动到底部
    QTextCursor cursor = m_logTextEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_logTextEdit->setTextCursor(cursor);
}