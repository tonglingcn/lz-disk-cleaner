/*
 * History Chart - Implementation
 * 历史图表组件 - 实现文件
 * 
 * 使用 QPainter 自绘制，不依赖 Qt Charts
 */

#include "historychart.h"
#include "../utils/logger.h"

#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QBrush>
#include <QFont>
#include <QPaintEvent>
#include <QFontMetrics>

HistoryChart::HistoryChart(const QString &title, int seriesCount, QWidget *parent)
    : QWidget(parent)
    , m_title(title)
    , m_yTitle(tr("使用率 (%)"))
    , m_yMin(0)
    , m_yMax(100)
    , m_maxHistory(60)
    , m_leftMargin(60)
    , m_rightMargin(180)
    , m_topMargin(30)
    , m_bottomMargin(40)
{
    initColors();
    
    // 创建数据系列
    for (int i = 0; i < seriesCount; ++i) {
        DataSeries series;
        series.color = m_colors[i % m_colors.size()];
        series.currentValue = 0;
        m_seriesList.append(series);
    }
    
    setMinimumHeight(180);
    setStyleSheet("background-color: white; border: 1px solid #bdc3c7; border-radius: 6px;");
    
    LOG_DEBUG(QString("HistoryChart created: %1, series: %2").arg(title).arg(seriesCount));
}

HistoryChart::~HistoryChart()
{
    LOG_DEBUG(QString("HistoryChart destroyed: %1").arg(m_title));
}

void HistoryChart::initColors()
{
    // 预定义颜色调色板
    m_colors = {
        QColor("#2ecc71"),  // 绿色
        QColor("#e74c3c"),  // 红色
        QColor("#3498db"),  // 蓝色
        QColor("#f1c40f"),  // 黄色
        QColor("#e67e22"),  // 橙色
        QColor("#1abc9c"),  // 青绿
        QColor("#9b59b6"),  // 紫色
        QColor("#34495e"),  // 深灰蓝
        QColor("#d35400"),  // 深橙
        QColor("#c0392b"),  // 深红
        QColor("#8e44ad"),  // 深紫
        QColor("#16a085"),  // 深青
        QColor("#27ae60"),  // 深绿
        QColor("#2980b9"),  // 深蓝
        QColor("#f39c12"),  // 金橙
        QColor("#fd79a8"),  // 粉色
    };
}

void HistoryChart::updateSeries(int index, double value, const QString &name)
{
    if (index < 0 || index >= m_seriesList.size()) return;
    
    DataSeries &series = m_seriesList[index];
    
    // 将新值插入到开头
    series.values.prepend(value);
    series.currentValue = value;
    
    if (!name.isEmpty()) {
        series.name = name;
    }
    
    // 限制历史长度
    while (series.values.size() > m_maxHistory) {
        series.values.removeLast();
    }
    
    update();
}

void HistoryChart::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // 绘制背景
    painter.fillRect(rect(), Qt::white);
    
    // 绘制标题
    QFont titleFont = painter.font();
    titleFont.setBold(true);
    titleFont.setPointSize(11);
    painter.setFont(titleFont);
    painter.setPen(QColor("#2c3e50"));
    painter.drawText(QRect(10, 5, width() - 20, 25), Qt::AlignLeft | Qt::AlignVCenter, m_title);
    
    // 计算图表区域
    QRectF chartRect(m_leftMargin, m_topMargin, 
                     width() - m_leftMargin - m_rightMargin, 
                     height() - m_topMargin - m_bottomMargin);
    
    // 绘制网格
    drawGrid(painter, chartRect);
    
    // 绘制数据曲线
    drawSeries(painter, chartRect);
    
    // 绘制图例
    drawLegend(painter, chartRect);
}

