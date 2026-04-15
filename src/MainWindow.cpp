// ============================================================================
// ФАЙЛ MAINWINDOW.CPP - РЕАЛИЗАЦИЯ ГЛАВНОГО ОКНА LAB_2
// ============================================================================

#include "MainWindow.h"
#include "MathWidgets.h"
#include "PresentationBuilders.h"
#include "SamplingEngine.h"
#include "TableExport.h"
#include "TableModels.h"
#include "UiFormatting.h"

#include <QAbstractItemView>
#include <QFormLayout>
#include <QFrame>
#include <QFutureWatcher>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QPalette>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSpinBox>
#include <QSplitter>
#include <QStyle>
#include <QTabWidget>
#include <QTableView>
#include <QVBoxLayout>
#include <QWidget>

#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <array>
#include <limits>

namespace
{
constexpr int kMaxRecommendedSampleSize = 500000;
constexpr int kMaxRecommendedHistogramBins = 100;
constexpr int kMaxRecommendedPreviewCount = 3000;

template <typename TWidget>
void ApplyInputPalette(TWidget* widget)
{
    QPalette palette = widget->palette();
    palette.setColor(QPalette::Base, QColor("#ffffff"));
    palette.setColor(QPalette::Text, QColor("#102033"));
    palette.setColor(QPalette::WindowText, QColor("#102033"));
    palette.setColor(QPalette::ButtonText, QColor("#102033"));
    palette.setColor(QPalette::Highlight, QColor("#2a6df4"));
    palette.setColor(QPalette::HighlightedText, QColor("#ffffff"));
    palette.setColor(QPalette::PlaceholderText, QColor("#7a8a9c"));
    widget->setPalette(palette);
}

QWidget* CreateSpinField(QSpinBox*& spinBox, int initialValue, const QString& placeholder, QWidget* parent)
{
    auto* container = new QWidget(parent);
    container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto* layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    spinBox = new IntegerSpinBox(container);
    spinBox->setMinimumWidth(118);
    spinBox->setMinimumHeight(42);
    spinBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    spinBox->setValue(initialValue);
    static_cast<IntegerSpinBox*>(spinBox)->SetPlaceholderText(placeholder);

    auto* buttonsHost = new QWidget(container);
    buttonsHost->setObjectName("spinButtonColumn");
    buttonsHost->setFixedWidth(30);
    buttonsHost->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    auto* buttonsLayout = new QVBoxLayout(buttonsHost);
    buttonsLayout->setContentsMargins(0, 0, 0, 0);
    buttonsLayout->setSpacing(4);

    auto* plusButton = new QPushButton("+", buttonsHost);
    auto* minusButton = new QPushButton("-", buttonsHost);
    plusButton->setObjectName("spinAdjustButton");
    minusButton->setObjectName("spinAdjustButton");
    plusButton->setFixedSize(30, 18);
    minusButton->setFixedSize(30, 18);

    QObject::connect(plusButton, &QPushButton::clicked, spinBox, [spinBox]() { spinBox->stepUp(); });
    QObject::connect(minusButton, &QPushButton::clicked, spinBox, [spinBox]() { spinBox->stepDown(); });

    buttonsLayout->addWidget(plusButton);
    buttonsLayout->addWidget(minusButton);
    buttonsLayout->addStretch();

    layout->addWidget(spinBox, 1);
    layout->addWidget(buttonsHost, 0, Qt::AlignVCenter);
    return container;
}

void SetFieldContainerEnabled(QWidget* field, bool enabled)
{
    if (field == nullptr)
    {
        return;
    }

    if (auto* container = field->parentWidget())
    {
        container->setEnabled(enabled);
    }
    else
    {
        field->setEnabled(enabled);
    }
}

void ConfigureMatrixTable(QTableView* table)
{
    table->setAlternatingRowColors(true);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setWordWrap(false);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);
    table->verticalHeader()->setDefaultAlignment(Qt::AlignCenter);
    table->setMinimumHeight(180);
}

void ConfigureSampleTable(QTableView* table)
{
    ConfigureMatrixTable(table);
    table->verticalHeader()->setVisible(false);
    table->setMinimumHeight(420);
}

QWidget* CreateCardWithTable(const QString& title, QTableView* table, QWidget* parent)
{
    auto* card = new QFrame(parent);
    card->setObjectName("plotCard");

    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    auto* titleLabel = new QLabel(title, card);
    titleLabel->setObjectName("tableTitle");
    titleLabel->setTextFormat(Qt::RichText);
    titleLabel->setWordWrap(false);
    titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    auto* headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(10);

    auto* exportButton = new QPushButton("Сохранить PNG", card);
    exportButton->setObjectName("secondaryButton");
    exportButton->setMinimumHeight(32);
    exportButton->setMinimumWidth(148);
    exportButton->setToolTip("Сохранить эту таблицу в PNG");

    QObject::connect(exportButton, &QPushButton::clicked, card, [card, table, title]() {
        ExportTableViewPng(card, table, title);
    });

    headerLayout->addWidget(titleLabel, 1);
    headerLayout->addStretch();
    headerLayout->addWidget(exportButton);

    layout->addLayout(headerLayout);
    layout->addWidget(table, 1);
    return card;
}

