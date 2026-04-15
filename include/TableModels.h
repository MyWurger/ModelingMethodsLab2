// ============================================================================
// ФАЙЛ TABLEMODELS.H - МОДЕЛИ ТАБЛИЦ GUI LAB_2
// ============================================================================
// Назначение файла:
// 1) объявить Qt-модели таблиц, которые используются в интерфейсе LAB_2;
// 2) отделить представление табличных данных от MainWindow;
// 3) централизовать логику доступа к матрицам и выборкам в формате,
//    удобном для QTableView.
//
// Здесь собраны две модели:
// - NumericMatrixModel для матриц и preview-таблиц;
// - SampleTableModel для показа полной сгенерированной выборки.
// ============================================================================

// Защита от повторного включения заголовочного файла.
#pragma once

// Подключаем общие численные типы проекта.
#include "SamplingTypes.h"

// QAbstractTableModel - базовый класс табличных моделей Qt.
#include <QAbstractTableModel>
// QStringList нужен для заголовков строк и столбцов.
#include <QStringList>

// ----------------------------------------------------------------------------
// КЛАСС NumericMatrixModel
// ----------------------------------------------------------------------------
// Назначение:
// Представить произвольную численную матрицу в виде модели Qt,
// пригодной для показа в QTableView.
//
// Эта модель удобна для:
// - preview-матриц;
// - кратких табличных фрагментов;
// - любых матричных данных, которые уже есть в формате NumericMatrix.
// ----------------------------------------------------------------------------
class NumericMatrixModel final : public QAbstractTableModel
{
public:
    // Конструктор модели матрицы.
    //
    // Принимает:
    // - parent : родительский QObject.
    explicit NumericMatrixModel(QObject* parent = nullptr);

    // Передает модели новую матрицу и соответствующие заголовки.
    //
    // Принимает:
    // - values        : значения матрицы;
    // - columnHeaders : подписи столбцов;
    // - rowHeaders    : подписи строк; могут отсутствовать.
    void SetMatrixData(NumericMatrix values, QStringList columnHeaders, QStringList rowHeaders = {});

    // Возвращает число строк матрицы.
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    // Возвращает число столбцов матрицы.
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    // Возвращает данные ячейки для указанной роли Qt.
    QVariant data(const QModelIndex& index, int role) const override;
    // Возвращает данные заголовков строк и столбцов.
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

private:
    // Значения текущей матрицы.
    NumericMatrix values_;
    // Заголовки столбцов.
    QStringList columnHeaders_;
    // Заголовки строк.
    QStringList rowHeaders_;
};

// ----------------------------------------------------------------------------
// КЛАСС SampleTableModel
// ----------------------------------------------------------------------------
// Назначение:
// Представить полную выборку X в табличном виде без лишнего копирования.
//
// В отличие от NumericMatrixModel, здесь хранится не собственная копия данных,
// а только указатель на уже существующую матрицу выборки.
// Это полезно для больших наборов данных, где копировать всю выборку
// ради таблицы было бы лишним.
// ----------------------------------------------------------------------------
class SampleTableModel final : public QAbstractTableModel
{
public:
    // Конструктор модели полной выборки.
    //
    // Принимает:
    // - parent : родительский QObject.
    explicit SampleTableModel(QObject* parent = nullptr);

    // Передает модели указатель на выборку, которую нужно отображать.
    //
    // Принимает:
    // - samples : указатель на матрицу полной выборки;
    //             может быть nullptr, если данных еще нет.
    void SetSamples(const NumericMatrix* samples);

    // Возвращает число строк выборки.
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    // Возвращает число столбцов выборки.
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    // Возвращает данные конкретной ячейки для нужной роли Qt.
    QVariant data(const QModelIndex& index, int role) const override;
    // Возвращает подписи заголовков строк и столбцов таблицы.
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

private:
    // Указатель на матрицу полной выборки.
    // Модель ей не владеет, а только читает данные из внешнего источника.
    const NumericMatrix* samples_ = nullptr;
};
