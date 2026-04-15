// ============================================================================
// ФАЙЛ SAMPLINGENGINE.CPP - ВЫЧИСЛИТЕЛЬНОЕ ЯДРО LAB_2
// ============================================================================

#include "SamplingEngine.h"
#include "Lab2Variant.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <random>
#include <sstream>
#include <stdexcept>

namespace
{
bool IsSquareMatrix(const NumericMatrix& matrix)
{
    if (matrix.empty())
    {
        return false;
    }

    const std::size_t size = matrix.size();
    for (const NumericVector& row : matrix)
    {
        if (row.size() != size)
        {
            return false;
        }
    }

    return true;
}

void ValidateOptions(const SamplingOptions& options)
{
    if (options.sampleSize == 0)
    {
        throw std::invalid_argument("Размер выборки должен быть больше нуля");
    }

    if (options.histogramBins == 0)
    {
        throw std::invalid_argument("Число интервалов гистограммы должно быть больше нуля");
    }

    if (options.previewCount == 0)
    {
        throw std::invalid_argument("Число отображаемых точек должно быть больше нуля");
    }
}

NumericMatrix ComputeCholeskyFactor(const NumericMatrix& covariance)
{
    if (!IsSquareMatrix(covariance))
    {
        throw std::invalid_argument("Ковариационная матрица должна быть квадратной");
    }

    const std::size_t dimension = covariance.size();
    NumericMatrix factor(dimension, NumericVector(dimension, 0.0));

    for (std::size_t i = 0; i < dimension; ++i)
    {
        for (std::size_t j = 0; j < dimension; ++j)
        {
            if (std::abs(covariance[i][j] - covariance[j][i]) > 1.0e-12)
            {
                throw std::invalid_argument("Ковариационная матрица должна быть симметричной");
            }
        }

        double diagonalRemainder = covariance[i][i];
        for (std::size_t p = 0; p < i; ++p)
        {
            diagonalRemainder -= factor[i][p] * factor[i][p];
        }

        if (!(diagonalRemainder > 0.0))
        {
            throw std::invalid_argument("Матрица варианта не является положительно определённой");
        }

        factor[i][i] = std::sqrt(diagonalRemainder);

        for (std::size_t j = i + 1; j < dimension; ++j)
        {
            double crossRemainder = covariance[j][i];
            for (std::size_t p = 0; p < i; ++p)
            {
                crossRemainder -= factor[i][p] * factor[j][p];
            }

            factor[j][i] = crossRemainder / factor[i][i];
        }
    }

    return factor;
}

NumericVector MultiplyLowerTriangular(const NumericMatrix& factor, const NumericVector& vector)
{
    const std::size_t dimension = factor.size();
    NumericVector result(dimension, 0.0);

    for (std::size_t row = 0; row < dimension; ++row)
    {
        for (std::size_t column = 0; column <= row; ++column)
        {
            result[row] += factor[row][column] * vector[column];
        }
    }

    return result;
}

NumericVector AddVectors(const NumericVector& left, const NumericVector& right)
{
    if (left.size() != right.size())
    {
        throw std::invalid_argument("Размерности векторов не совпадают");
    }

    NumericVector result(left.size(), 0.0);
    for (std::size_t index = 0; index < left.size(); ++index)
    {
        result[index] = left[index] + right[index];
    }

    return result;
}

NumericVector ComputeEmpiricalMean(const NumericMatrix& samples)
{
    if (samples.empty())
    {
        return {};
    }

    const std::size_t dimension = samples.front().size();
    NumericVector mean(dimension, 0.0);

    for (const NumericVector& sample : samples)
    {
        if (sample.size() != dimension)
        {
            throw std::invalid_argument("Все сгенерированные векторы должны иметь одинаковую размерность");
        }

        for (std::size_t index = 0; index < dimension; ++index)
        {
            mean[index] += sample[index];
        }
    }

    const double count = static_cast<double>(samples.size());
    for (double& value : mean)
    {
        value /= count;
    }

    return mean;
}

NumericMatrix ComputeEmpiricalCovariance(const NumericMatrix& samples, const NumericVector& mean)
{
    if (samples.empty())
    {
        return {};
    }

    const std::size_t dimension = mean.size();
    NumericMatrix covariance(dimension, NumericVector(dimension, 0.0));

    for (const NumericVector& sample : samples)
    {
        for (std::size_t row = 0; row < dimension; ++row)
        {
            const double rowDelta = sample[row] - mean[row];
            for (std::size_t column = 0; column < dimension; ++column)
            {
                covariance[row][column] += rowDelta * (sample[column] - mean[column]);
            }
        }
    }

    const double count = static_cast<double>(samples.size());
    for (NumericVector& row : covariance)
    {
        for (double& value : row)
        {
            value /= count;
        }
    }

    return covariance;
}

NumericMatrix ComputeCorrelationMatrix(const NumericMatrix& covariance)
{
    if (covariance.empty())
    {
        return {};
    }

    const std::size_t dimension = covariance.size();
    NumericMatrix correlation(dimension, NumericVector(dimension, 0.0));

    for (std::size_t row = 0; row < dimension; ++row)
    {
        for (std::size_t column = 0; column < dimension; ++column)
        {
            const double denominator =
                std::sqrt(std::max(0.0, covariance[row][row]) * std::max(0.0, covariance[column][column]));

            if (denominator > 0.0)
            {
                correlation[row][column] = covariance[row][column] / denominator;
            }

            if (row == column)
            {
                correlation[row][column] = 1.0;
            }
        }
    }

    return correlation;
}

NumericVector ExtractComponent(const NumericMatrix& samples, std::size_t componentIndex)
{
    NumericVector component;
    component.reserve(samples.size());

    for (const NumericVector& sample : samples)
    {
        component.push_back(sample[componentIndex]);
    }

    return component;
}

ComponentStatistics BuildComponentStatistics(const NumericVector& values,
                                            std::size_t componentIndex,
                                            double theoreticalMean,
                                            double theoreticalVariance)
{
    if (values.empty())
    {
        throw std::invalid_argument("Нельзя вычислить статистики по пустому столбцу");
    }

    ComponentStatistics statistics;
    statistics.componentIndex = componentIndex;
    statistics.theoreticalMean = theoreticalMean;
    statistics.theoreticalVariance = theoreticalVariance;

    const auto [minimumIt, maximumIt] = std::minmax_element(values.begin(), values.end());
    statistics.minimum = *minimumIt;
    statistics.maximum = *maximumIt;

    const double sum = std::accumulate(values.begin(), values.end(), 0.0);
    statistics.empiricalMean = sum / static_cast<double>(values.size());

    double varianceAccumulator = 0.0;
    for (double value : values)
    {
        const double delta = value - statistics.empiricalMean;
        varianceAccumulator += delta * delta;
    }

    statistics.empiricalVariance = varianceAccumulator / static_cast<double>(values.size());
    statistics.empiricalStandardDeviation = std::sqrt(std::max(0.0, statistics.empiricalVariance));
    return statistics;
}

std::vector<HistogramBin> BuildHistogram(const NumericVector& values, std::size_t histogramBins)
{
    if (values.empty())
    {
        return {};
    }

    const auto [minimumIt, maximumIt] = std::minmax_element(values.begin(), values.end());
    double minimum = *minimumIt;
    double maximum = *maximumIt;

    if (!(maximum > minimum))
    {
        minimum -= 0.5;
        maximum += 0.5;
    }

    const double width = (maximum - minimum) / static_cast<double>(histogramBins);
    std::vector<HistogramBin> bins(histogramBins);

    for (std::size_t index = 0; index < histogramBins; ++index)
    {
        bins[index].left = minimum + static_cast<double>(index) * width;
        bins[index].right = bins[index].left + width;
    }

    for (double value : values)
    {
        std::size_t index = histogramBins - 1;
        if (value < maximum)
        {
            const double normalized = (value - minimum) / width;
            index = std::clamp<std::size_t>(static_cast<std::size_t>(normalized), 0, histogramBins - 1);
        }

        bins[index].count += 1;
    }

    const double sampleCount = static_cast<double>(values.size());
    for (HistogramBin& bin : bins)
    {
        bin.density = bin.count / (sampleCount * width);
    }

    return bins;
}

SamplingSummary BuildSummary(const NumericVector& theoreticalMean,
                             const NumericMatrix& theoreticalCovariance,
                             const NumericVector& empiricalMean,
                             const NumericMatrix& empiricalCovariance,
                             std::size_t sampleSize)
{
    SamplingSummary summary;
    summary.dimension = theoreticalMean.size();
    summary.sampleSize = sampleSize;

    for (std::size_t index = 0; index < theoreticalMean.size(); ++index)
    {
        summary.maxMeanError =
            std::max(summary.maxMeanError, std::abs(empiricalMean[index] - theoreticalMean[index]));
    }

    double frobeniusAccumulator = 0.0;
    for (std::size_t row = 0; row < theoreticalCovariance.size(); ++row)
    {
        for (std::size_t column = 0; column < theoreticalCovariance[row].size(); ++column)
        {
            const double delta =
                empiricalCovariance[row][column] - theoreticalCovariance[row][column];
            summary.maxCovarianceError = std::max(summary.maxCovarianceError, std::abs(delta));
            frobeniusAccumulator += delta * delta;
        }
    }

    summary.frobeniusCovarianceError = std::sqrt(frobeniusAccumulator);
    return summary;
}
} // namespace