QWidget* CreateCardWithWidget(const QString& title,
                              QWidget* contentWidget,
                              QWidget* parent)
{
    auto* card = new QFrame(parent);
    card->setObjectName("plotCard");
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    card->setMinimumHeight(280);
    card->setMaximumHeight(280);

    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(0, 4, 0, 4);
    layout->setSpacing(0);

    auto* titleLabel = new MatrixCardTitleWidget(title, card);

    auto* headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(0);

    headerLayout->addSpacing(14);
    headerLayout->addWidget(titleLabel, 1);
    headerLayout->addSpacing(14);

    layout->addLayout(headerLayout);
    layout->addWidget(contentWidget, 1);
    return card;
}

QFrame* CreateSummaryMetricCard(const QString& title,
                                SummaryFormulaKind formulaKind,
                                QLabel*& valueLabel,
                                QWidget* parent)
{
    auto* card = new QFrame(parent);
    card->setObjectName("summaryMetricCard");
    card->setMinimumHeight(108);
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(8);

    auto* titleLabel = new QLabel(title, card);
    titleLabel->setObjectName("summaryMetricTitle");
    titleLabel->setTextFormat(Qt::RichText);

    valueLabel = new QLabel("—", card);
    valueLabel->setObjectName("summaryMetricValue");
    valueLabel->setTextFormat(Qt::PlainText);

    auto* formulaWidget = new SummaryFormulaWidget(formulaKind, card);
    formulaWidget->setObjectName("summaryMetricFormula");

    layout->addWidget(titleLabel);
    layout->addWidget(valueLabel);
    layout->addWidget(formulaWidget);
    return card;
}

} // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    simulationWatcher_ = new QFutureWatcher<SamplingResult>(this);

    BuildUi();
    ApplyTheme();
    SetupSignals();
    ResetOutputs();
    ShowStatus("Готово к моделированию варианта 1 по заданной матрице K<sub>X</sub>.", true);
}

