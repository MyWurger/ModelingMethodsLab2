// ============================================================================
// ФАЙЛ SAMPLINGTYPES.H - ОБЩИЕ ТИПЫ ДАННЫХ ДЛЯ LAB_2
// ============================================================================
// Назначение файла:
// 1) хранить общие типы, которыми обмениваются GUI и вычислительное ядро;
// 2) отделить математику лабораторной от представления в Qt;
// 3) собрать в одном месте все структуры результата моделирования.
// ============================================================================

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

using NumericVector = std::vector<double>;
using NumericMatrix = std::vector<NumericVector>;

struct SamplingOptions
{
    std::size_t sampleSize = 20000;
    std::size_t histogramBins = 24;
    std::size_t previewCount = 400;
    std::uint32_t seed = 20260414u;
};

struct HistogramBin
{
    double left = 0.0;
    double right = 0.0;
    std::size_t count = 0;
    double density = 0.0;
};

struct ComponentStatistics
{
    std::size_t componentIndex = 0;
    double theoreticalMean = 0.0;
    double theoreticalVariance = 0.0;
    double empiricalMean = 0.0;
    double empiricalVariance = 0.0;
    double empiricalStandardDeviation = 0.0;
    double minimum = 0.0;
    double maximum = 0.0;
};

struct ComponentHistogram
{
    std::size_t componentIndex = 0;
    double theoreticalMean = 0.0;
    double theoreticalVariance = 0.0;
    std::vector<HistogramBin> bins;
};

struct SamplingSummary
{
    std::size_t dimension = 0;
    std::size_t sampleSize = 0;
    double maxMeanError = 0.0;
    double maxCovarianceError = 0.0;
    double frobeniusCovarianceError = 0.0;
};

struct SamplingResult
{
    bool success = false;
    std::string message;
    SamplingOptions options;

    NumericVector theoreticalMean;
    NumericMatrix theoreticalCovariance;
    NumericMatrix theoreticalCorrelation;
    NumericMatrix choleskyFactor;

    NumericVector empiricalMean;
    NumericMatrix empiricalCovariance;
    NumericMatrix empiricalCorrelation;

    std::vector<ComponentStatistics> componentStatistics;
    std::vector<ComponentHistogram> histograms;

    NumericMatrix samples;
    NumericMatrix independentPreviewSamples;
    NumericMatrix transformedPreviewSamples;

    SamplingSummary summary;
};
