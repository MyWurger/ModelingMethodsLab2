// ============================================================================
// ФАЙЛ INTERACTIVECHARTVIEW.CPP - СОБЫТИЯ И OVERLAY ГРАФИКОВ LAB_2
// ============================================================================

#include "InteractiveChartView.h"
#include "PlotChartUtils.h"

#include <QFontMetricsF>
#include <QGestureEvent>
#include <QGraphicsView>
#include <QLineF>
#include <QMouseEvent>
#include <QNativeGestureEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPinchGesture>
#include <QWheelEvent>

#include <QtCharts/QChart>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

#include <algorithm>
#include <cmath>
#include <optional>

using namespace PlotChartUtils;

InteractiveChartView::InteractiveChartView(PlotChartWidget* owner, QChart* chart, QWidget* parent)
    : QChartView(chart, parent), owner_(owner)
{
    setDragMode(QGraphicsView::NoDrag);
    setRubberBand(QChartView::NoRubberBand);
    setMouseTracking(true);
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    setAttribute(Qt::WA_AcceptTouchEvents, true);
    viewport()->setAttribute(Qt::WA_AcceptTouchEvents, true);
    grabGesture(Qt::PinchGesture);
    viewport()->grabGesture(Qt::PinchGesture);
}

void InteractiveChartView::paintEvent(QPaintEvent* event)
{
    QChartView::paintEvent(event);

    if (owner_ == nullptr || !owner_->hasData_ || chart() == nullptr || owner_->anchorSeries_ == nullptr)
    {
        return;
    }

    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor("#4b647b"), 1.35));

    const double xMin = owner_->axisX_->min();
    const double xMax = owner_->axisX_->max();
    const double yMin = owner_->axisY_->min();
    const double yMax = owner_->axisY_->max();

    if (yMin <= 0.0 && yMax >= 0.0)
    {
        DrawHorizontalAxis(&painter, xMin, xMax);
    }

    if (xMin <= 0.0 && xMax >= 0.0)
    {
        DrawVerticalAxis(&painter, yMin, yMax);
    }

    if (owner_->xAxisLabel_ == "x")
    {
        DrawSupportBoundary(&painter, kSupportLeft);
        DrawSupportBoundary(&painter, kSupportRight);
    }

    if (hoverActive_)
    {
        DrawHoverOverlay(&painter);
    }
}

bool InteractiveChartView::viewportEvent(QEvent* event)
{
    if (event == nullptr || owner_ == nullptr)
    {
        return QChartView::viewportEvent(event);
    }

    if (event->type() == QEvent::NativeGesture)
    {
        auto* nativeEvent = static_cast<QNativeGestureEvent*>(event);
        if (HandleNativeGesture(nativeEvent))
        {
            return true;
        }
    }

    if (event->type() == QEvent::Gesture)
    {
        auto* gestureEvent = static_cast<QGestureEvent*>(event);
        if (HandleGesture(gestureEvent))
        {
            return true;
        }
    }

    return QChartView::viewportEvent(event);
}

void InteractiveChartView::wheelEvent(QWheelEvent* event)
{
    if (event == nullptr || owner_ == nullptr || !owner_->hasData_)
    {
        QChartView::wheelEvent(event);
        return;
    }

    if ((event->modifiers() & Qt::ControlModifier) != 0)
    {
        QChartView::wheelEvent(event);
        return;
    }

    const QPoint pixelDelta = event->pixelDelta();
    if (!pixelDelta.isNull())
    {
        owner_->PanByPixels(pixelDelta);
        event->accept();
        return;
    }

    if (event->angleDelta().y() != 0)
    {
        const double factor = event->angleDelta().y() > 0 ? (1.0 / kWheelZoomStep) : kWheelZoomStep;
        owner_->ZoomAt(event->position(), factor);
        event->accept();
        return;
    }

    QChartView::wheelEvent(event);
}

void InteractiveChartView::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (owner_ != nullptr)
    {
        owner_->ResetView();
        event->accept();
        return;
    }

    QChartView::mouseDoubleClickEvent(event);
}