SamplingResult RunSampling(const SamplingOptions& options)
{
    SamplingResult result;
    result.options = options;

    try
    {
        ValidateOptions(options);

        const NumericVector theoreticalMean = Lab2Variant::MeanVector();
        const NumericMatrix theoreticalCovariance = Lab2Variant::CovarianceMatrix();
        const NumericMatrix choleskyFactor = ComputeCholeskyFactor(theoreticalCovariance);
        const std::size_t dimension = theoreticalMean.size();

        result.options.previewCount = std::min(options.previewCount, options.sampleSize);

        std::mt19937_64 generator(options.seed);
        std::normal_distribution<double> standardNormal(0.0, 1.0);

        result.samples.reserve(options.sampleSize);
        result.independentPreviewSamples.reserve(result.options.previewCount);
        result.transformedPreviewSamples.reserve(result.options.previewCount);

        for (std::size_t sampleIndex = 0; sampleIndex < options.sampleSize; ++sampleIndex)
        {
            NumericVector independentVector(dimension, 0.0);
            for (double& value : independentVector)
            {
                value = standardNormal(generator);
            }

            const NumericVector correlatedVector =
                AddVectors(MultiplyLowerTriangular(choleskyFactor, independentVector), theoreticalMean);

            result.samples.push_back(correlatedVector);

            if (sampleIndex < result.options.previewCount)
            {
                result.independentPreviewSamples.push_back(independentVector);
                result.transformedPreviewSamples.push_back(correlatedVector);
            }
        }

        result.theoreticalMean = theoreticalMean;
        result.theoreticalCovariance = theoreticalCovariance;
        result.theoreticalCorrelation = ComputeCorrelationMatrix(theoreticalCovariance);
        result.choleskyFactor = choleskyFactor;

        result.empiricalMean = ComputeEmpiricalMean(result.samples);
        result.empiricalCovariance = ComputeEmpiricalCovariance(result.samples, result.empiricalMean);
        result.empiricalCorrelation = ComputeCorrelationMatrix(result.empiricalCovariance);

        for (std::size_t componentIndex = 0; componentIndex < dimension; ++componentIndex)
        {
            const NumericVector componentValues = ExtractComponent(result.samples, componentIndex);
            const double theoreticalVariance = theoreticalCovariance[componentIndex][componentIndex];

            result.componentStatistics.push_back(BuildComponentStatistics(componentValues,
                                                                          componentIndex,
                                                                          theoreticalMean[componentIndex],
                                                                          theoreticalVariance));

            ComponentHistogram histogram;
            histogram.componentIndex = componentIndex;
            histogram.theoreticalMean = theoreticalMean[componentIndex];
            histogram.theoreticalVariance = theoreticalVariance;
            histogram.bins = BuildHistogram(componentValues, options.histogramBins);
            result.histograms.push_back(std::move(histogram));
        }

        result.summary = BuildSummary(result.theoreticalMean,
                                      result.theoreticalCovariance,
                                      result.empiricalMean,
                                      result.empiricalCovariance,
                                      options.sampleSize);

        std::ostringstream message;
        message << "Вариант 1 смоделирован: n=" << options.sampleSize
                << ", матрица A получена разложением Холецкого, "
                << "эмпирическая K<sub>X</sub> рассчитана по сформированной выборке.";

        result.message = message.str();
        result.success = true;
    }
    catch (const std::exception& error)
    {
        result.success = false;
        result.message = error.what();
    }

    return result;
}
