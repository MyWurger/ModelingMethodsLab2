// ============================================================================
// ФАЙЛ TABLEMODELS.H - МОДЕЛИ ТАБЛИЦ GUI LAB_2
// ============================================================================

#pragma once

#include "SamplingTypes.h"

#include <QAbstractTableModel>
#include <QStringList>

class NumericMatrixModel final : public QAbstractTableModel
{
public:
    explicit NumericMatrixModel(QObject* parent = nullptr);

    void SetMatrixData(NumericMatrix values, QStringList columnHeaders, QStringList rowHeaders = {});

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

private:
    NumericMatrix values_;
    QStringList columnHeaders_;
    QStringList rowHeaders_;
};

class SampleTableModel final : public QAbstractTableModel
{
public:
    explicit SampleTableModel(QObject* parent = nullptr);

    void SetSamples(const NumericMatrix* samples);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

private:
    const NumericMatrix* samples_ = nullptr;
};