void InteractiveChartView::mouseMoveEvent(QMouseEvent* event)
{
    if (event != nullptr)
    {
        hoverViewportPos_ = event->position();
        hoverActive_ = IsPointInsidePlot(hoverViewportPos_);
        viewport()->update();
    }

    QChartView::mouseMoveEvent(event);
}

void InteractiveChartView::leaveEvent(QEvent* event)
{
    hoverActive_ = false;
    viewport()->update();
    QChartView::leaveEvent(event);
}

bool InteractiveChartView::IsPointInsidePlot(const QPointF& viewportPoint) const
{
    if (chart() == nullptr)
    {
        return false;
    }

    const QPointF scenePos = mapToScene(viewportPoint.toPoint());
    const QPointF chartPos = chart()->mapFromScene(scenePos);
    return chart()->plotArea().contains(chartPos);
}

void InteractiveChartView::DrawHoverOverlay(QPainter* painter)
{
    if (painter == nullptr || owner_ == nullptr || chart() == nullptr || owner_->anchorSeries_ == nullptr ||
        !IsPointInsidePlot(hoverViewportPos_))
    {
        return;
    }

    const QPointF scenePos = mapToScene(hoverViewportPos_.toPoint());
    const QPointF chartPos = chart()->mapFromScene(scenePos);
    const QPointF hoverValue = chart()->mapToValue(chartPos, owner_->anchorSeries_);
    const double xValue = hoverValue.x();
    if (!std::isfinite(xValue))
    {
        return;
    }

    const double yMin = owner_->axisY_->min();
    const double yMax = owner_->axisY_->max();
    const QPointF topChartPos = chart()->mapToPosition(QPointF(xValue, yMax), owner_->anchorSeries_);
    const QPointF bottomChartPos = chart()->mapToPosition(QPointF(xValue, yMin), owner_->anchorSeries_);
    const QPoint topViewPos = mapFromScene(chart()->mapToScene(topChartPos));
    const QPoint bottomViewPos = mapFromScene(chart()->mapToScene(bottomChartPos));

    QPen guidePen(QColor(59, 130, 246, 170));
    guidePen.setWidthF(1.2);
    guidePen.setStyle(Qt::DashLine);
    painter->setPen(guidePen);
    painter->drawLine(QLineF(topViewPos, bottomViewPos));

    QVector<HoverEntry> entries;
    entries.reserve(owner_->seriesStates_.size());

    for (qsizetype seriesIndex = 0; seriesIndex < owner_->seriesStates_.size(); ++seriesIndex)
    {
        if (owner_->highlightedSeriesIndex_ >= 0 && owner_->highlightedSeriesIndex_ != seriesIndex)
        {
            continue;
        }

        const PlotChartWidget::SeriesState& state = owner_->seriesStates_[seriesIndex];
        const std::optional<double> yValue =
            InterpolateSeriesValueAtX(state.fullPoints, state.monotonicByX, state.hoverMode, xValue);
        if (!yValue.has_value())
        {
            continue;
        }

        const QPointF markerChartPos = chart()->mapToPosition(QPointF(xValue, *yValue), owner_->anchorSeries_);
        const QPoint markerViewPos = mapFromScene(chart()->mapToScene(markerChartPos));
        entries.append({state.name, FormatHoverValue(*yValue), state.color, QPointF(markerViewPos)});
    }

    for (int index = entries.size() - 1; index >= 0; --index)
    {
        const HoverEntry& entry = entries[index];
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(255, 255, 255, 235));
        painter->drawEllipse(entry.markerViewPoint, 5.8, 5.8);
        painter->setBrush(QColor(entry.color.red(), entry.color.green(), entry.color.blue(), 235));
        painter->drawEllipse(entry.markerViewPoint, 4.2, 4.2);
    }

    DrawHoverTooltip(painter, xValue, entries);
    painter->setBrush(Qt::NoBrush);
    painter->setPen(QColor("#4b647b"));
}