void MainWindow::BuildUi()
{
    setWindowTitle("Лабораторная работа 2 — моделирование коррелированных случайных величин");
    resize(1600, 980);
    setMinimumSize(1360, 840);

    auto* central = new QWidget(this);
    auto* rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(10, 10, 10, 10);
    rootLayout->setSpacing(0);

    auto* splitter = new QSplitter(Qt::Horizontal, central);
    splitter->setChildrenCollapsible(false);
    splitter->setHandleWidth(4);

    auto* controlCard = new QWidget(splitter);
    controlCard->setObjectName("controlCard");
    controlCard->setMinimumWidth(440);
    controlCard->setMaximumWidth(590);

    auto* controlLayout = new QVBoxLayout(controlCard);
    controlLayout->setContentsMargins(18, 18, 18, 18);
    controlLayout->setSpacing(12);

    auto* titleLabel = new QLabel("Моделирование коррелированных случайных величин", controlCard);
    titleLabel->setObjectName("titleLabel");
    titleLabel->setWordWrap(true);

    auto* parametersGroup = new QGroupBox("Параметры эксперимента", controlCard);
    parametersGroup->setObjectName("parametersGroup");
    auto* parametersForm = new QFormLayout(parametersGroup);
    parametersForm->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    parametersForm->setFormAlignment(Qt::AlignTop);
    parametersForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    parametersForm->setRowWrapPolicy(QFormLayout::DontWrapRows);
    parametersForm->setContentsMargins(14, 18, 14, 14);
    parametersForm->setSpacing(10);
    parametersForm->setHorizontalSpacing(16);

    auto* sampleSizeField = CreateSpinField(sampleSizeEdit_, 20000, "20000", parametersGroup);
    sampleSizeField->setMinimumWidth(180);
    sampleSizeEdit_->setRange(1000, kMaxRecommendedSampleSize);
    sampleSizeEdit_->setSingleStep(5000);

    auto* binsField = CreateSpinField(binsEdit_, 24, "24", parametersGroup);
    binsField->setMinimumWidth(180);
    binsEdit_->setRange(8, kMaxRecommendedHistogramBins);
    binsEdit_->setSingleStep(4);

    auto* previewField = CreateSpinField(previewCountEdit_, 400, "400", parametersGroup);
    previewField->setMinimumWidth(180);
    previewCountEdit_->setRange(50, kMaxRecommendedPreviewCount);
    previewCountEdit_->setSingleStep(50);

    auto* seedField = CreateSpinField(seedEdit_, 20260414, "20260414", parametersGroup);
    seedField->setMinimumWidth(180);
    seedEdit_->setRange(1, std::numeric_limits<int>::max());
    seedEdit_->setSingleStep(1);

    ApplyInputPalette(sampleSizeEdit_);
    ApplyInputPalette(binsEdit_);
    ApplyInputPalette(previewCountEdit_);
    ApplyInputPalette(seedEdit_);

    parametersForm->addRow("Размер выборки N", sampleSizeField);
    parametersForm->addRow("Интервалы гистограммы", binsField);
    parametersForm->addRow("Показать первых точек", previewField);
    parametersForm->addRow("Seed генератора", seedField);

    auto* theoryGroup = new QGroupBox("Параметры варианта", controlCard);
    theoryGroup->setObjectName("theoryGroup");
    auto* theoryLayout = new QVBoxLayout(theoryGroup);
    theoryLayout->setContentsMargins(18, 22, 18, 16);
    theoryLayout->setSpacing(0);
    theoryLayout->addWidget(new VariantMathWidget(theoryGroup));

    runButton_ = new QPushButton("Выполнить моделирование", controlCard);
    runButton_->setObjectName("primaryButton");
    runButton_->setMinimumHeight(46);

    statusBadge_ = new QLabel(controlCard);
    statusBadge_->setWordWrap(true);

    controlLayout->addWidget(titleLabel);
    controlLayout->addWidget(parametersGroup);
    controlLayout->addWidget(theoryGroup);
    controlLayout->addWidget(runButton_);
    controlLayout->addWidget(statusBadge_);
    controlLayout->addStretch();

    auto* resultsCard = new QWidget(splitter);
    resultsCard->setObjectName("resultsCard");

    auto* resultsLayout = new QVBoxLayout(resultsCard);
    resultsLayout->setContentsMargins(18, 18, 18, 18);
    resultsLayout->setSpacing(12);

    auto* resultsTitle = new QLabel("Результаты моделирования", resultsCard);
    resultsTitle->setObjectName("resultsTitle");

    resultsTabs_ = new QTabWidget(resultsCard);
    resultsTabs_->setObjectName("resultsTabs");
    resultsTabs_->setMovable(true);

    auto* summaryTab = new QWidget(resultsTabs_);
    auto* summaryTabLayout = new QVBoxLayout(summaryTab);
    summaryTabLayout->setContentsMargins(0, 0, 0, 0);
    summaryTabLayout->setSpacing(0);

    auto* summaryContent = new QWidget(summaryTab);
    summaryContent->setObjectName("summaryContent");
    auto* summaryLayout = new QGridLayout(summaryContent);
    summaryLayout->setContentsMargins(0, 12, 0, 0);
    summaryLayout->setHorizontalSpacing(12);
    summaryLayout->setVerticalSpacing(12);

    auto addSummaryMetric = [summaryLayout, summaryContent](int row,
                                                            int column,
                                                            const QString& title,
                                                            SummaryFormulaKind formulaKind,
                                                            QLabel*& valueLabel) {
        summaryLayout->addWidget(CreateSummaryMetricCard(title, formulaKind, valueLabel, summaryContent), row, column);
        summaryLayout->setColumnStretch(column, 1);
    };

    addSummaryMetric(0, 0, "Размерность", SummaryFormulaKind::Dimension, dimensionValue_);
    addSummaryMetric(0, 1, "Размер выборки", SummaryFormulaKind::SampleSize, sampleCountValue_);
    addSummaryMetric(0, 2, "Seed", SummaryFormulaKind::Seed, seedValue_);
    addSummaryMetric(1, 0, "Ошибка среднего", SummaryFormulaKind::MeanError, meanErrorValue_);
    addSummaryMetric(1, 1, "Ошибка K<sub>X</sub>", SummaryFormulaKind::CovarianceError, covarianceErrorValue_);
    addSummaryMetric(1, 2, "F-норма", SummaryFormulaKind::FrobeniusError, frobeniusErrorValue_);

    summaryTabLayout->addWidget(summaryContent);
    summaryTabLayout->addStretch(1);

    auto* matricesTab = new QWidget(resultsTabs_);
    auto* matricesTabLayout = new QVBoxLayout(matricesTab);
    matricesTabLayout->setContentsMargins(0, 0, 0, 0);
    matricesTabLayout->setSpacing(0);

    auto* matricesScroll = new QScrollArea(matricesTab);
    matricesScroll->setWidgetResizable(true);
    matricesScroll->setFrameShape(QFrame::NoFrame);

    auto* matricesContent = new QWidget(matricesScroll);
    auto* matricesContentLayout = new QVBoxLayout(matricesContent);
    matricesContentLayout->setContentsMargins(0, 0, 0, 0);
    matricesContentLayout->setSpacing(0);

    auto* matricesBundleCard = new QFrame(matricesContent);
    matricesBundleCard->setObjectName("contentBundleCard");
    auto* matricesGrid = new QGridLayout(matricesBundleCard);
    matricesGrid->setContentsMargins(10, 10, 10, 10);
    matricesGrid->setHorizontalSpacing(12);
    matricesGrid->setVerticalSpacing(12);

    theoreticalMeanMatrix_ = new MatrixDisplayWidget(matricesContent);
    theoreticalCovarianceMatrix_ = new MatrixDisplayWidget(matricesContent);
    choleskyMatrix_ = new MatrixDisplayWidget(matricesContent);
    empiricalMeanMatrix_ = new MatrixDisplayWidget(matricesContent);
    empiricalCovarianceMatrix_ = new MatrixDisplayWidget(matricesContent);
    empiricalCorrelationMatrix_ = new MatrixDisplayWidget(matricesContent);
    static_cast<MatrixDisplayWidget*>(theoreticalCovarianceMatrix_)
        ->SetHorizontalPlacement(MatrixDisplayWidget::HorizontalPlacement::Center);

    auto* theoreticalMeanCard = CreateCardWithWidget(
        "Теоретический вектор m<sub>X</sub>",
        theoreticalMeanMatrix_,
        matricesContent);
    auto* theoreticalCovarianceCard = CreateCardWithWidget(
        "Теоретическая матрица K<sub>X</sub>",
        theoreticalCovarianceMatrix_,
        matricesContent);
    auto* choleskyCard = CreateCardWithWidget(
        "Фактор Холецкого A",
        choleskyMatrix_,
        matricesContent);
    auto* empiricalMeanCard = CreateCardWithWidget(
        "Выборочная оценка m&#770;<sub>X</sub>",
        empiricalMeanMatrix_,
        matricesContent);
    auto* empiricalCovarianceCard = CreateCardWithWidget(
        "Выборочная оценка K&#770;<sub>X</sub>",
        empiricalCovarianceMatrix_,
        matricesContent);
    auto* empiricalCorrelationCard = CreateCardWithWidget(
        "Выборочная корреляция R&#770;",
        empiricalCorrelationMatrix_,
        matricesContent);

    matricesGrid->addWidget(theoreticalMeanCard, 0, 0);
    matricesGrid->addWidget(theoreticalCovarianceCard, 0, 1);
    matricesGrid->addWidget(choleskyCard, 1, 0);
    matricesGrid->addWidget(empiricalMeanCard, 1, 1);
    matricesGrid->addWidget(empiricalCovarianceCard, 2, 0);
    matricesGrid->addWidget(empiricalCorrelationCard, 2, 1);
    matricesGrid->setColumnStretch(0, 1);
    matricesGrid->setColumnStretch(1, 1);

    matricesContentLayout->addWidget(matricesBundleCard);
    matricesScroll->setWidget(matricesContent);
    matricesTabLayout->addWidget(matricesScroll);

    auto* histogramsTab = new QWidget(resultsTabs_);
    auto* histogramsTabLayout = new QVBoxLayout(histogramsTab);
    histogramsTabLayout->setContentsMargins(0, 0, 0, 0);
    histogramsTabLayout->setSpacing(0);

    auto* histogramsBundleCard = new QFrame(histogramsTab);
    histogramsBundleCard->setObjectName("contentBundleCard");
    auto* histogramsGrid = new QGridLayout(histogramsBundleCard);
    histogramsGrid->setContentsMargins(10, 10, 10, 10);
    histogramsGrid->setHorizontalSpacing(10);
    histogramsGrid->setVerticalSpacing(10);

    const QStringList xHeaders = BuildComponentHeaders("x", Lab2Variant::kDimension);
    for (std::size_t index = 0; index < histogramPlots_.size(); ++index)
    {
        histogramPlots_[index] =
            new PlotChartWidget(QString("Компонента %1: гистограмма и плотность")
                                    .arg(xHeaders.value(static_cast<int>(index))),
                                "значение",
                                "плотность",
                                histogramsBundleCard);
        histogramPlots_[index]->SetBottomAxisLabelReserve(12, 4);
        histogramPlots_[index]->SetLegendVisible(false);
        histogramPlots_[index]->SetTitleAlignment(Qt::AlignHCenter);
        histogramPlots_[index]->setMinimumHeight(336);
        histogramsGrid->addWidget(histogramPlots_[index], static_cast<int>(index / 2), static_cast<int>(index % 2));
    }
    histogramsGrid->setColumnStretch(0, 1);
    histogramsGrid->setColumnStretch(1, 1);
    histogramsGrid->setRowStretch(0, 1);
    histogramsGrid->setRowStretch(1, 1);
    histogramsTabLayout->addWidget(histogramsBundleCard, 1);

    auto* trajectoryTab = new QWidget(resultsTabs_);
    auto* trajectoryLayout = new QVBoxLayout(trajectoryTab);
    trajectoryLayout->setContentsMargins(0, 0, 0, 0);
    trajectoryLayout->setSpacing(0);

    auto* trajectoryBundleCard = new QFrame(trajectoryTab);
    trajectoryBundleCard->setObjectName("contentBundleCard");
    auto* trajectoryBundleLayout = new QVBoxLayout(trajectoryBundleCard);
    trajectoryBundleLayout->setContentsMargins(14, 14, 14, 14);
    trajectoryBundleLayout->setSpacing(12);

    sequencePlot_ =
        new PlotChartWidget("Первые наблюдения компонентов X", "номер наблюдения", "значение", trajectoryBundleCard);
    sequencePlot_->setObjectName("embeddedPlotCard");
    sequencePlot_->SetBottomAxisLabelReserve(24, 8);
    sequencePlot_->SetHomeViewScale(0.32, 1.0);
    sequencePlot_->SetAutoFitYToVisibleX(false);
    sequencePlot_->SetHomeViewStartAtZero(true);
    sequencePlot_->setMinimumHeight(400);
    sequencePlot_->setMaximumHeight(480);

    auto* trajectoryPlotCard = new QFrame(trajectoryBundleCard);
    trajectoryPlotCard->setObjectName("plotCard");
    auto* trajectoryPlotLayout = new QVBoxLayout(trajectoryPlotCard);
    trajectoryPlotLayout->setContentsMargins(4, 4, 4, 0);
    trajectoryPlotLayout->setSpacing(0);
    trajectoryPlotLayout->addWidget(sequencePlot_);

    auto* trajectoryBottomBorder = new QFrame(trajectoryPlotCard);
    trajectoryBottomBorder->setObjectName("trajectoryCardBottomBorder");
    trajectoryBottomBorder->setFixedHeight(1);
    auto* trajectoryBottomBorderHost = new QWidget(trajectoryPlotCard);
    auto* trajectoryBottomBorderLayout = new QHBoxLayout(trajectoryBottomBorderHost);
    trajectoryBottomBorderLayout->setContentsMargins(5, 0, 5, 0);
    trajectoryBottomBorderLayout->setSpacing(0);
    trajectoryBottomBorderLayout->addWidget(trajectoryBottomBorder);
    trajectoryPlotLayout->addSpacing(0);
    trajectoryPlotLayout->addWidget(trajectoryBottomBorderHost);
    trajectoryBundleLayout->addWidget(trajectoryPlotCard, 1);

    previewTableModel_ = new NumericMatrixModel(this);
    previewTable_ = new QTableView(trajectoryBundleCard);
    previewTable_->setModel(previewTableModel_);
    ConfigureSampleTable(previewTable_);
    previewTable_->setMinimumHeight(300);
    previewTable_->setMaximumHeight(360);
    auto* previewTableCard = CreateCardWithTable("Первые векторы U и X", previewTable_, trajectoryBundleCard);
    previewTableCard->setMinimumHeight(320);
    previewTableCard->setMaximumHeight(400);
    trajectoryBundleLayout->addWidget(previewTableCard, 1);

    trajectoryLayout->addWidget(trajectoryBundleCard, 1);

    auto* samplesTab = new QWidget(resultsTabs_);
    auto* samplesLayout = new QVBoxLayout(samplesTab);
    samplesLayout->setContentsMargins(0, 0, 0, 0);

    sampleTableModel_ = new SampleTableModel(this);
    samplesTable_ = new QTableView(samplesTab);
    samplesTable_->setModel(sampleTableModel_);
    ConfigureSampleTable(samplesTable_);
    samplesLayout->addWidget(CreateCardWithTable("Сгенерированная выборка X", samplesTable_, samplesTab));

    resultsTabs_->addTab(summaryTab, "Итоги");
    resultsTabs_->addTab(matricesTab, "Матрицы");
    resultsTabs_->addTab(histogramsTab, "Гистограммы");
    resultsTabs_->addTab(trajectoryTab, "Траектории");
    resultsTabs_->addTab(samplesTab, "Выборка");

    resultsLayout->addWidget(resultsTitle);
    resultsLayout->addWidget(resultsTabs_, 1);

    splitter->addWidget(controlCard);
    splitter->addWidget(resultsCard);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({480, 1080});

    rootLayout->addWidget(splitter);
    setCentralWidget(central);
}

