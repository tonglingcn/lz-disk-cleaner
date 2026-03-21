/*
 * Temperature Widget - Implementation
 * 温度显示组件 - 实现文件
 */

#include "temperaturewidget.h"
#include "../utils/logger.h"

#include <QApplication>

// ============================================================================
// TempCard Implementation
// ============================================================================

TempCard::TempCard(const QString &name, QWidget *parent)
    : QFrame(parent)
    , m_name(name)
    , m_temperature(-1)
    , m_unit("°C")
    , m_icon("🌡️")
    , m_iconSvg(nullptr)
    , m_sharedTemp(false)
{
    setFrameShape(QFrame::StyledPanel);
    setFrameShadow(QFrame::Raised);
    setFixedSize(240, 140);
    initUI();
}

void TempCard::initUI()
{
    // 检测深色主题
    bool darkMode = isDarkTheme();
    QString cardBgColor = darkMode ? "#3d3d3d" : "white";
    QString borderColor = darkMode ? "#555555" : "#e0e0e0";
    QString nameColor = darkMode ? "#a0a0a0" : "#666";
    QString tempColor = darkMode ? "#e0e0e0" : "#333";
    QString statusColor = darkMode ? "#808080" : "#999";

    // 主布局
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(15, 12, 15, 12);
    mainLayout->setSpacing(15);

    // 左侧：名称、温度、状态（透明背景）
    QVBoxLayout *leftLayout = new QVBoxLayout();
    leftLayout->setSpacing(6);
    leftLayout->setAlignment(Qt::AlignVCenter);

    m_nameLabel = new QLabel(m_name, this);
    m_nameLabel->setStyleSheet(QString("font-size: 14px; color: %1; background: transparent; border: none;").arg(nameColor));
    leftLayout->addWidget(m_nameLabel);

    m_tempLabel = new QLabel("--", this);
    m_tempLabel->setStyleSheet(QString("font-size: 36px; font-weight: bold; color: %1; background: transparent; border: none;").arg(tempColor));
    leftLayout->addWidget(m_tempLabel);

    m_statusLabel = new QLabel("检测中...", this);
    m_statusLabel->setStyleSheet(QString("font-size: 12px; color: %1; background: transparent; border: none;").arg(statusColor));
    leftLayout->addWidget(m_statusLabel);

    mainLayout->addLayout(leftLayout);

    // 右侧：图标容器（更大）
    QWidget *iconContainer = new QWidget(this);
    iconContainer->setFixedSize(80, 80);
    QVBoxLayout *iconLayout = new QVBoxLayout(iconContainer);
    iconLayout->setContentsMargins(0, 0, 0, 0);
    iconLayout->setSpacing(0);
    iconLayout->setAlignment(Qt::AlignCenter);

    // 默认使用QLabel显示emoji
    m_iconLabel = new QLabel(m_icon, iconContainer);
    m_iconLabel->setAlignment(Qt::AlignCenter);
    m_iconLabel->setStyleSheet("font-size: 40px; background: transparent; border: none;");
    iconLayout->addWidget(m_iconLabel, 0, Qt::AlignCenter);

    mainLayout->addWidget(iconContainer, 0, Qt::AlignVCenter | Qt::AlignRight);

    // 设置卡片样式
    setStyleSheet(QString("TempCard { background-color: %1; border: 1px solid %2; border-radius: 10px; }").arg(cardBgColor, borderColor));
}

void TempCard::setTemperature(double temp)
{
    m_temperature = temp;
    updateDisplay();
}