void InteractiveChartView::DrawHoverTooltip(QPainter* painter, double xValue, const QVector<HoverEntry>& entries)
{
    if (painter == nullptr)
    {
        return;
    }

    QFont titleFont("Avenir Next");
    titleFont.setPointSize(11);
    titleFont.setWeight(QFont::DemiBold);

    QFont bodyFont("Avenir Next");
    bodyFont.setPointSize(10);
    bodyFont.setWeight(QFont::Medium);

    const QFontMetricsF titleMetrics(titleFont);
    const QFontMetricsF bodyMetrics(bodyFont);
    const QString titleText = QString("x = %1").arg(FormatHoverValue(xValue));

    constexpr qreal swatchSize = 10.0;
    constexpr qreal boxPadding = 12.0;
    constexpr qreal sectionSpacing = 8.0;
    constexpr qreal lineSpacing = 6.0;

    qreal contentWidth = titleMetrics.horizontalAdvance(titleText);
    for (const HoverEntry& entry : entries)
    {
        const QString lineText = QString("%1 = %2").arg(entry.name, entry.value);
        contentWidth = std::max(contentWidth, swatchSize + 8.0 + bodyMetrics.horizontalAdvance(lineText));
    }

    qreal contentHeight = titleMetrics.height();
    if (!entries.isEmpty())
    {
        contentHeight += sectionSpacing + entries.size() * bodyMetrics.height() + (entries.size() - 1) * lineSpacing;
    }

    QRectF tooltipRect(0.0, 0.0, contentWidth + boxPadding * 2.0, contentHeight + boxPadding * 2.0);
    QPointF topLeft = hoverViewportPos_ + QPointF(18.0, -18.0);

    const QRectF viewportRect = QRectF(viewport()->rect()).adjusted(8.0, 8.0, -8.0, -8.0);
    if (topLeft.x() + tooltipRect.width() > viewportRect.right())
    {
        topLeft.setX(hoverViewportPos_.x() - tooltipRect.width() - 18.0);
    }
    if (topLeft.x() < viewportRect.left())
    {
        topLeft.setX(viewportRect.left());
    }
    if (topLeft.y() + tooltipRect.height() > viewportRect.bottom())
    {
        topLeft.setY(viewportRect.bottom() - tooltipRect.height());
    }
    if (topLeft.y() < viewportRect.top())
    {
        topLeft.setY(viewportRect.top());
    }

    tooltipRect.moveTopLeft(topLeft);

    painter->setBrush(QColor(255, 255, 255, 242));
    painter->setPen(QPen(QColor("#d7e3ef"), 1.0));
    painter->drawRoundedRect(tooltipRect, 10.0, 10.0);

    painter->setFont(titleFont);
    painter->setPen(QColor("#102033"));
    qreal currentY = tooltipRect.top() + boxPadding + titleMetrics.ascent();
    painter->drawText(QPointF(tooltipRect.left() + boxPadding, currentY), titleText);

    if (entries.isEmpty())
    {
        return;
    }

    painter->setFont(bodyFont);
    currentY += titleMetrics.descent() + sectionSpacing;
    for (int index = 0; index < entries.size(); ++index)
    {
        const HoverEntry& entry = entries[index];
        currentY += bodyMetrics.ascent();

        const QRectF swatchRect(tooltipRect.left() + boxPadding,
                                currentY - bodyMetrics.ascent() + 1.0,
                                swatchSize,
                                swatchSize);
        painter->setPen(Qt::NoPen);
        painter->setBrush(entry.color);
        painter->drawRoundedRect(swatchRect, 3.0, 3.0);

        painter->setPen(QColor("#22364d"));
        painter->drawText(QPointF(swatchRect.right() + 8.0, currentY),
                          QString("%1 = %2").arg(entry.name, entry.value));

        currentY += bodyMetrics.descent();
        if (index + 1 < entries.size())
        {
            currentY += lineSpacing;
        }
    }
}