void MainWindow::ApplyTheme()
{
    setStyleSheet(R"(
        QMainWindow {
            background: #eef3f8;
        }

        QWidget#controlCard, QWidget#resultsCard {
            background: #fbfdff;
            border: 1px solid #d8e2ed;
            border-radius: 18px;
        }

        QSplitter::handle {
            background: #dbe6f2;
            margin: 20px 0;
            border-radius: 2px;
        }

        QSplitter::handle:hover {
            background: #c5d5e6;
        }

        QLabel {
            color: #102033;
            font-size: 15px;
        }

        QLabel#titleLabel, QLabel#resultsTitle {
            color: #102033;
            font-size: 24px;
            font-weight: 700;
        }

        QLabel#tableTitle {
            color: #102033;
            font-size: 18px;
            font-weight: 700;
        }

        QFrame#summaryMetricCard {
            border: 1px solid #dbe5ef;
            border-radius: 16px;
            background: #fbfdff;
        }

        QLabel#summaryMetricTitle {
            color: #496073;
            font-size: 14px;
            font-weight: 500;
        }

        QLabel#summaryMetricValue {
            color: #102033;
            font-size: 24px;
            font-weight: 500;
        }

        QLabel#summaryMetricFormula {
            color: #718394;
            font-size: 14px;
            font-weight: 500;
        }

        QLabel#summaryCaption {
            color: #4d647a;
            font-size: 15px;
            font-weight: 600;
        }

        QLabel#summaryValue {
            color: #102033;
            font-size: 15px;
            font-weight: 700;
        }

        QFrame#plotCard {
            border: 1px solid #d9e2ec;
            border-radius: 16px;
            background: #ffffff;
        }

        QFrame#trajectoryCardBottomBorder {
            background: #d9e2ec;
            border: none;
            min-height: 1px;
            max-height: 1px;
        }

        QFrame#embeddedPlotCard {
            border: none;
            border-radius: 0px;
            background: transparent;
        }

        QFrame#contentBundleCard {
            border: 1px solid #d8e2ed;
            border-radius: 20px;
            background: #fbfdff;
        }

        QLabel#plotTitle {
            color: #102033;
            font-size: 16px;
            font-weight: 700;
        }

        QChartView#plotChartView {
            background: transparent;
            border: none;
        }

        QGroupBox {
            color: #102033;
            font-size: 14px;
            font-weight: 700;
            border: 1px solid #d9e2ec;
            border-radius: 14px;
            margin-top: 12px;
            background: #ffffff;
        }

        QGroupBox::title {
            subcontrol-origin: margin;
            left: 12px;
            padding: 0 6px;
        }

        QGroupBox#parametersGroup,
        QGroupBox#theoryGroup {
            margin-top: 18px;
        }

        QGroupBox#parametersGroup::title,
        QGroupBox#theoryGroup::title {
            color: #17324d;
            font-size: 26px;
            font-weight: 800;
            left: 16px;
            padding: 0 10px 2px 10px;
        }

        QGroupBox#parametersGroup QLabel {
            font-size: 15px;
            font-weight: 600;
            color: #20384f;
        }

        QLabel#theoryKey {
            color: #294560;
            font-size: 15px;
            font-weight: 700;
        }

        QLabel#theoryValue {
            color: #102033;
            font-size: 15px;
            font-weight: 500;
        }

        QLineEdit, QSpinBox {
            font-size: 15px;
            min-height: 36px;
            border: 1px solid #c8d4e2;
            border-radius: 10px;
            padding: 0 10px;
            background: #ffffff;
            color: #102033;
            selection-background-color: #2a6df4;
            selection-color: #ffffff;
        }

        QLineEdit:focus, QSpinBox:focus {
            border: 1px solid #2a6df4;
        }

        QPushButton#spinAdjustButton {
            background: #f7fbff;
            color: #1e3655;
            border: 1px solid #d7e2ee;
            border-radius: 8px;
            font-size: 14px;
            font-weight: 700;
            padding: 0;
        }

        QPushButton#spinAdjustButton:hover {
            background: #edf5ff;
            border-color: #bdd0e6;
        }

        QPushButton#spinAdjustButton:pressed {
            background: #dfeaf8;
        }

        QPushButton#primaryButton {
            background: #2a6df4;
            color: #ffffff;
            border: none;
            border-radius: 12px;
            font-size: 15px;
            font-weight: 700;
            padding: 0 16px;
        }

        QPushButton#primaryButton:hover {
            background: #1f61e4;
        }

        QPushButton#primaryButton:pressed {
            background: #174fc4;
        }

        QPushButton#primaryButton:disabled {
            background: #b2c8f2;
            color: #eaf1ff;
        }

        QPushButton#secondaryButton {
            background: #ffffff;
            color: #102033;
            border: 1px solid #102033;
            border-radius: 10px;
            font-size: 13px;
            font-weight: 700;
            padding: 0 18px;
        }

        QPushButton#secondaryButton:hover:enabled {
            background: #102033;
            color: #ffffff;
            border-color: #102033;
        }

        QPushButton#secondaryButton:pressed:enabled {
            background: #26394c;
            color: #ffffff;
        }

        QScrollArea {
            border: none;
            background: transparent;
        }

        QScrollArea > QWidget > QWidget {
            background: transparent;
        }

        QTabWidget#resultsTabs::pane {
            border: none;
            top: -1px;
            background: transparent;
        }

        QTabWidget#resultsTabs::tab-bar {
            left: 14px;
        }

        QTabWidget#resultsTabs QTabBar::tab {
            background: #eef3f8;
            color: #46627d;
            border: 1px solid #d8e2ed;
            border-bottom: none;
            padding: 11px 18px;
            margin-right: 6px;
            min-width: 120px;
            border-top-left-radius: 12px;
            border-top-right-radius: 12px;
            font-size: 15px;
            font-weight: 700;
        }

        QTabWidget#resultsTabs QTabBar::tab:hover {
            background: #e8f0f8;
            color: #284560;
        }

        QTabWidget#resultsTabs QTabBar::tab:selected {
            background: #ffffff;
            color: #102033;
            margin-bottom: -1px;
        }

        QTableView {
            border: 1px solid #d9e2ec;
            border-radius: 12px;
            gridline-color: #e5edf5;
            color: #102033;
            selection-background-color: #dce8ff;
            selection-color: #102033;
            alternate-background-color: #f7fbff;
            background: #ffffff;
            font-size: 15px;
        }

        QHeaderView {
            background: transparent;
            border: none;
        }

        QHeaderView::section {
            background: #f3f7fb;
            color: #102033;
            border: none;
            border-right: 1px solid #d9e2ec;
            border-bottom: 1px solid #d9e2ec;
            padding: 8px;
            font-size: 15px;
            font-weight: 700;
        }

        QHeaderView::section:horizontal:first {
            border-top-left-radius: 11px;
        }

        QHeaderView::section:horizontal:last {
            border-top-right-radius: 11px;
            border-right: none;
        }

        QHeaderView::section:vertical {
            border-right: 1px solid #d9e2ec;
            border-bottom: 1px solid #d9e2ec;
        }

        QTableCornerButton::section {
            background: #f3f7fb;
            border: none;
            border-top-left-radius: 11px;
            border-right: 1px solid #d9e2ec;
            border-bottom: 1px solid #d9e2ec;
        }

        QScrollBar:vertical {
            background: #eef4fb;
            width: 12px;
            margin: 10px 2px 10px 2px;
            border-radius: 6px;
        }

        QScrollBar::handle:vertical {
            background: #c7d7e8;
            min-height: 36px;
            border-radius: 6px;
        }

        QScrollBar::handle:vertical:hover {
            background: #b4c9de;
        }

        QScrollBar::add-line:vertical,
        QScrollBar::sub-line:vertical {
            height: 0px;
            background: transparent;
            border: none;
        }

        QScrollBar::add-page:vertical,
        QScrollBar::sub-page:vertical {
            background: transparent;
        }

        QScrollBar:horizontal {
            background: #eef4fb;
            height: 12px;
            margin: 2px 10px 2px 10px;
            border-radius: 6px;
        }

        QScrollBar::handle:horizontal {
            background: #c7d7e8;
            min-width: 36px;
            border-radius: 6px;
        }

        QScrollBar::handle:horizontal:hover {
            background: #b4c9de;
        }

        QScrollBar::add-line:horizontal,
        QScrollBar::sub-line:horizontal {
            width: 0px;
            background: transparent;
            border: none;
        }

        QScrollBar::add-page:horizontal,
        QScrollBar::sub-page:horizontal {
            background: transparent;
        }
    )");
}

