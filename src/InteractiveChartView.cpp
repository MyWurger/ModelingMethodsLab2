// ============================================================================
// ФАЙЛ INTERACTIVECHARTVIEW.CPP - СОБЫТИЯ И OVERLAY ГРАФИКОВ LAB_2
// ============================================================================
// В этом файле собрана вся "живая" часть графического поведения графиков LAB_2.
//
// Если PlotChartWidget отвечает за хранение серий, осей, цветов, диапазонов
// и за высокоуровневые операции вроде ZoomAt / ResetView / PanByPixels,
// то InteractiveChartView отвечает уже за непосредственную интерактивность:
// - как реагировать на колесо мыши;
// - как реагировать на pinch/native gesture;
// - как обновлять hover-состояние при движении курсора;
// - как дорисовывать поверх графика направляющие, оси, стрелки и tooltip;
// - как переводить координаты между viewport, scene, chart и системой данных.
//
// Важно понимать архитектурную границу:
// - здесь НЕ вычисляются статистики, матрицы, плотности, гистограммы;
// - здесь НЕ создаются исходные точки серий;
// - здесь НЕ принимаются решения о математической корректности данных;
// - здесь ТОЛЬКО визуальное поведение и геометрия взаимодействия.
//
// То есть этот файл берет уже готовую диаграмму Qt Charts и добавляет к ней
// лабораторную интерактивность, без которой график был бы просто статичным
// изображением без наведения, zoom, сброса вида и поясняющих overlay-элементов.
// ============================================================================

// Подключаем собственный заголовок с объявлением InteractiveChartView.
#include "InteractiveChartView.h"
// Подключаем утилиты геометрии и численного поведения графиков.
#include "PlotChartUtils.h"

// QFontMetricsF нужен для точного измерения размеров текста tooltip и подписей осей.
#include <QFontMetricsF>
// QGestureEvent нужен для обработки Qt-gesture-событий.
#include <QGestureEvent>
// QGraphicsView нужен для настройки drag/update-поведения базового view.
#include <QGraphicsView>
// QLineF нужен для рисования направляющих и стрелок как геометрических отрезков.
#include <QLineF>
// QMouseEvent нужен для обработки движения мыши и double click.
#include <QMouseEvent>
// QNativeGestureEvent нужен для platform-native жестов, особенно на macOS.
#include <QNativeGestureEvent>
// QPainter нужен для ручной отрисовки overlay поверх диаграммы.
#include <QPainter>
// QPaintEvent - базовый event paintEvent.
#include <QPaintEvent>
// QPinchGesture нужен для pinch-scale жестов.
#include <QPinchGesture>
// QWheelEvent нужен для zoom/pan колесом мыши и тачпада.
#include <QWheelEvent>

// QChart - сама диаграмма Qt Charts, отображаемая внутри view.
#include <QtCharts/QChart>
// QLineSeries используется как одна из базовых серий Qt Charts.
#include <QtCharts/QLineSeries>
// QValueAxis нужен для доступа к численным диапазонам осей.
#include <QtCharts/QValueAxis>

// std::max и std::clamp используются при геометрической стабилизации подписей и zoom.
#include <algorithm>
// std::exp, std::isfinite и другая элементарная математика нужны для жестов и координат.
#include <cmath>
// std::optional нужен для интерполированных значений hover-режима.
#include <optional>

// Импортируем пространство имен утилит графиков.
// Важно, что это делается именно в .cpp-файле, а не в заголовке:
// так мы не "засоряем" глобальное пространство имен для всех файлов проекта,
// а лишь локально сокращаем запись внутри данной реализации.
//
// Это типичный и безопасный компромисс:
// - в header using namespace делать плохо;
// - в implementation file это допустимо, если повышает читаемость.
using namespace PlotChartUtils;

// ----------------------------------------------------------------------------
// КОНСТРУКТОР InteractiveChartView
// ----------------------------------------------------------------------------
// Назначение:
// Инициализировать интерактивный view поверх уже созданной диаграммы QChart.
//
// Принимает:
// - owner  : виджет-владелец PlotChartWidget, в котором хранится состояние графика;
// - chart  : объект диаграммы Qt Charts;
// - parent : родительский виджет Qt.
//
// Возвращает:
// - объект InteractiveChartView в полностью инициализированном состоянии.
// ----------------------------------------------------------------------------
InteractiveChartView::InteractiveChartView(PlotChartWidget* owner, QChart* chart, QWidget* parent)
    : QChartView(chart, parent), owner_(owner)
{
    // Здесь используется список инициализации конструктора.
    // Это стандартный и правильный способ инициализировать:
    // - базовый класс QChartView(chart, parent);
    // - собственные поля owner_(owner).
    //
    // Почему именно так, а не присваиванием в теле конструктора:
    // - базовый класс вообще нельзя "присвоить" после входа в тело;
    // - поля и базовый класс инициализируются до начала тела конструктора;
    // - это и быстрее, и концептуально корректнее.
    //
    // Отдельно важно: owner_ здесь просто сохраняется как не-владеющий указатель.
    // InteractiveChartView не создает и не уничтожает PlotChartWidget,
    // а только обращается к нему как к внешнему источнику состояния.

    // Базовый QChartView по умолчанию умеет drag / rubber-band режимы,
    // но в LAB_2 вся логика перемещения и масштабирования контролируется нами.
    // Поэтому сначала отключаем стандартные режимы, чтобы они не конфликтовали
    // с пользовательской логикой внутри PlotChartWidget.

    // Отключаем стандартный drag-mode QGraphicsView,
    // потому что панорамирование в проекте реализуется собственным кодом через PlotChartWidget.
    setDragMode(QGraphicsView::NoDrag);

    // Отключаем встроенный rubber-band zoom,
    // чтобы он не конфликтовал с кастомным поведением zoom и hover-overlay.
    setRubberBand(QChartView::NoRubberBand);

    // Включаем постоянное отслеживание мыши даже без зажатых кнопок.
    // Без этого Qt будет отправлять mouseMoveEvent только при нажатой кнопке,
    // а hover-tooltip должен работать просто при наведении.
    setMouseTracking(true);

    // Требуем полный апдейт viewport при перерисовке,
    // потому что overlay рисуется поверх всей области,
    // и частичное обновление здесь может оставлять "грязные" следы
    // от старого tooltip или направляющей.
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);

    // Разрешаем touch-события на самом view.
    // Это нужно для корректного прохождения gesture-событий на устройствах
    // с трекпадом или сенсорным вводом.
    setAttribute(Qt::WA_AcceptTouchEvents, true);

    // Отдельно разрешаем touch-события и на viewport,
    // потому что реальные жесты часто приходят именно туда.
    viewport()->setAttribute(Qt::WA_AcceptTouchEvents, true);

    // Регистрируем pinch-жест на уровне виджета.
    // Qt не начинает присылать Gesture-события сам по себе:
    // их нужно явно запросить через grabGesture.
    grabGesture(Qt::PinchGesture);

    // И дублируем регистрацию pinch-жеста на уровне viewport,
    // чтобы не потерять его в зависимости от маршрутизации событий Qt.
    viewport()->grabGesture(Qt::PinchGesture);
}

