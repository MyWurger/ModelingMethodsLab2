// ============================================================================
// ФАЙЛ UIFORMATTING.H - ОБЩЕЕ ФОРМАТИРОВАНИЕ GUI LAB_2
// ============================================================================

#pragma once

#include <QString>
#include <QStringList>
#include <cstddef>

QString FormatNumericValue(double value);
QString ToSubscriptDigits(const QString& text);
QString IndexedSymbol(const QString& prefix, const QString& indices);
QStringList BuildComponentHeaders(const QString& prefix, std::size_t dimension);
