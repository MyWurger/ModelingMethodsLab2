// ============================================================================
// ФАЙЛ MATHWIDGETS.CPP - ОТРИСОВКА МАТЕМАТИЧЕСКИХ ВИДЖЕТОВ LAB_2
// ============================================================================
// В этом файле реализованы все нестандартные виджеты, которые нельзя красиво
// собрать обычными QLabel/QTableView/QSpinBox без потери математического вида.
//
// Здесь живут две большие группы логики:
// 1) поведение и мелкая кастомизация полей ввода IntegerSpinBox;
// 2) ручная отрисовка математически оформленных элементов интерфейса:
//    - блока параметров варианта;
//    - формул карточек итогов;
//    - матриц и векторов в "тетрадном" стиле;
//    - заголовков карточек с индексами и шапочками.
//
// Ключевая идея файла:
// вместо тяжелой верстки из множества мелких QLabel мы вручную рисуем текст,
// индексы, скобки и шапочки через QPainter. Это дает:
// - точный контроль над геометрией;
// - единый математический стиль;
// - меньшее количество служебных QWidget внутри интерфейса.
// ============================================================================

// Подключаем заголовок с объявлениями реализуемых виджетов.
#include "MathWidgets.h"
// Подключаем единое форматирование чисел проекта.
#include "UiFormatting.h"

// QAbstractSpinBox нужен для доступа к общим режимам поведения spinbox.
#include <QAbstractSpinBox>
// QFocusEvent нужен для обработки потери фокуса полем ввода.
#include <QFocusEvent>
// QLineEdit нужен для доступа к внутреннему editor у QSpinBox.
#include <QLineEdit>
// QPainter используется для ручной отрисовки формул, матриц и заголовков.
#include <QPainter>
// QPaintEvent нужен в сигнатурах paintEvent.
#include <QPaintEvent>
// QPen нужен для настройки толщины, цвета и стиля линий скобок/шапочек.
#include <QPen>
// QRegularExpression нужен для очистки HTML-подобных маркеров из заголовков.
#include <QRegularExpression>
// QSizePolicy нужен для настройки участия кастомных виджетов в layout.
#include <QSizePolicy>

// std::max, std::fill, std::clamp используются при геометрических расчетах.
#include <algorithm>
// std::array удобно хранит фиксированные матрицы и наборы координат.
#include <array>
// std::move используется при передаче данных матриц и строк без лишних копий.
#include <utility>

// ----------------------------------------------------------------------------
// КОНСТРУКТОР IntegerSpinBox
// ----------------------------------------------------------------------------
// Назначение:
// Инициализировать улучшенный spinbox для параметров эксперимента.
//
// Принимает:
// - parent : родительский Qt-виджет.
//
// Возвращает:
// - полностью настроенный IntegerSpinBox.
// ----------------------------------------------------------------------------

IntegerSpinBox::IntegerSpinBox(QWidget* parent) : QSpinBox(parent)
{
    // Убираем встроенные кнопки-стрелки справа.
    // В проекте рядом используются собственные внешние кнопки +/-,
    // поэтому стандартные элементы QSpinBox только дублировали бы управление.
    setButtonSymbols(QAbstractSpinBox::NoButtons);

    // Включаем ускорение при удержании step-событий.
    // Если пользователь долго увеличивает значение, QSpinBox начнет менять его быстрее.
    setAccelerated(true);

    // Если введено невалидное или промежуточное значение,
    // при завершении редактирования Qt попробует привести его к ближайшему допустимому.
    setCorrectionMode(QAbstractSpinBox::CorrectToNearestValue);

    // Отключаем разделители разрядов, чтобы значения выглядели строго и компактно.
    // Для лабораторной работы это обычно предпочтительнее "1 000" или "1,000".
    setGroupSeparatorShown(false);
}

// ----------------------------------------------------------------------------
// МЕТОД SetPlaceholderText
// ----------------------------------------------------------------------------
// Назначение:
// Установить placeholder-текст во внутренний line edit spinbox.
//
// Принимает:
// - text : текст-заполнитель, видимый при пустом поле.
//
// Возвращает:
// - void.
// ----------------------------------------------------------------------------
void IntegerSpinBox::SetPlaceholderText(const QString& text)
{
    // lineEdit() возвращает внутренний редактор QSpinBox.
    // Он может отсутствовать только в экзотических состояниях,
    // поэтому доступ к нему проверяем через if с инициализацией.
    if (auto* edit = lineEdit())
    {
        // Сам placeholder хранится именно во внутреннем QLineEdit.
        edit->setPlaceholderText(text);
    }
}

// ----------------------------------------------------------------------------
// МЕТОД HasInput
// ----------------------------------------------------------------------------
// Назначение:
// Проверить, содержит ли поле осмысленный пользовательский ввод.
//
// Принимает:
// - ничего.
//
// Возвращает:
// - bool : true, если поле не пустое после trim, иначе false.
// ----------------------------------------------------------------------------
bool IntegerSpinBox::HasInput() const
{
    // Сначала получаем внутренний line edit.
    const auto* edit = lineEdit();

    // trimmed() убирает пробелы по краям,
    // чтобы строка из одних пробелов не считалась валидным вводом.
    return edit != nullptr && !edit->text().trimmed().isEmpty();
}

// ----------------------------------------------------------------------------
// МЕТОД focusOutEvent
// ----------------------------------------------------------------------------
// Назначение:
// Аккуратно обработать потерю фокуса spinbox'ом.
//
// Принимает:
// - event : событие потери фокуса.
//
// Возвращает:
// - void.
// ----------------------------------------------------------------------------
void IntegerSpinBox::focusOutEvent(QFocusEvent* event)
{
    // Запоминаем, было ли поле пустым до стандартной обработки.
    // Это важно, потому что базовый QSpinBox при потере фокуса
    // может сам попытаться синхронизировать текст и значение.
    const bool wasEmpty = !HasInput();

    // Сначала обязательно даем базовому классу выполнить штатную обработку.
    QSpinBox::focusOutEvent(event);

    // Если поле и до этого было пустым, после обработки снова очищаем текст.
    // Иначе QSpinBox мог бы подставить туда числовое значение по умолчанию,
    // а пользовательский UX здесь требует сохранить визуально пустое поле.
    if (wasEmpty)
    {
        clear();
    }
}

// ----------------------------------------------------------------------------
// МЕТОД stepBy
// ----------------------------------------------------------------------------
// Назначение:
// Переопределить шаговое изменение значения spinbox.
//
// Принимает:
// - steps : число шагов, которое нужно применить.
//
// Возвращает:
// - void.
// ----------------------------------------------------------------------------
void IntegerSpinBox::stepBy(int steps)
{
    // Если пользователь нажал внешнюю кнопку +/- при пустом поле,
    // сначала инициализируем значение минимумом допустимого диапазона.
    if (!HasInput())
    {
        setValue(minimum());
    }

    // После этого используем штатный алгоритм QSpinBox для step-изменения.
    QSpinBox::stepBy(steps);
}

