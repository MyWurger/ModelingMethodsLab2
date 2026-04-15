// ============================================================================
// ФАЙЛ PRESENTATIONBUILDERS.H - ПОДГОТОВКА ДАННЫХ ДЛЯ ОТОБРАЖЕНИЯ LAB_2
// ============================================================================

#pragma once

#include "SamplingTypes.h"

#include <QPointF>
#include <QVector>

#include <cstddef>
#include <vector>

NumericMatrix MakeSingleRowMatrix(const NumericVector& vector);
NumericMatrix BuildPreviewMatrix(const SamplingResult& result);

QVector<QPointF> BuildHistogramOutline(const std::vector<HistogramBin>& bins);
QVector<QPointF> BuildNormalDensityCurve(const ComponentHistogram& histogram);
QVector<QPointF> BuildSequenceSeries(const NumericMatrix& previewSamples, std::size_t componentIndex);
