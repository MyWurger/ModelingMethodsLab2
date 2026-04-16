// ============================================================================
// ФАЙЛ SAMPLINGENGINE.CPP - ВЫЧИСЛИТЕЛЬНОЕ ЯДРО LAB_2
// ============================================================================
// В этом файле сосредоточена вся основная математика лабораторной работы:
// - проверка входных параметров моделирования;
// - разложение заданной ковариационной матрицы по Холецкому;
// - генерация независимого вектора U со стандартным нормальным законом;
// - построение зависимого вектора X по формуле X = A * U + m_X;
// - вычисление выборочных оценок m^, K^ и R;
// - построение гистограмм и сводных ошибок.
//
// Это и есть вычислительное ядро проекта.
// GUI только вызывает эти функции и отображает уже готовый результат.
// ============================================================================

// Подключаем публичный интерфейс вычислительного ядра.
#include "SamplingEngine.h"
// Подключаем теоретические данные варианта: m_X и K_X.
#include "Lab2Variant.h"
// std::minmax_element, std::max и std::clamp используются в служебной математике.
#include <algorithm>
// std::sqrt, std::abs и другие элементарные функции нужны для формул.
#include <cmath>
// numeric limits подключен под граничные численные значения.
#include <limits>
// std::accumulate используется для суммирования выборочных значений.
#include <numeric>
// std::mt19937_64 и std::normal_distribution используются для генерации U.
#include <random>
// std::ostringstream нужен для сборки итогового текстового сообщения.
#include <sstream>
// std::invalid_argument используется для понятных ошибок валидации и математики.
#include <stdexcept>