// ----------------------------------------------------------------------------
// МЕТОД paintEvent
// ----------------------------------------------------------------------------
// Назначение:
// Сначала дать QChartView нарисовать основную диаграмму,
// а затем дорисовать поверх нее служебные элементы overlay.
//
// Принимает:
// - event : стандартное событие перерисовки Qt.
//
// Возвращает:
// - void : метод рисует view и завершается.
// ----------------------------------------------------------------------------
void InteractiveChartView::paintEvent(QPaintEvent* event)
{
    // Сначала обязательно вызываем базовую реализацию,
    // чтобы сама диаграмма и ее серии были нарисованы штатным кодом Qt Charts.
    // Даже если мы хотим полностью контролировать overlay,
    // "основной холст" графика должен сначала построить именно базовый класс.
    //
    // Отсюда важный порядок:
    // 1) QChartView::paintEvent(event);
    // 2) наша пост-отрисовка поверх готовой диаграммы.
    QChartView::paintEvent(event);

    // Дальше начинается уже наш слой post-paint отрисовки.
    //
    // Overlay нельзя рисовать "вслепую": для него нужны
    // - валидный PlotChartWidget-владелец;
    // - факт наличия данных;
    // - живая диаграмма QChart;
    // - anchor-серия, относительно которой выполняется mapToValue/mapToPosition.
    //
    // Если хотя бы один из этих элементов отсутствует,
    // значит диаграмма либо еще не инициализирована, либо очищена,
    // и любая попытка дорисовки будет либо бессмысленной, либо аварийной.
    if (owner_ == nullptr || !owner_->hasData_ || chart() == nullptr || owner_->anchorSeries_ == nullptr)
    {
        return;
    }

    // Создаем локальный объект QPainter прямо на стеке.
    // Это обычный RAII-подход в Qt:
    // объект создается, работает в пределах текущей функции
    // и автоматически завершает рисование при выходе из области видимости.
    //
    // Создаем painter именно поверх viewport.
    // Это принципиально важно: мы рисуем не внутри самой серии и не внутри QChart,
    // а прямо поверх уже готового изображения графика.
    QPainter painter(viewport());

    // Включаем сглаживание.
    // Без него тонкие стрелки, кружки маркеров и пунктирные линии выглядели бы грубо.
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Устанавливаем базовое перо для внутренних осей и служебных линий.
    painter.setPen(QPen(QColor("#4b647b"), 1.35));

    // Считываем текущий видимый диапазон по X.
    const double xMin = owner_->axisX_->min();
    const double xMax = owner_->axisX_->max();

    // Считываем текущий видимый диапазон по Y.
    const double yMin = owner_->axisY_->min();
    const double yMax = owner_->axisY_->max();

    // Внутренняя горизонтальная ось соответствует линии y = 0.
    // Она видна только тогда, когда 0 действительно лежит внутри текущего диапазона Y.
    // Если весь диапазон строго выше или строго ниже нуля,
    // линия y = 0 физически не попадает в окно наблюдения.
    if (yMin <= 0.0 && yMax >= 0.0)
    {
        DrawHorizontalAxis(&painter, xMin, xMax);
    }

    // Аналогично вертикальная внутренняя ось соответствует линии x = 0.
    // Ее можно дорисовать только если 0 попадает в текущее окно по X.
    if (xMin <= 0.0 && xMax >= 0.0)
    {
        DrawVerticalAxis(&painter, yMin, yMax);
    }

    // Специальные support-boundary нужны только для графиков с носителем [0, 1].
    // В текущем проекте это определяется текстовой подписью оси X.
    // Для таких графиков пользователю полезно явно видеть границы x = 0 и x = 1.
    if (owner_->xAxisLabel_ == "x")
    {
        DrawSupportBoundary(&painter, kSupportLeft);
        DrawSupportBoundary(&painter, kSupportRight);
    }

    // Hover-overlay рисуем только когда курсор ранее вошел в plotArea
    // и mouseMoveEvent активировал соответствующий режим.
    if (hoverActive_)
    {
        DrawHoverOverlay(&painter);
    }
}

// ----------------------------------------------------------------------------
// МЕТОД viewportEvent
// ----------------------------------------------------------------------------
// Назначение:
// Перехватывать низкоуровневые события viewport и отдавать жесты
// в собственные обработчики до того, как их съест базовый QChartView.
//
// Принимает:
// - event : произвольное событие, пришедшее в viewport.
//
// Возвращает:
// - bool : true, если событие обработано здесь, иначе результат базового класса.
// ----------------------------------------------------------------------------
bool InteractiveChartView::viewportEvent(QEvent* event)
{
    // Нулевой event обрабатывать нечего.
    // Потеря owner_ означает, что наш специальный интерактивный слой
    // больше не может корректно обратиться к PlotChartWidget.
    // В обоих случаях просто отдаем управление стандартному поведению.
    if (event == nullptr || owner_ == nullptr)
    {
        return QChartView::viewportEvent(event);
    }

    // Дальше идет классический паттерн "type-check -> cast -> specialized handler".
    // Сначала смотрим точный тип события через event->type(),
    // а потом приводим его к нужному подклассу.
    //
    // NativeGesture обрабатываем отдельно, потому что это platform-specific события,
    // которые Qt не всегда удобно сводит к обычным gesture event.
    if (event->type() == QEvent::NativeGesture)
    {
        // Приводим QEvent к конкретному типу native gesture.
        // static_cast безопасен, потому что мы уже проверили точный type().
        auto* nativeEvent = static_cast<QNativeGestureEvent*>(event);

        // Возврат true из viewportEvent означает:
        // "событие полностью обработано, распространять дальше его не надо".
        //
        // Это важно для интерактивности:
        // если после нашей обработки жест уйдет еще и в базовый класс,
        // можно получить двойную реакцию на одно действие пользователя.
        if (HandleNativeGesture(nativeEvent))
        {
            return true;
        }
    }

    // Обычные Qt gesture-события, например pinch, также перехватываем здесь.
    if (event->type() == QEvent::Gesture)
    {
        // Приводим событие к QGestureEvent после проверки его типа.
        auto* gestureEvent = static_cast<QGestureEvent*>(event);

        // Если собственный обработчик жеста сработал, событие считаем обработанным.
        if (HandleGesture(gestureEvent))
        {
            return true;
        }
    }

    // Все прочие события или необработанные жесты передаем в базовый класс.
    return QChartView::viewportEvent(event);
}

