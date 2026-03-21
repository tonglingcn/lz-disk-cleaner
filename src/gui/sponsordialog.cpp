/*
 * Sponsor Dialog - Implementation
 * 赞助对话框 - 实现文件
 * 
 * 提供赞助支持功能的对话框
 */

#include "sponsordialog.h"
#include <QPixmap>
#include <QApplication>
#include <QPalette>
#include <QFile>

SponsorDialog::SponsorDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("💝 赞助支持"));
    setFixedSize(480, 420);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    
    initUI();
    applyTheme();
}

SponsorDialog::~SponsorDialog()
{
}

void SponsorDialog::initUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(20, 15, 20, 15);

    // 描述
    m_descLabel = new QLabel(
        tr("<p>感谢您使用磁盘清理工具，如果条件允许可以赞助支持。</p>"
           "<p>您的支持是我持续改进的动力！❤️</p>"),
        this);
    m_descLabel->setAlignment(Qt::AlignCenter);
    m_descLabel->setWordWrap(true);
    m_descLabel->setStyleSheet("font-size: 13px;");
    mainLayout->addWidget(m_descLabel);

    // 二维码容器
    QHBoxLayout *qrLayout = new QHBoxLayout();
    qrLayout->setSpacing(30);

    // 支付宝
    QVBoxLayout *alipayLayout = new QVBoxLayout();
    alipayLayout->setSpacing(6);
    
    QLabel *alipayTitle = new QLabel(tr("支付宝"), this);
    alipayTitle->setAlignment(Qt::AlignCenter);
    alipayTitle->setStyleSheet("font-size: 14px; font-weight: bold; color: #1677FF;");
    alipayLayout->addWidget(alipayTitle);

    m_alipayLabel = new QLabel(this);
    m_alipayLabel->setFixedSize(150, 150);
    m_alipayLabel->setAlignment(Qt::AlignCenter);
    m_alipayLabel->setStyleSheet(
        "border: 2px solid #1677FF; "
        "border-radius: 8px; "
        "background-color: white;");
    
    // 加载支付宝二维码
    QPixmap alipayPixmap(":/sponsor/alipay.png");
    if (!alipayPixmap.isNull()) {
        m_alipayLabel->setPixmap(alipayPixmap.scaled(140, 140, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        m_alipayLabel->setText(tr("二维码\n未加载"));
        m_alipayLabel->setStyleSheet(
            "border: 2px dashed #1677FF; "
            "border-radius: 8px; "
            "background-color: #F0F8FF; "
            "color: #1677FF; "
            "font-size: 12px;");
    }
    alipayLayout->addWidget(m_alipayLabel, 0, Qt::AlignCenter);

    qrLayout->addLayout(alipayLayout);

    // 微信
    QVBoxLayout *wechatLayout = new QVBoxLayout();
    wechatLayout->setSpacing(6);
    
    QLabel *wechatTitle = new QLabel(tr("微信支付"), this);
    wechatTitle->setAlignment(Qt::AlignCenter);
    wechatTitle->setStyleSheet("font-size: 14px; font-weight: bold; color: #07C160;");
    wechatLayout->addWidget(wechatTitle);

    m_wechatLabel = new QLabel(this);
    m_wechatLabel->setFixedSize(150, 150);
    m_wechatLabel->setAlignment(Qt::AlignCenter);
    m_wechatLabel->setStyleSheet(
        "border: 2px solid #07C160; "
        "border-radius: 8px; "
        "background-color: white;");
    
    // 加载微信二维码
    QPixmap wechatPixmap(":/sponsor/wechat.png");
    if (!wechatPixmap.isNull()) {
        m_wechatLabel->setPixmap(wechatPixmap.scaled(140, 140, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        m_wechatLabel->setText(tr("二维码\n未加载"));
        m_wechatLabel->setStyleSheet(
            "border: 2px dashed #07C160; "
            "border-radius: 8px; "
            "background-color: #F0FFF0; "
            "color: #07C160; "
            "font-size: 12px;");
    }
    wechatLayout->addWidget(m_wechatLabel, 0, Qt::AlignCenter);

    qrLayout->addLayout(wechatLayout);
    mainLayout->addLayout(qrLayout);

    // 提示
    m_tipLabel = new QLabel(
        tr("💡 扫描二维码即可赞助，金额随意，心意最重要 ❤️"),
        this);
    m_tipLabel->setAlignment(Qt::AlignCenter);
    m_tipLabel->setWordWrap(true);
    m_tipLabel->setStyleSheet("font-size: 12px; color: #666;");
    mainLayout->addWidget(m_tipLabel);

    // 关闭按钮
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    
    m_closeBtn = new QPushButton(tr("关闭"), this);
    m_closeBtn->setFixedSize(90, 32);
    m_closeBtn->setStyleSheet(
        "QPushButton {"
        "   background-color: #4CAF50;"
        "   color: white;"
        "   border: none;"
        "   border-radius: 6px;"
        "   font-size: 14px;"
        "}"
        "QPushButton:hover {"
        "   background-color: #45A049;"
        "}"
        "QPushButton:pressed {"
        "   background-color: #3D8B40;"
        "}");
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnLayout->addWidget(m_closeBtn);
    btnLayout->addStretch();
    
    mainLayout->addLayout(btnLayout);
}

void SponsorDialog::applyTheme()
{
    bool darkMode = isDarkTheme();
    
    if (darkMode) {
        setStyleSheet(
            "QDialog { background-color: #2d2d2d; }"
            "QLabel { color: #e0e0e0; }");
        m_descLabel->setStyleSheet("font-size: 13px; color: #b0b0b0;");
        m_tipLabel->setStyleSheet("font-size: 12px; color: #888;");
        
        // 深色模式下二维码背景
        if (m_alipayLabel->pixmap().isNull()) {
            m_alipayLabel->setStyleSheet(
                "border: 2px dashed #1677FF; "
                "border-radius: 8px; "
                "background-color: #1a3a5c; "
                "color: #1677FF; "
                "font-size: 12px;");
        }
        if (m_wechatLabel->pixmap().isNull()) {
            m_wechatLabel->setStyleSheet(
                "border: 2px dashed #07C160; "
                "border-radius: 8px; "
                "background-color: #1a3a2a; "
                "color: #07C160; "
                "font-size: 12px;");
        }
    } else {
        setStyleSheet(
            "QDialog { background-color: #ffffff; }");
        m_descLabel->setStyleSheet("font-size: 13px; color: #555555;");
        m_tipLabel->setStyleSheet("font-size: 12px; color: #666666;");
    }
}

bool SponsorDialog::isDarkTheme()
{
    QPalette palette = qApp->palette();
    QColor windowColor = palette.color(QPalette::Window);
    int brightness = (windowColor.red() * 299 + windowColor.green() * 587 + windowColor.blue() * 114) / 1000;
    return brightness < 128;
}