void MainWindow::SetupSignals()
{
    connect(runButton_, &QPushButton::clicked, this, &MainWindow::RunSampling);
    connect(simulationWatcher_, &QFutureWatcher<SamplingResult>::finished, this, [this]() {
        FinishSampling(simulationWatcher_->result());
    });
}

void MainWindow::ResetOutputs()
{
    dimensionValue_->setText("—");
    sampleCountValue_->setText("—");
    meanErrorValue_->setText("—");
    covarianceErrorValue_->setText("—");
    frobeniusErrorValue_->setText("—");
    seedValue_->setText("—");

    if (theoreticalMeanMatrix_ != nullptr)
    {
        static_cast<MatrixDisplayWidget*>(theoreticalMeanMatrix_)->SetMatrixData({}, {});
    }
    if (theoreticalCovarianceMatrix_ != nullptr)
    {
        static_cast<MatrixDisplayWidget*>(theoreticalCovarianceMatrix_)->SetMatrixData({}, {});
    }
    if (choleskyMatrix_ != nullptr)
    {
        static_cast<MatrixDisplayWidget*>(choleskyMatrix_)->SetMatrixData({}, {});
    }
    if (empiricalMeanMatrix_ != nullptr)
    {
        static_cast<MatrixDisplayWidget*>(empiricalMeanMatrix_)->SetMatrixData({}, {});
    }
    if (empiricalCovarianceMatrix_ != nullptr)
    {
        static_cast<MatrixDisplayWidget*>(empiricalCovarianceMatrix_)->SetMatrixData({}, {});
    }
    if (empiricalCorrelationMatrix_ != nullptr)
    {
        static_cast<MatrixDisplayWidget*>(empiricalCorrelationMatrix_)->SetMatrixData({}, {});
    }
    static_cast<NumericMatrixModel*>(previewTableModel_)->SetMatrixData({}, {});
    static_cast<SampleTableModel*>(sampleTableModel_)->SetSamples(nullptr);

    for (PlotChartWidget* plot : histogramPlots_)
    {
        if (plot != nullptr)
        {
            plot->Clear();
        }
    }

    if (sequencePlot_ != nullptr)
    {
        sequencePlot_->Clear();
    }

    hasResult_ = false;
    lastResult_ = SamplingResult {};
}