// ----------------------------------------------------------------------------
// МЕТОД wheelEvent
// ----------------------------------------------------------------------------
// Назначение:
// Обработать колесо мыши или тачпад для zoom/pan графика.
//
// Принимает:
// - event : событие вращения колеса или scroll-жеста.
//
// Возвращает:
// - void : метод либо обрабатывает событие сам, либо передает его базовому классу.
// ----------------------------------------------------------------------------
void InteractiveChartView::wheelEvent(QWheelEvent* event)
{
    // До начала кастомной обработки проверяем базовые предусловия.
    // Если нет самого события, нет владельца или на графике еще нет данных,
    // наш zoom/pan-режим ничего осмысленного сделать не сможет.
    if (event == nullptr || owner_ == nullptr || !owner_->hasData_)
    {
        QChartView::wheelEvent(event);
        return;
    }

    // Комбинацию с Ctrl сознательно не перехватываем.
    // Это снижает риск конфликта с системной логикой зума/жестов
    // или с поведением, к которому пользователь уже привык в среде ОС.
    if ((event->modifiers() & Qt::ControlModifier) != 0)
    {
        QChartView::wheelEvent(event);
        return;
    }

    // Для трекпадов Qt часто присылает pixelDelta().
    // Это уже готовое смещение в пикселях viewport, которое естественно трактовать
    // именно как панорамирование, а не как дискретный zoom.
    const QPoint pixelDelta = event->pixelDelta();

    // Ветка pixelDelta() фактически отвечает за "непрерывный" жест прокрутки.
    // Типичный источник - тачпад.
    //
    // Если pixelDelta не нулевой, трактуем событие как панорамирование в пикселях.
    if (!pixelDelta.isNull())
    {
        // Панорамирование передаем владельцу как есть.
        // Здесь не нужно вручную масштабировать delta:
        // PlotChartWidget уже знает, как перевести пиксельный сдвиг
        // в новый диапазон координат осей.
        owner_->PanByPixels(pixelDelta);

        // accept() говорит Qt, что событие поглощено именно этой веткой.
        event->accept();
        return;
    }

    // Классическое колесо мыши обычно приходит через angleDelta().
    // В этом режиме событие удобнее интерпретировать как дискретный zoom,
    // а не как сдвиг окна.
    if (event->angleDelta().y() != 0)
    {
        // Используем тернарный оператор как компактную запись
        // двуветочного выбора коэффициента масштаба.
        //
        // Положительный delta обычно означает прокрутку "вверх",
        // и мы сопоставляем ее с zoom in.
        // Отрицательный delta трактуем как zoom out.
        const double factor = event->angleDelta().y() > 0 ? (1.0 / kWheelZoomStep) : kWheelZoomStep;

        // Масштабируем относительно позиции курсора, а не центра графика.
        // Так пользователь "приближает туда, куда смотрит",
        // и управление ощущается существенно естественнее.
        // position() возвращает координату курсора в системе viewport.
        // Именно она и нужна владельцу для zoom относительно точки наведения.
        owner_->ZoomAt(event->position(), factor);
        event->accept();
        return;
    }

    // Если ни один из наших сценариев не сработал, отдаём событие базовому классу.
    QChartView::wheelEvent(event);
}

// ----------------------------------------------------------------------------
// МЕТОД mouseDoubleClickEvent
// ----------------------------------------------------------------------------
// Назначение:
// Сбросить масштаб и панорамирование графика двойным кликом.
//
// Принимает:
// - event : событие двойного клика мыши.
//
// Возвращает:
// - void : метод либо сбрасывает вид, либо передает событие дальше.
// ----------------------------------------------------------------------------
void InteractiveChartView::mouseDoubleClickEvent(QMouseEvent* event)
{
    // Двойной клик в LAB_2 зарезервирован под быстрый возврат к исходному виду:
    // это проще и быстрее, чем отдельная кнопка reset для каждого графика.
    if (owner_ != nullptr)
    {
        // ResetView инкапсулирует восстановление исходных диапазонов осей.
        // То есть сам InteractiveChartView не знает деталей reset-логики,
        // а только делегирует действие владельцу.
        owner_->ResetView();
        event->accept();
        return;
    }

    // Без владельца оставляем стандартное поведение QChartView.
    QChartView::mouseDoubleClickEvent(event);
}

// ----------------------------------------------------------------------------
// МЕТОД mouseMoveEvent
// ----------------------------------------------------------------------------
// Назначение:
// Обновлять hover-состояние при перемещении мыши над графиком.
//
// Принимает:
// - event : событие перемещения мыши.
//
// Возвращает:
// - void : метод обновляет hover-координаты и передает событие базовому классу.
// ----------------------------------------------------------------------------
void InteractiveChartView::mouseMoveEvent(QMouseEvent* event)
{
    // Hover-режим строится именно на постоянном обновлении координат мыши.
    // Поэтому при каждом движении курсора запоминаем новую позицию и сразу
    // пересчитываем, нужно ли рисовать overlay.
    if (event != nullptr)
    {
        // position() возвращает координату с плавающей точкой.
        // Это лучше, чем старые целочисленные QPoint-координаты,
        // потому что геометрия tooltip и маркеров в Qt 6 строится через QPointF.
        //
        // Запоминаем координату курсора в системе viewport.
        hoverViewportPos_ = event->position();

        // Hover активен только внутри plotArea.
        // Это важно: если курсор находится в полях, около заголовка или рамки,
        // tooltip и вертикальная направляющая пользователю только мешали бы.
        hoverActive_ = IsPointInsidePlot(hoverViewportPos_);

        // Просим viewport немедленно перерисоваться,
        // чтобы вертикальная направляющая и tooltip двигались без задержки.
        viewport()->update();
    }

    // Даем базовому классу выполнить свою часть обработки движения мыши.
    QChartView::mouseMoveEvent(event);
}

// ----------------------------------------------------------------------------
// МЕТОД leaveEvent
// ----------------------------------------------------------------------------
// Назначение:
// Сбрасывать hover-режим, когда курсор уходит из области view.
//
// Принимает:
// - event : событие ухода курсора.
//
// Возвращает:
// - void : метод выключает hover и завершает обработку.
// ----------------------------------------------------------------------------
void InteractiveChartView::leaveEvent(QEvent* event)
{
    // Уход курсора за пределы view означает, что hover-режим должен быть полностью сброшен.
    hoverActive_ = false;

    // Запрашиваем перерисовку, чтобы удалить все визуальные следы hover-состояния.
    viewport()->update();

    // Передаем событие дальше в базовый класс,
    // чтобы стандартная внутренняя обработка Qt тоже завершилась корректно.
    QChartView::leaveEvent(event);
}

// ----------------------------------------------------------------------------
// МЕТОД IsPointInsidePlot
// ----------------------------------------------------------------------------
// Назначение:
// Проверить, попадает ли точка viewport внутрь реальной области plotArea диаграммы.
//
// Принимает:
// - viewportPoint : точку в координатах viewport.
//
// Возвращает:
// - bool : true, если точка лежит внутри plotArea, иначе false.
// ----------------------------------------------------------------------------
bool InteractiveChartView::IsPointInsidePlot(const QPointF& viewportPoint) const
{
    // Без диаграммы невозможно ни определить plotArea, ни выполнить преобразование координат.
    if (chart() == nullptr)
    {
        return false;
    }

    // Координаты события мыши приходят в системе viewport.
    // plotArea диаграммы живет в системе координат QChart.
    // Поэтому требуется двухшаговый перевод:
    // viewport -> scene -> chart.

    // Сначала переводим точку из системы viewport в систему scene.
    const QPointF scenePos = mapToScene(viewportPoint.toPoint());

    // Затем переводим точку из scene в локальную систему координат диаграммы.
    const QPointF chartPos = chart()->mapFromScene(scenePos);

    // contains(...) - это обычная геометрическая проверка в терминах QRectF.
    //
    // Проверяем, попала ли точка внутрь прямоугольника plotArea.
    // Именно plotArea соответствует реальной области построения серий.
    return chart()->plotArea().contains(chartPos);
}

