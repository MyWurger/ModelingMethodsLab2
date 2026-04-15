// ============================================================================
// ФАЙЛ MAINWINDOW.H - ОБЪЯВЛЕНИЕ ГЛАВНОГО ОКНА LAB_2
// ============================================================================
// Назначение файла:
// 1) объявить класс MainWindow - центральный GUI-класс лабораторной работы;
// 2) описать публичный и приватный интерфейс главного окна;
// 3) зафиксировать набор виджетов, моделей и состояний,
//    которыми управляет интерфейс приложения;
// 4) отделить декларацию интерфейса главного окна от его реализации в .cpp.
//
// Именно MainWindow собирает воедино три основные части приложения:
// - пользовательский интерфейс Qt Widgets;
// - вычислительное ядро моделирования;
// - визуализацию результатов в таблицах, матрицах и графиках.
// ============================================================================

// Защита от повторного включения заголовочного файла.
#pragma once

// Подключаем данные варианта лабораторной.
#include "Lab2Variant.h"
// Подключаем виджет графика, который используется на вкладках результатов.
#include "PlotChartWidget.h"
// Подключаем общие типы результатов моделирования.
#include "SamplingTypes.h"

// QMainWindow - базовый класс главного окна приложения Qt Widgets.
#include <QMainWindow>
// std::array нужен для фиксированного массива гистограмм по числу компонент X.
#include <array>

QT_BEGIN_NAMESPACE
// Предварительное объявление абстрактной модели таблицы Qt.
class QAbstractTableModel;
// Предварительное объявление базового watcher-класса Qt Concurrent.
class QFutureWatcherBase;
// Предварительное объявление шаблонного QFutureWatcher для ожидания результата.
template <typename T>
class QFutureWatcher;
// Предварительное объявление QLabel для текстовых полей интерфейса.
class QLabel;
// Предварительное объявление QPushButton для кнопки запуска расчета.
class QPushButton;
// Предварительное объявление QSpinBox для числовых параметров эксперимента.
class QSpinBox;
// Предварительное объявление QTabWidget для вкладок результатов.
class QTabWidget;
// Предварительное объявление QTableView для табличного показа выборки и preview.
class QTableView;
// Предварительное объявление QWidget для универсальных контейнеров и матриц.
class QWidget;
QT_END_NAMESPACE

// ----------------------------------------------------------------------------
// КЛАСС MainWindow
// ----------------------------------------------------------------------------
// Назначение:
// Представить главное окно приложения и координировать всю работу GUI.
//
// MainWindow отвечает за:
// - построение интерфейса и настройку темы;
// - чтение пользовательских параметров моделирования;
// - запуск генерации выборки;
// - получение и обработку результатов;
// - заполнение вкладок итогов, матриц, графиков и таблиц.
//
// Это главный "оркестратор" desktop-приложения: именно он связывает
// элементы управления, асинхронный расчет и последующее отображение
// результатов пользователю.
// ----------------------------------------------------------------------------
class MainWindow final : public QMainWindow
{
    // Q_OBJECT включает поддержку метаобъектной системы Qt:
    // сигналов, слотов, runtime-информации о типе и других механизмов Qt.
    Q_OBJECT

public:
    // Конструктор главного окна.
    //
    // Принимает:
    // - parent : родительский виджет Qt.
    explicit MainWindow(QWidget* parent = nullptr);

private:
    // Строит все виджеты и раскладки главного окна.
    void BuildUi();
    // Применяет общую тему оформления ко всем основным элементам интерфейса.
    void ApplyTheme();
    // Подключает сигналы виджетов к нужным обработчикам.
    void SetupSignals();
    // Возвращает интерфейс в исходное состояние до выполнения расчета.
    void ResetOutputs();
    // Запускает процесс моделирования по текущим параметрам пользователя.
    void RunSampling();
    // Проверяет корректность введенных параметров перед запуском расчета.
    bool ValidateInput(QString& errorMessage) const;
    // Переключает интерфейс между состояниями "идет расчет" и "готов к работе".
    void ApplySamplingRunningState(bool running);
    // Завершает сценарий обработки после получения результата моделирования.
    void FinishSampling(const SamplingResult& result);
    // Заполняет вкладку сводных итогов.
    void PopulateSummary(const SamplingResult& result);
    // Заполняет вкладку матриц и матричных оценок.
    void PopulateMatrices(const SamplingResult& result);
    // Заполняет preview-таблицу первых векторов U и X.
    void PopulatePreviewTable(const SamplingResult& result);
    // Заполняет полную таблицу сгенерированной выборки.
    void PopulateSamplesTable(const SamplingResult& result);
    // Обновляет все графики: гистограммы и траектории компонент.
    void UpdatePlots(const SamplingResult& result);
    // Показывает пользователю статусное сообщение об успехе или ошибке.
    void ShowStatus(const QString& text, bool success);

