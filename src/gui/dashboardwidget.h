/*
 * Dashboard Widget - Header
 * 仪表盘组件 - 头文件
 */

#ifndef DASHBOARDWIDGET_H
#define DASHBOARDWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>
#include <QPainter>
#include <QProcess>
#include "../core/diskanalyzer.h"
#include "../core/systeminfo.h"

// 圆形进度指示器组件
class CircularProgressWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CircularProgressWidget(const QString &title, const QColor &color, QWidget *parent = nullptr);
    
    void setValue(double value);
    void setText(const QString &text);
    void setMaxValue(double max) { m_maxValue = max; }
    void setDarkMode(bool dark);
    
protected:
    void paintEvent(QPaintEvent *event) override;
    
private:
    QString m_title;
    QColor m_color;
    double m_value;
    double m_maxValue;
    QString m_text;
    bool m_darkMode;
};

class DashboardWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DashboardWidget(QWidget *parent = nullptr);
    ~DashboardWidget();
    
    void updateDiskUsage(const QList<DiskUsage> &disks);
    void updateImmutableSystem(const ImmutableSystemInfo &info);
    void updateLinglongApps(const QList<LinglongAppInfo> &apps);
    
private slots:
    void updateSystemStats();
    
private:
    void initUI();
    void createTopSection();
    void createBottomSection();
    QWidget* createInfoCard(const QString &title, const QString &value, const QString &icon, const QColor &iconColor);
    QString getUptime();
    QString getGPUInfo();
    QString getDisplayInfo();
    
    QString formatSize(qint64 bytes);
    double getCpuUsage();
    
    // 主题检测和应用
    bool isDarkTheme();
    void applyTheme();
    
    // UI 组件
    CircularProgressWidget *m_cpuWidget;
    CircularProgressWidget *m_memoryWidget;
    CircularProgressWidget *m_diskWidget;
    
    // 数据
    SystemInfo *m_systemInfo;
    QTimer *m_updateTimer;
    QList<DiskUsage> m_diskUsage;
};

#endif // DASHBOARDWIDGET_H