// ----------------------------------------------------------------------------
// МЕТОД DrawHoverOverlay
// ----------------------------------------------------------------------------
// Назначение:
// Нарисовать весь hover-overlay: вертикальную направляющую,
// маркеры серий и всплывающий tooltip.
//
// Принимает:
// - painter : painter, рисующий поверх viewport.
//
// Возвращает:
// - void : метод дорисовывает overlay и завершается.
// ----------------------------------------------------------------------------
void InteractiveChartView::DrawHoverOverlay(QPainter* painter)
{
    // Hover-overlay зависит сразу от нескольких источников:
    // - painter, который рисует поверх viewport;
    // - owner_, у которого лежат серии и оси;
    // - chart(), которая умеет преобразовывать координаты;
    // - anchorSeries_, относительно которой Qt Charts делает mapToValue/mapToPosition;
    // - текущего положения курсора именно внутри plotArea.
    //
    // Если любой из этих элементов отсутствует, overlay становится некорректным,
    // поэтому выходим немедленно.
    if (painter == nullptr || owner_ == nullptr || chart() == nullptr || owner_->anchorSeries_ == nullptr ||
        !IsPointInsidePlot(hoverViewportPos_))
    {
        return;
    }

    // Дальше снова проходим цепочку преобразований координат:
    // viewport -> scene -> chart -> data.
    //
    // Это центральная идея hover-режима:
    // курсор существует в пикселях viewport,
    // а tooltip должен работать в численной системе координат графика.

    // Переводим текущую hover-позицию из viewport в scene.
    const QPointF scenePos = mapToScene(hoverViewportPos_.toPoint());

    // Затем в координаты диаграммы.
    const QPointF chartPos = chart()->mapFromScene(scenePos);

    // И наконец получаем численную точку графика.
    // mapToValue использует anchor-series, чтобы интерпретировать экранную позицию
    // в координатах соответствующих осей.
    const QPointF hoverValue = chart()->mapToValue(chartPos, owner_->anchorSeries_);

    // На уровне интерфейса hover работает по принципу:
    // "фиксируем x и смотрим, какие значения y имеют разные серии при этом x".
    //
    // Поэтому для hover-вертикали нам нужна только координата x.
    // Именно по ней затем будут интерполироваться значения всех серий.
    const double xValue = hoverValue.x();

    // Если x получился нечисловым, дальнейшая геометрия перестает быть определенной:
    // нельзя построить ни вертикальную линию, ни интерполяцию, ни tooltip.
    if (!std::isfinite(xValue))
    {
        return;
    }

    // Берем текущие вертикальные границы окна для построения направляющей.
    const double yMin = owner_->axisY_->min();
    const double yMax = owner_->axisY_->max();

    // Строим верхнюю точку вертикальной hover-направляющей.
    // В системе данных это просто точка (xValue, yMax).
    const QPointF topChartPos = chart()->mapToPosition(QPointF(xValue, yMax), owner_->anchorSeries_);

    // Строим нижнюю точку той же направляющей: (xValue, yMin).
    const QPointF bottomChartPos = chart()->mapToPosition(QPointF(xValue, yMin), owner_->anchorSeries_);

    // Возвращаем обе точки обратно в координаты viewport,
    // потому что QPainter рисует именно в пиксельной системе viewport.
    const QPoint topViewPos = mapFromScene(chart()->mapToScene(topChartPos));

    // То же самое делаем для нижней точки.
    const QPoint bottomViewPos = mapFromScene(chart()->mapToScene(bottomChartPos));

    // Создаем отдельный объект QPen для hover-направляющей.
    // Такой объект инкапсулирует цвет, толщину и стиль линии.
    QPen guidePen(QColor(59, 130, 246, 170));
    guidePen.setWidthF(1.2);
    guidePen.setStyle(Qt::DashLine);

    // Устанавливаем это перо painter'у.
    painter->setPen(guidePen);

    // Рисуем вертикальную направляющую по текущему xValue.
    painter->drawLine(QLineF(topViewPos, bottomViewPos));

    // Теперь нужно собрать содержимое tooltip и координаты маркеров серий.
    // Для этого формируем промежуточный список HoverEntry.
    QVector<HoverEntry> entries;

    // Резервируем память заранее по числу известных серий.
    // Это не критично для производительности, но убирает лишние realloc.
    entries.reserve(owner_->seriesStates_.size());

    // qsizetype - "родной" размерный тип Qt-контейнеров.
    // Использовать его здесь правильнее, чем int, потому что size()
    // у QVector возвращает именно qsizetype.
    //
    // Проходим по всем сериям, зарегистрированным в PlotChartWidget.
    for (qsizetype seriesIndex = 0; seriesIndex < owner_->seriesStates_.size(); ++seriesIndex)
    {
        // Если пользователь выделил одну конкретную серию,
        // tooltip и маркеры должны показывать только ее.
        // Остальные серии в этом режиме намеренно игнорируются.
        if (owner_->highlightedSeriesIndex_ >= 0 && owner_->highlightedSeriesIndex_ != seriesIndex)
        {
            continue;
        }

        // Берем состояние текущей серии по ссылке.
        // Ссылка avoids копирование структуры и делает код дешевле и чище.
        const PlotChartWidget::SeriesState& state = owner_->seriesStates_[seriesIndex];

        // Получаем значение серии в точке xValue.
        // В зависимости от режима hover это может быть:
        // - точное считывание ближайшего дискретного значения;
        // - интерполяция между соседними точками;
        // - другой способ, заложенный в PlotChartUtils.
        // std::optional<double> здесь означает:
        // "значение либо успешно найдено, либо его нет".
        // Это намного яснее и безопаснее, чем передавать магические значения
        // вроде NaN или специального числа-заглушки.
        const std::optional<double> yValue =
            InterpolateSeriesValueAtX(state.fullPoints, state.monotonicByX, state.hoverMode, xValue);

        // Если в данной x-позиции значение определить нельзя,
        // например точка вне диапазона серии, эту серию пропускаем.
        if (!yValue.has_value())
        {
            continue;
        }

        // Получаем экранную позицию маркера серии.
        // Для этого переходим из системы данных (xValue, yValue)
        // обратно в систему пикселей viewport.
        const QPointF markerChartPos = chart()->mapToPosition(QPointF(xValue, *yValue), owner_->anchorSeries_);

        // Переводим позицию маркера в координаты viewport.
        const QPoint markerViewPos = mapFromScene(chart()->mapToScene(markerChartPos));

        // Добавляем новую запись в QVector через append.
        // Здесь используется list-initialization временного HoverEntry:
        // {name, value, color, markerPoint}.
        //
        // Сохраняем все, что нужно для дальнейшей отрисовки:
        // имя серии, уже отформатированное числовое значение,
        // цвет серии и экранную позицию цветного маркера.
        entries.append({state.name, FormatHoverValue(*yValue), state.color, QPointF(markerViewPos)});
    }

    // Маркеры рисуем с конца списка.
    // Так элементы, добавленные позже, визуально окажутся поверх предыдущих,
    // и пользователь лучше увидит верхние активные точки.
    for (int index = entries.size() - 1; index >= 0; --index)
    {
        // Берем одну запись hover-tooltip.
        // Индексация с конца используется здесь осознанно,
        // а не как случайный стиль цикла.
        const HoverEntry& entry = entries[index];

        // Для круглых маркеров контур отключаем,
        // чтобы акцент шел на цвет и мягкую подложку, а не на обводку.
        painter->setPen(Qt::NoPen);

        // Сначала рисуем белую подложку большего радиуса.
        // Она повышает читаемость маркера даже если серия проходит
        // по темному или насыщенному фону из других линий.
        painter->setBrush(QColor(255, 255, 255, 235));
        painter->drawEllipse(entry.markerViewPoint, 5.8, 5.8);

        // Затем рисуем внутренний цветной круг.
        // Такой двухслойный маркер лучше читается, чем просто один цветной диск.
        painter->setBrush(QColor(entry.color.red(), entry.color.green(), entry.color.blue(), 235));
        painter->drawEllipse(entry.markerViewPoint, 4.2, 4.2);
    }

    // После направляющей и маркеров рисуем сам tooltip.
    DrawHoverTooltip(painter, xValue, entries);

    // Возвращаем painter в более нейтральное состояние после кастомной отрисовки.
    painter->setBrush(Qt::NoBrush);
    painter->setPen(QColor("#4b647b"));
}