namespace
{
// ----------------------------------------------------------------------------
// ФУНКЦИЯ IsSquareMatrix
// ----------------------------------------------------------------------------
// Назначение:
// Проверить, является ли матрица квадратной.
//
// Это обязательная предварительная проверка перед разложением Холецкого,
// потому что ковариационная матрица по определению должна быть квадратной.
//
// Принимает:
// - matrix : матрицу, которую нужно проверить.
//
// Возвращает:
// - bool : true, если матрица квадратная, иначе false.
// ----------------------------------------------------------------------------
bool IsSquareMatrix(const NumericMatrix& matrix)
{
    // Пустую матрицу квадратной не считаем:
    // для ковариационной матрицы такой случай не имеет смысла.
    if (matrix.empty())
    {
        return false;
    }

    // У квадратной матрицы число строк совпадает с числом столбцов.
    const std::size_t size = matrix.size();

    // Проверяем, что длина каждой строки равна общему числу строк.
    for (const NumericVector& row : matrix)
    {
        if (row.size() != size)
        {
            return false;
        }
    }

    return true;
}

// ----------------------------------------------------------------------------
// ФУНКЦИЯ ValidateOptions
// ----------------------------------------------------------------------------
// Назначение:
// Проверить корректность параметров моделирования до начала вычислений.
//
// Принимает:
// - options : пользовательские параметры запуска моделирования.
//
// Возвращает:
// - void : при корректных параметрах функция просто завершается,
//          при ошибке генерирует исключение.
// ----------------------------------------------------------------------------
void ValidateOptions(const SamplingOptions& options)
{
    // Размер выборки N должен быть положительным,
    // иначе сами выборочные оценки не имеют смысла.
    if (options.sampleSize == 0)
    {
        throw std::invalid_argument("Размер выборки должен быть больше нуля");
    }

    // Число интервалов гистограммы тоже должно быть положительным,
    // потому что делить диапазон значений на 0 интервалов невозможно.
    if (options.histogramBins == 0)
    {
        throw std::invalid_argument("Число интервалов гистограммы должно быть больше нуля");
    }

    // Preview-count нужен для первых отображаемых наблюдений.
    // Ноль здесь приводит к бессмысленному пустому представлению.
    if (options.previewCount == 0)
    {
        throw std::invalid_argument("Число отображаемых точек должно быть больше нуля");
    }
}

// ----------------------------------------------------------------------------
// ФУНКЦИЯ ComputeCholeskyFactor
// ----------------------------------------------------------------------------
// Назначение:
// Построить нижнетреугольный фактор A такой, что:
//
// K_X = A * A^T.
//
// Именно этот шаг реализует теоретический алгоритм лабораторной работы:
// по заданной ковариационной матрице K_X получить матрицу A,
// которая затем используется в преобразовании X = A * U + m_X.
//
// Принимает:
// - covariance : теоретическую ковариационную матрицу K_X.
//
// Возвращает:
// - NumericMatrix : нижнетреугольную матрицу A из разложения Холецкого.
// ----------------------------------------------------------------------------
NumericMatrix ComputeCholeskyFactor(const NumericMatrix& covariance)
{
    // Перед началом разложения убеждаемся, что матрица вообще имеет корректную форму.
    if (!IsSquareMatrix(covariance))
    {
        throw std::invalid_argument("Ковариационная матрица должна быть квадратной");
    }

    // Размерность n определяется числом строк квадратной ковариационной матрицы.
    const std::size_t dimension = covariance.size();

    // Инициализируем фактор A как нижнетреугольную матрицу той же размерности,
    // изначально заполненную нулями.
    NumericMatrix factor(dimension, NumericVector(dimension, 0.0));

    // Проходим строка за строкой, последовательно вычисляя элементы матрицы A.
    for (std::size_t i = 0; i < dimension; ++i)
    {
        // Сначала убеждаемся, что covariance симметрична:
        // K_ij должно совпадать с K_ji.
        // Для ковариационной матрицы это обязательное свойство.
        for (std::size_t j = 0; j < dimension; ++j)
        {
            if (std::abs(covariance[i][j] - covariance[j][i]) > 1.0e-12)
            {
                throw std::invalid_argument("Ковариационная матрица должна быть симметричной");
            }
        }

        // Вычисляем диагональный элемент A_ii.
        //
        // Формула Холецкого для диагонали:
        // A_ii = sqrt(K_ii - sum_{p=0}^{i-1} A_ip^2).
        //
        // В diagonalRemainder как раз аккумулируется выражение под корнем.
        double diagonalRemainder = covariance[i][i];
        for (std::size_t p = 0; p < i; ++p)
        {
            // Вычитаем вклад уже найденных элементов текущей строки.
            diagonalRemainder -= factor[i][p] * factor[i][p];
        }

        // Если подкоренное выражение не положительно, матрица не является
        // положительно определенной, а разложение Холецкого в реальном виде невозможно.
        if (!(diagonalRemainder > 0.0))
        {
            throw std::invalid_argument("Матрица варианта не является положительно определённой");
        }

        // Берем квадратный корень и получаем диагональный элемент фактора A.
        factor[i][i] = std::sqrt(diagonalRemainder);

        // Теперь вычисляем элементы ниже диагонали в текущем столбце i:
        //
        // A_ji = (K_ji - sum_{p=0}^{i-1} A_ip * A_jp) / A_ii,  при j > i.
        for (std::size_t j = i + 1; j < dimension; ++j)
        {
            // Начинаем с соответствующего элемента ковариационной матрицы.
            double crossRemainder = covariance[j][i];
            for (std::size_t p = 0; p < i; ++p)
            {
                // Вычитаем вклад уже вычисленных столбцов.
                crossRemainder -= factor[i][p] * factor[j][p];
            }

            // Делим на диагональный элемент A_ii и получаем A_ji.
            factor[j][i] = crossRemainder / factor[i][i];
        }
    }

    // В результате factor содержит именно нижнетреугольную матрицу A,
    // которая удовлетворяет связи K_X = A * A^T.
    return factor;
}

// ----------------------------------------------------------------------------
// ФУНКЦИЯ MultiplyLowerTriangular
// ----------------------------------------------------------------------------
// Назначение:
// Умножить нижнетреугольную матрицу A на вектор U.
//
// В контексте лабораторной это первая часть формулы:
//
// X = A * U + m_X.
//
// Принимает:
// - factor : нижнетреугольную матрицу A;
// - vector : независимый вектор U.
//
// Возвращает:
// - NumericVector : результат произведения A * U.
// ----------------------------------------------------------------------------
NumericVector MultiplyLowerTriangular(const NumericMatrix& factor, const NumericVector& vector)
{
    // Размерность результата совпадает с числом строк матрицы A.
    const std::size_t dimension = factor.size();

    // Инициализируем вектор результата нулями.
    NumericVector result(dimension, 0.0);

    // Для каждой строки считаем скалярное произведение этой строки
    // на соответствующую часть вектора U.
    for (std::size_t row = 0; row < dimension; ++row)
    {
        // Так как матрица нижнетреугольная, элементы правее диагонали равны нулю,
        // поэтому достаточно суммировать только column <= row.
        for (std::size_t column = 0; column <= row; ++column)
        {
            // Формула обычного матрично-векторного умножения:
            // result[row] += A[row][column] * U[column].
            result[row] += factor[row][column] * vector[column];
        }
    }

    // Возвращаем вектор A * U.
    return result;
}

// ----------------------------------------------------------------------------
// ФУНКЦИЯ AddVectors
// ----------------------------------------------------------------------------
// Назначение:
// Сложить два вектора покомпонентно.
//
// В лабораторной эта операция реализует добавление математического ожидания:
//
// X = A * U + m_X.
//
// Принимает:
// - left  : левый вектор;
// - right : правый вектор.
//
// Возвращает:
// - NumericVector : покомпонентную сумму двух векторов.
// ----------------------------------------------------------------------------
NumericVector AddVectors(const NumericVector& left, const NumericVector& right)
{
    // Складывать можно только векторы одинаковой размерности.
    if (left.size() != right.size())
    {
        throw std::invalid_argument("Размерности векторов не совпадают");
    }

    // Подготавливаем вектор результата.
    NumericVector result(left.size(), 0.0);

    // Выполняем обычное покомпонентное сложение:
    // result[i] = left[i] + right[i].
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

    // Размерность случайного вектора определяется длиной первой строки выборки.
    const std::size_t dimension = samples.front().size();

    // Инициализируем накопитель среднего значениями 0.
    NumericVector mean(dimension, 0.0);

    // Суммируем все наблюдения покомпонентно.
    for (const NumericVector& sample : samples)
    {
        if (sample.size() != dimension)
        {
            throw std::invalid_argument("Все сгенерированные векторы должны иметь одинаковую размерность");
        }

        for (std::size_t index = 0; index < dimension; ++index)
        {
            // Накапливаем сумму по формуле:
            // sum_i += x_i^(k), где k - номер наблюдения.
            mean[index] += sample[index];
        }
    }

    // Делитель в формуле среднего - размер выборки N.
    const double count = static_cast<double>(samples.size());
    for (double& value : mean)
    {
        // Получаем выборочную оценку математического ожидания:
        // m^ = (1 / N) * sum_{k=1}^{N} x^(k).
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

    // Размерность ковариационной матрицы совпадает с размерностью вектора среднего.
    const std::size_t dimension = mean.size();

    // Подготавливаем матрицу накопления значений ковариации.
    NumericMatrix covariance(dimension, NumericVector(dimension, 0.0));

    // Строим выборочную ковариацию по формуле:
    //
    // K^ = (1 / N) * sum_{k=1}^{N} (x^(k) - m^) * (x^(k) - m^)^T.
    //
    // Ниже эта формула реализуется покомпонентно.
    for (const NumericVector& sample : samples)
    {
        for (std::size_t row = 0; row < dimension; ++row)
        {
            // Отклонение row-компоненты текущего наблюдения от выборочного среднего.
            const double rowDelta = sample[row] - mean[row];
            for (std::size_t column = 0; column < dimension; ++column)
            {
                // Прибавляем произведение отклонений двух компонент:
                // (x_row^(k) - m^_row) * (x_col^(k) - m^_col).
                covariance[row][column] += rowDelta * (sample[column] - mean[column]);
            }
        }
    }

    // В конце делим все накопленные суммы на размер выборки N.
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

    // Корреляционная матрица R строится из ковариационной K по формуле:
    //
    // R_ij = K_ij / sqrt(K_ii * K_jj).
    //
    // То есть мы нормируем ковариацию на стандартные отклонения компонент.
    for (std::size_t row = 0; row < dimension; ++row)
    {
        for (std::size_t column = 0; column < dimension; ++column)
        {
            const double denominator =
                std::sqrt(std::max(0.0, covariance[row][row]) * std::max(0.0, covariance[column][column]));

            if (denominator > 0.0)
            {
                // Если знаменатель корректен, нормируем ковариацию
                // и получаем безразмерный коэффициент корреляции.
                correlation[row][column] = covariance[row][column] / denominator;
            }

            if (row == column)
            {
                // На диагонали корреляционной матрицы всегда стоят единицы:
                // каждая величина полностью коррелирована сама с собой.
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
        // Из каждой строки выборки выбираем одну фиксированную компоненту,
        // то есть фактически извлекаем столбец матрицы samples.
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

    // Выборочное среднее считаем по формуле:
    // m^ = (1 / N) * sum x_k.
    const double sum = std::accumulate(values.begin(), values.end(), 0.0);
    statistics.empiricalMean = sum / static_cast<double>(values.size());

    // Далее считаем выборочную дисперсию как среднее квадрата отклонений:
    // D^ = (1 / N) * sum (x_k - m^)^2.
    double varianceAccumulator = 0.0;
    for (double value : values)
    {
        // delta - отклонение текущего наблюдения от выборочного среднего.
        const double delta = value - statistics.empiricalMean;

        // В аккумулятор дисперсии идет квадрат этого отклонения.
        varianceAccumulator += delta * delta;
    }

    // Получаем саму дисперсию делением на N.
    statistics.empiricalVariance = varianceAccumulator / static_cast<double>(values.size());

    // Стандартное отклонение - это корень из дисперсии.
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

    // Ширина одного интервала гистограммы:
    // h = (max - min) / M,
    // где M - число интервалов.
    const double width = (maximum - minimum) / static_cast<double>(histogramBins);
    std::vector<HistogramBin> bins(histogramBins);

    for (std::size_t index = 0; index < histogramBins; ++index)
    {
        // Левая граница i-го интервала: min + i * h.
        bins[index].left = minimum + static_cast<double>(index) * width;

        // Правая граница - левая плюс ширина интервала.
        bins[index].right = bins[index].left + width;
    }

    // Разносим все наблюдения по интервалам гистограммы.
    for (double value : values)
    {
        // По умолчанию отправляем значение в последний интервал.
        // Это нужно для корректного включения правой границы max.
        std::size_t index = histogramBins - 1;
        if (value < maximum)
        {
            // Нормируем значение относительно общего диапазона
            // и переводим его в индекс интервала.
            const double normalized = (value - minimum) / width;
            index = std::clamp<std::size_t>(static_cast<std::size_t>(normalized), 0, histogramBins - 1);
        }

        // Увеличиваем число попаданий в найденный интервал.
        bins[index].count += 1;
    }

    // Плотность по интервалу считаем не как "голую" частоту count / N,
    // а как гистограммную плотность:
    //
    // density = count / (N * h),
    //
    // чтобы площадь всех столбиков приближенно давала 1
    // и гистограмму можно было корректно сравнивать с теоретической плотностью.
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

    // Ошибку среднего берем в равномерной норме:
    // max_i |m^_i - m_i|.
    for (std::size_t index = 0; index < theoreticalMean.size(); ++index)
    {
        summary.maxMeanError =
            std::max(summary.maxMeanError, std::abs(empiricalMean[index] - theoreticalMean[index]));
    }

    // Для матрицы ковариации считаем сразу две метрики:
    // 1) максимальную покомпонентную ошибку;
    // 2) норму Фробениуса.
    double frobeniusAccumulator = 0.0;
    for (std::size_t row = 0; row < theoreticalCovariance.size(); ++row)
    {
        for (std::size_t column = 0; column < theoreticalCovariance[row].size(); ++column)
        {
            const double delta =
                empiricalCovariance[row][column] - theoreticalCovariance[row][column];

            // Максимальная ошибка - это максимум абсолютных отклонений по всем элементам.
            summary.maxCovarianceError = std::max(summary.maxCovarianceError, std::abs(delta));

            // Для нормы Фробениуса накапливаем сумму квадратов отклонений:
            // ||K^ - K||_F = sqrt(sum_{i,j} delta_{ij}^2).
            frobeniusAccumulator += delta * delta;
        }
    }

    // Завершаем вычисление нормы Фробениуса квадратным корнем.
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
        // Сначала отсекаем некорректные параметры пользователя.
        ValidateOptions(options);

        // Загружаем теоретические характеристики варианта:
        // m_X и K_X.
        const NumericVector theoreticalMean = Lab2Variant::MeanVector();
        const NumericMatrix theoreticalCovariance = Lab2Variant::CovarianceMatrix();

        // Строим матрицу A из разложения Холецкого:
        // K_X = A * A^T.
        const NumericMatrix choleskyFactor = ComputeCholeskyFactor(theoreticalCovariance);

        // Размерность случайного вектора равна размерности теоретического среднего.
        const std::size_t dimension = theoreticalMean.size();

        // Preview не может быть длиннее всей выборки.
        result.options.previewCount = std::min(options.previewCount, options.sampleSize);

        // Генератор псевдослучайных чисел и стандартное нормальное распределение
        // задают независимые компоненты U ~ N(0, I).
        std::mt19937_64 generator(options.seed);
        std::normal_distribution<double> standardNormal(0.0, 1.0);

        // Сразу резервируем память под основные контейнеры результата.
        result.samples.reserve(options.sampleSize);
        result.independentPreviewSamples.reserve(result.options.previewCount);
        result.transformedPreviewSamples.reserve(result.options.previewCount);

        // Основной цикл моделирования: одно прохождение = одно наблюдение случайного вектора X.
        for (std::size_t sampleIndex = 0; sampleIndex < options.sampleSize; ++sampleIndex)
        {
            // Инициализируем независимый вектор U нулями.
            NumericVector independentVector(dimension, 0.0);

            // Каждую компоненту U_i генерируем как независимую N(0, 1).
            for (double& value : independentVector)
            {
                value = standardNormal(generator);
            }

            // Применяем теоретическую формулу лабораторной:
            //
            // X = A * U + m_X.
            //
            // Так из независимого стандартного вектора U
            // получаем вектор X с нужным средним и ковариацией.
            const NumericVector correlatedVector =
                AddVectors(MultiplyLowerTriangular(choleskyFactor, independentVector), theoreticalMean);

            // Сохраняем полное наблюдение X в основную выборку.
            result.samples.push_back(correlatedVector);

            // Первые previewCount наблюдений отдельно сохраняем для GUI,
            // чтобы потом можно было показать конкретные первые U и X в интерфейсе.
            if (sampleIndex < result.options.previewCount)
            {
                result.independentPreviewSamples.push_back(independentVector);
                result.transformedPreviewSamples.push_back(correlatedVector);
            }
        }

        // Сохраняем теоретические матрицы и векторы, чтобы GUI мог сравнивать
        // теорию и эксперимент в одном месте.
        result.theoreticalMean = theoreticalMean;
        result.theoreticalCovariance = theoreticalCovariance;
        result.theoreticalCorrelation = ComputeCorrelationMatrix(theoreticalCovariance);
        result.choleskyFactor = choleskyFactor;

        // Строим выборочные оценки по уже сгенерированной выборке X.
        result.empiricalMean = ComputeEmpiricalMean(result.samples);
        result.empiricalCovariance = ComputeEmpiricalCovariance(result.samples, result.empiricalMean);
        result.empiricalCorrelation = ComputeCorrelationMatrix(result.empiricalCovariance);

        // Для каждой компоненты отдельно считаем статистики и строим гистограмму.
        for (std::size_t componentIndex = 0; componentIndex < dimension; ++componentIndex)
        {
            // Извлекаем один столбец выборки, то есть все значения фиксированной компоненты X_i.
            const NumericVector componentValues = ExtractComponent(result.samples, componentIndex);

            // Теоретическая дисперсия компоненты лежит на диагонали K_X.
            const double theoreticalVariance = theoreticalCovariance[componentIndex][componentIndex];

            // Сохраняем выборочные статистики компоненты.
            result.componentStatistics.push_back(BuildComponentStatistics(componentValues,
                                                                          componentIndex,
                                                                          theoreticalMean[componentIndex],
                                                                          theoreticalVariance));

            // Формируем гистограмму той же компоненты для последующего сравнения
            // с теоретической нормальной плотностью.
            ComponentHistogram histogram;
            histogram.componentIndex = componentIndex;
            histogram.theoreticalMean = theoreticalMean[componentIndex];
            histogram.theoreticalVariance = theoreticalVariance;
            histogram.bins = BuildHistogram(componentValues, options.histogramBins);
            result.histograms.push_back(std::move(histogram));
        }

        // Сводим теоретические и эмпирические характеристики в итоговый summary-блок.
        result.summary = BuildSummary(result.theoreticalMean,
                                      result.theoreticalCovariance,
                                      result.empiricalMean,
                                      result.empiricalCovariance,
                                      options.sampleSize);

        // Формируем короткое HTML-совместимое сообщение для GUI.
        std::ostringstream message;
        message << "Вариант 1 смоделирован: n=" << options.sampleSize
                << ", матрица A получена разложением Холецкого, "
                << "эмпирическая K<sub>X</sub> рассчитана по сформированной выборке.";

        result.message = message.str();
        result.success = true;
    }
    catch (const std::exception& error)
    {
        // Любая ошибка в валидации или вычислениях переводит результат в fail-состояние,
        // а текст исключения показывается пользователю как причина неудачи.
        result.success = false;
        result.message = error.what();
    }

    return result;
}
