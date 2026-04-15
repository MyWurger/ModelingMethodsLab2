// ============================================================================
// ФАЙЛ INTERACTIVECHARTVIEW.H - ИНТЕРАКТИВНЫЙ VIEW ДЛЯ ГРАФИКОВ LAB_2
// ============================================================================
// Назначение файла:
// 1) объявить класс InteractiveChartView, который расширяет стандартный
//    QChartView поведением, нужным именно в данной лабораторной работе;
// 2) описать интерфейс обработки пользовательских действий над графиком:
//    hover, wheel zoom, double click reset, жесты и выход курсора из области;
// 3) объявить служебные методы дорисовки поверх Qt Charts:
//    подсказок, внутренних осей, стрелок и границ носителя распределения;
// 4) хранить минимальное состояние интерактивности, которое относится именно
//    к view-уровню, а не к модели данных самого графика.
//
// По архитектурной роли этот класс является "интерактивной оболочкой" вокруг
// QChart. PlotChartWidget отвечает за данные, диапазоны, серии и общую логику
// карточки графика, а InteractiveChartView берет на себя:
// - обработку жестов и мыши;
// - преобразование событий пользователя в zoom/pan/reset;
// - ручную отрисовку поверх диаграммы там, где стандартного поведения
//   Qt Charts уже недостаточно.
// ============================================================================

// Защита от повторного включения заголовочного файла.
#pragma once

// Подключаем объявление владельца PlotChartWidget,
// потому что InteractiveChartView тесно связан с ним и использует его
// как "хозяина" состояния графика.
#include "PlotChartWidget.h"

// QPointer нужен для безопасственного хранения указателя на QObject-потомка.
// Если владелец будет уничтожен, QPointer автоматически станет nullptr.
#include <QPointer>

// QChartView - базовый виджет Qt Charts, который отображает объект QChart.
#include <QtCharts/QChartView>

// Предварительные объявления типов событий и служебных классов Qt,
// которые используются в сигнатурах методов.
class QGestureEvent;
class QMouseEvent;
class QNativeGestureEvent;
class QPainter;
class QPaintEvent;
class QWheelEvent;

QT_BEGIN_NAMESPACE
// Предварительное объявление самой диаграммы Qt Charts.
class QChart;
QT_END_NAMESPACE

// ----------------------------------------------------------------------------
// КЛАСС InteractiveChartView
// ----------------------------------------------------------------------------
// Назначение:
// Расширить стандартный QChartView интерактивным поведением и ручной
// дорисовкой служебных элементов поверх графика.
//
// Этот класс отвечает за:
// - реакцию на перемещение курсора внутри области графика;
// - колесо мыши для zoom;
// - double click для возврата к исходному виду;
// - обработку native gestures и gesture events;
// - показ hover-tooltip со значениями серий;
// - ручную отрисовку осей, стрелок, подписей и специальных границ.
// ----------------------------------------------------------------------------
class InteractiveChartView final : public QChartView
{
public:
    // Конструктор интерактивного view.
    //
    // Принимает:
    // - owner  : виджет-владелец PlotChartWidget, в котором хранится
    //            основное состояние данных, диапазонов и серий;
    // - chart  : объект диаграммы QChart, который должен отображаться;
    // - parent : родительский Qt-виджет.
    explicit InteractiveChartView(PlotChartWidget* owner, QChart* chart, QWidget* parent = nullptr);

protected:
    // paintEvent используется для дополнительной дорисовки поверх стандартного
    // содержимого QChartView. Здесь рисуются hover-элементы, оси, стрелки и
    // прочие накладки, которые не покрываются штатным поведением Qt Charts.
    void paintEvent(QPaintEvent* event) override;
    // viewportEvent перехватывает низкоуровневые события viewport, включая
    // жесты, которые не всегда удобно обрабатывать только обычными handlers.
    bool viewportEvent(QEvent* event) override;
    // wheelEvent отвечает за масштабирование графика колесом мыши.
    void wheelEvent(QWheelEvent* event) override;
    // mouseDoubleClickEvent используется для быстрого возврата к начальному
    // масштабу и положению графика.
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    // mouseMoveEvent отслеживает положение курсора и обновляет hover-состояние.
    void mouseMoveEvent(QMouseEvent* event) override;
    // leaveEvent нужен для очистки hover-состояния, когда курсор выходит
    // за пределы области view.
    void leaveEvent(QEvent* event) override;

private:
    // ----------------------------------------------------------------------------
    // СТРУКТУРА HoverEntry
    // ----------------------------------------------------------------------------
    // Назначение:
    // Хранить одну строку данных для hover-подсказки.
    //
    // Каждая запись описывает конкретную серию в точке под курсором:
    // - имя серии;
    // - форматированное значение;
    // - цвет серии;
    // - координату маркера в системе координат viewport.
    // ----------------------------------------------------------------------------
    struct HoverEntry
    {
        // Имя серии, которое будет показано в tooltip.
        QString name;
        // Уже подготовленное строковое представление значения.
        QString value;
        // Цвет серии для цветового маркера в подсказке.
        QColor color;
        // Координата точки в координатах view, где нужно рисовать маркер.
        QPointF markerViewPoint;
    };

    // Проверяет, лежит ли переданная точка viewport внутри реальной области
    // отрисовки plotArea, а не в заголовке, полях или вне графика.
    bool IsPointInsidePlot(const QPointF& viewportPoint) const;
    // Рисует весь hover-overlay поверх графика:
    // вертикальный индикатор, маркеры и tooltip со значениями.
    void DrawHoverOverlay(QPainter* painter);
    // Рисует сам tooltip с x-координатой и списком значений серий.
    void DrawHoverTooltip(QPainter* painter, double xValue, const QVector<HoverEntry>& entries);
    // Рисует внутреннюю горизонтальную ось внутри plotArea.
    void DrawHorizontalAxis(QPainter* painter, double xMin, double xMax);
    // Рисует внутреннюю вертикальную ось внутри plotArea.
    void DrawVerticalAxis(QPainter* painter, double yMin, double yMax);
    // Рисует наконечник стрелки для визуально оформленных осей.
    void DrawArrowHead(QPainter* painter, const QPointF& tip, const QPointF& from);
    // Рисует подпись оси или служебного элемента относительно опорной точки
    // с указанным выравниванием.
    void DrawAxisLabel(QPainter* painter, const QString& text, const QPointF& anchor, Qt::Alignment alignment);
    // Рисует границу носителя распределения, если она нужна данному графику.
    void DrawSupportBoundary(QPainter* painter, double xValue);
    // Обрабатывает native gesture события платформы, например pinch на macOS.
    bool HandleNativeGesture(QNativeGestureEvent* event);
    // Обрабатывает более общие gesture events Qt.
    bool HandleGesture(QGestureEvent* event);

    // Ссылка на виджет-владелец.
    // Через него InteractiveChartView получает доступ к состоянию графика,
    // не владея этим состоянием напрямую.
    QPointer<PlotChartWidget> owner_;
    // Последняя точка курсора в координатах viewport.
    QPointF hoverViewportPos_;
    // Признак того, что hover-состояние сейчас активно
    // и поверх графика нужно рисовать интерактивную подсказку.
    bool hoverActive_ = false;
};