// ----------------------------------------------------------------------------
// МЕТОД DrawHoverTooltip
// ----------------------------------------------------------------------------
// Назначение:
// Нарисовать tooltip для hover-режима в удобной и читаемой форме.
//
// Принимает:
// - painter : painter для рисования;
// - xValue  : текущая координата x под курсором;
// - entries : список строк tooltip по сериям.
//
// Возвращает:
// - void : метод рисует tooltip и завершается.
// ----------------------------------------------------------------------------
void InteractiveChartView::DrawHoverTooltip(QPainter* painter, double xValue, const QVector<HoverEntry>& entries)
{
    // Без painter рисовать нечем.
    if (painter == nullptr)
    {
        return;
    }

    // Дальше явно создаем два независимых объекта QFont.
    // Это лучше, чем постоянно менять один и тот же шрифт:
    // - код проще читать;
    // - меньше риска случайно "утащить" старые настройки.
    //
    // Шрифт заголовка tooltip делаем немного крупнее основного текста,
    // чтобы координата x воспринималась как заголовок блока.
    QFont titleFont("Avenir Next");
    titleFont.setPointSize(11);
    titleFont.setWeight(QFont::DemiBold);

    // Шрифт строк tooltip делаем чуть компактнее и спокойнее,
    // потому что это вторичная информация по сериям.
    QFont bodyFont("Avenir Next");
    bodyFont.setPointSize(10);
    bodyFont.setWeight(QFont::Medium);

    // Метрики заголовочного шрифта нужны для точной геометрии.
    const QFontMetricsF titleMetrics(titleFont);

    // Метрики основного текста нужны для расчета высоты строк.
    const QFontMetricsF bodyMetrics(bodyFont);

    // Заголовок tooltip показывает текущую координату x.
    const QString titleText = QString("x = %1").arg(FormatHoverValue(xValue));

    // constexpr-константы используются здесь как compile-time параметры геометрии.
    // Это удобнее и чище, чем магические числа, разбросанные прямо по коду.
    //
    // Константы геометрии tooltip:
    // размер цветного swatch, внутренние поля и расстояния между секциями.
    constexpr qreal swatchSize = 10.0;
    constexpr qreal boxPadding = 12.0;
    constexpr qreal sectionSpacing = 8.0;
    constexpr qreal lineSpacing = 6.0;

    // Сначала считаем минимальную ширину tooltip только по заголовку "x = ...".
    qreal contentWidth = titleMetrics.horizontalAdvance(titleText);

    // Затем обходим все строки серий и расширяем оценку ширины,
    // если какая-то строка длиннее заголовка.
    for (const HoverEntry& entry : entries)
    {
        // Формируем текст одной строки вида "имя = значение".
        const QString lineText = QString("%1 = %2").arg(entry.name, entry.value);

        // Учитываем место под swatch + отступ + текст строки.
        contentWidth = std::max(contentWidth, swatchSize + 8.0 + bodyMetrics.horizontalAdvance(lineText));
    }

    // По высоте сначала учитываем только строку заголовка.
    qreal contentHeight = titleMetrics.height();

    // Если есть строки серий, добавляем:
    // - зазор между заголовком и списком;
    // - высоту всех строк;
    // - межстрочные интервалы.
    if (!entries.isEmpty())
    {
        contentHeight += sectionSpacing + entries.size() * bodyMetrics.height() + (entries.size() - 1) * lineSpacing;
    }

    // QRectF хранит геометрию во float/double-координатах.
    // Для UI-геометрии это удобнее, чем целочисленный QRect, потому что
    // рисование в Qt 6 активно использует дробные координаты.
    //
    // Создаем прямоугольник tooltip в локальных координатах.
    QRectF tooltipRect(0.0, 0.0, contentWidth + boxPadding * 2.0, contentHeight + boxPadding * 2.0);

    // Исходная позиция tooltip выбирается правее и чуть выше курсора.
    // Такой сдвиг снижает вероятность перекрытия самого маркера и направляющей.
    QPointF topLeft = hoverViewportPos_ + QPointF(18.0, -18.0);

    // Вводим "безопасную" рамку внутри viewport.
    // Tooltip должен полностью помещаться внутри окна и не прилипать вплотную к краям.
    const QRectF viewportRect = QRectF(viewport()->rect()).adjusted(8.0, 8.0, -8.0, -8.0);

    // Если при такой позиции tooltip вылезает вправо,
    // переносим его на противоположную сторону от курсора.
    if (topLeft.x() + tooltipRect.width() > viewportRect.right())
    {
        topLeft.setX(hoverViewportPos_.x() - tooltipRect.width() - 18.0);
    }

    // Если после этого tooltip ушел слишком влево, прижимаем его к допустимой границе.
    if (topLeft.x() < viewportRect.left())
    {
        topLeft.setX(viewportRect.left());
    }

    // Если tooltip вылезает вниз, поднимаем его вверх в пределы viewport.
    if (topLeft.y() + tooltipRect.height() > viewportRect.bottom())
    {
        topLeft.setY(viewportRect.bottom() - tooltipRect.height());
    }

    // Если tooltip ушел выше допустимой области, опускаем его до верхней границы.
    if (topLeft.y() < viewportRect.top())
    {
        topLeft.setY(viewportRect.top());
    }

    // После всех проверок финализируем геометрию:
    // прямоугольник уже имеет правильный размер, осталось только поставить его в нужную точку.
    tooltipRect.moveTopLeft(topLeft);

    // Рисуем мягкую светлую подложку tooltip.
    // Высокая непрозрачность нужна, чтобы текст не смешивался с графиком под ним.
    painter->setBrush(QColor(255, 255, 255, 242));

    // Рисуем мягкую рамку tooltip.
    painter->setPen(QPen(QColor("#d7e3ef"), 1.0));
    painter->drawRoundedRect(tooltipRect, 10.0, 10.0);

    // С этого момента painter рисует заголовочную часть tooltip.
    // Поэтому сначала переключаем его на заголовочный шрифт.
    painter->setFont(titleFont);

    // Цвет текста заголовка.
    painter->setPen(QColor("#102033"));

    // currentY храним именно как baseline текста, а не как верх прямоугольника.
    // Это позволяет более точно выравнивать строки с учетом ascent/descent шрифта.
    qreal currentY = tooltipRect.top() + boxPadding + titleMetrics.ascent();

    // Рисуем заголовок "x = ...".
    painter->drawText(QPointF(tooltipRect.left() + boxPadding, currentY), titleText);

    // Если список серий пуст, tooltip состоит только из одной строки "x = ...".
    if (entries.isEmpty())
    {
        return;
    }

    // После заголовка меняем конфигурацию painter и переходим к телу tooltip.
    painter->setFont(bodyFont);

    // Смещаемся ниже заголовка с учетом descent и зазора между секциями.
    currentY += titleMetrics.descent() + sectionSpacing;

    // Теперь рисуем строки серий одну за другой.
    for (int index = 0; index < entries.size(); ++index)
    {
        // Берем очередную запись tooltip.
        const HoverEntry& entry = entries[index];

        // Смещаем baseline на ascent новой строки,
        // чтобы drawText работал в корректной текстовой геометрии.
        currentY += bodyMetrics.ascent();

        // Строим маленький цветной swatch слева от текста.
        // Он связывает строку tooltip с цветом линии на графике.
        const QRectF swatchRect(tooltipRect.left() + boxPadding,
                                currentY - bodyMetrics.ascent() + 1.0,
                                swatchSize,
                                swatchSize);

        // Для swatch не нужен контур.
        painter->setPen(Qt::NoPen);

        // Заливаем swatch цветом серии.
        painter->setBrush(entry.color);
        painter->drawRoundedRect(swatchRect, 3.0, 3.0);

        // Возвращаем цвет пера для текста строки.
        painter->setPen(QColor("#22364d"));

        // arg(entry.name, entry.value) - это форматирование строки Qt-способом.
        // Получаем компактную текстовую форму "имя = значение".
        //
        // Рисуем строку вида "имя = значение" правее цветного swatch.
        painter->drawText(QPointF(swatchRect.right() + 8.0, currentY),
                          QString("%1 = %2").arg(entry.name, entry.value));

        // Смещаемся вниз на descent текущей строки.
        currentY += bodyMetrics.descent();

        // Если это не последняя строка, добавляем межстрочный зазор.
        if (index + 1 < entries.size())
        {
            currentY += lineSpacing;
        }
    }
}

