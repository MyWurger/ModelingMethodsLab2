// ============================================================================
// ФАЙЛ MATHWIDGETS.CPP - ОТРИСОВКА МАТЕМАТИЧЕСКИХ ВИДЖЕТОВ LAB_2
// ============================================================================

#include "MathWidgets.h"
#include "UiFormatting.h"

#include <QAbstractSpinBox>
#include <QFocusEvent>
#include <QLineEdit>
#include <QPainter>
#include <QPaintEvent>
#include <QPen>
#include <QRegularExpression>
#include <QSizePolicy>

#include <algorithm>
#include <array>
#include <utility>

IntegerSpinBox::IntegerSpinBox(QWidget* parent) : QSpinBox(parent)
{
    setButtonSymbols(QAbstractSpinBox::NoButtons);
    setAccelerated(true);
    setCorrectionMode(QAbstractSpinBox::CorrectToNearestValue);
    setGroupSeparatorShown(false);
}

void IntegerSpinBox::SetPlaceholderText(const QString& text)
{
    if (auto* edit = lineEdit())
    {
        edit->setPlaceholderText(text);
    }
}

bool IntegerSpinBox::HasInput() const
{
    const auto* edit = lineEdit();
    return edit != nullptr && !edit->text().trimmed().isEmpty();
}

void IntegerSpinBox::focusOutEvent(QFocusEvent* event)
{
    const bool wasEmpty = !HasInput();
    QSpinBox::focusOutEvent(event);
    if (wasEmpty)
    {
        clear();
    }
}

void IntegerSpinBox::stepBy(int steps)
{
    if (!HasInput())
    {
        setValue(minimum());
    }

    QSpinBox::stepBy(steps);
}

