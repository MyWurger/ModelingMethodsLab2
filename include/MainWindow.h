// ============================================================================
// ФАЙЛ MAINWINDOW.H - ОБЪЯВЛЕНИЕ ГЛАВНОГО ОКНА LAB_2
// ============================================================================

#pragma once

#include "Lab2Variant.h"
#include "PlotChartWidget.h"
#include "SamplingTypes.h"
#include <QMainWindow>
#include <array>

QT_BEGIN_NAMESPACE
class QAbstractTableModel;
class QFutureWatcherBase;
template <typename T>
class QFutureWatcher;
class QLabel;
class QPushButton;
class QSpinBox;
class QTabWidget;
class QTableView;
class QWidget;
QT_END_NAMESPACE

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    void BuildUi();
    void ApplyTheme();
    void SetupSignals();
    void ResetOutputs();
    void RunSampling();
    bool ValidateInput(QString& errorMessage) const;
    void ApplySamplingRunningState(bool running);
    void FinishSampling(const SamplingResult& result);
    void PopulateSummary(const SamplingResult& result);
    void PopulateMatrices(const SamplingResult& result);
    void PopulatePreviewTable(const SamplingResult& result);
    void PopulateSamplesTable(const SamplingResult& result);
    void UpdatePlots(const SamplingResult& result);
    void ShowStatus(const QString& text, bool success);

    static QString FormatNumber(double value);

private:
    QSpinBox* sampleSizeEdit_ = nullptr;
    QSpinBox* binsEdit_ = nullptr;
    QSpinBox* previewCountEdit_ = nullptr;
    QSpinBox* seedEdit_ = nullptr;

    QPushButton* runButton_ = nullptr;
    QLabel* statusBadge_ = nullptr;

    QLabel* dimensionValue_ = nullptr;
    QLabel* sampleCountValue_ = nullptr;
    QLabel* meanErrorValue_ = nullptr;
    QLabel* covarianceErrorValue_ = nullptr;
    QLabel* frobeniusErrorValue_ = nullptr;
    QLabel* seedValue_ = nullptr;

    QTabWidget* resultsTabs_ = nullptr;

    QAbstractTableModel* previewTableModel_ = nullptr;
    QAbstractTableModel* sampleTableModel_ = nullptr;

    QWidget* theoreticalMeanMatrix_ = nullptr;
    QWidget* theoreticalCovarianceMatrix_ = nullptr;
    QWidget* choleskyMatrix_ = nullptr;
    QWidget* empiricalMeanMatrix_ = nullptr;
    QWidget* empiricalCovarianceMatrix_ = nullptr;
    QWidget* empiricalCorrelationMatrix_ = nullptr;
    QTableView* previewTable_ = nullptr;
    QTableView* samplesTable_ = nullptr;

    std::array<PlotChartWidget*, Lab2Variant::kDimension> histogramPlots_ {};
    PlotChartWidget* sequencePlot_ = nullptr;

    SamplingResult lastResult_;
    bool hasResult_ = false;
    bool isSamplingRunning_ = false;

    QFutureWatcher<SamplingResult>* simulationWatcher_ = nullptr;
};