// ----------------------------------------------------------------------------
// МЕТОД DrawHorizontalAxis
// ----------------------------------------------------------------------------
// Назначение:
// Дорисовать внутреннюю горизонтальную ось через y = 0.
//
// Принимает:
// - painter : painter для рисования;
// - xMin    : левая видимая граница по X;
// - xMax    : правая видимая граница по X.
//
// Возвращает:
// - void : метод рисует внутреннюю ось и завершается.
// ----------------------------------------------------------------------------
void InteractiveChartView::DrawHorizontalAxis(QPainter* painter, double xMin, double xMax)
{
    // Горизонтальная внутренняя ось рисуется в экранных координатах,
    // но задается в координатах данных как линия от (xMin, 0) до (xMax, 0).
    // Для такого перевода нужны все основные объекты диаграммы.
    if (painter == nullptr || owner_ == nullptr || chart() == nullptr || owner_->anchorSeries_ == nullptr)
    {
        return;
    }

    // Сначала задаем ось в математической системе координат графика.
    // Левая точка оси: (xMin, 0) в координатах данных.
    const QPointF leftChartPos = chart()->mapToPosition(QPointF(xMin, 0.0), owner_->anchorSeries_);

    // Правая точка оси: (xMax, 0) в координатах данных.
    const QPointF rightChartPos = chart()->mapToPosition(QPointF(xMax, 0.0), owner_->anchorSeries_);

    // Затем обе точки проводим через цепочку chart -> scene -> viewport,
    // чтобы получить реальные пиксельные позиции для QPainter.
    //
    // Переводим левую точку в координаты viewport.
    const QPoint leftViewPos = mapFromScene(chart()->mapToScene(leftChartPos));

    // Переводим правую точку в координаты viewport.
    const QPoint rightViewPos = mapFromScene(chart()->mapToScene(rightChartPos));

    // Рисуем наконечник стрелки у правого конца.
    // Сам отрезок оси визуально уже задается тем, что линия y = 0
    // проходит через сам график; здесь добавляем только акцент в виде стрелки.
    DrawArrowHead(painter, QPointF(rightViewPos), QPointF(leftViewPos));

    // Подпись оси прижимаем к правому концу, чтобы не перекрывать график в центре.
    DrawAxisLabel(painter,
                  owner_->xAxisLabel_,
                  QPointF(rightViewPos.x() - 10.0, rightViewPos.y() - 5.0),
                  Qt::AlignRight | Qt::AlignBottom);
}

// ----------------------------------------------------------------------------
// МЕТОД DrawVerticalAxis
// ----------------------------------------------------------------------------
// Назначение:
// Дорисовать внутреннюю вертикальную ось через x = 0.
//
// Принимает:
// - painter : painter для рисования;
// - yMin    : нижняя видимая граница по Y;
// - yMax    : верхняя видимая граница по Y.
//
// Возвращает:
// - void : метод рисует внутреннюю ось и завершается.
// ----------------------------------------------------------------------------
void InteractiveChartView::DrawVerticalAxis(QPainter* painter, double yMin, double yMax)
{
    // Вертикальная внутренняя ось задается в координатах данных как линия x = 0,
    // проходящая от yMin до yMax текущего видимого окна.
    if (painter == nullptr || owner_ == nullptr || chart() == nullptr || owner_->anchorSeries_ == nullptr)
    {
        return;
    }

    // Аналогично горизонтальной оси,
    // сначала задаем вертикальную ось в системе данных.
    //
    // Верхняя точка оси: (0, yMax) в системе данных.
    const QPointF topChartPos = chart()->mapToPosition(QPointF(0.0, yMax), owner_->anchorSeries_);

    // Нижняя точка оси: (0, yMin) в системе данных.
    const QPointF bottomChartPos = chart()->mapToPosition(QPointF(0.0, yMin), owner_->anchorSeries_);

    // Переводим верхнюю точку в координаты viewport.
    const QPoint topViewPos = mapFromScene(chart()->mapToScene(topChartPos));

    // Переводим нижнюю точку в координаты viewport.
    const QPoint bottomViewPos = mapFromScene(chart()->mapToScene(bottomChartPos));

    // Стрелку рисуем у верхнего конца, чтобы ось читалась как "растущая вверх".
    DrawArrowHead(painter, QPointF(topViewPos), QPointF(bottomViewPos));

    // Подпись Y ставим у верхнего конца со смещением вправо и вниз,
    // чтобы она не наезжала на наконечник стрелки.
    DrawAxisLabel(painter,
                  owner_->yAxisLabel_,
                  QPointF(topViewPos.x() + 8.0, topViewPos.y() + 8.0),
                  Qt::AlignLeft | Qt::AlignTop);
}

// ----------------------------------------------------------------------------
// МЕТОД DrawArrowHead
// ----------------------------------------------------------------------------
// Назначение:
// Нарисовать наконечник стрелки по направлению от `from` к `tip`.
//
// Принимает:
// - painter : painter для рисования;
// - tip     : вершину стрелки;
// - from    : точку, задающую направление подхода к вершине.
//
// Возвращает:
// - void : метод рисует наконечник и завершается.
// ----------------------------------------------------------------------------
void InteractiveChartView::DrawArrowHead(QPainter* painter, const QPointF& tip, const QPointF& from)
{
    // Наконечник стрелки вычисляется чисто геометрически:
    // 1) берем направление from -> tip;
    // 2) нормируем его;
    // 3) отступаем назад на длину наконечника;
    // 4) строим два боковых луча через перпендикуляр.

    // Без painter рисование невозможно.
    if (painter == nullptr)
    {
        return;
    }

    // Строим направляющий отрезок от from к tip.
    // Он задает геометрию и ориентацию будущего наконечника.
    const QLineF direction(from, tip);

    // Если длина почти нулевая, направление стрелки фактически не определено.
    // В таком случае боковые стороны вычислить корректно нельзя.
    if (qFuzzyIsNull(direction.length()))
    {
        return;
    }

    // unitVector() возвращает отрезок той же направленности, но единичной длины.
    // Это удобно: можно дальше работать уже не с "произвольной" длиной,
    // а с чистым направлением.
    const QLineF unit = direction.unitVector();

    // delta - единичный вектор направления стрелки.
    // Он показывает, куда "смотрит" вершина наконечника.
    const QPointF delta = unit.p2() - unit.p1();

    // back - точка, отстоящая от вершины назад на фиксированную длину наконечника.
    // От нее затем влево и вправо откладываются боковые ребра.
    const QPointF back = tip - delta * kAxisArrowLength;

    // normal - вектор, перпендикулярный направлению.
    // Он задает поперечное смещение для двух боковых граней наконечника.
    const QPointF normal(-delta.y(), delta.x());

    // Рисуем одну сторону наконечника.
    painter->drawLine(QLineF(tip, back + normal * kAxisArrowHalfWidth));

    // Рисуем вторую сторону наконечника.
    painter->drawLine(QLineF(tip, back - normal * kAxisArrowHalfWidth));
}

