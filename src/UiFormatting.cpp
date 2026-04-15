// ============================================================================
// ФАЙЛ UIFORMATTING.CPP - ОБЩЕЕ ФОРМАТИРОВАНИЕ GUI LAB_2
// ============================================================================

#include "UiFormatting.h"

#include <QtGlobal>

#include <cmath>

QString FormatNumericValue(double value)
{
    if (!std::isfinite(value))
    {
        return "не число";
    }

    if (qFuzzyIsNull(value))
    {
        return "0";
    }

    const double absoluteValue = std::abs(value);
    if (absoluteValue < 1e-4 || absoluteValue >= 1e8)
    {
        return QString::number(value, 'e', 5);
    }

    QString text = QString::number(value, 'f', 6);
    while (text.contains('.') && (text.endsWith('0') || text.endsWith('.')))
    {
        text.chop(1);
    }

    if (text == "-0")
    {
        return "0";
    }

    return text;
}

QString ToSubscriptDigits(const QString& text)
{
    static const QStringList subscriptDigits = {
        QStringLiteral("₀"),
        QStringLiteral("₁"),
        QStringLiteral("₂"),
        QStringLiteral("₃"),
        QStringLiteral("₄"),
        QStringLiteral("₅"),
        QStringLiteral("₆"),
        QStringLiteral("₇"),
        QStringLiteral("₈"),
        QStringLiteral("₉"),
    };

    QString result;
    result.reserve(text.size());
    for (const QChar character : text)
    {
        if (character.isDigit())
        {
            result += subscriptDigits[character.digitValue()];
        }
        else
        {
            result += character;
        }
    }

    return result;
}

QString IndexedSymbol(const QString& prefix, const QString& indices)
{
    return prefix + ToSubscriptDigits(indices);
}

QStringList BuildComponentHeaders(const QString& prefix, std::size_t dimension)
{
    QStringList headers;
    for (std::size_t index = 0; index < dimension; ++index)
    {
        headers << IndexedSymbol(prefix, QString::number(index + 1));
    }
    return headers;
}