void HistoryChart::drawGrid(QPainter &painter, const QRectF &chartRect)
{
    painter.setPen(QPen(QColor("#ecf0f1"), 1));
    
    // 绘制水平网格线
    int hLines = 5;
    for (int i = 0; i <= hLines; ++i) {
        double y = chartRect.top() + (chartRect.height() * i / hLines);
        painter.drawLine(QPointF(chartRect.left(), y), QPointF(chartRect.right(), y));
        
        // Y轴标签
        double value = m_yMax - (m_yMax - m_yMin) * i / hLines;
        QString label = QString::number((int)value);
        
        QFont font = painter.font();
        font.setPointSize(9);
        painter.setFont(font);
        painter.setPen(QColor("#7f8c8d"));
        painter.drawText(QRectF(5, y - 10, m_leftMargin - 10, 20), 
                        Qt::AlignRight | Qt::AlignVCenter, label);
    }
    
    // 绘制垂直网格线
    int vLines = 6;
    for (int i = 0; i <= vLines; ++i) {
        double x = chartRect.left() + (chartRect.width() * i / vLines);
        painter.setPen(QPen(QColor("#ecf0f1"), 1));
        painter.drawLine(QPointF(x, chartRect.top()), QPointF(x, chartRect.bottom()));
    }
    
    // 绘制边框
    painter.setPen(QPen(QColor("#bdc3c7"), 1));
    painter.drawRect(chartRect);
    
    // X轴标签
    QFont font = painter.font();
    font.setPointSize(9);
    painter.setFont(font);
    painter.setPen(QColor("#7f8c8d"));
    painter.drawText(QRectF(chartRect.left(), chartRect.bottom() + 5, 
                           chartRect.width(), 20), 
                    Qt::AlignCenter, tr("时间 (秒)"));
}

void HistoryChart::drawSeries(QPainter &painter, const QRectF &chartRect)
{
    for (const DataSeries &series : m_seriesList) {
        if (series.values.isEmpty()) continue;
        
        // 创建路径
        QPainterPath path;
        bool first = true;
        double lastX = 0;
        
        for (int i = 0; i < series.values.size(); ++i) {
            double px = chartRect.right() - (chartRect.width() * i / (m_maxHistory - 1));
            double normalizedValue = (series.values[i] - m_yMin) / (m_yMax - m_yMin);
            normalizedValue = qBound(0.0, normalizedValue, 1.0);
            double py = chartRect.bottom() - (chartRect.height() * normalizedValue);
            
            if (first) {
                path.moveTo(px, py);
                first = false;
            } else {
                path.lineTo(px, py);
            }
            lastX = px;
        }
        
        // 绘制曲线
        painter.setPen(QPen(series.color, 2));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(path);
        
        // 绘制填充区域（半透明）
        if (series.values.size() > 1) {
            QPainterPath fillPath = path;
            fillPath.lineTo(chartRect.right(), chartRect.bottom());
            fillPath.lineTo(lastX, chartRect.bottom());
            fillPath.closeSubpath();
            
            QColor fillColor = series.color;
            fillColor.setAlpha(30);
            painter.fillPath(fillPath, fillColor);
        }
    }
}

void HistoryChart::drawLegend(QPainter &painter, const QRectF &chartRect)
{
    QFont font = painter.font();
    font.setPointSize(9);
    painter.setFont(font);
    
    int legendY = m_topMargin;
    int legendX = chartRect.right() + 10;
    
    for (const DataSeries &series : m_seriesList) {
        // 绘制颜色方块
        painter.fillRect(legendX, legendY + 2, 12, 12, series.color);
        
        // 绘制名称
        painter.setPen(QColor("#2c3e50"));
        QString displayText = series.name;
        if (displayText.isEmpty()) {
            displayText = QString("%1").arg(series.currentValue, 0, 'f', 1);
        }
        
        // 截断过长的文本
        QFontMetrics fm(font);
        int maxWidth = m_rightMargin - 20;
        if (fm.horizontalAdvance(displayText) > maxWidth) {
            displayText = fm.elidedText(displayText, Qt::ElideRight, maxWidth);
        }
        
        painter.drawText(legendX + 16, legendY + 12, displayText);
        legendY += 18;
    }
}