// ----------------------------------------------------------------------------
// МЕТОД DrawAxisLabel
// ----------------------------------------------------------------------------
// Назначение:
// Нарисовать подпись оси или служебного элемента возле заданной точки anchor.
//
// Принимает:
// - painter   : painter для рисования;
// - text      : текст подписи;
// - anchor    : опорная точка позиционирования;
// - alignment : выравнивание текста относительно anchor.
//
// Возвращает:
// - void : метод рисует подпись и завершает работу.
// ----------------------------------------------------------------------------
void InteractiveChartView::DrawAxisLabel(QPainter* painter,
                                         const QString& text,
                                         const QPointF& anchor,
                                         Qt::Alignment alignment)
{
    // Подпись оси рисуется как отдельный текстовый прямоугольник,
    // позиция которого вычисляется относительно anchor и флага alignment.
    // То есть anchor интерпретируется не как левый верхний угол,
    // а как опорная точка для выравнивания.

    // Если painter отсутствует или текст пустой, рисовать нечего.
    if (painter == nullptr || text.isEmpty())
    {
        return;
    }

    // Берем текущий шрифт painter'а как базу,
    // чтобы сохранить общую стилистику внешнего контекста рисования
    // и модифицировать только нужные параметры.
    QFont font = painter->font();

    // Гарантируем нижнюю границу размера шрифта.
    // Это защищает подписи от чрезмерного уменьшения при наследовании
    // "случайно мелкого" шрифта из внешнего painter-состояния.
    font.setPointSizeF(std::max(font.pointSizeF(), 10.5));

    // Подписи осей и support-boundary делаем жирнее,
    // потому что они являются структурными элементами графика.
    font.setBold(true);
    painter->setFont(font);

    // Метрики нужны для точного расчета прямоугольника текста.
    const QFontMetricsF metrics(font);

    // Формируем прямоугольник текста с его натуральными размерами.
    // QSizeF здесь хранит "естественную" ширину и высоту надписи,
    // вычисленную через метрики шрифта.
    QRectF textRect(QPointF(0.0, 0.0), QSizeF(metrics.horizontalAdvance(text), metrics.height()));

    // topLeft будем вычислять как реальную левую верхнюю точку прямоугольника.
    QPointF topLeft = anchor;

    // Если требуется правое выравнивание,
    // то anchor интерпретируется как правая граница текста.
    if (alignment & Qt::AlignRight)
    {
        topLeft.rx() -= textRect.width();
    }
    // Если нужен горизонтальный центр,
    // anchor трактуется как центр текстового прямоугольника.
    else if (alignment & Qt::AlignHCenter)
    {
        topLeft.rx() -= textRect.width() * 0.5;
    }

    // При вертикальном выравнивании по нижнему краю
    // anchor трактуется как нижняя граница прямоугольника текста.
    if (alignment & Qt::AlignBottom)
    {
        topLeft.ry() -= textRect.height();
    }
    // При выравнивании по вертикальному центру
    // anchor становится серединой текстового блока.
    else if (alignment & Qt::AlignVCenter)
    {
        topLeft.ry() -= textRect.height() * 0.5;
    }

    // std::clamp здесь используется как удобный способ
    // ограничить координаты подписи допустимым диапазоном.
    //
    // После вычисления желаемой позиции дополнительно зажимаем подпись
    // внутрь безопасной области viewport.
    // Это защищает текст от обрезки по краям виджета.
    const QRectF viewportRect = QRectF(viewport()->rect()).adjusted(6.0, 6.0, -6.0, -6.0);
    topLeft.setX(std::clamp(topLeft.x(), viewportRect.left(), viewportRect.right() - textRect.width()));
    topLeft.setY(std::clamp(topLeft.y(), viewportRect.top(), viewportRect.bottom() - textRect.height()));

    // Переносим текстовый прямоугольник в рассчитанную позицию.
    textRect.moveTopLeft(topLeft);

    // Устанавливаем цвет текста подписи.
    painter->setPen(QColor("#203146"));

    // Рисуем подпись по центру внутри рассчитанного прямоугольника.
    painter->drawText(textRect, Qt::AlignCenter, text);

    // Возвращаем базовый цвет пера после рисования подписи.
    painter->setPen(QColor("#4b647b"));
}

// ----------------------------------------------------------------------------
// МЕТОД DrawSupportBoundary
// ----------------------------------------------------------------------------
// Назначение:
// Нарисовать вертикальную границу носителя распределения в точке xValue.
//
// Принимает:
// - painter : painter для рисования;
// - xValue  : x-координату границы носителя.
//
// Возвращает:
// - void : метод рисует support-boundary при необходимости.
// ----------------------------------------------------------------------------
void InteractiveChartView::DrawSupportBoundary(QPainter* painter, double xValue)
{
    // Граница носителя - это специальная вертикальная направляющая
    // для случаев, когда ось X имеет естественные пределы.
    // В LAB_2 она нужна для графиков на интервале [0, 1].

    // Если базовые объекты недоступны, рисование support-boundary невозможно.
    if (painter == nullptr || owner_ == nullptr || chart() == nullptr || owner_->anchorSeries_ == nullptr)
    {
        return;
    }

    // Перед рисованием проверяем, попадает ли boundary вообще в текущее окно.
    // Нет смысла переводить координаты и рисовать линии,
    // которые все равно окажутся вне видимой области.
    //
    // Получаем текущий видимый диапазон по X.
    const double xMin = owner_->axisX_->min();
    const double xMax = owner_->axisX_->max();

    // Если граница носителя не попала в текущее окно по X,
    // пользователь ее просто не увидит, значит рисование лишено смысла.
    if (xValue < xMin || xValue > xMax)
    {
        return;
    }

    // Получаем текущий диапазон по Y для построения полной вертикальной линии.
    const double yMin = owner_->axisY_->min();
    const double yMax = owner_->axisY_->max();

    // Строим верхнюю точку boundary в системе координат данных.
    const QPointF topChartPos = chart()->mapToPosition(QPointF(xValue, yMax), owner_->anchorSeries_);

    // Строим нижнюю точку той же линии.
    const QPointF bottomChartPos = chart()->mapToPosition(QPointF(xValue, yMin), owner_->anchorSeries_);

    // Переводим верхнюю точку в координаты viewport.
    const QPoint topViewPos = mapFromScene(chart()->mapToScene(topChartPos));

    // Переводим нижнюю точку в координаты viewport.
    const QPoint bottomViewPos = mapFromScene(chart()->mapToScene(bottomChartPos));

    // Для boundary используем более мягкое и менее акцентное перо,
    // чем для основных внутренних осей.
    QPen boundaryPen(QColor("#94a3b8"));
    boundaryPen.setWidthF(1.2);
    boundaryPen.setStyle(Qt::DashLine);
    painter->setPen(boundaryPen);

    // Рисуем саму вертикальную boundary-линию.
    painter->drawLine(QLineF(topViewPos, bottomViewPos));

    // Подписываем линию как x=0 или x=1.
    // Здесь сравнение идет с известными константами support-boundary,
    // поэтому двоичная точность не создает практической проблемы.
    DrawAxisLabel(painter,
                  QString("x=%1").arg(xValue == 0.0 ? "0" : "1"),
                  QPointF(topViewPos.x() + 8.0, topViewPos.y() + 20.0),
                  Qt::AlignLeft | Qt::AlignTop);

    // Возвращаем базовый цвет пера после завершения специальной отрисовки.
    painter->setPen(QColor("#4b647b"));
}