void TempCard::updateDisplay()
{
    if (m_temperature < 0) {
        m_tempLabel->setText("--");
        m_tempLabel->setStyleSheet("font-size: 36px; font-weight: bold; color: #999; background: transparent; border: none;");
        m_statusLabel->setText("不可用");
        m_statusLabel->setStyleSheet("font-size: 12px; color: #999; background: transparent; border: none;");
        return;
    }
    
    // 处理集成显卡共享温度的情况 (m_sharedTemp 标志位表示)
    if (m_sharedTemp && m_name == "显卡") {
        QString tempText = QString::number(static_cast<int>(m_temperature));
        m_tempLabel->setText(tempText + m_unit);
        m_tempLabel->setStyleSheet("font-size: 36px; font-weight: bold; color: #3498db; background: transparent; border: none;");
        m_statusLabel->setText("✓ 与CPU共享温度");
        m_statusLabel->setStyleSheet("font-size: 12px; color: #3498db; background: transparent; border: none;");
        return;
    }

    // 温度数值
    QString tempText = QString::number(static_cast<int>(m_temperature));
    m_tempLabel->setText(tempText + m_unit);

    // 根据温度设置颜色
    QString color;
    QString bgColor;
    QString status;

    if (m_temperature < 50) {
        color = "#27ae60";      // 绿色
        bgColor = "#e8f5e9";    // 浅绿背景
        status = "✓ 温度正常";
    } else if (m_temperature < 70) {
        color = "#f39c12";      // 橙色
        bgColor = "#fff3e0";    // 浅橙背景
        status = "⚠ 温度偏高";
    } else {
        color = "#e74c3c";      // 红色
        bgColor = "#ffebee";    // 浅红背景
        status = "🔥 高温告警";
    }

    m_tempLabel->setStyleSheet(QString("font-size: 36px; font-weight: bold; color: %1; background: transparent; border: none;").arg(color));
    m_statusLabel->setText(status);
    m_statusLabel->setStyleSheet(QString("font-size: 12px; color: %1; background: transparent; border: none;").arg(color));
}

void TempCard::setUnit(const QString &unit)
{
    m_unit = unit;
    if (m_temperature >= 0) {
        updateDisplay();
    }
}

void TempCard::setIcon(const QString &icon)
{
    m_icon = icon;
    if (m_iconLabel) {
        m_iconLabel->setText(icon);
        m_iconLabel->setVisible(true);
    }
    if (m_iconSvg) {
        m_iconSvg->setVisible(false);
    }
}

void TempCard::setIconSvg(const QString &svgPath)
{
    m_svgPath = svgPath;

    // 隐藏emoji标签
    if (m_iconLabel) {
        m_iconLabel->setVisible(false);
    }

    // 创建或更新SVG控件
    if (!m_iconSvg) {
        // 找到图标容器（主布局的第2个item）
        QLayoutItem *item = layout()->itemAt(1);
        if (item) {
            QWidget *iconContainer = item->widget();
            if (iconContainer) {
                m_iconSvg = new QSvgWidget(svgPath, iconContainer);
                m_iconSvg->setFixedSize(56, 56);
                QVBoxLayout *iconLayout = qobject_cast<QVBoxLayout*>(iconContainer->layout());
                if (iconLayout) {
                    iconLayout->addWidget(m_iconSvg, 0, Qt::AlignCenter);
                }
            }
        }
    } else {
        m_iconSvg->load(svgPath);
        m_iconSvg->setFixedSize(56, 56);
        m_iconSvg->setVisible(true);
    }
}

void TempCard::setSharedTemp(bool shared)
{
    m_sharedTemp = shared;
    updateDisplay();
}

// ============================================================================
// TemperatureWidget Implementation
// ============================================================================

TemperatureWidget::TemperatureWidget(QWidget *parent)
    : QWidget(parent)
    , m_cpuCard(nullptr)
    , m_boardCard(nullptr)
    , m_gpuCard(nullptr)
    , m_diskCard(nullptr)
{
    initUI();
}

TemperatureWidget::~TemperatureWidget()
{
}

