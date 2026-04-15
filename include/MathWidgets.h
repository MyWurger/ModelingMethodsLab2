// ============================================================================
// ФАЙЛ MATHWIDGETS.H - КАСТОМНЫЕ ВИДЖЕТЫ МАТЕМАТИЧЕСКОГО GUI LAB_2
// ============================================================================

#pragma once

#include "SamplingTypes.h"

#include <QFontMetricsF>
#include <QSize>
#include <QSpinBox>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

class QFocusEvent;
class QPaintEvent;
class QPainter;

class IntegerSpinBox final : public QSpinBox
{
public:
    explicit IntegerSpinBox(QWidget* parent = nullptr);

    void SetPlaceholderText(const QString& text);
    bool HasInput() const;

protected:
    void focusOutEvent(QFocusEvent* event) override;
    void stepBy(int steps) override;
};

class VariantMathWidget final : public QWidget
{
public:
    explicit VariantMathWidget(QWidget* parent = nullptr);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
};

enum class SummaryFormulaKind
{
    Dimension,
    SampleSize,
    Seed,
    MeanError,
    CovarianceError,
    FrobeniusError,
};

class SummaryFormulaWidget final : public QWidget
{
public:
    explicit SummaryFormulaWidget(SummaryFormulaKind formulaKind, QWidget* parent = nullptr);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    SummaryFormulaKind formulaKind_;
};

class MatrixDisplayWidget final : public QWidget
{
public:
    enum class HorizontalPlacement
    {
        Default,
        Center,
    };

    explicit MatrixDisplayWidget(QWidget* parent = nullptr);

    void SetMatrixData(NumericMatrix values, QStringList columnHeaders, QStringList rowHeaders = {});
    void SetHorizontalPlacement(HorizontalPlacement placement);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    NumericMatrix values_;
    QStringList columnHeaders_;
    QStringList rowHeaders_;
    HorizontalPlacement horizontalPlacement_ = HorizontalPlacement::Default;
};

class MatrixCardTitleWidget final : public QWidget
{
public:
    explicit MatrixCardTitleWidget(QString title, QWidget* parent = nullptr);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    struct TitlePart
    {
        QString text;
        bool hasHat = false;
        QString subscript;
    };

    static QVector<TitlePart> BuildParts(QString title);
    qreal MeasureWidth(const QFontMetricsF& titleMetrics, const QFontMetricsF& subMetrics) const;
    static void DrawHat(QPainter& painter, qreal symbolLeft, qreal symbolWidth, qreal top);

    QVector<TitlePart> parts_;
};
