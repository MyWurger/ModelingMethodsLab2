// ============================================================================
// ФАЙЛ PLOTCHARTUTILS.H - СЛУЖЕБНАЯ МАТЕМАТИКА ГРАФИКОВ LAB_2
// ============================================================================

#pragma once

#include "PlotChartWidget.h"

#include <QPointF>
#include <QString>
#include <QVector>

#include <optional>

namespace PlotChartUtils
{
constexpr int kMaxRenderedPoints = 20000;
constexpr double kMinZoomFactor = 0.75;
constexpr double kMaxZoomFactor = 1.35;
constexpr double kWheelZoomStep = 1.15;
constexpr double kAxisArrowLength = 9.0;
constexpr double kAxisArrowHalfWidth = 4.5;
constexpr double kSupportLeft = 0.0;
constexpr double kSupportRight = 1.0;

double NiceTickStep(double span, int targetTicks);
int DecimalsForStep(double step);
double TickAnchorForRange(double minValue, double step);
double StableTickStep(double previousStep, double span, int targetTicks);
QString FormatHoverValue(double value);

std::optional<double> InterpolateSeriesValueAtX(const QVector<QPointF>& points,
                                                bool monotonicByX,
                                                PlotSeriesData::HoverMode hoverMode,
                                                double xValue);

QVector<QPointF> DownsamplePoints(const QVector<QPointF>& points);
bool IsMonotonicByX(const QVector<QPointF>& points);
QVector<QPointF> BuildRenderedPoints(const QVector<QPointF>& fullPoints,
                                     bool monotonicByX,
                                     double visibleXMin,
                                     double visibleXMax);
} // namespace PlotChartUtils