void MainWindow::RunSampling()
{
    if (isSamplingRunning_)
    {
        return;
    }

    ResetOutputs();

    QString errorMessage;
    if (!ValidateInput(errorMessage))
    {
        ShowStatus(errorMessage, false);
        return;
    }

    SamplingOptions options;
    options.sampleSize = static_cast<std::size_t>(sampleSizeEdit_->value());
    options.histogramBins = static_cast<std::size_t>(binsEdit_->value());
    options.previewCount = static_cast<std::size_t>(previewCountEdit_->value());
    options.seed = static_cast<std::uint32_t>(seedEdit_->value());

    ApplySamplingRunningState(true);
    ShowStatus("Идёт моделирование. Вычисляю A, генерирую U и строю выборочные оценки.", true);

    simulationWatcher_->setFuture(QtConcurrent::run([options]() { return ::RunSampling(options); }));
}

bool MainWindow::ValidateInput(QString& errorMessage) const
{
    if (sampleSizeEdit_->value() <= 0)
    {
        errorMessage = "Размер выборки должен быть больше нуля.";
        return false;
    }

    if (binsEdit_->value() <= 1)
    {
        errorMessage = "Число интервалов гистограммы должно быть больше единицы.";
        return false;
    }

    if (previewCountEdit_->value() <= 0)
    {
        errorMessage = "Число отображаемых точек должно быть больше нуля.";
        return false;
    }

    if (previewCountEdit_->value() > sampleSizeEdit_->value())
    {
        errorMessage = "Число отображаемых точек не должно превышать размер выборки.";
        return false;
    }

    if (seedEdit_->value() <= 0)
    {
        errorMessage = "Seed генератора должен быть положительным.";
        return false;
    }

    return true;
}

