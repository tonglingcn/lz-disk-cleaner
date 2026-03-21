/*
 * History Chart - Header
 * 历史图表组件 - 头文件
 * 
 * 使用 QPainter 自绘制，不依赖 Qt Charts
 */

#ifndef HISTORYCHART_H
#define HISTORYCHART_H

#include <QWidget>
#include <QVector>
#include <QString>
#include <QColor>
#include <QList>
#include <QPointF>

// 数据点结构
struct DataPoint {
    double value;
    QString label;
};

// 数据系列
struct DataSeries {
    QString name;
    QColor color;
    QVector<double> values;  // 历史值（索引0为最新）
    double currentValue;
};

class HistoryChart : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param title 图表标题
     * @param seriesCount 数据系列数量
     * @param parent 父组件
     */
    explicit HistoryChart(const QString &title, int seriesCount, QWidget *parent = nullptr);
    ~HistoryChart();

    // 设置标题
    void setTitle(const QString &title) { m_title = title; update(); }
    
    // 设置Y轴范围
    void setYRange(double min, double max) { m_yMin = min; m_yMax = max; update(); }
    
    // 设置Y轴标题
    void setYTitle(const QString &title) { m_yTitle = title; update(); }
    
    // 获取数据系列
    QList<DataSeries>& getSeriesList() { return m_seriesList; }
    
    // 更新系列数据
    void updateSeries(int index, double value, const QString &name = QString());
    
    // 设置历史长度
    void setMaxHistory(int seconds) { m_maxHistory = seconds; }
    
    // 获取标题
    QString getTitle() const { return m_title; }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void initColors();
    void drawGrid(QPainter &painter, const QRectF &chartRect);
    QVector<QPointF> drawSeries(QPainter &painter, const QRectF &chartRect);
    void drawLegend(QPainter &painter, const QRectF &chartRect, const QVector<QPointF> &endPoints);
    
    // 计算图例需要的行数
    int calculateLegendRows(int legendAreaWidth) const;
    
    // 检测深色主题
    bool isDarkTheme();

private:
    QString m_title;
    QString m_yTitle;
    
    QList<DataSeries> m_seriesList;
    QList<QColor> m_colors;
    
    double m_yMin;
    double m_yMax;
    int m_maxHistory;
    
    // 边距
    int m_leftMargin;
    int m_rightMargin;
    int m_topMargin;
    int m_bottomMargin;
};

#endif // HISTORYCHART_H
