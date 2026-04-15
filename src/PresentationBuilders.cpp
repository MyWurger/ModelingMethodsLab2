// ============================================================================
// ФАЙЛ PRESENTATIONBUILDERS.CPP - ПОДГОТОВКА GUI-ДАННЫХ LAB_2
// ============================================================================

#include "PresentationBuilders.h"

#include <algorithm>
#include <cmath>
#include <utility>

NumericMatrix MakeSingleRowMatrix(const NumericVector& vector)
{
    return {vector};
}

NumericMatrix BuildPreviewMatrix(const SamplingResult& result)
{
    NumericMatrix matrix;
    const std::size_t rows = std::min(result.independentPreviewSamples.size(), result.transformedPreviewSamples.size());
    matrix.reserve(rows);

    for (std::size_t row = 0; row < rows; ++row)
    {
        NumericVector values;
        values.reserve(1 + result.independentPreviewSamples[row].size() + result.transformedPreviewSamples[row].size());
        values.push_back(static_cast<double>(row + 1));
        values.insert(values.end(),
                      result.independentPreviewSamples[row].begin(),
                      result.independentPreviewSamples[row].end());
        values.insert(values.end(),
                      result.transformedPreviewSamples[row].begin(),
                      result.transformedPreviewSamples[row].end());
        matrix.push_back(std::move(values));
    }

    return matrix;
}

QVector<QPointF> BuildHistogramOutline(const std::vector<HistogramBin>& bins)
{
    QVector<QPointF> points;
    points.reserve(static_cast<int>(bins.size() * 2 + 2));

    if (bins.empty())
    {
        return points;
    }

    points.append(QPointF(bins.front().left, 0.0));
    for (const HistogramBin& bin : bins)
    {
        points.append(QPointF(bin.left, bin.density));
        points.append(QPointF(bin.right, bin.density));
    }
    points.append(QPointF(bins.back().right, 0.0));
    return points;
}

namespace
{
double NormalDensity(double x, double mean, double variance)
{
    constexpr double kPi = 3.14159265358979323846;

    if (!(variance > 0.0))
    {
        return 0.0;
    }

    const double sigma = std::sqrt(variance);
    const double z = (x - mean) / sigma;
    const double normalization = sigma * std::sqrt(2.0 * kPi);
    return std::exp(-0.5 * z * z) / normalization;
}
} // namespace

QVector<QPointF> BuildNormalDensityCurve(const ComponentHistogram& histogram)
{
    QVector<QPointF> points;
    if (histogram.bins.empty())
    {
        return points;
    }

    const double xMin = histogram.bins.front().left;
    const double xMax = histogram.bins.back().right;
    const int sampleCount = 220;
    points.reserve(sampleCount);

    for (int index = 0; index < sampleCount; ++index)
    {
        const double t = static_cast<double>(index) / static_cast<double>(sampleCount - 1);
        const double x = xMin + (xMax - xMin) * t;
        points.append(QPointF(x, NormalDensity(x, histogram.theoreticalMean, histogram.theoreticalVariance)));
    }

    return points;
}

QVector<QPointF> BuildSequenceSeries(const NumericMatrix& previewSamples, std::size_t componentIndex)
{
    QVector<QPointF> points;
    points.reserve(static_cast<int>(previewSamples.size()));

    for (std::size_t index = 0; index < previewSamples.size(); ++index)
    {
        points.append(QPointF(static_cast<double>(index + 1), previewSamples[index][componentIndex]));
    }

    return points;
}