void MainWindow::ApplySamplingRunningState(bool running)
{
    isSamplingRunning_ = running;

    SetFieldContainerEnabled(sampleSizeEdit_, !running);
    SetFieldContainerEnabled(binsEdit_, !running);
    SetFieldContainerEnabled(previewCountEdit_, !running);
    SetFieldContainerEnabled(seedEdit_, !running);

    runButton_->setEnabled(!running);
    runButton_->setText(running ? "Идёт моделирование..." : "Выполнить моделирование");
}

void MainWindow::FinishSampling(const SamplingResult& result)
{
    ApplySamplingRunningState(false);

    if (!result.success)
    {
        ShowStatus(QString::fromStdString(result.message), false);
        return;
    }

    lastResult_ = result;
    hasResult_ = true;

    PopulateSummary(result);
    PopulateMatrices(result);
    PopulatePreviewTable(result);
    PopulateSamplesTable(result);
    UpdatePlots(result);

    ShowStatus(QString::fromStdString(result.message), true);
}

void MainWindow::PopulateSummary(const SamplingResult& result)
{
    const QLocale ruLocale(QLocale::Russian, QLocale::Russia);
    dimensionValue_->setText(ruLocale.toString(static_cast<qulonglong>(result.summary.dimension)));
    sampleCountValue_->setText(ruLocale.toString(static_cast<qulonglong>(result.summary.sampleSize)));
    meanErrorValue_->setText(FormatNumber(result.summary.maxMeanError));
    covarianceErrorValue_->setText(FormatNumber(result.summary.maxCovarianceError));
    frobeniusErrorValue_->setText(FormatNumber(result.summary.frobeniusCovarianceError));
    seedValue_->setText(ruLocale.toString(static_cast<qulonglong>(result.options.seed)));
}