void TemperatureWidget::initUI()
{
    // 检测深色主题
    bool darkMode = isDarkTheme();
    QString titleColor = darkMode ? "#e0e0e0" : "#333";
    QString hintColor = darkMode ? "#808080" : "#999";
    QString containerBgColor = darkMode ? "#2d2d2d" : "#f5f5f5";

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(15);

    // 标题行
    QHBoxLayout *titleLayout = new QHBoxLayout();

    QLabel *titleLabel = new QLabel("🌡️ 硬件温度监控", this);
    titleLabel->setStyleSheet(QString("font-size: 16px; font-weight: bold; color: %1;").arg(titleColor));
    titleLayout->addWidget(titleLabel);

    QLabel *hintLabel = new QLabel("实时监测", this);
    hintLabel->setStyleSheet(QString("font-size: 11px; color: %1;").arg(hintColor));
    titleLayout->addWidget(hintLabel);
    titleLayout->addStretch();

    mainLayout->addLayout(titleLayout);

    // 卡片容器
    QFrame *container = new QFrame(this);
    container->setFrameShape(QFrame::NoFrame);
    container->setStyleSheet(QString("QFrame { background-color: %1; border-radius: 12px; }").arg(containerBgColor));

    QHBoxLayout *cardsLayout = new QHBoxLayout(container);
    cardsLayout->setSpacing(20);
    cardsLayout->setContentsMargins(15, 15, 15, 15);
    cardsLayout->setAlignment(Qt::AlignCenter);

    // SVG图标路径
    QString iconPath = ":/icons/";

    m_cpuCard = new TempCard("CPU", container);
    m_cpuCard->setIconSvg(iconPath + "cpu.svg");
    cardsLayout->addWidget(m_cpuCard, 0, Qt::AlignCenter);

    m_boardCard = new TempCard("主板", container);
    m_boardCard->setIconSvg(iconPath + "motherboard.svg");
    cardsLayout->addWidget(m_boardCard, 0, Qt::AlignCenter);

    m_gpuCard = new TempCard("显卡", container);
    m_gpuCard->setIconSvg(iconPath + "gpu.svg");
    cardsLayout->addWidget(m_gpuCard, 0, Qt::AlignCenter);

    m_diskCard = new TempCard("硬盘", container);
    m_diskCard->setIconSvg(iconPath + "hdd.svg");
    cardsLayout->addWidget(m_diskCard, 0, Qt::AlignCenter);

    mainLayout->addWidget(container);
}

void TemperatureWidget::updateTemperatures(const HardwareTemps &temps)
{
    LOG_INFO(QString("[TempWidget] CPU: %1°C, GPUs: %2, Disks: %3")
        .arg(temps.cpu.isValid ? temps.cpu.package : -1)
        .arg(temps.gpus.size())
        .arg(temps.disks.size()));

    // 更新 CPU
    if (temps.cpu.isValid) {
        m_cpuCard->setTemperature(temps.cpu.package);
    } else {
        m_cpuCard->setTemperature(-1);
    }

    // 更新主板
    if (temps.board.isValid) {
        m_boardCard->setTemperature(temps.board.temperature);
    } else {
        m_boardCard->setTemperature(-1);
    }

    // 更新显卡
    if (!temps.gpus.isEmpty()) {
        const GpuTemperature &gpu = temps.gpus.first();
        // 集成显卡：isIntegrated=true，显示共享温度（实际CPU温度值）
        // 独立显卡：正常显示温度
        if (gpu.isIntegrated) {
            m_gpuCard->setSharedTemp(true);
            m_gpuCard->setTemperature(gpu.temperature);  // 显示CPU温度值
        } else if (gpu.isValid) {
            m_gpuCard->setSharedTemp(false);
            m_gpuCard->setTemperature(gpu.temperature);
        } else {
            m_gpuCard->setSharedTemp(false);
            m_gpuCard->setTemperature(-1);
        }
    } else {
        m_gpuCard->setSharedTemp(false);
        m_gpuCard->setTemperature(-1);
    }

    // 更新硬盘
    if (!temps.disks.isEmpty()) {
        const DiskTemperature &disk = temps.disks.first();
        m_diskCard->setTemperature(disk.temperature);
    } else {
        m_diskCard->setTemperature(-1);
    }
}

bool TempCard::isDarkTheme()
{
    QPalette palette = qApp->palette();
    QColor windowColor = palette.color(QPalette::Window);
    int brightness = (windowColor.red() * 299 + windowColor.green() * 587 + windowColor.blue() * 114) / 1000;
    return brightness < 128;
}

bool TemperatureWidget::isDarkTheme()
{
    QPalette palette = qApp->palette();
    QColor windowColor = palette.color(QPalette::Window);
    int brightness = (windowColor.red() * 299 + windowColor.green() * 587 + windowColor.blue() * 114) / 1000;
    return brightness < 128;
}
