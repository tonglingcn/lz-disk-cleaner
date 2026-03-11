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
    , m_rightMargin(20)
    , m_topMargin(30)
    , m_bottomMargin(60)  // 基础底部边距
{
    initColors();
    
    // 创建数据系列
    for (int i = 0; i < seriesCount; ++i) {
        DataSeries series;
        series.color = m_colors[i % m_colors.size()];
        series.currentValue = 0;
        m_seriesList.append(series);
    }
    
    // 根据系列数量动态计算底部边距和最小高度
    // 估算每行大约能放的项目数（基于平均项目宽度约100px）
    int estimatedItemsPerRow = qMax(4, seriesCount <= 8 ? seriesCount : 8);
    int estimatedRows = (seriesCount + estimatedItemsPerRow - 1) / estimatedItemsPerRow;
    
    // 每行图例约18px高度，加上额外的padding
    int legendHeight = estimatedRows * 18 + 15;
    m_bottomMargin = qMax(60, legendHeight);
    
    // 基础高度 + 图例区域高度
    int minH = m_topMargin + 130 + m_bottomMargin;
    setMinimumHeight(minH);
    
    setStyleSheet("background-color: white; border: 1px solid #bdc3c7; border-radius: 6px;");
    
    LOG_DEBUG(QString("HistoryChart created: %1, series: %2, estimatedRows: %3, minHeight: %4")
        .arg(title).arg(seriesCount).arg(estimatedRows).arg(minH));
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
    
    // 动态计算图例需要的行数，确保有足够空间
    int legendAreaWidth = width() - m_leftMargin - m_rightMargin;
    int neededRows = calculateLegendRows(legendAreaWidth);
    int neededBottomMargin = neededRows * 18 + 20;
    
    // 如果当前高度不够，动态调整最小高度
    if (neededBottomMargin > m_bottomMargin) {
        m_bottomMargin = neededBottomMargin;
        int minH = m_topMargin + 130 + m_bottomMargin;
        if (height() < minH) {
            setMinimumHeight(minH);
        }
    }
    
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
    
    // 计算图表区域（右侧边距减小，底部留出空间给图例）
    QRectF chartRect(m_leftMargin, m_topMargin, 
                     width() - m_leftMargin - m_rightMargin, 
                     height() - m_topMargin - m_bottomMargin);
    
    // 绘制网格
    drawGrid(painter, chartRect);
    
    // 绘制数据曲线并返回曲线末端位置
    QVector<QPointF> endPoints = drawSeries(painter, chartRect);
    
    // 在底部绘制图例（曲线末端下方）
    drawLegend(painter, chartRect, endPoints);
}

int HistoryChart::calculateLegendRows(int legendAreaWidth) const
{
    if (m_seriesList.isEmpty()) return 0;
    
    QFont font;
    font.setPointSize(9);
    QFontMetrics fm(font);
    
    int textOffset = 10 + 5;  // colorBoxSize + spacing
    int totalWidth = 0;
    
    for (const DataSeries &series : m_seriesList) {
        QString text = series.name.isEmpty() ? 
            QString("%1").arg(series.currentValue, 0, 'f', 1) : series.name;
        int w = fm.horizontalAdvance(text) + textOffset + 15;
        totalWidth += w;
    }
    
    if (totalWidth <= legendAreaWidth) {
        return 1;
    }
    
    // 计算需要的行数，尽量均匀分布
    int totalItems = m_seriesList.size();
    
    // 先计算最少需要的行数
    int minRows = 1;
    int testWidth = 0;
    int itemsInCurrentRow = 0;
    QVector<int> itemWidths;
    
    for (const DataSeries &series : m_seriesList) {
        QString text = series.name.isEmpty() ? 
            QString("%1").arg(series.currentValue, 0, 'f', 1) : series.name;
        itemWidths.append(fm.horizontalAdvance(text) + textOffset + 15);
    }
    
    for (int i = 0; i < totalItems; ++i) {
        if (testWidth + itemWidths[i] > legendAreaWidth && itemsInCurrentRow > 0) {
            minRows++;
            testWidth = itemWidths[i];
            itemsInCurrentRow = 1;
        } else {
            testWidth += itemWidths[i];
            itemsInCurrentRow++;
        }
    }
    
    return minRows;
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
                           chartRect.width(), 15), 
                    Qt::AlignCenter, tr("时间 (秒)"));
}

QVector<QPointF> HistoryChart::drawSeries(QPainter &painter, const QRectF &chartRect)
{
    QVector<QPointF> endPoints;
    
    for (const DataSeries &series : m_seriesList) {
        if (series.values.isEmpty()) {
            endPoints.append(QPointF(chartRect.right(), chartRect.bottom()));
            continue;
        }
        
        // 创建路径
        QPainterPath path;
        bool first = true;
        double lastX = chartRect.right();
        double lastY = chartRect.bottom();
        
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
            lastY = py;
        }
        
        // 记录曲线末端位置（最新数据点）
        endPoints.append(QPointF(lastX, lastY));
        
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
        
        // 在曲线末端绘制小圆点
        painter.setBrush(series.color);
        painter.setPen(QPen(Qt::white, 1));
        painter.drawEllipse(endPoints.last(), 4, 4);
    }
    
    return endPoints;
}