VariantMathWidget::VariantMathWidget(QWidget* parent) : QWidget(parent)
{
    setMinimumHeight(182);
    setMaximumHeight(190);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

QSize VariantMathWidget::sizeHint() const
{
    return QSize(520, 182);
}

void VariantMathWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QColor ink("#102033");
    const QColor muted("#294560");

    QFont labelFont("Avenir Next", 15);
    labelFont.setWeight(QFont::DemiBold);
    QFont mathFont("Avenir Next", 15);
    mathFont.setWeight(QFont::DemiBold);
    QFont monoFont("Menlo", 15);
    monoFont.setWeight(QFont::DemiBold);
    QFont subFont("Avenir Next", 12);
    subFont.setWeight(QFont::DemiBold);

    auto drawTextLabel = [&](double x, double centerY, const QString& text) {
        painter.setFont(labelFont);
        painter.setPen(muted);
        const QFontMetricsF metrics(labelFont);
        painter.drawText(QPointF(x, centerY + (metrics.ascent() - metrics.descent()) * 0.5), text);
    };

    auto drawIndexedLabel = [&](double x, double centerY, const QString& prefix, const QString& symbol) {
        painter.setPen(muted);
        const QFontMetricsF labelMetrics(labelFont);
        const double baseline = centerY + (labelMetrics.ascent() - labelMetrics.descent()) * 0.5;

        painter.setFont(labelFont);
        painter.drawText(QPointF(x, baseline), prefix + symbol);

        painter.setFont(subFont);
        const double subscriptX = x + labelMetrics.horizontalAdvance(prefix + symbol) + 1.0;
        painter.drawText(QPointF(subscriptX, baseline + 5.0), "x");
    };

    const double widgetWidth = static_cast<double>(width());
    const double matrixLeft = std::clamp(widgetWidth * 0.40, 178.0, 205.0);
    const double matrixRight = std::min(widgetWidth - 16.0, matrixLeft + 250.0);
    const double firstColumnX = matrixLeft + 28.0;
    const double lastColumnX = matrixRight - 28.0;
    const double columnStep = (lastColumnX - firstColumnX) / 3.0;

    auto columnX = [&](int column) {
        return firstColumnX + static_cast<double>(column) * columnStep;
    };

    auto drawCenteredMono = [&](const QString& text, double centerX, double centerY) {
        painter.setFont(monoFont);
        painter.setPen(ink);
        painter.drawText(QRectF(centerX - 16.0, centerY - 11.0, 32.0, 22.0), Qt::AlignCenter, text);
    };

    auto drawBracket = [&](double left, double top, double bottom, bool openLeft) {
        painter.setPen(QPen(ink, 2.1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        const double lip = 8.0;
        if (openLeft)
        {
            painter.drawLine(QPointF(left + lip, top), QPointF(left, top));
            painter.drawLine(QPointF(left, top), QPointF(left, bottom));
            painter.drawLine(QPointF(left, bottom), QPointF(left + lip, bottom));
        }
        else
        {
            painter.drawLine(QPointF(left - lip, top), QPointF(left, top));
            painter.drawLine(QPointF(left, top), QPointF(left, bottom));
            painter.drawLine(QPointF(left, bottom), QPointF(left - lip, bottom));
        }
    };

    drawTextLabel(0.0, 18.0, "Размерность");
    painter.setFont(mathFont);
    painter.setPen(ink);
    painter.drawText(QPointF(matrixLeft, 24.0), "n = 4");

    drawIndexedLabel(0.0, 70.0, "Вектор ", "m");
    drawBracket(matrixLeft, 50.0, 90.0, true);
    drawBracket(matrixRight, 50.0, 90.0, false);
    drawCenteredMono("1", columnX(0), 71.0);
    drawCenteredMono("0", columnX(1), 71.0);
    drawCenteredMono("1", columnX(2), 71.0);
    drawCenteredMono("0", columnX(3), 71.0);
    painter.setFont(subFont);
    painter.drawText(QPointF(matrixRight + 8.0, 56.0), "T");

    drawIndexedLabel(0.0, 139.0, "Матрица ", "K");
    drawBracket(matrixLeft, 105.0, 181.0, true);
    drawBracket(matrixRight, 105.0, 181.0, false);

    const std::array<std::array<QString, 4>, 4> matrixValues {{
        {QStringLiteral("3"), QStringLiteral("2"), QStringLiteral("1"), QStringLiteral("0")},
        {QStringLiteral("2"), QStringLiteral("8"), QStringLiteral("3"), QStringLiteral("0")},
        {QStringLiteral("1"), QStringLiteral("3"), QStringLiteral("4"), QStringLiteral("0")},
        {QStringLiteral("0"), QStringLiteral("0"), QStringLiteral("0"), QStringLiteral("9")},
    }};
    const std::array<double, 4> rowCenters {116.0, 134.0, 152.0, 170.0};
    for (std::size_t row = 0; row < matrixValues.size(); ++row)
    {
        for (std::size_t column = 0; column < matrixValues[row].size(); ++column)
        {
            drawCenteredMono(matrixValues[row][column], columnX(static_cast<int>(column)), rowCenters[row]);
        }
    }
}

SummaryFormulaWidget::SummaryFormulaWidget(SummaryFormulaKind formulaKind, QWidget* parent)
    : QWidget(parent), formulaKind_(formulaKind)
{
    setMinimumHeight(28);
    setMaximumHeight(32);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

QSize SummaryFormulaWidget::sizeHint() const
{
    return QSize(210, 30);
}

void SummaryFormulaWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QColor muted("#718394");
    QFont textFont("Avenir Next", 14);
    textFont.setWeight(QFont::Medium);
    QFont subFont("Avenir Next", 10);
    subFont.setWeight(QFont::Medium);

    const double baseline = 20.0;
    double x = 0.0;

    auto drawText = [&](const QString& text) {
        painter.setFont(textFont);
        painter.setPen(muted);
        const QFontMetricsF metrics(textFont);
        painter.drawText(QPointF(x, baseline), text);
        x += metrics.horizontalAdvance(text);
    };

    auto drawSubscript = [&](const QString& text) {
        painter.setFont(subFont);
        painter.setPen(muted);
        const QFontMetricsF metrics(subFont);
        painter.drawText(QPointF(x + 1.0, baseline + 4.0), text);
        x += metrics.horizontalAdvance(text) + 3.0;
    };

    auto drawHat = [&](double symbolLeft, double symbolWidth) {
        const double center = symbolLeft + symbolWidth * 0.5;
        const double top = 3.8;
        painter.setPen(QPen(muted, 1.45, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawLine(QPointF(center - 4.0, top + 4.0), QPointF(center, top));
        painter.drawLine(QPointF(center, top), QPointF(center + 4.0, top + 4.0));
    };

    auto drawSymbol = [&](const QString& symbol, bool withHat, bool withXSubscript) {
        painter.setFont(textFont);
        painter.setPen(muted);
        const QFontMetricsF metrics(textFont);
        const double symbolLeft = x;
        const double symbolWidth = metrics.horizontalAdvance(symbol);

        painter.drawText(QPointF(x, baseline), symbol);
        if (withHat)
        {
            drawHat(symbolLeft, symbolWidth);
        }

        x += symbolWidth;
        if (withXSubscript)
        {
            drawSubscript("X");
        }
    };

    auto drawFSubscript = [&]() {
        drawSubscript("F");
    };

    switch (formulaKind_)
    {
    case SummaryFormulaKind::Dimension:
        drawText("dim X");
        break;
    case SummaryFormulaKind::SampleSize:
        drawText("N");
        break;
    case SummaryFormulaKind::Seed:
        drawText("генератор U");
        break;
    case SummaryFormulaKind::MeanError:
        drawText("max |");
        drawSymbol("m", true, true);
        drawText(" - ");
        drawSymbol("m", false, true);
        drawText("|");
        break;
    case SummaryFormulaKind::CovarianceError:
        drawText("max |");
        drawSymbol("K", true, true);
        drawText(" - ");
        drawSymbol("K", false, true);
        drawText("|");
        break;
    case SummaryFormulaKind::FrobeniusError:
        drawText("||");
        drawSymbol("K", true, true);
        drawText(" - ");
        drawSymbol("K", false, true);
        drawText("||");
        drawFSubscript();
        break;
    }
}

MatrixDisplayWidget::MatrixDisplayWidget(QWidget* parent) : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    setMinimumWidth(0);
    setMinimumHeight(170);
}

void MatrixDisplayWidget::SetMatrixData(NumericMatrix values, QStringList columnHeaders, QStringList rowHeaders)
{
    values_ = std::move(values);
    columnHeaders_ = std::move(columnHeaders);
    rowHeaders_ = std::move(rowHeaders);
    updateGeometry();
    update();
}

void MatrixDisplayWidget::SetHorizontalPlacement(HorizontalPlacement placement)
{
    horizontalPlacement_ = placement;
    update();
}

QSize MatrixDisplayWidget::sizeHint() const
{
    const int rows = std::max<int>(1, static_cast<int>(values_.size()));
    const int columns = values_.empty() ? std::max(1, static_cast<int>(columnHeaders_.size()))
                                        : static_cast<int>(values_.front().size());
    const int baseHeight = 72 + (columnHeaders_.isEmpty() ? 0 : 24) + rows * 34;
    const int baseWidth = 180 + columns * 68;
    return QSize(baseWidth, std::max(170, baseHeight));
}

void MatrixDisplayWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const QRectF contentRect = rect().adjusted(2, 4, -2, -4);
    if (values_.empty())
    {
        painter.setPen(QColor("#7a8a9c"));
        QFont emptyFont("Avenir Next", 15);
        emptyFont.setWeight(QFont::DemiBold);
        painter.setFont(emptyFont);
        painter.drawText(contentRect, Qt::AlignCenter, "Нет матричных данных");
        return;
    }

    const int rowCount = static_cast<int>(values_.size());
    const int columnCount = static_cast<int>(values_.front().size());
    if (rowCount <= 0 || columnCount <= 0)
    {
        return;
    }

    const bool isSingleRowVector = rowCount == 1;

    int headerPointSize = 15;
    int valuePointSize = isSingleRowVector ? 17 : 16;

    qreal rowLabelWidth = 0.0;
    qreal effectiveRowLabelWidth = 0.0;
    qreal maxValueWidth = 0.0;
    qreal maxHeaderWidth = 0.0;
    QVector<qreal> maxValueWidths(columnCount, 0.0);
    QVector<qreal> maxHeaderWidths(columnCount, 0.0);
    QVector<qreal> columnWidths(columnCount, 0.0);
    qreal columnHeaderHeight = 0.0;
    qreal rowHeight = 34.0;
    const qreal labelGap = rowHeaders_.isEmpty() ? 0.0 : (isSingleRowVector ? 14.0 : 12.0);
    const qreal bracketGap = isSingleRowVector ? 12.0 : 14.0;
    const qreal interColumnGap = columnCount == 1 ? 0.0 : (isSingleRowVector ? 10.0 : 10.0);

    QFont headerFont("Avenir Next", headerPointSize);
    QFont valueFont("Menlo", valuePointSize);

    auto recomputeMetrics = [&]() {
        headerFont = QFont("Avenir Next", headerPointSize);
        headerFont.setWeight(QFont::DemiBold);
        valueFont = QFont("Menlo", valuePointSize);
        valueFont.setWeight(QFont::DemiBold);

        const QFontMetricsF headerMetrics(headerFont);
        const QFontMetricsF valueMetrics(valueFont);

        rowLabelWidth = 0.0;
        for (const QString& rowHeader : rowHeaders_)
        {
            rowLabelWidth = std::max(rowLabelWidth, headerMetrics.horizontalAdvance(rowHeader));
        }
        effectiveRowLabelWidth =
            rowHeaders_.isEmpty()
                ? 0.0
                : (isSingleRowVector ? rowLabelWidth + 2.0 : std::max<qreal>(40.0, rowLabelWidth + 2.0));

        maxHeaderWidth = 0.0;
        std::fill(maxHeaderWidths.begin(), maxHeaderWidths.end(), 0.0);
        for (int column = 0; column < columnCount && column < columnHeaders_.size(); ++column)
        {
            const qreal headerWidth = headerMetrics.horizontalAdvance(columnHeaders_[column]);
            maxHeaderWidths[column] = headerWidth;
            maxHeaderWidth = std::max(maxHeaderWidth, headerWidth);
        }

        maxValueWidth = 0.0;
        std::fill(maxValueWidths.begin(), maxValueWidths.end(), 0.0);
        for (const NumericVector& row : values_)
        {
            for (int column = 0; column < columnCount && column < static_cast<int>(row.size()); ++column)
            {
                const qreal valueWidth = valueMetrics.horizontalAdvance(FormatNumericValue(row[column]));
                maxValueWidths[column] = std::max(maxValueWidths[column], valueWidth);
                maxValueWidth = std::max(maxValueWidth, valueWidth);
            }
        }

        columnHeaderHeight = columnHeaders_.isEmpty() ? 0.0 : std::max<qreal>(22.0, headerMetrics.height());
        rowHeight = std::max<qreal>(30.0, valueMetrics.height() + 6.0);
    };

    recomputeMetrics();

    qreal cellWidth = 0.0;
    qreal matrixWidth = 0.0;
    for (;;)
    {
        if (isSingleRowVector)
        {
            cellWidth = std::max<qreal>(34.0, std::max(maxValueWidth + 6.0, maxHeaderWidth + 4.0));
            std::fill(columnWidths.begin(), columnWidths.end(), cellWidth);
        }
        else
        {
            const qreal preferredLeftInset = horizontalPlacement_ == HorizontalPlacement::Center ? 0.0 : 50.0;
            const qreal availableWidth = std::max<qreal>(
                120.0,
                contentRect.width() - preferredLeftInset - effectiveRowLabelWidth - labelGap - bracketGap * 2.0);
            qreal naturalWidth = interColumnGap * (columnCount - 1);
            for (int column = 0; column < columnCount; ++column)
            {
                columnWidths[column] =
                    std::max<qreal>(46.0, std::max(maxValueWidths[column] + 18.0, maxHeaderWidths[column] + 14.0));
                naturalWidth += columnWidths[column];
            }

            if (naturalWidth > availableWidth)
            {
                cellWidth = std::max<qreal>(34.0, (availableWidth - interColumnGap * (columnCount - 1)) / columnCount);
                std::fill(columnWidths.begin(), columnWidths.end(), cellWidth);
            }
        }
        matrixWidth = interColumnGap * (columnCount - 1);
        for (qreal width : columnWidths)
        {
            matrixWidth += width;
        }

        bool fitsValues = true;
        bool fitsHeaders = true;
        for (int column = 0; column < columnCount; ++column)
        {
            fitsValues = fitsValues && maxValueWidths[column] <= columnWidths[column] - 8.0;
            fitsHeaders = fitsHeaders && maxHeaderWidths[column] <= columnWidths[column] - 6.0;
        }
        if ((fitsValues && fitsHeaders) || (valuePointSize <= 12 && headerPointSize <= 11))
        {
            break;
        }

        if (valuePointSize > 12)
        {
            --valuePointSize;
        }
        if (headerPointSize > 11)
        {
            --headerPointSize;
        }
        recomputeMetrics();
    }

    const qreal totalBlockWidth = effectiveRowLabelWidth + labelGap + bracketGap + matrixWidth + bracketGap;
    const qreal centeredBlockLeft =
        contentRect.left() + std::max<qreal>(0.0, (contentRect.width() - totalBlockWidth) * 0.5);
    const qreal preferredBlockLeft = (isSingleRowVector || horizontalPlacement_ == HorizontalPlacement::Center)
                                         ? centeredBlockLeft
                                         : contentRect.left() + 18.0;
    const qreal maxBlockLeft = std::max<qreal>(contentRect.left(), contentRect.right() - totalBlockWidth);
    const qreal blockLeft = std::clamp(preferredBlockLeft, contentRect.left(), maxBlockLeft);
    const qreal totalBlockHeight = columnHeaderHeight + 8.0 + rowHeight * rowCount;
    const qreal blockTop = contentRect.top() + std::max<qreal>(0.0, (contentRect.height() - totalBlockHeight) * 0.5);

    const qreal matrixLeft = blockLeft + effectiveRowLabelWidth + labelGap + bracketGap;
    const qreal matrixTop = blockTop + columnHeaderHeight + 8.0;
    const qreal matrixHeight = rowHeight * rowCount;
    const qreal matrixRight = matrixLeft + matrixWidth;
    const qreal bracketLeft = matrixLeft - (isSingleRowVector ? 10.0 : 12.0);
    const qreal bracketRight = matrixRight + (isSingleRowVector ? 10.0 : 12.0);
    const qreal bracketTop = matrixTop - 2.0;
    const qreal bracketBottom = matrixTop + matrixHeight + 2.0;
    QVector<qreal> columnLefts(columnCount, matrixLeft);
    qreal columnLeft = matrixLeft;
    for (int column = 0; column < columnCount; ++column)
    {
        columnLefts[column] = columnLeft;
        columnLeft += columnWidths[column] + interColumnGap;
    }

    painter.setFont(headerFont);
    painter.setPen(QColor("#294560"));

    for (int column = 0; column < columnCount && column < columnHeaders_.size(); ++column)
    {
        const QRectF headerRect(columnLefts[column], blockTop, columnWidths[column], columnHeaderHeight);
        painter.drawText(headerRect, Qt::AlignCenter, columnHeaders_[column]);
    }

    for (int row = 0; row < rowCount && row < rowHeaders_.size(); ++row)
    {
        const QRectF rowRect(blockLeft, matrixTop + rowHeight * row, effectiveRowLabelWidth, rowHeight);
        painter.drawText(rowRect, Qt::AlignVCenter | Qt::AlignRight, rowHeaders_[row]);
    }

    painter.setPen(QPen(QColor("#102033"), 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    const qreal lip = 10.0;
    painter.drawLine(QPointF(bracketLeft + lip, bracketTop), QPointF(bracketLeft, bracketTop));
    painter.drawLine(QPointF(bracketLeft, bracketTop), QPointF(bracketLeft, bracketBottom));
    painter.drawLine(QPointF(bracketLeft, bracketBottom), QPointF(bracketLeft + lip, bracketBottom));

    painter.drawLine(QPointF(bracketRight - lip, bracketTop), QPointF(bracketRight, bracketTop));
    painter.drawLine(QPointF(bracketRight, bracketTop), QPointF(bracketRight, bracketBottom));
    painter.drawLine(QPointF(bracketRight, bracketBottom), QPointF(bracketRight - lip, bracketBottom));

    if (isSingleRowVector)
    {
        QFont transposeFont("Avenir Next", 13);
        transposeFont.setWeight(QFont::DemiBold);
        painter.setFont(transposeFont);
        painter.setPen(QColor("#294560"));
        painter.drawText(QRectF(bracketRight + 6.0, bracketTop - 2.0, 20.0, 16.0), Qt::AlignLeft | Qt::AlignTop, "T");
    }

    painter.setFont(valueFont);
    painter.setPen(QColor("#102033"));
    for (int row = 0; row < rowCount; ++row)
    {
        for (int column = 0; column < columnCount; ++column)
        {
            const QRectF valueRect(columnLefts[column], matrixTop + rowHeight * row, columnWidths[column], rowHeight);
            painter.drawText(valueRect, Qt::AlignCenter, FormatNumericValue(values_[row][column]));
        }
    }
}

MatrixCardTitleWidget::MatrixCardTitleWidget(QString title, QWidget* parent)
    : QWidget(parent), parts_(BuildParts(std::move(title)))
{
    setMinimumHeight(38);
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
}

QSize MatrixCardTitleWidget::sizeHint() const
{
    return QSize(360, 38);
}

void MatrixCardTitleWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    QFont titleFont("Avenir Next", 18);
    titleFont.setWeight(QFont::Bold);
    QFont subFont("Avenir Next", 12);
    subFont.setWeight(QFont::DemiBold);

    const QFontMetricsF titleMetrics(titleFont);
    const QFontMetricsF subMetrics(subFont);
    const qreal totalWidth = MeasureWidth(titleMetrics, subMetrics);
    qreal x = std::max<qreal>(0.0, (width() - totalWidth) * 0.5);
    const qreal baseline = (height() + titleMetrics.ascent() - titleMetrics.descent()) * 0.5 + 2.0;

    painter.setPen(QColor("#102033"));
    for (const TitlePart& part : parts_)
    {
        painter.setFont(titleFont);
        const qreal symbolLeft = x;
        const qreal symbolWidth = titleMetrics.horizontalAdvance(part.text);
        painter.drawText(QPointF(x, baseline), part.text);

        if (part.hasHat)
        {
            DrawHat(painter, symbolLeft, symbolWidth, baseline - titleMetrics.ascent() - 1.0);
        }

        x += symbolWidth;
        if (!part.subscript.isEmpty())
        {
            painter.setFont(subFont);
            painter.drawText(QPointF(x + 1.0, baseline + 5.0), part.subscript);
            x += subMetrics.horizontalAdvance(part.subscript) + 2.0;
        }
    }
}

QVector<MatrixCardTitleWidget::TitlePart> MatrixCardTitleWidget::BuildParts(QString title)
{
    if (title == "Теоретический вектор m<sub>X</sub>")
    {
        return {{QStringLiteral("Теоретический вектор ")}, {QStringLiteral("m"), false, QStringLiteral("X")}};
    }
    if (title == "Теоретическая матрица K<sub>X</sub>")
    {
        return {{QStringLiteral("Теоретическая матрица ")}, {QStringLiteral("K"), false, QStringLiteral("X")}};
    }
    if (title == "Выборочная оценка m&#770;<sub>X</sub>")
    {
        return {{QStringLiteral("Выборочная оценка ")}, {QStringLiteral("m"), true, QStringLiteral("X")}};
    }
    if (title == "Выборочная оценка K&#770;<sub>X</sub>")
    {
        return {{QStringLiteral("Выборочная оценка ")}, {QStringLiteral("K"), true, QStringLiteral("X")}};
    }
    if (title == "Выборочная корреляция R&#770;")
    {
        return {{QStringLiteral("Выборочная корреляция ")}, {QStringLiteral("R"), true, QString()}};
    }

    title.remove(QRegularExpression("<[^>]*>"));
    title.replace("&#770;", QString());
    return {{title}};
}

qreal MatrixCardTitleWidget::MeasureWidth(const QFontMetricsF& titleMetrics, const QFontMetricsF& subMetrics) const
{
    qreal width = 0.0;
    for (const TitlePart& part : parts_)
    {
        width += titleMetrics.horizontalAdvance(part.text);
        if (!part.subscript.isEmpty())
        {
            width += subMetrics.horizontalAdvance(part.subscript) + 2.0;
        }
    }
    return width;
}

void MatrixCardTitleWidget::DrawHat(QPainter& painter, qreal symbolLeft, qreal symbolWidth, qreal top)
{
    const qreal center = symbolLeft + symbolWidth * 0.5;
    painter.save();
    painter.setPen(QPen(QColor("#102033"), 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawLine(QPointF(center - 5.0, top + 5.0), QPointF(center, top));
    painter.drawLine(QPointF(center, top), QPointF(center + 5.0, top + 5.0));
    painter.restore();
}