void InteractiveChartView::DrawHorizontalAxis(QPainter* painter, double xMin, double xMax)
{
    if (painter == nullptr || owner_ == nullptr || chart() == nullptr || owner_->anchorSeries_ == nullptr)
    {
        return;
    }

    const QPointF leftChartPos = chart()->mapToPosition(QPointF(xMin, 0.0), owner_->anchorSeries_);
    const QPointF rightChartPos = chart()->mapToPosition(QPointF(xMax, 0.0), owner_->anchorSeries_);
    const QPoint leftViewPos = mapFromScene(chart()->mapToScene(leftChartPos));
    const QPoint rightViewPos = mapFromScene(chart()->mapToScene(rightChartPos));

    DrawArrowHead(painter, QPointF(rightViewPos), QPointF(leftViewPos));
    DrawAxisLabel(painter,
                  owner_->xAxisLabel_,
                  QPointF(rightViewPos.x() - 10.0, rightViewPos.y() - 5.0),
                  Qt::AlignRight | Qt::AlignBottom);
}

void InteractiveChartView::DrawVerticalAxis(QPainter* painter, double yMin, double yMax)
{
    if (painter == nullptr || owner_ == nullptr || chart() == nullptr || owner_->anchorSeries_ == nullptr)
    {
        return;
    }

    const QPointF topChartPos = chart()->mapToPosition(QPointF(0.0, yMax), owner_->anchorSeries_);
    const QPointF bottomChartPos = chart()->mapToPosition(QPointF(0.0, yMin), owner_->anchorSeries_);
    const QPoint topViewPos = mapFromScene(chart()->mapToScene(topChartPos));
    const QPoint bottomViewPos = mapFromScene(chart()->mapToScene(bottomChartPos));

    DrawArrowHead(painter, QPointF(topViewPos), QPointF(bottomViewPos));
    DrawAxisLabel(painter,
                  owner_->yAxisLabel_,
                  QPointF(topViewPos.x() + 8.0, topViewPos.y() + 8.0),
                  Qt::AlignLeft | Qt::AlignTop);
}

void InteractiveChartView::DrawArrowHead(QPainter* painter, const QPointF& tip, const QPointF& from)
{
    if (painter == nullptr)
    {
        return;
    }

    const QLineF direction(from, tip);
    if (qFuzzyIsNull(direction.length()))
    {
        return;
    }

    const QLineF unit = direction.unitVector();
    const QPointF delta = unit.p2() - unit.p1();
    const QPointF back = tip - delta * kAxisArrowLength;
    const QPointF normal(-delta.y(), delta.x());
    painter->drawLine(QLineF(tip, back + normal * kAxisArrowHalfWidth));
    painter->drawLine(QLineF(tip, back - normal * kAxisArrowHalfWidth));
}

void InteractiveChartView::DrawAxisLabel(QPainter* painter,
                                         const QString& text,
                                         const QPointF& anchor,
                                         Qt::Alignment alignment)
{
    if (painter == nullptr || text.isEmpty())
    {
        return;
    }

    QFont font = painter->font();
    font.setPointSizeF(std::max(font.pointSizeF(), 10.5));
    font.setBold(true);
    painter->setFont(font);

    const QFontMetricsF metrics(font);
    QRectF textRect(QPointF(0.0, 0.0), QSizeF(metrics.horizontalAdvance(text), metrics.height()));
    QPointF topLeft = anchor;

    if (alignment & Qt::AlignRight)
    {
        topLeft.rx() -= textRect.width();
    }
    else if (alignment & Qt::AlignHCenter)
    {
        topLeft.rx() -= textRect.width() * 0.5;
    }

    if (alignment & Qt::AlignBottom)
    {
        topLeft.ry() -= textRect.height();
    }
    else if (alignment & Qt::AlignVCenter)
    {
        topLeft.ry() -= textRect.height() * 0.5;
    }

    const QRectF viewportRect = QRectF(viewport()->rect()).adjusted(6.0, 6.0, -6.0, -6.0);
    topLeft.setX(std::clamp(topLeft.x(), viewportRect.left(), viewportRect.right() - textRect.width()));
    topLeft.setY(std::clamp(topLeft.y(), viewportRect.top(), viewportRect.bottom() - textRect.height()));
    textRect.moveTopLeft(topLeft);

    painter->setPen(QColor("#203146"));
    painter->drawText(textRect, Qt::AlignCenter, text);
    painter->setPen(QColor("#4b647b"));
}