void HistoryChart::drawLegend(QPainter &painter, const QRectF &chartRect, const QVector<QPointF> &endPoints)
{
    if (m_seriesList.isEmpty()) return;
    
    QFont font = painter.font();
    font.setPointSize(9);
    painter.setFont(font);
    QFontMetrics fm(font);
    
    int legendAreaTop = chartRect.bottom() + 18;
    int legendAreaLeft = m_leftMargin;
    int legendAreaWidth = width() - m_leftMargin - m_rightMargin;
    
    int colorBoxSize = 10;
    int textOffset = colorBoxSize + 5;
    int legendItemHeight = 18;
    
    int totalItems = m_seriesList.size();
    
    // 计算每个图例项的实际宽度
    QVector<int> itemWidths;
    int totalWidth = 0;
    for (const DataSeries &series : m_seriesList) {
        QString text = series.name.isEmpty() ? 
            QString("%1").arg(series.currentValue, 0, 'f', 1) : series.name;
        int w = fm.horizontalAdvance(text) + textOffset + 15;  // 颜色块 + 文本 + 间距
        itemWidths.append(w);
        totalWidth += w;
    }
    
    // 改进的布局算法：尽量均匀分布
    int rows = 1;
    int itemsPerRow = totalItems;
    
    if (totalWidth > legendAreaWidth) {
        // 计算最少需要的行数
        int minRows = 1;
        int testWidth = 0;
        int itemsInCurrentRow = 0;
        
        for (int i = 0; i < totalItems; ++i) {
            if (testWidth + itemWidths[i] > legendAreaWidth && itemsInCurrentRow > 0) {
                // 当前行放不下，换行
                minRows++;
                testWidth = itemWidths[i];
                itemsInCurrentRow = 1;
            } else {
                testWidth += itemWidths[i];
                itemsInCurrentRow++;
            }
        }
        
        rows = minRows;
        
        // 计算均匀分布时每行的项目数
        // 尽量让每行项目数相等或相差不超过1
        itemsPerRow = (totalItems + rows - 1) / rows;  // 向上取整
        
        // 验证这个分布是否可行（每行宽度是否超限）
        bool valid = true;
        int rowWidth = 0;
        int itemCount = 0;
        for (int i = 0; i < totalItems; ++i) {
            rowWidth += itemWidths[i];
            itemCount++;
            if (itemCount >= itemsPerRow || i == totalItems - 1) {
                if (rowWidth > legendAreaWidth) {
                    valid = false;
                    break;
                }
                rowWidth = 0;
                itemCount = 0;
            }
        }
        
        // 如果不可行，增加行数重试
        while (!valid && rows < totalItems) {
            rows++;
            itemsPerRow = (totalItems + rows - 1) / rows;
            valid = true;
            rowWidth = 0;
            itemCount = 0;
            for (int i = 0; i < totalItems; ++i) {
                rowWidth += itemWidths[i];
                itemCount++;
                if (itemCount >= itemsPerRow || i == totalItems - 1) {
                    if (rowWidth > legendAreaWidth) {
                        valid = false;
                        break;
                    }
                    rowWidth = 0;
                    itemCount = 0;
                }
            }
        }
    }
    
    // 绘制图例
    int itemIndex = 0;
    for (int row = 0; row < rows && itemIndex < totalItems; ++row) {
        // 计算当前行的项目范围
        int startIdx = row * itemsPerRow;
        int endIdx = qMin(startIdx + itemsPerRow, totalItems);
        int itemCountInRow = endIdx - startIdx;
        
        // 计算当前行所有项的总宽度
        int rowWidth = 0;
        for (int i = startIdx; i < endIdx; ++i) {
            rowWidth += itemWidths[i];
        }
        
        // 计算间距（居中两端对齐）
        int spacing = 0;
        int startX = legendAreaLeft;
        if (itemCountInRow > 1) {
            spacing = (legendAreaWidth - rowWidth) / (itemCountInRow - 1);
            // 确保间距不为负
            spacing = qMax(0, spacing);
        } else {
            // 单项居中
            startX = legendAreaLeft + (legendAreaWidth - rowWidth) / 2;
        }
        
        int currentX = startX;
        int itemY = legendAreaTop + row * legendItemHeight;
        
        for (int i = startIdx; i < endIdx; ++i) {
            const DataSeries &series = m_seriesList[i];
            
            // 绘制颜色方块
            painter.fillRect(currentX, itemY + 2, colorBoxSize, colorBoxSize, series.color);
            
            // 绘制名称（不截断）
            painter.setPen(QColor("#2c3e50"));
            QString displayText = series.name;
            if (displayText.isEmpty()) {
                displayText = QString("%1").arg(series.currentValue, 0, 'f', 1);
            }
            
            painter.drawText(currentX + textOffset, itemY + 12, displayText);
            
            currentX += itemWidths[i] + spacing;
            itemIndex++;
        }
    }
}