    // Форматирует вещественное число для отображения в интерфейсе.
    static QString FormatNumber(double value);

private:
    // Поле ввода размера выборки N.
    QSpinBox* sampleSizeEdit_ = nullptr;
    // Поле ввода числа интервалов гистограммы.
    QSpinBox* binsEdit_ = nullptr;
    // Поле ввода количества первых точек/наблюдений для preview.
    QSpinBox* previewCountEdit_ = nullptr;
    // Поле ввода seed генератора случайных чисел.
    QSpinBox* seedEdit_ = nullptr;

    // Кнопка запуска моделирования.
    QPushButton* runButton_ = nullptr;
    // Текстовый статусный бейдж под кнопкой запуска.
    QLabel* statusBadge_ = nullptr;

    // Значение размерности на вкладке итогов.
    QLabel* dimensionValue_ = nullptr;
    // Значение фактического размера выборки на вкладке итогов.
    QLabel* sampleCountValue_ = nullptr;
    // Числовое отображение ошибки оценки среднего.
    QLabel* meanErrorValue_ = nullptr;
    // Числовое отображение ошибки оценки ковариационной матрицы.
    QLabel* covarianceErrorValue_ = nullptr;
    // Значение нормы отклонения матрицы.
    QLabel* frobeniusErrorValue_ = nullptr;
    // Отображение использованного seed.
    QLabel* seedValue_ = nullptr;

    // Набор вкладок со всеми результатами моделирования.
    QTabWidget* resultsTabs_ = nullptr;

    // Модель таблицы preview-данных.
    QAbstractTableModel* previewTableModel_ = nullptr;
    // Модель таблицы полной выборки.
    QAbstractTableModel* sampleTableModel_ = nullptr;

    // Виджет теоретического вектора среднего.
    QWidget* theoreticalMeanMatrix_ = nullptr;
    // Виджет теоретической матрицы ковариации.
    QWidget* theoreticalCovarianceMatrix_ = nullptr;
    // Виджет фактора Холецкого A.
    QWidget* choleskyMatrix_ = nullptr;
    // Виджет выборочной оценки среднего.
    QWidget* empiricalMeanMatrix_ = nullptr;
    // Виджет выборочной оценки ковариационной матрицы.
    QWidget* empiricalCovarianceMatrix_ = nullptr;
    // Виджет выборочной матрицы корреляции.
    QWidget* empiricalCorrelationMatrix_ = nullptr;
    // Таблица первых векторов U и X.
    QTableView* previewTable_ = nullptr;
    // Таблица всей сгенерированной выборки X.
    QTableView* samplesTable_ = nullptr;

    // Набор графиков-гистограмм по одной для каждой компоненты X.
    std::array<PlotChartWidget*, Lab2Variant::kDimension> histogramPlots_ {};
    // График траекторий первых наблюдений компонент X.
    PlotChartWidget* sequencePlot_ = nullptr;

    // Последний успешно полученный результат моделирования.
    SamplingResult lastResult_;
    // Признак того, что в окне уже есть валидный результат расчета.
    bool hasResult_ = false;
    // Признак того, что расчет сейчас выполняется.
    bool isSamplingRunning_ = false;

    // Watcher, который отслеживает завершение фонового моделирования
    // и возвращает результат обратно в GUI-поток.
    QFutureWatcher<SamplingResult>* simulationWatcher_ = nullptr;
};