// ----------------------------------------------------------------------------
// МЕТОД HandleNativeGesture
// ----------------------------------------------------------------------------
// Назначение:
// Обработать native-gesture события платформы: zoom, pan, swipe и smart zoom.
//
// Принимает:
// - event : native gesture event платформы.
//
// Возвращает:
// - bool : true, если жест обработан здесь, иначе false.
// ----------------------------------------------------------------------------
bool InteractiveChartView::HandleNativeGesture(QNativeGestureEvent* event)
{
    // Native gesture - это платформенный жест более низкого уровня,
    // чем обычный Qt::Gesture. На macOS такие события часто дают наиболее
    // естественную реакцию на трекпад, поэтому обрабатываем их отдельно.

    // Если события нет или владелец недоступен, жест здесь обработать нельзя.
    if (event == nullptr || owner_ == nullptr)
    {
        return false;
    }

    // Позиция жеста в координатах viewport нужна для zoom относительно точки касания.
    const QPointF viewportPos = event->position();

    // Ниже используется switch по enum-типу жеста.
    // Это естественный и читаемый способ разветвить логику,
    // когда у события есть конечный набор возможных режимов.
    //
    // Ветвим обработку по типу native-жеста.
    switch (event->gestureType())
    {
    case Qt::ZoomNativeGesture:
    {
        // value() содержит интенсивность zoom-жеста.
        // Это еще не готовый коэффициент масштаба, а именно "сила" жеста.
        const double delta = event->value();

        // qFuzzyIsNull используется вместо строгого сравнения с 0,
        // потому что значение жеста приходит как число с плавающей точкой.
        //
        // Некорректная или почти нулевая величина жеста
        // не должна вызывать скачка масштаба.
        if (!std::isfinite(delta) || qFuzzyIsNull(delta))
        {
            event->accept();
            return true;
        }

        // Преобразуем "силу" жеста в реальный multiplicative factor.
        // Формула exp(-delta) удобна тем, что:
        // - всегда дает положительный коэффициент;
        // - реагирует плавно;
        // - естественно чувствуется на трекпаде.
        //
        // Затем дополнительно ограничиваем коэффициент допустимыми пределами,
        // чтобы zoom не стал слишком резким или слишком слабым.
        owner_->ZoomAt(viewportPos, std::clamp(std::exp(-delta), kMinZoomFactor, kMaxZoomFactor));
        event->accept();
        return true;
    }
    case Qt::SmartZoomNativeGesture:
        // Smart zoom интерпретируем как команду вернуть график к исходному виду.
        owner_->ResetView();
        event->accept();
        return true;
    case Qt::PanNativeGesture:
    case Qt::SwipeNativeGesture:
        // Pan и swipe сводим к одному и тому же действию:
        // физическому сдвигу окна наблюдения в пикселях viewport.
        owner_->PanByPixels(event->delta());
        event->accept();
        return true;
    case Qt::BeginNativeGesture:
    case Qt::EndNativeGesture:
        // Начало и конец native-жеста просто подтверждаем,
        // чтобы цепочка событий считалась корректно обработанной.
        event->accept();
        return true;
    default:
        // Неизвестные или неиспользуемые native-жесты не обрабатываем.
        return false;
    }
}

// ----------------------------------------------------------------------------
// МЕТОД HandleGesture
// ----------------------------------------------------------------------------
// Назначение:
// Обработать обычные gesture events Qt, в первую очередь pinch.
//
// Принимает:
// - event : gesture event Qt.
//
// Возвращает:
// - bool : true, если жест обработан здесь, иначе false.
// ----------------------------------------------------------------------------
bool InteractiveChartView::HandleGesture(QGestureEvent* event)
{
    // Этот обработчик нужен для обычных Qt gesture events,
    // прежде всего для pinch-жеста, который может приходить не как native gesture,
    // а как стандартный QGesture.

    // Без события или владельца обработка невозможна.
    if (event == nullptr || owner_ == nullptr)
    {
        return false;
    }

    // Один QGestureEvent может содержать сразу несколько жестов.
    // Поэтому сначала извлекаем именно pinch-жест из контейнера событий.
    QGesture* gesture = event->gesture(Qt::PinchGesture);

    // Если внутри данного QGestureEvent pinch-жеста нет,
    // значит событие относится не к нашей зоне ответственности.
    if (gesture == nullptr)
    {
        return false;
    }

    // После проверки на nullptr приводим базовый QGesture к QPinchGesture.
    // static_cast здесь корректен, потому что мы сами запросили
    // именно жест типа Qt::PinchGesture.
    auto* pinch = static_cast<QPinchGesture*>(gesture);

    // changeFlags() - это битовая маска изменений текущего жеста.
    // Через побитовое "&" проверяем, входит ли в текущее обновление
    // именно изменение масштаба.
    //
    // QPinchGesture может приходить серией обновлений.
    // Не каждое обновление реально содержит изменение scale-factor.
    // Если именно изменения масштаба нет, жест все равно принимаем,
    // но реальный ZoomAt не вызываем.
    if ((pinch->changeFlags() & QPinchGesture::ScaleFactorChanged) == 0)
    {
        event->accept(gesture);
        return true;
    }

    // scaleFactor хранит текущий коэффициент pinch-масштабирования.
    const double scale = pinch->scaleFactor();

    // Масштаб должен быть конечным и положительным.
    // Нулевой, отрицательный или NaN-scale разрушил бы геометрию зума.
    if (!std::isfinite(scale) || scale <= 0.0)
    {
        event->accept(gesture);
        return true;
    }

    // Выполняем zoom относительно центра pinch-жеста.
    // Используем именно 1 / scale, потому что внутренняя логика PlotChartWidget::ZoomAt
    // ожидает коэффициент в том направлении, которое уже принято в этом проекте:
    // "меньше единицы" - приблизить, "больше единицы" - отдалить.
    //
    // После этого снова ограничиваем коэффициент допустимым диапазоном,
    // чтобы жест не давал экстремальных скачков масштаба.
    owner_->ZoomAt(pinch->centerPoint(), std::clamp(1.0 / scale, kMinZoomFactor, kMaxZoomFactor));
    event->accept(gesture);
    return true;
}