void InteractiveChartView::DrawSupportBoundary(QPainter* painter, double xValue)
{
    if (painter == nullptr || owner_ == nullptr || chart() == nullptr || owner_->anchorSeries_ == nullptr)
    {
        return;
    }

    const double xMin = owner_->axisX_->min();
    const double xMax = owner_->axisX_->max();
    if (xValue < xMin || xValue > xMax)
    {
        return;
    }

    const double yMin = owner_->axisY_->min();
    const double yMax = owner_->axisY_->max();
    const QPointF topChartPos = chart()->mapToPosition(QPointF(xValue, yMax), owner_->anchorSeries_);
    const QPointF bottomChartPos = chart()->mapToPosition(QPointF(xValue, yMin), owner_->anchorSeries_);
    const QPoint topViewPos = mapFromScene(chart()->mapToScene(topChartPos));
    const QPoint bottomViewPos = mapFromScene(chart()->mapToScene(bottomChartPos));

    QPen boundaryPen(QColor("#94a3b8"));
    boundaryPen.setWidthF(1.2);
    boundaryPen.setStyle(Qt::DashLine);
    painter->setPen(boundaryPen);
    painter->drawLine(QLineF(topViewPos, bottomViewPos));

    DrawAxisLabel(painter,
                  QString("x=%1").arg(xValue == 0.0 ? "0" : "1"),
                  QPointF(topViewPos.x() + 8.0, topViewPos.y() + 20.0),
                  Qt::AlignLeft | Qt::AlignTop);
    painter->setPen(QColor("#4b647b"));
}

bool InteractiveChartView::HandleNativeGesture(QNativeGestureEvent* event)
{
    if (event == nullptr || owner_ == nullptr)
    {
        return false;
    }

    const QPointF viewportPos = event->position();
    switch (event->gestureType())
    {
    case Qt::ZoomNativeGesture:
    {
        const double delta = event->value();
        if (!std::isfinite(delta) || qFuzzyIsNull(delta))
        {
            event->accept();
            return true;
        }

        owner_->ZoomAt(viewportPos, std::clamp(std::exp(-delta), kMinZoomFactor, kMaxZoomFactor));
        event->accept();
        return true;
    }
    case Qt::SmartZoomNativeGesture:
        owner_->ResetView();
        event->accept();
        return true;
    case Qt::PanNativeGesture:
    case Qt::SwipeNativeGesture:
        owner_->PanByPixels(event->delta());
        event->accept();
        return true;
    case Qt::BeginNativeGesture:
    case Qt::EndNativeGesture:
        event->accept();
        return true;
    default:
        return false;
    }
}

bool InteractiveChartView::HandleGesture(QGestureEvent* event)
{
    if (event == nullptr || owner_ == nullptr)
    {
        return false;
    }

    QGesture* gesture = event->gesture(Qt::PinchGesture);
    if (gesture == nullptr)
    {
        return false;
    }

    auto* pinch = static_cast<QPinchGesture*>(gesture);
    if ((pinch->changeFlags() & QPinchGesture::ScaleFactorChanged) == 0)
    {
        event->accept(gesture);
        return true;
    }

    const double scale = pinch->scaleFactor();
    if (!std::isfinite(scale) || scale <= 0.0)
    {
        event->accept(gesture);
        return true;
    }

    owner_->ZoomAt(pinch->centerPoint(), std::clamp(1.0 / scale, kMinZoomFactor, kMaxZoomFactor));
    event->accept(gesture);
    return true;
}
