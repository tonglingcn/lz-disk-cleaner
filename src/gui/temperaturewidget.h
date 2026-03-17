/*
 * Temperature Widget - Header
 * 温度显示组件 - 头文件
 */

#ifndef TEMPERATUREWIDGET_H
#define TEMPERATUREWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QProgressBar>
#include <QtSvgWidgets/QSvgWidget>
#include "../core/hardwaremonitor.h"

// 单个温度卡片 - 新设计
class TempCard : public QFrame
{
    Q_OBJECT

public:
    explicit TempCard(const QString &name, QWidget *parent = nullptr);
    
    void setTemperature(double temp);
    void setUnit(const QString &unit);
    void setIcon(const QString &icon);
    void setIconSvg(const QString &svgPath);
    void setSharedTemp(bool shared);  // 设置是否为共享温度（集成显卡）

private:
    void initUI();
    void updateDisplay();
    QString getStatusText(double temp);
    QString getStatusIcon(double temp);

    QString m_name;
    double m_temperature;
    QString m_unit;
    QString m_icon;
    QString m_svgPath;
    bool m_sharedTemp;  // 是否为共享温度（集成显卡）

    QLabel *m_nameLabel;
    QLabel *m_tempLabel;
    QLabel *m_iconLabel;
    QSvgWidget *m_iconSvg;
    QLabel *m_statusLabel;
};

// 硬件温度监控区域
class TemperatureWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TemperatureWidget(QWidget *parent = nullptr);
    ~TemperatureWidget();
    
    void updateTemperatures(const HardwareTemps &temps);

private:
    void initUI();
    QString getGpuIcon(bool isNvidia);
    QString getDiskIcon(bool isSSD, bool isNVMe);

private:
    TempCard *m_cpuCard;
    TempCard *m_boardCard;
    TempCard *m_gpuCard;
    TempCard *m_diskCard;
};

#endif // TEMPERATUREWIDGET_H