void MainWindow::PopulateMatrices(const SamplingResult& result)
{
    const QStringList xHeaders = BuildComponentHeaders("x", Lab2Variant::kDimension);
    const QStringList uHeaders = BuildComponentHeaders("u", Lab2Variant::kDimension);
    const QString meanVectorLabel = QStringLiteral("mₓ");
    const QString empiricalMeanVectorLabel = QStringLiteral("m̂ₓ");

    static_cast<MatrixDisplayWidget*>(theoreticalMeanMatrix_)
        ->SetMatrixData(MakeSingleRowMatrix(result.theoreticalMean), xHeaders, {meanVectorLabel});
    static_cast<MatrixDisplayWidget*>(theoreticalCovarianceMatrix_)
        ->SetMatrixData(result.theoreticalCovariance, xHeaders, xHeaders);
    static_cast<MatrixDisplayWidget*>(choleskyMatrix_)->SetMatrixData(result.choleskyFactor, uHeaders, xHeaders);
    static_cast<MatrixDisplayWidget*>(empiricalMeanMatrix_)
        ->SetMatrixData(MakeSingleRowMatrix(result.empiricalMean), xHeaders, {empiricalMeanVectorLabel});
    static_cast<MatrixDisplayWidget*>(empiricalCovarianceMatrix_)
        ->SetMatrixData(result.empiricalCovariance, xHeaders, xHeaders);
    static_cast<MatrixDisplayWidget*>(empiricalCorrelationMatrix_)
        ->SetMatrixData(result.empiricalCorrelation, xHeaders, xHeaders);
}

void MainWindow::PopulatePreviewTable(const SamplingResult& result)
{
    QStringList headers {"№"};
    headers.append(BuildComponentHeaders("u", Lab2Variant::kDimension));
    headers.append(BuildComponentHeaders("x", Lab2Variant::kDimension));

    static_cast<NumericMatrixModel*>(previewTableModel_)
        ->SetMatrixData(BuildPreviewMatrix(result), headers);
}

void MainWindow::PopulateSamplesTable(const SamplingResult& result)
{
    Q_UNUSED(result);
    static_cast<SampleTableModel*>(sampleTableModel_)->SetSamples(&lastResult_.samples);
}

void MainWindow::UpdatePlots(const SamplingResult& result)
{
    const std::array<QColor, Lab2Variant::kDimension> colors = {
        QColor("#2563eb"),
        QColor("#dc2626"),
        QColor("#059669"),
        QColor("#7c3aed"),
    };
    const QStringList xHeaders = BuildComponentHeaders("x", Lab2Variant::kDimension);

    for (std::size_t index = 0; index < histogramPlots_.size() && index < result.histograms.size(); ++index)
    {
        const QString componentLabel = xHeaders.value(static_cast<int>(index));
        const QString meanLabel = IndexedSymbol("m", QString::number(index + 1));
        const QString covarianceLabel =
            IndexedSymbol("k", QString("%1%1").arg(index + 1));

        histogramPlots_[index]->SetSeries(
            {
                {QString("Гистограмма %1").arg(componentLabel),
                 BuildHistogramOutline(result.histograms[index].bins),
                 colors[index],
                 2.4,
                 Qt::SolidLine,
                 PlotSeriesData::HoverMode::Step},
                {QString("Теоретическая плотность N(%1, %2)").arg(meanLabel, covarianceLabel),
                 BuildNormalDensityCurve(result.histograms[index]),
                 QColor("#0f4c81"),
                 2.0,
                 Qt::DashLine},
            });
    }

    QVector<PlotSeriesData> sequenceSeries;
    sequenceSeries.reserve(static_cast<int>(Lab2Variant::kDimension));
    for (std::size_t index = 0; index < Lab2Variant::kDimension; ++index)
    {
        sequenceSeries.push_back(
            {xHeaders.value(static_cast<int>(index)),
             BuildSequenceSeries(result.transformedPreviewSamples, index),
             colors[index],
             1.9});
    }
    sequencePlot_->SetSeries(sequenceSeries);
}

void MainWindow::ShowStatus(const QString& text, bool success)
{
    statusBadge_->setText(text);
    statusBadge_->setTextFormat(Qt::RichText);

    if (success)
    {
        statusBadge_->setStyleSheet(
            "border: 1px solid #94d7aa; background: #eaf8ef; color: #1d5a32; "
            "border-radius: 12px; padding: 10px 12px;");
    }
    else
    {
        statusBadge_->setStyleSheet(
            "border: 1px solid #efb7bf; background: #fff1f3; color: #7b2636; "
            "border-radius: 12px; padding: 10px 12px;");
    }
}

QString MainWindow::FormatNumber(double value)
{
    return FormatNumericValue(value);
}
