// ============================================================================
// ФАЙЛ PLOTCHARTUTILS.CPP - СЛУЖЕБНАЯ МАТЕМАТИКА ГРАФИКОВ LAB_2
// ============================================================================

#include "PlotChartUtils.h"

#include <QtGlobal>

#include <algorithm>
#include <cmath>

namespace PlotChartUtils
{
double NiceTickStep(double span, int targetTicks)
{
    if (!std::isfinite(span) || span <= 0.0 || targetTicks < 2)
    {
        return 1.0;
    }

    const double rawStep = span / static_cast<double>(targetTicks - 1);
    const double magnitude = std::pow(10.0, std::floor(std::log10(rawStep)));
    const double normalized = rawStep / magnitude;

    double nice = 1.0;
    if (normalized >= 7.5)
    {
        nice = 10.0;
    }
    else if (normalized >= 3.5)
    {
        nice = 5.0;
    }
    else if (normalized >= 1.5)
    {
        nice = 2.0;
    }

    return nice * magnitude;
}

int DecimalsForStep(double step)
{
    if (!std::isfinite(step) || step <= 0.0)
    {
        return 6;
    }

    if (step >= 1.0)
    {
        return 0;
    }

    const int decimals = static_cast<int>(std::ceil(-std::log10(step))) + 1;
    return std::clamp(decimals, 0, 12);
}

double TickAnchorForRange(double minValue, double step)
{
    if (!std::isfinite(minValue) || !(step > 0.0) || !std::isfinite(step))
    {
        return 0.0;
    }

    return std::floor(minValue / step) * step;
}

double StableTickStep(double previousStep, double span, int targetTicks)
{
    Q_UNUSED(previousStep);
    return NiceTickStep(span, targetTicks);
}

QString FormatHoverValue(double value)
{
    if (!std::isfinite(value))
    {
        return QStringLiteral("—");
    }

    if (qFuzzyIsNull(value))
    {
        return QStringLiteral("0");
    }

    const double absoluteValue = std::abs(value);
    if (absoluteValue >= 1.0e4 || absoluteValue < 1.0e-4)
    {
        return QString::number(value, 'e', 3);
    }

    QString text = QString::number(value, 'f', 4);
    while (text.contains('.') && (text.endsWith('0') || text.endsWith('.')))
    {
        text.chop(1);
    }

    return text == "-0" ? QStringLiteral("0") : text;
}

std::optional<double> InterpolateSeriesValueAtX(const QVector<QPointF>& points,
                                                bool monotonicByX,
                                                PlotSeriesData::HoverMode hoverMode,
                                                double xValue)
{
    if (points.isEmpty() || !std::isfinite(xValue))
    {
        return std::nullopt;
    }

    if (hoverMode == PlotSeriesData::HoverMode::Step)
    {
        if (monotonicByX)
        {
            const auto upperIt =
                std::upper_bound(points.cbegin(), points.cend(), xValue, [](double value, const QPointF& point) {
                    return value < point.x();
                });

            return upperIt == points.cbegin() ? std::optional<double>(points.front().y())
                                              : std::optional<double>((upperIt - 1)->y());
        }

        constexpr double kStepTolerance = 1.0e-12;
        double lastY = points.front().y();
        bool foundPointOnTheLeft = false;

        for (const QPointF& point : points)
        {
            if (point.x() <= xValue + kStepTolerance)
            {
                lastY = point.y();
                foundPointOnTheLeft = true;
            }
            else
            {
                break;
            }
        }

        return foundPointOnTheLeft ? std::optional<double>(lastY)
                                   : std::optional<double>(points.front().y());
    }

    if (!monotonicByX || points.size() == 1)
    {
        const auto nearestIt =
            std::min_element(points.cbegin(), points.cend(), [xValue](const QPointF& left, const QPointF& right) {
                return std::abs(left.x() - xValue) < std::abs(right.x() - xValue);
            });

        return nearestIt != points.cend() ? std::optional<double>(nearestIt->y()) : std::nullopt;
    }

    const auto rightIt =
        std::lower_bound(points.cbegin(), points.cend(), xValue, [](const QPointF& point, double value) {
            return point.x() < value;
        });

    if (rightIt == points.cbegin())
    {
        return points.front().y();
    }
    if (rightIt == points.cend())
    {
        return points.back().y();
    }

    const QPointF rightPoint = *rightIt;
    const QPointF leftPoint = *(rightIt - 1);
    const double dx = rightPoint.x() - leftPoint.x();
    if (std::abs(dx) < 1.0e-12)
    {
        return std::max(leftPoint.y(), rightPoint.y());
    }

    const double t = std::clamp((xValue - leftPoint.x()) / dx, 0.0, 1.0);
    return leftPoint.y() + (rightPoint.y() - leftPoint.y()) * t;
}

QVector<QPointF> DownsamplePoints(const QVector<QPointF>& points)
{
    if (points.size() <= kMaxRenderedPoints)
    {
        return points;
    }

    QVector<QPointF> sampled;
    sampled.reserve(kMaxRenderedPoints);

    const double step = static_cast<double>(points.size() - 1) / static_cast<double>(kMaxRenderedPoints - 1);
    const int maxIndex = points.size() - 1;

    for (int i = 0; i < kMaxRenderedPoints - 1; ++i)
    {
        const int index = static_cast<int>(std::round(i * step));
        sampled.append(points[std::clamp(index, 0, maxIndex)]);
    }

    sampled.append(points.back());
    return sampled;
}

bool IsMonotonicByX(const QVector<QPointF>& points)
{
    for (int i = 1; i < points.size(); ++i)
    {
        if (points[i].x() < points[i - 1].x())
        {
            return false;
        }
    }

    return true;
}

QVector<QPointF> BuildRenderedPoints(const QVector<QPointF>& fullPoints,
                                     bool monotonicByX,
                                     double visibleXMin,
                                     double visibleXMax)
{
    if (fullPoints.size() <= kMaxRenderedPoints)
    {
        return fullPoints;
    }

    if (!monotonicByX)
    {
        return DownsamplePoints(fullPoints);
    }

    const auto leftIt =
        std::lower_bound(fullPoints.cbegin(), fullPoints.cend(), visibleXMin, [](const QPointF& point, double value) {
            return point.x() < value;
        });
    const auto rightIt =
        std::upper_bound(fullPoints.cbegin(), fullPoints.cend(), visibleXMax, [](double value, const QPointF& point) {
            return value < point.x();
        });

    int firstIndex = static_cast<int>(std::distance(fullPoints.cbegin(), leftIt));
    int lastIndex = static_cast<int>(std::distance(fullPoints.cbegin(), rightIt)) - 1;
    firstIndex = std::max(0, firstIndex - 1);
    lastIndex = std::min(static_cast<int>(fullPoints.size()) - 1, std::max(firstIndex, lastIndex + 1));

    QVector<QPointF> visiblePoints;
    visiblePoints.reserve(lastIndex - firstIndex + 1);
    for (int index = firstIndex; index <= lastIndex; ++index)
    {
        visiblePoints.append(fullPoints[index]);
    }

    return visiblePoints.size() <= kMaxRenderedPoints ? visiblePoints : DownsamplePoints(visiblePoints);
}
} // namespace PlotChartUtils
