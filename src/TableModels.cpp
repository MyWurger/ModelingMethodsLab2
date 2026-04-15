// ============================================================================
// ФАЙЛ TABLEMODELS.CPP - МОДЕЛИ ТАБЛИЦ GUI LAB_2
// ============================================================================

#include "TableModels.h"

#include "Lab2Variant.h"
#include "UiFormatting.h"

#include <utility>

NumericMatrixModel::NumericMatrixModel(QObject* parent) : QAbstractTableModel(parent) {}

void NumericMatrixModel::SetMatrixData(NumericMatrix values, QStringList columnHeaders, QStringList rowHeaders)
{
    beginResetModel();
    values_ = std::move(values);
    columnHeaders_ = std::move(columnHeaders);
    rowHeaders_ = std::move(rowHeaders);
    endResetModel();
}

int NumericMatrixModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
    {
        return 0;
    }

    return static_cast<int>(values_.size());
}

int NumericMatrixModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid() || values_.empty())
    {
        return 0;
    }

    return static_cast<int>(values_.front().size());
}

QVariant NumericMatrixModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
    {
        return {};
    }

    if (role == Qt::DisplayRole)
    {
        return FormatNumericValue(values_[index.row()][index.column()]);
    }

    if (role == Qt::TextAlignmentRole)
    {
        return Qt::AlignCenter;
    }

    return {};
}

QVariant NumericMatrixModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
    {
        return {};
    }

    if (orientation == Qt::Horizontal)
    {
        if (section >= 0 && section < columnHeaders_.size())
        {
            return columnHeaders_[section];
        }

        return QString::number(section + 1);
    }

    if (section >= 0 && section < rowHeaders_.size())
    {
        return rowHeaders_[section];
    }

    return QString::number(section + 1);
}

SampleTableModel::SampleTableModel(QObject* parent) : QAbstractTableModel(parent) {}

void SampleTableModel::SetSamples(const NumericMatrix* samples)
{
    beginResetModel();
    samples_ = samples;
    endResetModel();
}

int SampleTableModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid() || samples_ == nullptr)
    {
        return 0;
    }

    return static_cast<int>(samples_->size());
}

int SampleTableModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid())
    {
        return 0;
    }

    return static_cast<int>(Lab2Variant::kDimension + 1);
}

QVariant SampleTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || samples_ == nullptr)
    {
        return {};
    }

    if (role == Qt::DisplayRole)
    {
        if (index.column() == 0)
        {
            return index.row() + 1;
        }

        return FormatNumericValue((*samples_)[index.row()][index.column() - 1]);
    }

    if (role == Qt::TextAlignmentRole)
    {
        return Qt::AlignCenter;
    }

    return {};
}

QVariant SampleTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
    {
        return {};
    }

    if (orientation == Qt::Horizontal)
    {
        if (section == 0)
        {
            return "№";
        }

        return BuildComponentHeaders("x", Lab2Variant::kDimension).value(section - 1);
    }

    return {};
}
