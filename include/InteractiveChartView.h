// ============================================================================
// ФАЙЛ INTERACTIVECHARTVIEW.H - ИНТЕРАКТИВНЫЙ VIEW ДЛЯ ГРАФИКОВ LAB_2
// ============================================================================

#pragma once

#include "PlotChartWidget.h"

#include <QPointer>

#include <QtCharts/QChartView>

class QGestureEvent;
class QMouseEvent;
class QNativeGestureEvent;
class QPainter;
class QPaintEvent;
class QWheelEvent;

QT_BEGIN_NAMESPACE
class QChart;
QT_END_NAMESPACE

class InteractiveChartView final : public QChartView
{
public:
    explicit InteractiveChartView(PlotChartWidget* owner, QChart* chart, QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;
    bool viewportEvent(QEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    struct HoverEntry
    {
        QString name;
        QString value;
        QColor color;
        QPointF markerViewPoint;
    };

    bool IsPointInsidePlot(const QPointF& viewportPoint) const;
    void DrawHoverOverlay(QPainter* painter);
    void DrawHoverTooltip(QPainter* painter, double xValue, const QVector<HoverEntry>& entries);
    void DrawHorizontalAxis(QPainter* painter, double xMin, double xMax);
    void DrawVerticalAxis(QPainter* painter, double yMin, double yMax);
    void DrawArrowHead(QPainter* painter, const QPointF& tip, const QPointF& from);
    void DrawAxisLabel(QPainter* painter, const QString& text, const QPointF& anchor, Qt::Alignment alignment);
    void DrawSupportBoundary(QPainter* painter, double xValue);
    bool HandleNativeGesture(QNativeGestureEvent* event);
    bool HandleGesture(QGestureEvent* event);

    QPointer<PlotChartWidget> owner_;
    QPointF hoverViewportPos_;
    bool hoverActive_ = false;
};