// ----------------------------------------------------------------------------
// КОНСТРУКТОР VariantMathWidget
// ----------------------------------------------------------------------------
// Назначение:
// Создать виджет, вручную рисующий блок параметров варианта.
//
// Принимает:
// - parent : родительский Qt-виджет.
//
// Возвращает:
// - полностью настроенный VariantMathWidget.
// ----------------------------------------------------------------------------
VariantMathWidget::VariantMathWidget(QWidget* parent) : QWidget(parent)
{
    // Фиксируем высоту в узком диапазоне.
    // Этот виджет декоративный и имеет заранее известную композицию,
    // поэтому ему не нужна свободно растущая высота.
    setMinimumHeight(182);
    setMaximumHeight(190);

    // По ширине разрешаем растягивание, по высоте делаем фиксированным.
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

// ----------------------------------------------------------------------------
// МЕТОД sizeHint
// ----------------------------------------------------------------------------
// Назначение:
// Вернуть рекомендуемый размер виджета параметров варианта.
//
// Принимает:
// - ничего.
//
// Возвращает:
// - QSize : предпочтительный размер.
// ----------------------------------------------------------------------------
QSize VariantMathWidget::sizeHint() const
{
    return QSize(520, 182);
}

// ----------------------------------------------------------------------------
// МЕТОД paintEvent
// ----------------------------------------------------------------------------
// Назначение:
// Выполнить полную ручную отрисовку блока параметров варианта.
//
// Принимает:
// - event : событие перерисовки Qt.
//
// Возвращает:
// - void.
// ----------------------------------------------------------------------------
void VariantMathWidget::paintEvent(QPaintEvent* event)
{
    // Сам объект event здесь логически не нужен,
    // потому что мы целиком перерисовываем весь виджет.
    Q_UNUSED(event);

    // Создаем painter на текущем виджете.
    // Это стандартный Qt-способ рисования в paintEvent.
    QPainter painter(this);

    // Включаем сглаживание, чтобы скобки и текст выглядели мягко и аккуратно.
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Две базовые цветовые роли:
    // ink   - основной темный цвет для математических символов и значений;
    // muted - чуть более спокойный цвет для подписей и обозначений.
    const QColor ink("#102033");
    const QColor muted("#294560");

    // Подготавливаем набор шрифтов под разные роли.
    // Разделение шрифтов заранее удобнее, чем постоянно перенастраивать один объект.
    // Основной шрифт текстовых подписей слева: "Размерность", "Вектор", "Матрица".
    QFont labelFont("Avenir Next", 15);
    // Делаем его достаточно плотным, чтобы подписи не казались бледными.
    labelFont.setWeight(QFont::DemiBold);

    // Отдельный шрифт для коротких математических фрагментов вроде "n = 4".
    QFont mathFont("Avenir Next", 15);
    // По насыщенности держим его на том же уровне, что и подписи,
    // чтобы весь блок выглядел единообразно.
    mathFont.setWeight(QFont::DemiBold);

    // Моноширинный шрифт для чисел в матрицах и векторах.
    // Это помогает столбцам визуально лучше "собираться".
    QFont monoFont("Menlo", 15);
    // Делаем числа достаточно жирными для читаемости внутри карточки.
    monoFont.setWeight(QFont::DemiBold);

    // Уменьшенный шрифт для подстрочных индексов и знака транспонирования.
    QFont subFont("Avenir Next", 12);
    // Индексы тоже держим полужирными, иначе они теряются рядом с основным символом.
    subFont.setWeight(QFont::DemiBold);

    // Ниже определены локальные lambda-функции.
    // Это удобный способ вынести маленькие фрагменты повторяющейся логики,
    // не раздувая класс дополнительными private-методами,
    // которые нужны только внутри одного paintEvent.

    auto drawTextLabel = [&](double x, double centerY, const QString& text) {
        // Подпись рисуем label-шрифтом и muted-цветом.
        painter.setFont(labelFont);
        painter.setPen(muted);

        // QFontMetricsF дает точные размеры шрифта в дробных координатах.
        const QFontMetricsF metrics(labelFont);

        // Формула baseline нужна, чтобы текст визуально центрировался по vertical centerY.
        // ascent отвечает за часть над baseline, descent - за часть под ним.
        painter.drawText(QPointF(x, centerY + (metrics.ascent() - metrics.descent()) * 0.5), text);
    };

    auto drawIndexedLabel = [&](double x, double centerY, const QString& prefix, const QString& symbol) {
        // Эта lambda рисует конструкцию вида:
        // "Вектор m_x" или "Матрица K_x",
        // где основной символ и индекс рисуются вручную разными шрифтами.

        // Сначала выбираем спокойный muted-цвет для подписи.
        painter.setPen(muted);

        // Метрики базового label-шрифта нужны, чтобы вычислить baseline
        // и затем правильно пристыковать индекс.
        const QFontMetricsF labelMetrics(labelFont);

        // baseline - вертикальная опорная линия,
        // на которой будет стоять основной текст prefix + symbol.
        const double baseline = centerY + (labelMetrics.ascent() - labelMetrics.descent()) * 0.5;

        // Возвращаем painter к основному шрифту подписи.
        painter.setFont(labelFont);

        // Рисуем линейную часть надписи, например "Вектор m".
        painter.drawText(QPointF(x, baseline), prefix + symbol);

        // Индекс опускаем вниз и чуть сдвигаем вправо от основного символа.

        // Переключаемся на уменьшенный шрифт индекса.
        painter.setFont(subFont);

        // horizontalAdvance(prefix + symbol) дает ширину уже нарисованной основной части.
        // К ней прибавляем маленький зазор, чтобы индекс не слипался с символом.
        const double subscriptX = x + labelMetrics.horizontalAdvance(prefix + symbol) + 1.0;

        // Рисуем сам индекс "x" чуть ниже baseline,
        // чтобы он читался именно как подстрочный индекс.
        painter.drawText(QPointF(subscriptX, baseline + 5.0), "x");
    };

    // Дальше вычисляем геометрию правой математической части.
    // Виджет должен адаптироваться к разной ширине,
    // но при этом не расползаться и не ломать композицию.
    const double widgetWidth = static_cast<double>(width());

    // std::clamp удерживает левую границу матричного блока в разумном диапазоне:
    // не слишком близко к подписям слева и не слишком далеко от них.
    const double matrixLeft = std::clamp(widgetWidth * 0.40, 178.0, 205.0);

    // Правая граница ограничивается либо шириной виджета,
    // либо желаемой шириной математического блока.
    const double matrixRight = std::min(widgetWidth - 16.0, matrixLeft + 250.0);

    // Крайние центры столбцов.
    const double firstColumnX = matrixLeft + 28.0;
    const double lastColumnX = matrixRight - 28.0;

    // Поскольку столбцов ровно 4, между первым и последним центром
    // получаем 3 равных промежутка.
    const double columnStep = (lastColumnX - firstColumnX) / 3.0;

    auto columnX = [&](int column) {
        // Простейшая линейная формула центра очередного столбца.
        return firstColumnX + static_cast<double>(column) * columnStep;
    };

    auto drawCenteredMono = [&](const QString& text, double centerX, double centerY) {
        // Числа матриц и векторов рисуем моноширинным шрифтом,
        // чтобы столбцы визуально читались аккуратнее.
        painter.setFont(monoFont);
        painter.setPen(ink);

        // Рисуем значение по центру маленького прямоугольника,
        // а не через drawText в точку. Так проще получить стабильное выравнивание.
        painter.drawText(QRectF(centerX - 16.0, centerY - 11.0, 32.0, 22.0), Qt::AlignCenter, text);
    };

    auto drawBracket = [&](double left, double top, double bottom, bool openLeft) {
        // Квадратная скобка рисуется тремя линиями:
        // верхняя "губа", вертикаль и нижняя "губа".

        // Сначала настраиваем перо для скобки:
        // цвет, толщину и закругленные окончания.
        painter.setPen(QPen(ink, 2.1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));

        // lip - длина верхнего и нижнего горизонтального штриха скобки.
        const double lip = 8.0;
        if (openLeft)
        {
            // Левая открывающая скобка.

            // Верхний короткий штрих.
            painter.drawLine(QPointF(left + lip, top), QPointF(left, top));
            // Центральная вертикаль.
            painter.drawLine(QPointF(left, top), QPointF(left, bottom));
            // Нижний короткий штрих.
            painter.drawLine(QPointF(left, bottom), QPointF(left + lip, bottom));
        }
        else
        {
            // Правая закрывающая скобка.

            // Верхний короткий штрих.
            painter.drawLine(QPointF(left - lip, top), QPointF(left, top));
            // Центральная вертикаль.
            painter.drawLine(QPointF(left, top), QPointF(left, bottom));
            // Нижний короткий штрих.
            painter.drawLine(QPointF(left, bottom), QPointF(left - lip, bottom));
        }
    };

    // Сначала рисуем строку размерности.

    // Подпись слева.
    drawTextLabel(0.0, 18.0, "Размерность");
    // Шрифт для правой математической части.
    painter.setFont(mathFont);
    // Основной темный цвет математического содержимого.
    painter.setPen(ink);
    // Сама формула размерности.
    painter.drawText(QPointF(matrixLeft, 24.0), "n = 4");

    // Затем вручную рисуем вектор m_x.

    // Подпись "Вектор m_x".
    drawIndexedLabel(0.0, 70.0, "Вектор ", "m");
    // Левая скобка вектора.
    drawBracket(matrixLeft, 50.0, 90.0, true);
    // Правая скобка вектора.
    drawBracket(matrixRight, 50.0, 90.0, false);
    // Первая компонента вектора.
    drawCenteredMono("1", columnX(0), 71.0);
    // Вторая компонента вектора.
    drawCenteredMono("0", columnX(1), 71.0);
    // Третья компонента вектора.
    drawCenteredMono("1", columnX(2), 71.0);
    // Четвертая компонента вектора.
    drawCenteredMono("0", columnX(3), 71.0);
    // Шрифт для знака транспонирования.
    painter.setFont(subFont);
    // Рисуем T справа от вектора, показывая, что это столбцовый вектор в записи задания.
    painter.drawText(QPointF(matrixRight + 8.0, 56.0), "T");

    // Ниже рисуем матрицу K_x.

    // Подпись "Матрица K_x".
    drawIndexedLabel(0.0, 139.0, "Матрица ", "K");
    // Левая скобка матрицы.
    drawBracket(matrixLeft, 105.0, 181.0, true);
    // Правая скобка матрицы.
    drawBracket(matrixRight, 105.0, 181.0, false);

    // Значения матрицы варианта удобно держать в std::array,
    // потому что их размер фиксирован на этапе компиляции: 4x4.
    const std::array<std::array<QString, 4>, 4> matrixValues {{
        // Первая строка матрицы K_x.
        {QStringLiteral("3"), QStringLiteral("2"), QStringLiteral("1"), QStringLiteral("0")},
        // Вторая строка матрицы K_x.
        {QStringLiteral("2"), QStringLiteral("8"), QStringLiteral("3"), QStringLiteral("0")},
        // Третья строка матрицы K_x.
        {QStringLiteral("1"), QStringLiteral("3"), QStringLiteral("4"), QStringLiteral("0")},
        // Четвертая строка матрицы K_x.
        {QStringLiteral("0"), QStringLiteral("0"), QStringLiteral("0"), QStringLiteral("9")},
    }};
    // rowCenters хранят вертикальные центры строк матрицы.
    const std::array<double, 4> rowCenters {116.0, 134.0, 152.0, 170.0};

    // Обходим матрицу двойным циклом и рисуем каждую ячейку.
    // std::size_t естественен для индексации std::array.
    for (std::size_t row = 0; row < matrixValues.size(); ++row)
    {
        for (std::size_t column = 0; column < matrixValues[row].size(); ++column)
        {
            drawCenteredMono(matrixValues[row][column], columnX(static_cast<int>(column)), rowCenters[row]);
        }
    }
}

// ----------------------------------------------------------------------------
// КОНСТРУКТОР SummaryFormulaWidget
// ----------------------------------------------------------------------------
// Назначение:
// Создать виджет компактной формулы для карточки итогов.
//
// Принимает:
// - formulaKind : тип формулы, которую должен рисовать виджет;
// - parent      : родительский Qt-виджет.
//
// Возвращает:
// - полностью настроенный SummaryFormulaWidget.
// ----------------------------------------------------------------------------
SummaryFormulaWidget::SummaryFormulaWidget(SummaryFormulaKind formulaKind, QWidget* parent)
    : QWidget(parent), formulaKind_(formulaKind)
{
    // Для формулы нужна компактная фиксированная высота.
    setMinimumHeight(28);
    setMaximumHeight(32);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

// ----------------------------------------------------------------------------
// МЕТОД sizeHint
// ----------------------------------------------------------------------------
// Назначение:
// Вернуть рекомендуемый размер компактной формулы.
//
// Принимает:
// - ничего.
//
// Возвращает:
// - QSize : предпочтительный размер.
// ----------------------------------------------------------------------------
QSize SummaryFormulaWidget::sizeHint() const
{
    return QSize(210, 30);
}

// ----------------------------------------------------------------------------
// МЕТОД paintEvent
// ----------------------------------------------------------------------------
// Назначение:
// Нарисовать компактную формулу в стиле карточек итогов.
//
// Принимает:
// - event : событие перерисовки.
//
// Возвращает:
// - void.
// ----------------------------------------------------------------------------
void SummaryFormulaWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    // Painter создаем непосредственно на текущем виджете.
    QPainter painter(this);

    // Включаем геометрическое сглаживание,
    // чтобы шапочки и тонкие линии формул выглядели мягче.
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Все формулы рисуются одним muted-цветом,
    // чтобы не спорить визуально с большим числовым значением карточки.
    const QColor muted("#718394");
    // Базовый шрифт для основной линейной части формулы.
    QFont textFont("Avenir Next", 14);
    // Средняя насыщенность делает формулу спокойной и не слишком тяжелой.
    textFont.setWeight(QFont::Medium);

    // Уменьшенный шрифт для подстрочных индексов.
    QFont subFont("Avenir Next", 10);
    // Для индексов оставляем ту же насыщенность, чтобы стиль оставался единым.
    subFont.setWeight(QFont::Medium);

    // baseline фиксирует общую горизонталь текста.
    // Относительно нее вручную рисуются обычные символы, индексы и шапочки.
    const double baseline = 20.0;

    // x здесь играет роль "текущего пера":
    // после каждого фрагмента формулы сдвигается вправо.
    double x = 0.0;

    auto drawText = [&](const QString& text) {
        // Рисуем обычный линейный текст текущим основным шрифтом.

        // Выбираем основной шрифт формулы.
        painter.setFont(textFont);
        // Устанавливаем muted-цвет текста.
        painter.setPen(muted);
        // Вычисляем метрики текущего шрифта, чтобы знать фактическую ширину фрагмента.
        const QFontMetricsF metrics(textFont);

        // Рисуем текст в текущей x-позиции на общей baseline-линии.
        painter.drawText(QPointF(x, baseline), text);

        // После рисования сдвигаем текущую x-позицию на ширину фрагмента.
        x += metrics.horizontalAdvance(text);
    };

    auto drawSubscript = [&](const QString& text) {
        // Индекс рисуем меньшим шрифтом и ниже основного baseline.

        // Выбираем шрифт индекса.
        painter.setFont(subFont);
        // Оставляем тот же muted-цвет.
        painter.setPen(muted);
        // Метрики индекса нужны для корректного сдвига x после рисования.
        const QFontMetricsF metrics(subFont);

        // Рисуем индекс немного правее текущей позиции и ниже baseline.
        painter.drawText(QPointF(x + 1.0, baseline + 4.0), text);

        // Добавляем не только ширину индекса, но и маленький дополнительный зазор.
        x += metrics.horizontalAdvance(text) + 3.0;
    };

    auto drawHat = [&](double symbolLeft, double symbolWidth) {
        // Шапочка рисуется как две короткие линии-штриха, сходящиеся в вершине.

        // Центр шапочки совмещаем с центром символа.
        const double center = symbolLeft + symbolWidth * 0.5;
        // top задает верхнюю вертикальную координату вершины шапочки.
        const double top = 3.8;

        // Настраиваем перо именно для шапочки.
        painter.setPen(QPen(muted, 1.45, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        // Левая грань шапочки.
        painter.drawLine(QPointF(center - 4.0, top + 4.0), QPointF(center, top));
        // Правая грань шапочки.
        painter.drawLine(QPointF(center, top), QPointF(center + 4.0, top + 4.0));
    };

    auto drawSymbol = [&](const QString& symbol, bool withHat, bool withXSubscript) {
        // Этот helper рисует основной математический символ:
        // m, K, R и т.п., опционально с шапочкой и индексом X.

        // Возвращаем painter к основному шрифту формулы.
        painter.setFont(textFont);
        // Снова задаем muted-цвет для основного символа.
        painter.setPen(muted);
        // Метрики нужны, чтобы понять ширину символа и построить над ним шапочку.
        const QFontMetricsF metrics(textFont);

        // Запоминаем левую границу символа до рисования.
        const double symbolLeft = x;
        // Вычисляем ширину символа в текущем шрифте.
        const double symbolWidth = metrics.horizontalAdvance(symbol);

        // Рисуем сам основной символ в текущей позиции.
        painter.drawText(QPointF(x, baseline), symbol);
        if (withHat)
        {
            // Если нужно, дорисовываем сверху шапочку для оценки.
            drawHat(symbolLeft, symbolWidth);
        }

        // После рисования основного символа переносим "перо" вправо на его ширину.
        x += symbolWidth;
        if (withXSubscript)
        {
            // В этой формуле индекс у нас всегда именно X.
            drawSubscript("X");
        }
    };

    auto drawFSubscript = [&]() {
        // Отдельная маленькая lambda для индекса F у нормы Фробениуса.
        drawSubscript("F");
    };

    // Дальше выбираем шаблон формулы по enum-коду.
    // switch здесь естественнее длинной цепочки if/else,
    // потому что вариантов конечное, небольшое и взаимоисключающее количество.
    switch (formulaKind_)
    {
    case SummaryFormulaKind::Dimension:
        // Компактная подпись размерности.
        drawText("dim X");
        break;
    case SummaryFormulaKind::SampleSize:
        // Подпись объема выборки.
        drawText("N");
        break;
    case SummaryFormulaKind::Seed:
        // Подпись seed-параметра генератора.
        drawText("генератор U");
        break;
    case SummaryFormulaKind::MeanError:
        // Начало формулы: max |
        drawText("max |");
        // Символ m с шапочкой и индексом X.
        drawSymbol("m", true, true);
        // Разность между оценкой и теоретическим средним.
        drawText(" - ");
        // Теоретическое m без шапочки, но с индексом X.
        drawSymbol("m", false, true);
        // Закрывающая вертикальная черта модуля.
        drawText("|");
        break;
    case SummaryFormulaKind::CovarianceError:
        // Начало формулы: max |
        drawText("max |");
        // Оценка матрицы K с шапочкой и индексом X.
        drawSymbol("K", true, true);
        // Знак разности.
        drawText(" - ");
        // Теоретическая матрица K без шапочки.
        drawSymbol("K", false, true);
        // Закрывающая вертикальная черта модуля.
        drawText("|");
        break;
    case SummaryFormulaKind::FrobeniusError:
        // Открывающие двойные черты нормы.
        drawText("||");
        // Оценка матрицы K.
        drawSymbol("K", true, true);
        // Разность двух матриц.
        drawText(" - ");
        // Теоретическая матрица K.
        drawSymbol("K", false, true);
        // Закрывающие двойные черты нормы.
        drawText("||");
        // Индекс F, показывающий норму Фробениуса.
        drawFSubscript();
        break;
    }
}

// ----------------------------------------------------------------------------
// КОНСТРУКТОР MatrixDisplayWidget
// ----------------------------------------------------------------------------
// Назначение:
// Создать виджет для ручной отрисовки матрицы или вектора.
//
// Принимает:
// - parent : родительский Qt-виджет.
//
// Возвращает:
// - полностью настроенный MatrixDisplayWidget.
// ----------------------------------------------------------------------------
MatrixDisplayWidget::MatrixDisplayWidget(QWidget* parent) : QWidget(parent)
{
    // По ширине разрешаем виджету ужиматься и растягиваться по ситуации layout,
    // а по высоте даем расти вместе с содержимым.
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    setMinimumWidth(0);
    setMinimumHeight(170);
}

// ----------------------------------------------------------------------------
// МЕТОД SetMatrixData
// ----------------------------------------------------------------------------
// Назначение:
// Передать виджету новые данные матрицы и подписи строк/столбцов.
//
// Принимает:
// - values        : числовые данные матрицы;
// - columnHeaders : подписи столбцов;
// - rowHeaders    : подписи строк.
//
// Возвращает:
// - void.
// ----------------------------------------------------------------------------
void MatrixDisplayWidget::SetMatrixData(NumericMatrix values, QStringList columnHeaders, QStringList rowHeaders)
{
    // Забираем данные перемещением, чтобы не делать лишних копий.
    values_ = std::move(values);
    columnHeaders_ = std::move(columnHeaders);
    rowHeaders_ = std::move(rowHeaders);

    // updateGeometry() говорит layout-системе, что sizeHint мог измениться.
    updateGeometry();

    // update() запрашивает новую перерисовку содержимого.
    update();
}

// ----------------------------------------------------------------------------
// МЕТОД SetHorizontalPlacement
// ----------------------------------------------------------------------------
// Назначение:
// Задать режим горизонтального размещения матрицы внутри карточки.
//
// Принимает:
// - placement : режим размещения.
//
// Возвращает:
// - void.
// ----------------------------------------------------------------------------
void MatrixDisplayWidget::SetHorizontalPlacement(HorizontalPlacement placement)
{
    horizontalPlacement_ = placement;
    update();
}

// ----------------------------------------------------------------------------
// МЕТОД sizeHint
// ----------------------------------------------------------------------------
// Назначение:
// Оценить предпочтительный размер виджета матрицы по его содержимому.
//
// Принимает:
// - ничего.
//
// Возвращает:
// - QSize : рекомендуемый размер.
// ----------------------------------------------------------------------------
QSize MatrixDisplayWidget::sizeHint() const
{
    // Если матрицы нет, минимально ориентируемся хотя бы на заголовки.
    const int rows = std::max<int>(1, static_cast<int>(values_.size()));
    const int columns = values_.empty() ? std::max(1, static_cast<int>(columnHeaders_.size()))
                                        : static_cast<int>(values_.front().size());

    // baseHeight складывается из:
    // - верхнего поля;
    // - высоты заголовков столбцов;
    // - высоты всех строк значений.
    const int baseHeight = 72 + (columnHeaders_.isEmpty() ? 0 : 24) + rows * 34;

    // baseWidth - очень грубая оценка ширины:
    // базовое поле плюс типовая ширина под каждый столбец.
    const int baseWidth = 180 + columns * 68;
    return QSize(baseWidth, std::max(170, baseHeight));
}

// ----------------------------------------------------------------------------
// МЕТОД paintEvent
// ----------------------------------------------------------------------------
// Назначение:
// Выполнить полную ручную отрисовку матрицы/вектора внутри виджета.
//
// Принимает:
// - event : событие перерисовки.
//
// Возвращает:
// - void.
// ----------------------------------------------------------------------------
void MatrixDisplayWidget::paintEvent(QPaintEvent* event)
{
    // event явно не используется, потому что виджет не делает частичную отрисовку
    // по dirty-region, а каждый раз рисует матрицу целиком заново.
    Q_UNUSED(event);

    // Создаем painter и включаем оба вида сглаживания:
    // геометрическое для скобок и текстовое для аккуратных надписей.
    QPainter painter(this);

    // Antialiasing нужен для линий скобок, чтобы они не выглядели "лесенкой".
    painter.setRenderHint(QPainter::Antialiasing, true);

    // TextAntialiasing отдельно улучшает качество самих букв и цифр.
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    // contentRect немного уменьшаем относительно полной rect(),
    // чтобы оставить безопасные внутренние поля.
    const QRectF contentRect = rect().adjusted(2, 4, -2, -4);
    if (values_.empty())
    {
        // Если данных нет, рисуем только понятную заглушку по центру.

        // Серо-синий цвет заглушки делает ее вторичной по визуальному весу.
        painter.setPen(QColor("#7a8a9c"));

        // Шрифт заглушки.
        QFont emptyFont("Avenir Next", 15);

        // Полужирный вес нужен, чтобы текст пустого состояния оставался читаемым.
        emptyFont.setWeight(QFont::DemiBold);

        // Перед рисованием заглушки переключаем painter на соответствующий шрифт.
        painter.setFont(emptyFont);

        // Рисуем сообщение ровно по центру доступной области contentRect.
        painter.drawText(contentRect, Qt::AlignCenter, "Нет матричных данных");
        return;
    }

    // Размер матрицы определяем по данным.

    // Число строк берем по внешнему контейнеру values_.
    const int rowCount = static_cast<int>(values_.size());

    // Число столбцов берем по первой строке матрицы.
    const int columnCount = static_cast<int>(values_.front().size());

    // Дополнительная защита от вырожденных случаев.
    if (rowCount <= 0 || columnCount <= 0)
    {
        return;
    }

    // Однострочный объект трактуем как вектор-строку.
    // Для него действуют немного другие правила типографики и отступов.
    const bool isSingleRowVector = rowCount == 1;

    // Начальные размеры шрифтов.
    // Ниже они могут динамически уменьшаться,
    // если содержимое не помещается в доступную ширину.

    // Размер шрифта заголовков столбцов.
    int headerPointSize = 15;

    // Для однострочного вектора числа можно сделать чуть крупнее, чем для полной матрицы.
    int valuePointSize = isSingleRowVector ? 17 : 16;

    // Ниже идет набор промежуточных измеряемых величин.
    // Они вычисляются в два этапа:
    // 1) по шрифтам и контенту оцениваются реальные размеры текста;
    // 2) из них собирается геометрия итоговой матрицы.
    // Максимальная "сырая" ширина текстовых подписей строк.
    qreal rowLabelWidth = 0.0;

    // Эффективная ширина зоны подписей строк уже с внутренним запасом.
    qreal effectiveRowLabelWidth = 0.0;

    // Глобальный максимум ширины среди всех числовых значений.
    qreal maxValueWidth = 0.0;

    // Глобальный максимум ширины среди всех заголовков столбцов.
    qreal maxHeaderWidth = 0.0;

    // Поколоночно храним максимальную ширину значений.
    QVector<qreal> maxValueWidths(columnCount, 0.0);

    // Поколоночно храним максимальную ширину заголовков.
    QVector<qreal> maxHeaderWidths(columnCount, 0.0);

    // Сюда позже запишем итоговую ширину каждого столбца.
    QVector<qreal> columnWidths(columnCount, 0.0);

    // Высота строки заголовков столбцов.
    qreal columnHeaderHeight = 0.0;

    // Базовая высота строки числовых значений.
    qreal rowHeight = 34.0;
    // labelGap       - зазор между подписями строк и левой скобкой;
    // bracketGap     - зазор от скобки до области значений;
    // interColumnGap - расстояние между столбцами значений.
    // Если строковых подписей нет, зазор между ними и скобкой тоже не нужен.
    const qreal labelGap = rowHeaders_.isEmpty() ? 0.0 : (isSingleRowVector ? 14.0 : 12.0);

    // bracketGap определяет расстояние между скобкой и блоком чисел.
    const qreal bracketGap = isSingleRowVector ? 12.0 : 14.0;

    // При одном столбце межстолбцовый зазор, естественно, обнуляется.
    const qreal interColumnGap = columnCount == 1 ? 0.0 : (isSingleRowVector ? 10.0 : 10.0);

    // Шрифты текущей итерации под заголовки и значения.

    // Текущий шрифт заголовков столбцов.
    QFont headerFont("Avenir Next", headerPointSize);

    // Текущий шрифт числовых значений.
    QFont valueFont("Menlo", valuePointSize);

    auto recomputeMetrics = [&]() {
        // Эта lambda полностью пересчитывает метрики после возможного изменения размера шрифта.
        // Мы намеренно собрали все пересчеты в одном месте,
        // чтобы не дублировать один и тот же код внутри цикла подбора размеров.

        // Пересоздаем шрифт заголовков уже с текущим headerPointSize.
        headerFont = QFont("Avenir Next", headerPointSize);

        // Делаем заголовки полужирными.
        headerFont.setWeight(QFont::DemiBold);

        // Пересоздаем шрифт значений уже с текущим valuePointSize.
        valueFont = QFont("Menlo", valuePointSize);

        // Числа тоже делаем полужирными для уверенной читаемости.
        valueFont.setWeight(QFont::DemiBold);

        // Метрики нужны для точной оценки реальной занимаемой ширины текста.

        // Метрики заголовочного шрифта.
        const QFontMetricsF headerMetrics(headerFont);

        // Метрики шрифта значений.
        const QFontMetricsF valueMetrics(valueFont);

        // Ищем максимальную ширину подписи строки.

        // Перед новым проходом сбрасываем максимум.
        rowLabelWidth = 0.0;

        // Обходим все подписи строк.
        for (const QString& rowHeader : rowHeaders_)
        {
            // horizontalAdvance дает реальную горизонтальную ширину текста в текущем шрифте.
            rowLabelWidth = std::max(rowLabelWidth, headerMetrics.horizontalAdvance(rowHeader));
        }

        // effectiveRowLabelWidth - это уже не "сырая" ширина текста,
        // а ширина левого подполья с небольшим запасом.
        effectiveRowLabelWidth =
            rowHeaders_.isEmpty()
                ? 0.0
                : (isSingleRowVector ? rowLabelWidth + 2.0 : std::max<qreal>(40.0, rowLabelWidth + 2.0));

        // Оцениваем ширины заголовков столбцов по каждому столбцу отдельно.

        // Сбрасываем глобальный максимум ширины заголовков.
        maxHeaderWidth = 0.0;

        // И обнуляем поколоночный массив максимумов.
        std::fill(maxHeaderWidths.begin(), maxHeaderWidths.end(), 0.0);

        // Проходим только по тем столбцам, для которых реально есть заголовки.
        for (int column = 0; column < columnCount && column < columnHeaders_.size(); ++column)
        {
            // Измеряем ширину конкретного заголовка.
            const qreal headerWidth = headerMetrics.horizontalAdvance(columnHeaders_[column]);

            // Сохраняем ее как максимум для данного столбца.
            maxHeaderWidths[column] = headerWidth;

            // И одновременно обновляем глобальный максимум.
            maxHeaderWidth = std::max(maxHeaderWidth, headerWidth);
        }

        // Аналогично ищем максимальную фактическую ширину значений в каждом столбце.

        // Сбрасываем глобальный максимум ширины значений.
        maxValueWidth = 0.0;

        // И обнуляем поколоночные максимумы.
        std::fill(maxValueWidths.begin(), maxValueWidths.end(), 0.0);

        // Обходим все строки матрицы.
        for (const NumericVector& row : values_)
        {
            // Внутри строки обходим все валидные столбцы.
            for (int column = 0; column < columnCount && column < static_cast<int>(row.size()); ++column)
            {
                // Сначала форматируем число в том же виде, как оно будет реально нарисовано.
                const qreal valueWidth = valueMetrics.horizontalAdvance(FormatNumericValue(row[column]));

                // Обновляем максимум ширины для конкретного столбца.
                maxValueWidths[column] = std::max(maxValueWidths[column], valueWidth);

                // И глобальный максимум среди всех столбцов.
                maxValueWidth = std::max(maxValueWidth, valueWidth);
            }
        }

        // Итоговая высота заголовков и строк тоже зависит от шрифтов.

        // Для строки заголовков задаем минимально комфортную высоту.
        columnHeaderHeight = columnHeaders_.isEmpty() ? 0.0 : std::max<qreal>(22.0, headerMetrics.height());

        // Для строки значений также удерживаем нижнюю границу высоты,
        // чтобы текст не выглядел сдавленным.
        rowHeight = std::max<qreal>(30.0, valueMetrics.height() + 6.0);
    };

    // Выполняем первый расчет метрик на стартовых размерах шрифтов.
    recomputeMetrics();

    // cellWidth используется в режимах, где столбцы делаются одинаковыми.
    qreal cellWidth = 0.0;

    // matrixWidth позже будет хранить суммарную ширину всего блока чисел.
    qreal matrixWidth = 0.0;

    // Ниже цикл адаптации размеров:
    // если содержимое не помещается, постепенно уменьшаем шрифты
    // и заново пересчитываем геометрию, пока все не станет помещаться.
    for (;;)
    {
        if (isSingleRowVector)
        {
            // Для вектора-строки все столбцы делаем одинаковой ширины.
            // Это дает более чистый и "линейный" вид.

            // Берем максимум из:
            // - минимально допустимой ширины,
            // - ширины значений с запасом,
            // - ширины заголовков с запасом.
            cellWidth = std::max<qreal>(34.0, std::max(maxValueWidth + 6.0, maxHeaderWidth + 4.0));

            // Заполняем весь массив столбцов одинаковой шириной.
            std::fill(columnWidths.begin(), columnWidths.end(), cellWidth);
        }
        else
        {
            // Для обычной матрицы оцениваем доступную ширину под блок значений.
            // preferredLeftInset нужен только для режима Default:
            // он чуть смещает обычные матрицы влево относительно строгого центра,
            // чтобы композиция карточек выглядела визуально устойчивее.
            const qreal preferredLeftInset = horizontalPlacement_ == HorizontalPlacement::Center ? 0.0 : 50.0;
            const qreal availableWidth = std::max<qreal>(
                120.0,
                contentRect.width() - preferredLeftInset - effectiveRowLabelWidth - labelGap - bracketGap * 2.0);

            // naturalWidth - желаемая ширина матрицы при "естественных" размерах столбцов.

            // Сначала в naturalWidth закладываем только промежутки между столбцами.
            qreal naturalWidth = interColumnGap * (columnCount - 1);
            for (int column = 0; column < columnCount; ++column)
            {
                // Для каждого столбца выбираем разумную ширину:
                // минимум 46,
                // либо по реальным данным,
                // либо по заголовку - что больше.
                columnWidths[column] =
                    std::max<qreal>(46.0, std::max(maxValueWidths[column] + 18.0, maxHeaderWidths[column] + 14.0));

                // И добавляем эту ширину к полной естественной ширине матрицы.
                naturalWidth += columnWidths[column];
            }

            if (naturalWidth > availableWidth)
            {
                // Если естественная матрица не помещается,
                // ужимаем столбцы до равной компромиссной ширины.

                // Вычисляем единую ширину столбца исходя из доступной области.
                cellWidth = std::max<qreal>(34.0, (availableWidth - interColumnGap * (columnCount - 1)) / columnCount);

                // И равномерно назначаем ее всем столбцам.
                std::fill(columnWidths.begin(), columnWidths.end(), cellWidth);
            }
        }

        // После выбора ширин столбцов собираем суммарную ширину матричного тела.
        matrixWidth = interColumnGap * (columnCount - 1);
        for (qreal width : columnWidths)
        {
            matrixWidth += width;
        }

        // Проверяем, помещаются ли значения и заголовки в текущие столбцы.
        // Два независимых критерия:
        // - помещаются ли числовые значения;
        // - помещаются ли заголовки столбцов.
        bool fitsValues = true;
        bool fitsHeaders = true;
        for (int column = 0; column < columnCount; ++column)
        {
            // Оставляем небольшой внутренний запас в ячейке,
            // чтобы текст не прилипал вплотную к краям столбца.

            // Проверяем, хватает ли ширины под числовое значение.
            fitsValues = fitsValues && maxValueWidths[column] <= columnWidths[column] - 8.0;

            // И отдельно проверяем, хватает ли ширины под заголовок.
            fitsHeaders = fitsHeaders && maxHeaderWidths[column] <= columnWidths[column] - 6.0;
        }

        // Если все уже помещается, либо шрифты дошли до минимально приемлемых,
        // завершаем цикл адаптации.
        if ((fitsValues && fitsHeaders) || (valuePointSize <= 12 && headerPointSize <= 11))
        {
            break;
        }

        // Иначе уменьшаем шрифты по одному пункту и повторяем расчет.
        if (valuePointSize > 12)
        {
            // Сначала стараемся уменьшать шрифт значений.
            --valuePointSize;
        }
        if (headerPointSize > 11)
        {
            // Затем, если еще можно, уменьшаем и шрифт заголовков.
            --headerPointSize;
        }

        // После изменения шрифтов обязательно пересчитываем все текстовые метрики.
        recomputeMetrics();
    }

    // Дальше собираем общую геометрию блока матрицы.
    // totalBlockWidth - это полная ширина всей композиции:
    // подписи строк + зазор до скобки + тело матрицы + правая скобка.
    const qreal totalBlockWidth = effectiveRowLabelWidth + labelGap + bracketGap + matrixWidth + bracketGap;

    // centeredBlockLeft - координата левого края при идеальном центрировании всего блока.
    const qreal centeredBlockLeft =
        contentRect.left() + std::max<qreal>(0.0, (contentRect.width() - totalBlockWidth) * 0.5);

    // preferredBlockLeft - желаемый вариант размещения:
    // либо строго по центру, либо чуть левее для обычных матриц.
    const qreal preferredBlockLeft = (isSingleRowVector || horizontalPlacement_ == HorizontalPlacement::Center)
                                         ? centeredBlockLeft
                                         : contentRect.left() + 18.0;

    // maxBlockLeft защищает блок от выхода за правую границу contentRect.
    const qreal maxBlockLeft = std::max<qreal>(contentRect.left(), contentRect.right() - totalBlockWidth);

    // std::clamp окончательно зажимает блок в допустимом горизонтальном диапазоне.
    const qreal blockLeft = std::clamp(preferredBlockLeft, contentRect.left(), maxBlockLeft);
    // totalBlockHeight включает:
    // - высоту заголовков столбцов;
    // - небольшой зазор между заголовками и значениями;
    // - высоту всех строк матрицы.
    const qreal totalBlockHeight = columnHeaderHeight + 8.0 + rowHeight * rowCount;

    // blockTop подбираем так, чтобы матрица вертикально центрировалась в contentRect.
    const qreal blockTop = contentRect.top() + std::max<qreal>(0.0, (contentRect.height() - totalBlockHeight) * 0.5);

    // Ниже вычисляем итоговые ключевые точки:
    // тело матрицы, границы скобок, координаты столбцов.
    // matrixLeft - начало области чисел после зоны подписей строк и левой скобки.
    const qreal matrixLeft = blockLeft + effectiveRowLabelWidth + labelGap + bracketGap;
    // matrixTop - верхняя граница области чисел под строкой заголовков.
    const qreal matrixTop = blockTop + columnHeaderHeight + 8.0;

    // matrixHeight - общая высота всех числовых строк.
    const qreal matrixHeight = rowHeight * rowCount;

    // matrixRight - правая граница блока чисел.
    const qreal matrixRight = matrixLeft + matrixWidth;

    // bracketLeft / bracketRight - реальные x-координаты вертикалей скобок.
    const qreal bracketLeft = matrixLeft - (isSingleRowVector ? 10.0 : 12.0);
    const qreal bracketRight = matrixRight + (isSingleRowVector ? 10.0 : 12.0);
    // Верхняя точка скобок чуть выходит за тело матрицы для более "математического" вида.
    const qreal bracketTop = matrixTop - 2.0;

    // Нижняя точка скобок также чуть выступает вниз.
    const qreal bracketBottom = matrixTop + matrixHeight + 2.0;

    // columnLefts хранит левую границу каждого столбца.
    QVector<qreal> columnLefts(columnCount, matrixLeft);

    // columnLeft - бегущая координата левого края текущего столбца.
    qreal columnLeft = matrixLeft;
    for (int column = 0; column < columnCount; ++column)
    {
        columnLefts[column] = columnLeft;

        // Переходим к следующему столбцу:
        // ширина текущего столбца + зазор между столбцами.
        columnLeft += columnWidths[column] + interColumnGap;
    }

    // Сначала рисуем заголовки столбцов.

    // Включаем шрифт заголовков.
    painter.setFont(headerFont);

    // Цвет заголовков делаем чуть мягче, чем у чисел.
    painter.setPen(QColor("#294560"));

    for (int column = 0; column < columnCount && column < columnHeaders_.size(); ++column)
    {
        // Каждый заголовок получает свой собственный прямоугольник,
        // ширина которого совпадает с шириной соответствующего столбца.
        const QRectF headerRect(columnLefts[column], blockTop, columnWidths[column], columnHeaderHeight);
        painter.drawText(headerRect, Qt::AlignCenter, columnHeaders_[column]);
    }

    // Затем рисуем подписи строк, если они есть.
    for (int row = 0; row < rowCount && row < rowHeaders_.size(); ++row)
    {
        // Подпись строки выравниваем по правому краю своей области,
        // чтобы она "смотрела" в сторону матрицы и лучше читалась рядом со скобкой.
        const QRectF rowRect(blockLeft, matrixTop + rowHeight * row, effectiveRowLabelWidth, rowHeight);

        // Рисуем подпись строки с центровкой по вертикали.
        painter.drawText(rowRect, Qt::AlignVCenter | Qt::AlignRight, rowHeaders_[row]);
    }

    // Теперь рисуем основные квадратные скобки матрицы.

    // Настраиваем перо именно для скобок.
    painter.setPen(QPen(QColor("#102033"), 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));

    // lip - длина горизонтальных "полочек" скобки.
    const qreal lip = 10.0;

    // Верхняя полочка левой скобки.
    painter.drawLine(QPointF(bracketLeft + lip, bracketTop), QPointF(bracketLeft, bracketTop));

    // Вертикаль левой скобки.
    painter.drawLine(QPointF(bracketLeft, bracketTop), QPointF(bracketLeft, bracketBottom));

    // Нижняя полочка левой скобки.
    painter.drawLine(QPointF(bracketLeft, bracketBottom), QPointF(bracketLeft + lip, bracketBottom));

    // Верхняя полочка правой скобки.
    painter.drawLine(QPointF(bracketRight - lip, bracketTop), QPointF(bracketRight, bracketTop));

    // Вертикаль правой скобки.
    painter.drawLine(QPointF(bracketRight, bracketTop), QPointF(bracketRight, bracketBottom));

    // Нижняя полочка правой скобки.
    painter.drawLine(QPointF(bracketRight, bracketBottom), QPointF(bracketRight - lip, bracketBottom));

    if (isSingleRowVector)
    {
        // Для вектора-строки дополнительно дорисовываем знак транспонирования T.

        // Шрифт знака T делаем чуть меньше основного.
        QFont transposeFont("Avenir Next", 13);

        // Но сохраняем полужирность, чтобы T не потерялся визуально.
        transposeFont.setWeight(QFont::DemiBold);

        // Переключаем painter на этот шрифт.
        painter.setFont(transposeFont);

        // Цвет T совпадает с цветом заголовочных обозначений.
        painter.setPen(QColor("#294560"));

        // Рисуем знак T справа вверху от правой скобки.
        painter.drawText(QRectF(bracketRight + 6.0, bracketTop - 2.0, 20.0, 16.0), Qt::AlignLeft | Qt::AlignTop, "T");
    }

    // В конце рисуем сами числовые значения матрицы.

    // Включаем шрифт значений.
    painter.setFont(valueFont);

    // И основной темный цвет чисел.
    painter.setPen(QColor("#102033"));
    for (int row = 0; row < rowCount; ++row)
    {
        for (int column = 0; column < columnCount; ++column)
        {
            // valueRect описывает точную геометрию одной ячейки без явной сетки.
            // Сетка здесь не рисуется намеренно: матрица должна выглядеть как
            // математическая запись в скобках, а не как таблица.
            const QRectF valueRect(columnLefts[column], matrixTop + rowHeight * row, columnWidths[column], rowHeight);

            // Рисуем уже отформатированное число по центру ячейки.
            painter.drawText(valueRect, Qt::AlignCenter, FormatNumericValue(values_[row][column]));
        }
    }
}

// ----------------------------------------------------------------------------
// КОНСТРУКТОР MatrixCardTitleWidget
// ----------------------------------------------------------------------------
// Назначение:
// Создать виджет заголовка карточки с математическим оформлением.
//
// Принимает:
// - title  : исходный текст заголовка;
// - parent : родительский Qt-виджет.
//
// Возвращает:
// - полностью настроенный MatrixCardTitleWidget.
// ----------------------------------------------------------------------------
MatrixCardTitleWidget::MatrixCardTitleWidget(QString title, QWidget* parent)
    : QWidget(parent), parts_(BuildParts(std::move(title)))
{
    // Заголовок имеет компактную фиксированную высоту.
    setMinimumHeight(38);
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
}

// ----------------------------------------------------------------------------
// МЕТОД sizeHint
// ----------------------------------------------------------------------------
// Назначение:
// Вернуть рекомендуемый размер виджета заголовка карточки.
//
// Принимает:
// - ничего.
//
// Возвращает:
// - QSize : предпочтительный размер.
// ----------------------------------------------------------------------------
QSize MatrixCardTitleWidget::sizeHint() const
{
    return QSize(360, 38);
}

// ----------------------------------------------------------------------------
// МЕТОД paintEvent
// ----------------------------------------------------------------------------
// Назначение:
// Нарисовать составной заголовок карточки с индексами и шапочками.
//
// Принимает:
// - event : событие перерисовки.
//
// Возвращает:
// - void.
// ----------------------------------------------------------------------------
void MatrixCardTitleWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    // Создаем painter и включаем оба вида сглаживания для качественной типографики.
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    // titleFont - основной шрифт заголовка.
    // subFont   - уменьшенный шрифт для нижних индексов.
    QFont titleFont("Avenir Next", 18);
    titleFont.setWeight(QFont::Bold);
    QFont subFont("Avenir Next", 12);
    subFont.setWeight(QFont::DemiBold);

    const QFontMetricsF titleMetrics(titleFont);
    const QFontMetricsF subMetrics(subFont);
    // Сначала измеряем итоговую ширину составного заголовка,
    // чтобы потом целиком отцентрировать его внутри виджета.
    const qreal totalWidth = MeasureWidth(titleMetrics, subMetrics);
    qreal x = std::max<qreal>(0.0, (width() - totalWidth) * 0.5);
    const qreal baseline = (height() + titleMetrics.ascent() - titleMetrics.descent()) * 0.5 + 2.0;

    painter.setPen(QColor("#102033"));
    for (const TitlePart& part : parts_)
    {
        // Каждую часть сначала рисуем основным заголовочным шрифтом.
        painter.setFont(titleFont);
        const qreal symbolLeft = x;
        const qreal symbolWidth = titleMetrics.horizontalAdvance(part.text);
        painter.drawText(QPointF(x, baseline), part.text);

        if (part.hasHat)
        {
            // Если у части есть шапочка, рисуем ее над основным символом.
            DrawHat(painter, symbolLeft, symbolWidth, baseline - titleMetrics.ascent() - 1.0);
        }

        x += symbolWidth;
        if (!part.subscript.isEmpty())
        {
            // Подстрочный индекс рисуем уже меньшим шрифтом и ниже baseline.
            painter.setFont(subFont);
            painter.drawText(QPointF(x + 1.0, baseline + 5.0), part.subscript);
            x += subMetrics.horizontalAdvance(part.subscript) + 2.0;
        }
    }
}

// ----------------------------------------------------------------------------
// МЕТОД BuildParts
// ----------------------------------------------------------------------------
// Назначение:
// Разобрать исходный текст заголовка на логические части для ручной отрисовки.
//
// Принимает:
// - title : исходная строка заголовка.
//
// Возвращает:
// - QVector<TitlePart> : набор фрагментов заголовка.
// ----------------------------------------------------------------------------
QVector<MatrixCardTitleWidget::TitlePart> MatrixCardTitleWidget::BuildParts(QString title)
{
    // Здесь идет ручное сопоставление известных шаблонов заголовков
    // с их математически оформленным представлением.
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

    // Если строка не попала в известные шаблоны,
    // удаляем из нее HTML-подобные теги и сущности максимально мягко
    // и используем как обычный текстовый заголовок.
    title.remove(QRegularExpression("<[^>]*>"));
    title.replace("&#770;", QString());
    return {{title}};
}

// ----------------------------------------------------------------------------
// МЕТОД MeasureWidth
// ----------------------------------------------------------------------------
// Назначение:
// Измерить суммарную ширину составного заголовка.
//
// Принимает:
// - titleMetrics : метрики основного шрифта;
// - subMetrics   : метрики шрифта индексов.
//
// Возвращает:
// - qreal : итоговую ширину заголовка.
// ----------------------------------------------------------------------------
qreal MatrixCardTitleWidget::MeasureWidth(const QFontMetricsF& titleMetrics, const QFontMetricsF& subMetrics) const
{
    qreal width = 0.0;
    for (const TitlePart& part : parts_)
    {
        // Основная часть измеряется title-шрифтом.
        width += titleMetrics.horizontalAdvance(part.text);
        if (!part.subscript.isEmpty())
        {
            // Для индекса учитываем его собственную ширину и маленький зазор.
            width += subMetrics.horizontalAdvance(part.subscript) + 2.0;
        }
    }
    return width;
}

// ----------------------------------------------------------------------------
// МЕТОД DrawHat
// ----------------------------------------------------------------------------
// Назначение:
// Нарисовать "шапочку" над символом математической оценки.
//
// Принимает:
// - painter     : painter для рисования;
// - symbolLeft  : левая граница символа;
// - symbolWidth : ширина символа;
// - top         : верхняя координата области шапочки.
//
// Возвращает:
// - void.
// ----------------------------------------------------------------------------
void MatrixCardTitleWidget::DrawHat(QPainter& painter, qreal symbolLeft, qreal symbolWidth, qreal top)
{
    // Центр шапочки совмещаем с центром символа.
    const qreal center = symbolLeft + symbolWidth * 0.5;

    // save/restore локализует изменение пера только внутри этой функции.
    // Это хороший Qt-паттерн для маленьких служебных операций рисования.
    painter.save();
    painter.setPen(QPen(QColor("#102033"), 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));

    // Шапочка строится двумя короткими диагоналями, сходящимися в вершине.
    painter.drawLine(QPointF(center - 5.0, top + 5.0), QPointF(center, top));
    painter.drawLine(QPointF(center, top), QPointF(center + 5.0, top + 5.0));
    painter.restore();
}
