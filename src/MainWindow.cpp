// ============================================================================
// ФАЙЛ MAINWINDOW.CPP - РЕАЛИЗАЦИЯ ГЛАВНОГО ОКНА LAB_2
// ============================================================================
// Этот файл содержит:
// - сборку главного интерфейса лабораторной работы;
// - локальные фабрики карточек, таблиц и полей ввода;
// - связывание GUI с вычислительным движком;
// - запуск расчета в фоне;
// - обновление всех вкладок после завершения моделирования.
//
// По архитектуре MainWindow - это верхний координирующий слой GUI.
// Он сам не выполняет математические вычисления, а:
// 1) собирает параметры пользователя;
// 2) передает их в SamplingEngine;
// 3) принимает готовый SamplingResult;
// 4) раскладывает результат по таблицам, графикам и карточкам.
// ============================================================================

// Основной заголовок класса MainWindow.
#include "MainWindow.h"
// Кастомные математические виджеты: spinbox, матрицы, формулы и т.п.
#include "MathWidgets.h"
// Фабрики подготовленных данных для представления результата в GUI.
#include "PresentationBuilders.h"
// Вычислительный движок моделирования коррелированных случайных величин.
#include "SamplingEngine.h"
// Экспорт таблиц в PNG.
#include "TableExport.h"
// Модели данных для QTableView.
#include "TableModels.h"
// Единое форматирование чисел и подписей интерфейса.
#include "UiFormatting.h"

// Базовые политики выбора и редактирования таблиц.
#include <QAbstractItemView>
// Компоновка вида "подпись -> поле".
#include <QFormLayout>
// Карточки интерфейса удобно строить на основе QFrame.
#include <QFrame>
// Watcher нужен для отслеживания завершения фонового расчета.
#include <QFutureWatcher>
// Сетка используется для раскладки некоторых наборов карточек.
#include <QGridLayout>
// Группировка блоков параметров в оформленные секции.
#include <QGroupBox>
// Управление заголовками таблиц.
#include <QHeaderView>
// Горизонтальные компоновки для строк, заголовков и панелей кнопок.
#include <QHBoxLayout>
// Текстовые подписи и заголовки интерфейса.
#include <QLabel>
// Локаль нужна для корректного отображения чисел и текста.
#include <QLocale>
// Палитра используется для точечной настройки цветов полей ввода.
#include <QPalette>
// Кнопки запуска, экспорта и пользовательских действий.
#include <QPushButton>
// Scroll area нужна для больших прокручиваемых областей с результатами.
#include <QScrollArea>
// QSizePolicy управляет тем, как виджеты растягиваются в layout.
#include <QSizePolicy>
// Базовый тип для числовых полей ввода.
#include <QSpinBox>
// Splitter делит окно на левую панель параметров и правую панель результатов.
#include <QSplitter>
// QStyle используется для некоторых базовых метрик и визуальной интеграции.
#include <QStyle>
// Вкладки для переключения между итогами, матрицами, гистограммами и т.д.
#include <QTabWidget>
// Основной виджет отображения табличных данных.
#include <QTableView>
// Вертикальные компоновки для колонок виджетов.
#include <QVBoxLayout>
// Базовый QWidget используется как контейнер для составных блоков.
#include <QWidget>

// QtConcurrent::run позволяет запускать расчет вне GUI-потока.
#include <QtConcurrent/QtConcurrentRun>

// std::min, std::max, std::clamp и другие алгоритмы STL.
#include <algorithm>
// std::array используется для компактных фиксированных наборов.
#include <array>
// numeric_limits нужен для безопасных граничных значений.
#include <limits>

namespace
{
// Анонимный namespace ограничивает видимость всех helper-символов только этим .cpp.
// Это правильнее, чем делать их extern или тащить в заголовок:
// они не являются частью публичного API класса MainWindow.

// Рекомендованный верхний предел объема выборки.
// Это не жесткий математический лимит, а практический порог,
// после которого GUI и пользовательский сценарий становятся тяжелыми.
constexpr int kMaxRecommendedSampleSize = 500000;

// Рекомендованное максимальное число столбцов гистограммы.
// Слишком большое число бинов делает график шумным и плохо читаемым.
constexpr int kMaxRecommendedHistogramBins = 100;

// Рекомендованный максимум наблюдений для предварительного просмотра.
// Полную выборку можно хранить отдельно, но в preview и на графиках
// нет смысла бездумно показывать гигантские массивы точек.
constexpr int kMaxRecommendedPreviewCount = 3000;

// ----------------------------------------------------------------------------
// ФУНКЦИЯ ApplyInputPalette - ПАЛИТРА ДЛЯ ПОЛЕЙ ВВОДА
// ----------------------------------------------------------------------------
// Приводит палитру поля ввода к единому светлому стилю LAB_2.
//
// Принимает:
// - widget: любой виджет ввода, у которого есть palette()/setPalette().
//
// Возвращает:
// - ничего.
// ----------------------------------------------------------------------------
template <typename TWidget>
void ApplyInputPalette(TWidget* widget)
{
    // Забираем текущую палитру виджета как основу,
    // чтобы менять только нужные нам роли цветов.
    QPalette palette = widget->palette();

    // Base отвечает за фон области ввода.
    palette.setColor(QPalette::Base, QColor("#ffffff"));

    // Text - основной цвет вводимого текста.
    palette.setColor(QPalette::Text, QColor("#102033"));

    // WindowText нужен для некоторых вложенных подписей и состояний.
    palette.setColor(QPalette::WindowText, QColor("#102033"));

    // ButtonText влияет на текст внутренних кнопок/стрелок, если они есть у виджета.
    palette.setColor(QPalette::ButtonText, QColor("#102033"));

    // Highlight - цвет выделения текста или активной части ввода.
    palette.setColor(QPalette::Highlight, QColor("#2a6df4"));

    // HighlightedText - цвет текста на фоне выделения.
    palette.setColor(QPalette::HighlightedText, QColor("#ffffff"));

    // PlaceholderText - цвет текста-подсказки в пустом поле.
    palette.setColor(QPalette::PlaceholderText, QColor("#7a8a9c"));

    // Применяем собранную палитру обратно к виджету.
    widget->setPalette(palette);
}

// ----------------------------------------------------------------------------
// ФУНКЦИЯ CreateSpinField - СОСТАВНОЕ ПОЛЕ SPINBOX С КНОПКАМИ
// ----------------------------------------------------------------------------
// Собирает большое поле ввода в стиле LAB_1/LAB_2:
// слева сам IntegerSpinBox, справа вертикальная колонка кнопок + и -.
//
// Принимает:
// - spinBox: выходной параметр, через который наружу возвращается созданный spinbox;
// - initialValue: стартовое значение поля;
// - placeholder: текст-подсказка для пустого поля;
// - parent: родительский виджет.
//
// Возвращает:
// - QWidget*: контейнер, внутри которого уже лежат spinbox и кнопки изменения значения.
// ----------------------------------------------------------------------------
QWidget* CreateSpinField(QSpinBox*& spinBox, int initialValue, const QString& placeholder, QWidget* parent)
{
    // Внешний контейнер нужен, потому что одно поле состоит сразу из двух частей:
    // самого spinbox и отдельной вертикальной колонны управляющих кнопок.
    auto* container = new QWidget(parent);

    // По горизонтали контейнер может растягиваться,
    // по вертикали его высота должна оставаться фиксированной.
    container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    // Внутри контейнера используем горизонтальную компоновку.
    auto* layout = new QHBoxLayout(container);

    // Внешние отступы убираем, чтобы композитное поле выглядело единым блоком.
    layout->setContentsMargins(0, 0, 0, 0);

    // Небольшой зазор между полем и колонкой кнопок.
    layout->setSpacing(8);

    // Создаем кастомный IntegerSpinBox, а не обычный QSpinBox,
    // потому что у него в проекте уже настроено нужное поведение и стиль.
    spinBox = new IntegerSpinBox(container);

    // Минимальная ширина самого поля.
    spinBox->setMinimumWidth(118);

    // Минимальная высота поля.
    spinBox->setMinimumHeight(42);

    // Поле может тянуться по ширине, но не по высоте.
    spinBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    // Устанавливаем стартовое числовое значение.
    spinBox->setValue(initialValue);

    // SetPlaceholderText есть именно у IntegerSpinBox,
    // поэтому здесь нужен безопасный downcast к конкретному типу.
    static_cast<IntegerSpinBox*>(spinBox)->SetPlaceholderText(placeholder);

    // Отдельный контейнер под вертикальную колонку кнопок + и -.
    auto* buttonsHost = new QWidget(container);

    // objectName позволяет применить к колонке адресный QSS-стиль.
    buttonsHost->setObjectName("spinButtonColumn");

    // Колонка кнопок должна иметь фиксированную узкую ширину.
    buttonsHost->setFixedWidth(30);

    // Размер контейнера кнопок фиксирован по обеим осям.
    buttonsHost->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    // Внутри колонки кнопок используем вертикальную компоновку.
    auto* buttonsLayout = new QVBoxLayout(buttonsHost);

    // У колонки кнопок тоже убираем лишние внешние поля.
    buttonsLayout->setContentsMargins(0, 0, 0, 0);

    // Маленький зазор между + и -.
    buttonsLayout->setSpacing(4);

    // Кнопка увеличения значения.
    auto* plusButton = new QPushButton("+", buttonsHost);

    // Кнопка уменьшения значения.
    auto* minusButton = new QPushButton("-", buttonsHost);

    // Обе кнопки получают один и тот же style hook.
    plusButton->setObjectName("spinAdjustButton");
    minusButton->setObjectName("spinAdjustButton");

    // Фиксированный размер верхней кнопки.
    plusButton->setFixedSize(30, 18);

    // Фиксированный размер нижней кнопки.
    minusButton->setFixedSize(30, 18);

    // Клик по + проксируется в стандартный шаг вверх у spinbox.
    QObject::connect(plusButton, &QPushButton::clicked, spinBox, [spinBox]() { spinBox->stepUp(); });

    // Клик по - проксируется в стандартный шаг вниз у spinbox.
    QObject::connect(minusButton, &QPushButton::clicked, spinBox, [spinBox]() { spinBox->stepDown(); });

    // Кладем кнопку + наверх.
    buttonsLayout->addWidget(plusButton);

    // Кладем кнопку - под ней.
    buttonsLayout->addWidget(minusButton);

    // Stretch снизу поджимает пару кнопок вверх и не дает им расползаться по высоте.
    buttonsLayout->addStretch();

    // Основное поле занимает всю доступную ширину строки.
    layout->addWidget(spinBox, 1);

    // Колонка кнопок ставится справа и выравнивается по вертикальному центру.
    layout->addWidget(buttonsHost, 0, Qt::AlignVCenter);

    // Возвращаем готовый композитный контейнер целиком.
    return container;
}

// ----------------------------------------------------------------------------
// ФУНКЦИЯ SetFieldContainerEnabled - ВКЛЮЧЕНИЕ/ОТКЛЮЧЕНИЕ ПОЛЯ С КОНТЕЙНЕРОМ
// ----------------------------------------------------------------------------
// Аккуратно включает или выключает поле ввода.
// Если поле обернуто в контейнер, то состояние применяется к контейнеру целиком,
// чтобы визуально отключались и само поле, и связанные с ним кнопки.
//
// Принимает:
// - field: целевой виджет поля ввода;
// - enabled: новое состояние активности.
//
// Возвращает:
// - ничего.
// ----------------------------------------------------------------------------
void SetFieldContainerEnabled(QWidget* field, bool enabled)
{
    if (field == nullptr)
    {
        // Для пустого указателя делать нечего.
        return;
    }

    if (auto* container = field->parentWidget())
    {
        // Если у поля есть родитель-контейнер, логичнее отключить весь блок целиком.
        container->setEnabled(enabled);
    }
    else
    {
        // В противном случае меняем состояние самого поля.
        field->setEnabled(enabled);
    }
}

// ----------------------------------------------------------------------------
// ФУНКЦИЯ ConfigureMatrixTable - БАЗОВАЯ НАСТРОЙКА ТАБЛИЦ МАТРИЦ
// ----------------------------------------------------------------------------
// Применяет единые правила поведения и оформления к компактным таблицам матриц.
//
// Принимает:
// - table: целевой QTableView.
//
// Возвращает:
// - ничего.
// ----------------------------------------------------------------------------
void ConfigureMatrixTable(QTableView* table)
{
    // Чередование цветов строк повышает читаемость таблиц.
    table->setAlternatingRowColors(true);

    // Выделяется вся строка целиком, а не отдельная ячейка.
    table->setSelectionBehavior(QAbstractItemView::SelectRows);

    // Одновременно разрешаем выбирать только одну строку.
    table->setSelectionMode(QAbstractItemView::SingleSelection);

    // Редактирование таблиц пользователем отключено:
    // они служат только для просмотра расчетных данных.
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // Перенос слов выключаем, чтобы не ломать табличную геометрию.
    table->setWordWrap(false);

    // Все столбцы растягиваем на доступную ширину таблицы.
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    // Горизонтальные заголовки выравниваем по центру.
    table->horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);

    // Вертикальные заголовки тоже по центру.
    table->verticalHeader()->setDefaultAlignment(Qt::AlignCenter);

    // Минимальная высота не дает карточке схлопнуться слишком сильно.
    table->setMinimumHeight(180);
}

// ----------------------------------------------------------------------------
// ФУНКЦИЯ ConfigureSampleTable - НАСТРОЙКА ТАБЛИЦЫ ВЫБОРКИ
// ----------------------------------------------------------------------------
// Поверх базовой конфигурации матричной таблицы применяет правила
// именно для большой таблицы выборки.
//
// Принимает:
// - table: целевой QTableView.
//
// Возвращает:
// - ничего.
// ----------------------------------------------------------------------------
void ConfigureSampleTable(QTableView* table)
{
    // Сначала применяем общий набор правил для табличных карточек.
    ConfigureMatrixTable(table);

    // Вертикальный header скрываем:
    // номер наблюдения у выборки уже есть в первом столбце.
    table->verticalHeader()->setVisible(false);

    // Для большой выборки нужна увеличенная минимальная высота.
    table->setMinimumHeight(420);
}

// ----------------------------------------------------------------------------
// ФУНКЦИЯ CreateCardWithTable - КАРТОЧКА С ТАБЛИЦЕЙ И PNG-ЭКСПОРТОМ
// ----------------------------------------------------------------------------
// Собирает стандартную карточку таблицы:
// заголовок, кнопку "Сохранить PNG" и сам QTableView.
//
// Принимает:
// - title: заголовок карточки;
// - table: уже подготовленная таблица;
// - parent: родительский виджет.
//
// Возвращает:
// - QWidget*: готовая карточка-обертка.
// ----------------------------------------------------------------------------
QWidget* CreateCardWithTable(const QString& title, QTableView* table, QWidget* parent)
{
    // Внешняя карточка на основе QFrame.
    auto* card = new QFrame(parent);

    // plotCard - общий hook для QSS-стиля карточек проекта.
    card->setObjectName("plotCard");

    // Внутри карточки используем вертикальную компоновку.
    auto* layout = new QVBoxLayout(card);

    // Внутренние поля карточки.
    layout->setContentsMargins(12, 12, 12, 12);

    // Зазор между шапкой и таблицей.
    layout->setSpacing(10);

    // Заголовок карточки таблицы.
    auto* titleLabel = new QLabel(title, card);

    // objectName нужен для стилизации именно заголовков таблиц.
    titleLabel->setObjectName("tableTitle");

    // RichText разрешает заголовкам содержать HTML-подобную математическую разметку.
    titleLabel->setTextFormat(Qt::RichText);

    // Перенос отключен, чтобы заголовок не разваливался на две строки без необходимости.
    titleLabel->setWordWrap(false);

    // Заголовок может тянуться по ширине.
    titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    // Горизонтальная шапка карточки: слева заголовок, справа кнопка экспорта.
    auto* headerLayout = new QHBoxLayout();

    // Шапка не должна иметь лишних внешних полей.
    headerLayout->setContentsMargins(0, 0, 0, 0);

    // Зазор между элементами шапки.
    headerLayout->setSpacing(10);

    // Кнопка экспорта таблицы в PNG.
    auto* exportButton = new QPushButton("Сохранить PNG", card);

    // Отдельный style hook для вторичной кнопки.
    exportButton->setObjectName("secondaryButton");

    // Минимальная высота кнопки.
    exportButton->setMinimumHeight(32);

    // Минимальная ширина кнопки.
    exportButton->setMinimumWidth(148);

    // Tooltip поясняет действие кнопки.
    exportButton->setToolTip("Сохранить эту таблицу в PNG");

    // При клике экспортируем именно ту таблицу, которая вложена в текущую карточку.
    QObject::connect(exportButton, &QPushButton::clicked, card, [card, table, title]() {
        ExportTableViewPng(card, table, title);
    });

    // Заголовок занимает всю доступную ширину шапки.
    headerLayout->addWidget(titleLabel, 1);

    // Stretch отталкивает кнопку экспорта вправо.
    headerLayout->addStretch();

    // Сама кнопка кладется в правую часть шапки.
    headerLayout->addWidget(exportButton);

    // Сначала добавляем шапку.
    layout->addLayout(headerLayout);

    // Затем добавляем таблицу как основное содержимое карточки.
    layout->addWidget(table, 1);

    // Возвращаем полностью собранную карточку.
    return card;
}

// ----------------------------------------------------------------------------
// ФУНКЦИЯ CreateCardWithWidget - КАРТОЧКА С ПРОИЗВОЛЬНЫМ CONTENT WIDGET
// ----------------------------------------------------------------------------
// Собирает карточку под кастомный виджет, например под отрисованную матрицу.
//
// Принимает:
// - title: заголовок карточки;
// - contentWidget: вложенный содержательный виджет;
// - parent: родительский виджет.
//
// Возвращает:
// - QWidget*: готовая карточка.
// ----------------------------------------------------------------------------
QWidget* CreateCardWithWidget(const QString& title,
                              QWidget* contentWidget,
                              QWidget* parent)
{
    // Внешний контейнер карточки.
    auto* card = new QFrame(parent);
    card->setObjectName("plotCard");

    // Карточка должна тянуться по ширине, но иметь контролируемую высоту.
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    // Нижняя и верхняя граница высоты здесь совпадают,
    // то есть карточка получает фактически фиксированную высоту.
    card->setMinimumHeight(280);
    card->setMaximumHeight(280);

    // Внутренняя вертикальная компоновка карточки.
    auto* layout = new QVBoxLayout(card);

    // Боковые отступы убраны, чтобы математический виджет занимал ширину почти целиком.
    layout->setContentsMargins(0, 4, 0, 4);

    // Между заголовком и контентом лишний зазор не нужен.
    layout->setSpacing(0);

    // Для математически оформленного заголовка используем специальный виджет,
    // а не обычный QLabel.
    auto* titleLabel = new MatrixCardTitleWidget(title, card);

    // Отдельная горизонтальная строка под заголовок.
    auto* headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(0);

    // Симметричный отступ слева.
    headerLayout->addSpacing(14);

    // Сам заголовок занимает центральную часть шапки.
    headerLayout->addWidget(titleLabel, 1);

    // Симметричный отступ справа.
    headerLayout->addSpacing(14);

    // Добавляем шапку карточки.
    layout->addLayout(headerLayout);

    // Добавляем основной контент.
    layout->addWidget(contentWidget, 1);

    // Возвращаем карточку с кастомным содержимым.
    return card;
}

// ----------------------------------------------------------------------------
// ФУНКЦИЯ CreateSummaryMetricCard - КАРТОЧКА ИТОГОВОЙ МЕТРИКИ
// ----------------------------------------------------------------------------
// Собирает компактную карточку для итоговой числовой метрики:
// заголовок, крупное значение и подпись-формулу.
//
// Принимает:
// - title: заголовок метрики;
// - formulaKind: тип формулы для нижней подписи;
// - valueLabel: выходной параметр, через который наружу возвращается QLabel значения;
// - parent: родительский виджет.
//
// Возвращает:
// - QFrame*: готовая карточка итоговой метрики.
// ----------------------------------------------------------------------------
QFrame* CreateSummaryMetricCard(const QString& title,
                                SummaryFormulaKind formulaKind,
                                QLabel*& valueLabel,
                                QWidget* parent)
{
    // Создаем внешнюю карточку.
    auto* card = new QFrame(parent);
    card->setObjectName("summaryMetricCard");

    // Минимальная высота карточки.
    card->setMinimumHeight(108);

    // По ширине карточка может тянуться, по высоте остается фиксированной.
    card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    // Внутренняя вертикальная компоновка карточки метрики.
    auto* layout = new QVBoxLayout(card);

    // Внутренние поля карточки.
    layout->setContentsMargins(16, 14, 16, 14);

    // Зазор между заголовком, числом и формулой.
    layout->setSpacing(8);

    // Текстовый заголовок метрики.
    auto* titleLabel = new QLabel(title, card);
    titleLabel->setObjectName("summaryMetricTitle");

    // Разрешаем rich text, потому что заголовки могут содержать математические символы.
    titleLabel->setTextFormat(Qt::RichText);

    // valueLabel возвращается наружу по ссылке,
    // чтобы MainWindow потом мог быстро обновлять само число.
    valueLabel = new QLabel("—", card);
    valueLabel->setObjectName("summaryMetricValue");

    // Само число отображаем как plain text, без HTML-разметки.
    valueLabel->setTextFormat(Qt::PlainText);

    // Нижняя подпись-формула рисуется отдельным кастомным виджетом.
    auto* formulaWidget = new SummaryFormulaWidget(formulaKind, card);
    formulaWidget->setObjectName("summaryMetricFormula");

    // Сверху кладем заголовок.
    layout->addWidget(titleLabel);

    // Затем большое числовое значение.
    layout->addWidget(valueLabel);

    // И внизу краткую формульную подпись.
    layout->addWidget(formulaWidget);

    // Возвращаем полностью собранную карточку.
    return card;
}

} // namespace

// ----------------------------------------------------------------------------
// КОНСТРУКТОР MainWindow
// ----------------------------------------------------------------------------
// Создает главное окно, инициализирует watcher фонового расчета,
// строит интерфейс, применяет тему, подключает сигналы
// и переводит окно в стартовое состояние "готово к расчету".
//
// Принимает:
// - parent: родительский QWidget.
//
// Возвращает:
// - ничего.
// ----------------------------------------------------------------------------
MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    // Watcher нужен, чтобы получить сигнал о завершении расчета,
    // который запускается через QtConcurrent в отдельном потоке.
    // Родителем делаем this, чтобы не управлять временем жизни вручную.
    simulationWatcher_ = new QFutureWatcher<SamplingResult>(this);

    // Строим все дерево виджетов главного окна.
    BuildUi();

    // Применяем общую визуальную тему ко всем собранным элементам.
    ApplyTheme();

    // Соединяем кнопки, поля и watcher с обработчиками MainWindow.
    SetupSignals();

    // Очищаем графики, таблицы и метрики до стартового пустого состояния.
    ResetOutputs();

    // Показываем пользователю стартовый статус готовности.
    ShowStatus("Готово к моделированию варианта 1 по заданной матрице K<sub>X</sub>.", true);
}

// ----------------------------------------------------------------------------
// МЕТОД BuildUi - ПОЛНАЯ СБОРКА ДЕРЕВА ВИДЖЕТОВ ГЛАВНОГО ОКНА
// ----------------------------------------------------------------------------
// Создает все основные GUI-элементы:
// - левую колонку параметров;
// - правую колонку результатов;
// - вкладки итогов, матриц, гистограмм, траекторий и выборки;
// - все таблицы, карточки и графики.
//
// Принимает:
// - ничего.
//
// Возвращает:
// - ничего.
// ----------------------------------------------------------------------------
void MainWindow::BuildUi()
{
    // Заголовок системного окна приложения.
    setWindowTitle("Лабораторная работа 2 — моделирование коррелированных случайных величин");

    // Стартовый размер окна при первом показе.
    resize(1600, 980);

    // Минимальный размер защищает интерфейс от полного развала компоновки.
    setMinimumSize(1360, 840);

    // central - корневой QWidget, который будет установлен как central widget QMainWindow.
    auto* central = new QWidget(this);

    // На central сразу создаем корневую вертикальную компоновку.
    auto* rootLayout = new QVBoxLayout(central);

    // Внешние поля вокруг всего содержимого окна.
    rootLayout->setContentsMargins(10, 10, 10, 10);

    // Между единственным корневым элементом splitter дополнительный зазор не нужен.
    rootLayout->setSpacing(0);

    // splitter делит окно на левую панель управления и правую панель результатов.
    auto* splitter = new QSplitter(Qt::Horizontal, central);

    // Запрещаем полное схлопывание одной из панелей.
    splitter->setChildrenCollapsible(false);

    // Делаем ручку splitter узкой и аккуратной.
    splitter->setHandleWidth(4);

    // controlCard - левая колонка параметров и кнопки запуска.
    auto* controlCard = new QWidget(splitter);

    // objectName нужен для QSS-стилизации именно левой панели.
    controlCard->setObjectName("controlCard");

    // Минимальная ширина левой панели.
    controlCard->setMinimumWidth(440);

    // Максимальная ширина левой панели, чтобы она не разрасталась чрезмерно.
    controlCard->setMaximumWidth(590);

    // Внутри controlCard используем вертикальную компоновку.
    auto* controlLayout = new QVBoxLayout(controlCard);

    // Внутренние поля панели управления.
    controlLayout->setContentsMargins(18, 18, 18, 18);

    // Зазор между блоками на левой панели.
    controlLayout->setSpacing(12);

    // Главный заголовок левой панели.
    auto* titleLabel = new QLabel("Моделирование коррелированных случайных величин", controlCard);

    // objectName нужен для отдельного оформления этого заголовка.
    titleLabel->setObjectName("titleLabel");

    // Разрешаем перенос, потому что заголовок длинный и должен вести себя устойчиво.
    titleLabel->setWordWrap(true);

    // parametersGroup - оформленная секция параметров эксперимента.
    auto* parametersGroup = new QGroupBox("Параметры эксперимента", controlCard);

    // objectName нужен для адресной стилизации группы параметров.
    parametersGroup->setObjectName("parametersGroup");

    // Внутри группы параметров используем form-layout: подпись слева, поле справа.
    auto* parametersForm = new QFormLayout(parametersGroup);

    // Подписи полей выравниваем влево и по вертикальному центру.
    parametersForm->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    // Вся форма прижимается к верхней части группы.
    parametersForm->setFormAlignment(Qt::AlignTop);

    // Поля имеют право расти по ширине, занимая доступное место.
    parametersForm->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    // Запрещаем автоматический перенос строк формы.
    parametersForm->setRowWrapPolicy(QFormLayout::DontWrapRows);

    // Внутренние отступы группы параметров.
    parametersForm->setContentsMargins(14, 18, 14, 14);

    // Вертикальный зазор между строками формы.
    parametersForm->setSpacing(10);

    // Отдельно задаем горизонтальный зазор между подписью и полем.
    parametersForm->setHorizontalSpacing(16);

    // Создаем составное поле размера выборки: spinbox + кнопки + / -.
    auto* sampleSizeField = CreateSpinField(sampleSizeEdit_, 20000, "20000", parametersGroup);

    // Минимальная ширина контейнера поля.
    sampleSizeField->setMinimumWidth(180);

    // Нижняя и верхняя границы допустимого размера выборки.
    sampleSizeEdit_->setRange(1000, kMaxRecommendedSampleSize);

    // Шаг изменения по кнопкам/колесу.
    sampleSizeEdit_->setSingleStep(5000);

    // Создаем поле количества интервалов гистограммы.
    auto* binsField = CreateSpinField(binsEdit_, 24, "24", parametersGroup);

    // Минимальная ширина контейнера поля.
    binsField->setMinimumWidth(180);

    // Диапазон допустимых значений числа бинов.
    binsEdit_->setRange(8, kMaxRecommendedHistogramBins);

    // Шаг изменения числа бинов.
    binsEdit_->setSingleStep(4);

    // Создаем поле числа точек preview.
    auto* previewField = CreateSpinField(previewCountEdit_, 400, "400", parametersGroup);

    // Минимальная ширина контейнера поля.
    previewField->setMinimumWidth(180);

    // Допустимый диапазон количества показываемых первых точек.
    previewCountEdit_->setRange(50, kMaxRecommendedPreviewCount);

    // Шаг изменения preview count.
    previewCountEdit_->setSingleStep(50);

    // Создаем поле seed генератора.
    auto* seedField = CreateSpinField(seedEdit_, 20260414, "20260414", parametersGroup);

    // Минимальная ширина контейнера поля.
    seedField->setMinimumWidth(180);

    // Seed допускается от 1 до максимума int.
    seedEdit_->setRange(1, std::numeric_limits<int>::max());

    // Шаг seed оставляем единичным.
    seedEdit_->setSingleStep(1);

    // Применяем единую светлую палитру к полю размера выборки.
    ApplyInputPalette(sampleSizeEdit_);

    // Применяем ту же палитру к полю числа бинов.
    ApplyInputPalette(binsEdit_);

    // Применяем палитру к полю preview count.
    ApplyInputPalette(previewCountEdit_);

    // Применяем палитру к полю seed.
    ApplyInputPalette(seedEdit_);

    // Добавляем строку "Размер выборки N" в форму параметров.
    parametersForm->addRow("Размер выборки N", sampleSizeField);

    // Добавляем строку "Интервалы гистограммы".
    parametersForm->addRow("Интервалы гистограммы", binsField);

    // Добавляем строку числа точек предварительного просмотра.
    parametersForm->addRow("Показать первых точек", previewField);

    // Добавляем строку seed генератора.
    parametersForm->addRow("Seed генератора", seedField);

    // theoryGroup - отдельная карточка с параметрами заданного варианта.
    auto* theoryGroup = new QGroupBox("Параметры варианта", controlCard);

    // objectName нужен для ее собственного QSS-оформления.
    theoryGroup->setObjectName("theoryGroup");

    // Внутри блока варианта используем обычную вертикальную компоновку.
    auto* theoryLayout = new QVBoxLayout(theoryGroup);

    // Внутренние отступы блока варианта.
    theoryLayout->setContentsMargins(18, 22, 18, 16);

    // Дополнительный зазор внутри блока не нужен:
    // там лежит один большой математический виджет.
    theoryLayout->setSpacing(0);

    // VariantMathWidget вручную рисует размерность, вектор m_x и матрицу K_x.
    theoryLayout->addWidget(new VariantMathWidget(theoryGroup));

    // Главная кнопка запуска моделирования.
    runButton_ = new QPushButton("Выполнить моделирование", controlCard);

    // objectName нужен для primary-style кнопки.
    runButton_->setObjectName("primaryButton");

    // Делаем кнопку визуально крупнее обычных вторичных действий.
    runButton_->setMinimumHeight(46);

    // statusBadge_ - нижняя текстовая плашка состояния и ошибок.
    statusBadge_ = new QLabel(controlCard);

    // Разрешаем перенос строк, потому что сообщения статуса бывают длинными.
    statusBadge_->setWordWrap(true);

    // Кладем главный заголовок в левую колонку.
    controlLayout->addWidget(titleLabel);

    // Затем секцию параметров эксперимента.
    controlLayout->addWidget(parametersGroup);

    // Затем секцию с фиксированным вариантом.
    controlLayout->addWidget(theoryGroup);

    // Ниже главную кнопку запуска.
    controlLayout->addWidget(runButton_);

    // Ниже плашку статуса.
    controlLayout->addWidget(statusBadge_);

    // Stretch внизу прижимает все содержимое левой панели вверх.
    controlLayout->addStretch();

    // resultsCard - правая большая панель всех результатов.
    auto* resultsCard = new QWidget(splitter);

    // objectName нужен для QSS-оформления правой части.
    resultsCard->setObjectName("resultsCard");

    // Внутри панели результатов используем вертикальную компоновку.
    auto* resultsLayout = new QVBoxLayout(resultsCard);

    // Внутренние отступы панели результатов.
    resultsLayout->setContentsMargins(18, 18, 18, 18);

    // Зазор между заголовком панели и табами.
    resultsLayout->setSpacing(12);

    // Заголовок правой панели результатов.
    auto* resultsTitle = new QLabel("Результаты моделирования", resultsCard);

    // objectName нужен для стилизации этого заголовка.
    resultsTitle->setObjectName("resultsTitle");

    // resultsTabs_ - центральный переключатель между основными представлениями результата.
    resultsTabs_ = new QTabWidget(resultsCard);

    // objectName нужен для QSS-оформления вкладок.
    resultsTabs_->setObjectName("resultsTabs");

    // Разрешаем пользователю менять порядок вкладок drag-and-drop.
    resultsTabs_->setMovable(true);

    // summaryTab - вкладка с короткими итоговыми метриками расчета.
    auto* summaryTab = new QWidget(resultsTabs_);

    // У summaryTab своя вертикальная компоновка.
    auto* summaryTabLayout = new QVBoxLayout(summaryTab);

    // Убираем лишние поля у корневой компоновки вкладки итогов.
    summaryTabLayout->setContentsMargins(0, 0, 0, 0);

    // Лишний зазор между единственным содержимым и stretch не нужен.
    summaryTabLayout->setSpacing(0);

    // summaryContent - контейнер карточек метрик.
    auto* summaryContent = new QWidget(summaryTab);

    // objectName нужен для стилизации общего блока итогов.
    summaryContent->setObjectName("summaryContent");

    // Карточки итогов раскладываем сеткой 2x3.
    auto* summaryLayout = new QGridLayout(summaryContent);

    // Отступ сверху создает небольшой дыхательный зазор перед сеткой карточек.
    summaryLayout->setContentsMargins(0, 12, 0, 0);

    // Горизонтальный зазор между карточками.
    summaryLayout->setHorizontalSpacing(12);

    // Вертикальный зазор между карточками.
    summaryLayout->setVerticalSpacing(12);

    // Локальная lambda избавляет от дублирования однотипного кода добавления summary-карточек.
    auto addSummaryMetric = [summaryLayout, summaryContent](int row,
                                                            int column,
                                                            const QString& title,
                                                            SummaryFormulaKind formulaKind,
                                                            QLabel*& valueLabel) {
        // Создаем карточку метрики и ставим ее в нужную ячейку сетки.
        summaryLayout->addWidget(CreateSummaryMetricCard(title, formulaKind, valueLabel, summaryContent), row, column);

        // Соответствующий столбец делаем растягиваемым,
        // чтобы карточки делили ширину равномерно.
        summaryLayout->setColumnStretch(column, 1);
    };

    // Карточка размерности задачи.
    addSummaryMetric(0, 0, "Размерность", SummaryFormulaKind::Dimension, dimensionValue_);

    // Карточка объема выборки.
    addSummaryMetric(0, 1, "Размер выборки", SummaryFormulaKind::SampleSize, sampleCountValue_);

    // Карточка seed генератора.
    addSummaryMetric(0, 2, "Seed", SummaryFormulaKind::Seed, seedValue_);

    // Карточка ошибки оценки среднего.
    addSummaryMetric(1, 0, "Ошибка среднего", SummaryFormulaKind::MeanError, meanErrorValue_);

    // Карточка ошибки ковариационной матрицы.
    addSummaryMetric(1, 1, "Ошибка K<sub>X</sub>", SummaryFormulaKind::CovarianceError, covarianceErrorValue_);

    // Карточка Frobenius-нормы ошибки.
    addSummaryMetric(1, 2, "F-норма", SummaryFormulaKind::FrobeniusError, frobeniusErrorValue_);

    // Кладем контейнер карточек в summaryTab.
    summaryTabLayout->addWidget(summaryContent);

    // Stretch снизу прижимает итоговые карточки вверх.
    summaryTabLayout->addStretch(1);

    // matricesTab - вкладка с матрицами и векторами.
    auto* matricesTab = new QWidget(resultsTabs_);

    // У вкладки матриц своя вертикальная компоновка.
    auto* matricesTabLayout = new QVBoxLayout(matricesTab);

    // Лишние внешние поля у вкладки матриц убираем.
    matricesTabLayout->setContentsMargins(0, 0, 0, 0);

    // Дополнительный зазор не нужен: внутри будет один scroll area.
    matricesTabLayout->setSpacing(0);

    // matricesScroll позволяет прокручивать набор карточек матриц по вертикали.
    auto* matricesScroll = new QScrollArea(matricesTab);

    // Содержимое scroll area должно автоматически подстраиваться под ширину viewport.
    matricesScroll->setWidgetResizable(true);

    // Убираем стандартную рамку scroll area.
    matricesScroll->setFrameShape(QFrame::NoFrame);

    // matricesContent - внутренний контейнер scroll area.
    auto* matricesContent = new QWidget(matricesScroll);

    // На контейнер содержимого вешаем вертикальную компоновку.
    auto* matricesContentLayout = new QVBoxLayout(matricesContent);

    // Убираем поля у контейнера содержимого.
    matricesContentLayout->setContentsMargins(0, 0, 0, 0);

    // Лишний зазор между единственным bundleCard не нужен.
    matricesContentLayout->setSpacing(0);

    // matricesBundleCard - общая внешняя карточка для всей вкладки матриц.
    auto* matricesBundleCard = new QFrame(matricesContent);

    // objectName нужен для общего bundle-стиля.
    matricesBundleCard->setObjectName("contentBundleCard");

    // Внутри bundleCard используем сетку для раскладки отдельных карточек матриц.
    auto* matricesGrid = new QGridLayout(matricesBundleCard);

    // Внутренние поля большой карточки матриц.
    matricesGrid->setContentsMargins(10, 10, 10, 10);

    // Горизонтальный зазор между карточками.
    matricesGrid->setHorizontalSpacing(12);

    // Вертикальный зазор между карточками.
    matricesGrid->setVerticalSpacing(12);

    // Виджет теоретического вектора среднего.
    theoreticalMeanMatrix_ = new MatrixDisplayWidget(matricesContent);

    // Виджет теоретической ковариационной матрицы.
    theoreticalCovarianceMatrix_ = new MatrixDisplayWidget(matricesContent);

    // Виджет фактора Холецкого.
    choleskyMatrix_ = new MatrixDisplayWidget(matricesContent);

    // Виджет выборочной оценки среднего.
    empiricalMeanMatrix_ = new MatrixDisplayWidget(matricesContent);

    // Виджет выборочной оценки ковариации.
    empiricalCovarianceMatrix_ = new MatrixDisplayWidget(matricesContent);

    // Виджет выборочной корреляционной матрицы.
    empiricalCorrelationMatrix_ = new MatrixDisplayWidget(matricesContent);

    // Для теоретической Kx принудительно включаем строгое центрирование,
    // чтобы эта матрица визуально стояла ровно в карточке.
    static_cast<MatrixDisplayWidget*>(theoreticalCovarianceMatrix_)
        ->SetHorizontalPlacement(MatrixDisplayWidget::HorizontalPlacement::Center);

    // Карточка теоретического вектора m_x.
    auto* theoreticalMeanCard = CreateCardWithWidget(
        "Теоретический вектор m<sub>X</sub>",
        theoreticalMeanMatrix_,
        matricesContent);

    // Карточка теоретической матрицы K_x.
    auto* theoreticalCovarianceCard = CreateCardWithWidget(
        "Теоретическая матрица K<sub>X</sub>",
        theoreticalCovarianceMatrix_,
        matricesContent);

    // Карточка фактора Холецкого A.
    auto* choleskyCard = CreateCardWithWidget(
        "Фактор Холецкого A",
        choleskyMatrix_,
        matricesContent);

    // Карточка выборочной оценки среднего.
    auto* empiricalMeanCard = CreateCardWithWidget(
        "Выборочная оценка m&#770;<sub>X</sub>",
        empiricalMeanMatrix_,
        matricesContent);

    // Карточка выборочной оценки ковариации.
    auto* empiricalCovarianceCard = CreateCardWithWidget(
        "Выборочная оценка K&#770;<sub>X</sub>",
        empiricalCovarianceMatrix_,
        matricesContent);

    // Карточка выборочной корреляции.
    auto* empiricalCorrelationCard = CreateCardWithWidget(
        "Выборочная корреляция R&#770;",
        empiricalCorrelationMatrix_,
        matricesContent);

    // Ставим карточку теоретического среднего в левый верхний угол сетки.
    matricesGrid->addWidget(theoreticalMeanCard, 0, 0);

    // Ставим карточку теоретической ковариации в правый верхний угол.
    matricesGrid->addWidget(theoreticalCovarianceCard, 0, 1);

    // Ниже слева размещаем фактор Холецкого.
    matricesGrid->addWidget(choleskyCard, 1, 0);

    // Ниже справа размещаем выборочную оценку среднего.
    matricesGrid->addWidget(empiricalMeanCard, 1, 1);

    // В третьей строке слева - выборочная оценка Kx.
    matricesGrid->addWidget(empiricalCovarianceCard, 2, 0);

    // В третьей строке справа - выборочная корреляция.
    matricesGrid->addWidget(empiricalCorrelationCard, 2, 1);

    // Левый столбец сетки делаем растягиваемым.
    matricesGrid->setColumnStretch(0, 1);

    // Правый столбец сетки тоже делаем растягиваемым.
    matricesGrid->setColumnStretch(1, 1);

    // Вкладываем большую bundle-карточку в контейнер scroll area.
    matricesContentLayout->addWidget(matricesBundleCard);

    // Соединяем scroll area с ее внутренним содержимым.
    matricesScroll->setWidget(matricesContent);

    // Добавляем scroll area во вкладку матриц.
    matricesTabLayout->addWidget(matricesScroll);

    // histogramsTab - вкладка с 4 гистограммами компонент.
    auto* histogramsTab = new QWidget(resultsTabs_);

    // У вкладки гистограмм своя вертикальная компоновка.
    auto* histogramsTabLayout = new QVBoxLayout(histogramsTab);

    // Поля убираем: внутри будет одна большая внешняя карточка.
    histogramsTabLayout->setContentsMargins(0, 0, 0, 0);

    // Дополнительный зазор между единственными элементами не нужен.
    histogramsTabLayout->setSpacing(0);

    // histogramsBundleCard - общая внешняя карточка всех гистограмм.
    auto* histogramsBundleCard = new QFrame(histogramsTab);

    // objectName нужен для bundle-стиля.
    histogramsBundleCard->setObjectName("contentBundleCard");

    // Внутри большой карточки используем сетку 2x2.
    auto* histogramsGrid = new QGridLayout(histogramsBundleCard);

    // Внутренние поля внешней карточки.
    histogramsGrid->setContentsMargins(10, 10, 10, 10);

    // Горизонтальный зазор между карточками-гистограммами.
    histogramsGrid->setHorizontalSpacing(10);

    // Вертикальный зазор между карточками-гистограммами.
    histogramsGrid->setVerticalSpacing(10);

    // Готовим подписи x₁, x₂, x₃, x₄ для заголовков графиков.
    const QStringList xHeaders = BuildComponentHeaders("x", Lab2Variant::kDimension);

    // Идем по всем 4 слотам массива histogramPlots_.
    for (std::size_t index = 0; index < histogramPlots_.size(); ++index)
    {
        // Создаем PlotChartWidget для очередной компоненты.
        histogramPlots_[index] =
            new PlotChartWidget(QString("Компонента %1: гистограмма и плотность")
                                    .arg(xHeaders.value(static_cast<int>(index))),
                                "значение",
                                "плотность",
                                histogramsBundleCard);

        // Настраиваем компактный нижний резерв под подписи оси X.
        histogramPlots_[index]->SetBottomAxisLabelReserve(12, 4);

        // Внешнюю легенду на гистограммах отключаем:
        // там она была бы лишней и только съедала бы место.
        histogramPlots_[index]->SetLegendVisible(false);

        // Заголовок каждой гистограммы центрируем.
        histogramPlots_[index]->SetTitleAlignment(Qt::AlignHCenter);

        // Минимальная высота карточки гистограммы.
        histogramPlots_[index]->setMinimumHeight(336);

        // Раскладываем 4 гистограммы в сетку 2x2.
        histogramsGrid->addWidget(histogramPlots_[index], static_cast<int>(index / 2), static_cast<int>(index % 2));
    }

    // Левый столбец сетки гистограмм растягиваем.
    histogramsGrid->setColumnStretch(0, 1);

    // Правый столбец сетки гистограмм тоже растягиваем.
    histogramsGrid->setColumnStretch(1, 1);

    // Первая строка сетки гистограмм растягивается.
    histogramsGrid->setRowStretch(0, 1);

    // Вторая строка сетки гистограмм тоже растягивается.
    histogramsGrid->setRowStretch(1, 1);

    // Вкладываем общую bundle-карточку в саму вкладку.
    histogramsTabLayout->addWidget(histogramsBundleCard, 1);

    // trajectoryTab - вкладка траекторий и preview-таблицы.
    auto* trajectoryTab = new QWidget(resultsTabs_);

    // У вкладки траекторий своя вертикальная компоновка.
    auto* trajectoryLayout = new QVBoxLayout(trajectoryTab);

    // Внешние поля убираем.
    trajectoryLayout->setContentsMargins(0, 0, 0, 0);

    // Лишний зазор между bundleCard и краями не нужен.
    trajectoryLayout->setSpacing(0);

    // trajectoryBundleCard - общая внешняя карточка вкладки траекторий.
    auto* trajectoryBundleCard = new QFrame(trajectoryTab);

    // objectName для bundle-оформления.
    trajectoryBundleCard->setObjectName("contentBundleCard");

    // Внутри bundleCard используем вертикальную компоновку.
    auto* trajectoryBundleLayout = new QVBoxLayout(trajectoryBundleCard);

    // Внутренние поля большой карточки траекторий.
    trajectoryBundleLayout->setContentsMargins(14, 14, 14, 14);

    // Зазор между карточкой графика и карточкой таблицы preview.
    trajectoryBundleLayout->setSpacing(12);

    // Создаем главный график траекторий первых наблюдений X.
    sequencePlot_ =
        new PlotChartWidget("Первые наблюдения компонентов X", "номер наблюдения", "значение", trajectoryBundleCard);

    // objectName нужен для точечного стиля именно этого встроенного графика.
    sequencePlot_->setObjectName("embeddedPlotCard");

    // Нижний резерв увеличиваем, потому что у графика траекторий много подписей по X.
    sequencePlot_->SetBottomAxisLabelReserve(24, 8);

    // Стартовый домашний zoom по X делаем уже,
    // чтобы пользователь сразу видел характер колебаний.
    sequencePlot_->SetHomeViewScale(0.32, 1.0);

    // Автоподгон Y под видимый X здесь отключен:
    // пользователь должен видеть устойчивый вертикальный масштаб.
    sequencePlot_->SetAutoFitYToVisibleX(false);

    // Домашний вид должен начинаться от нуля по X, если это возможно.
    sequencePlot_->SetHomeViewStartAtZero(true);

    // Минимальная высота карточки графика траекторий.
    sequencePlot_->setMinimumHeight(400);

    // Максимальная высота, чтобы график не разрастался слишком агрессивно.
    sequencePlot_->setMaximumHeight(480);

    // trajectoryPlotCard - отдельная карточка вокруг самого графика траекторий.
    auto* trajectoryPlotCard = new QFrame(trajectoryBundleCard);

    // Оформляем ее как обычную plotCard.
    trajectoryPlotCard->setObjectName("plotCard");

    // Внутри карточки графика используем вертикальную компоновку.
    auto* trajectoryPlotLayout = new QVBoxLayout(trajectoryPlotCard);

    // Поля подогнаны так, чтобы график почти полностью занимал карточку.
    trajectoryPlotLayout->setContentsMargins(4, 4, 4, 0);

    // Лишний зазор между графиком и нижней служебной границей не нужен.
    trajectoryPlotLayout->setSpacing(0);

    // Кладем сам график в карточку.
    trajectoryPlotLayout->addWidget(sequencePlot_);

    // trajectoryBottomBorder - отдельная нижняя линия-граница карточки графика.
    auto* trajectoryBottomBorder = new QFrame(trajectoryPlotCard);

    // objectName нужен для ее QSS-стиля.
    trajectoryBottomBorder->setObjectName("trajectoryCardBottomBorder");

    // Высота линии ровно 1 пиксель.
    trajectoryBottomBorder->setFixedHeight(1);

    // Отдельный host нужен, чтобы аккуратно управлять боковыми отступами этой линии.
    auto* trajectoryBottomBorderHost = new QWidget(trajectoryPlotCard);

    // У host создаем горизонтальную компоновку под нижнюю линию.
    auto* trajectoryBottomBorderLayout = new QHBoxLayout(trajectoryBottomBorderHost);

    // Боковые поля линии.
    trajectoryBottomBorderLayout->setContentsMargins(5, 0, 5, 0);

    // Между host и самой линией зазора не нужно.
    trajectoryBottomBorderLayout->setSpacing(0);

    // Вкладываем линию в host.
    trajectoryBottomBorderLayout->addWidget(trajectoryBottomBorder);

    // Дополнительный spacer нулевой высоты оставлен как явный разделитель структуры layout.
    trajectoryPlotLayout->addSpacing(0);

    // Добавляем host с нижней линией в конец карточки графика.
    trajectoryPlotLayout->addWidget(trajectoryBottomBorderHost);

    // Добавляем карточку графика в общую вкладку траекторий.
    trajectoryBundleLayout->addWidget(trajectoryPlotCard, 1);

    // previewTableModel_ - модель компактной таблицы первых векторов U и X.
    previewTableModel_ = new NumericMatrixModel(this);

    // Создаем саму preview-таблицу.
    previewTable_ = new QTableView(trajectoryBundleCard);

    // Подключаем модель данных к таблице.
    previewTable_->setModel(previewTableModel_);

    // Применяем к preview-таблице правила sample-table.
    ConfigureSampleTable(previewTable_);

    // Минимальная высота карточки preview-таблицы.
    previewTable_->setMinimumHeight(300);

    // Максимальная высота preview-таблицы.
    previewTable_->setMaximumHeight(360);

    // Оборачиваем preview-таблицу в карточку с заголовком и PNG-экспортом.
    auto* previewTableCard = CreateCardWithTable("Первые векторы U и X", previewTable_, trajectoryBundleCard);

    // Минимальная высота всей карточки таблицы.
    previewTableCard->setMinimumHeight(320);

    // Максимальная высота всей карточки таблицы.
    previewTableCard->setMaximumHeight(400);

    // Кладем карточку preview-таблицы под графиком траекторий.
    trajectoryBundleLayout->addWidget(previewTableCard, 1);

    // Добавляем общую bundle-карточку траекторий во вкладку.
    trajectoryLayout->addWidget(trajectoryBundleCard, 1);

    // samplesTab - вкладка полной выборки X.
    auto* samplesTab = new QWidget(resultsTabs_);

    // У вкладки выборки своя вертикальная компоновка.
    auto* samplesLayout = new QVBoxLayout(samplesTab);

    // Внешние поля убираем.
    samplesLayout->setContentsMargins(0, 0, 0, 0);

    // sampleTableModel_ - модель полной выборки.
    sampleTableModel_ = new SampleTableModel(this);

    // Таблица полной выборки.
    samplesTable_ = new QTableView(samplesTab);

    // Подключаем к ней модель полной выборки.
    samplesTable_->setModel(sampleTableModel_);

    // Применяем общий стиль и правила таблицы выборки.
    ConfigureSampleTable(samplesTable_);

    // Оборачиваем таблицу выборки в карточку и добавляем во вкладку.
    samplesLayout->addWidget(CreateCardWithTable("Сгенерированная выборка X", samplesTable_, samplesTab));

    // Регистрируем вкладку итоговых метрик.
    resultsTabs_->addTab(summaryTab, "Итоги");

    // Регистрируем вкладку матриц.
    resultsTabs_->addTab(matricesTab, "Матрицы");

    // Регистрируем вкладку гистограмм.
    resultsTabs_->addTab(histogramsTab, "Гистограммы");

    // Регистрируем вкладку траекторий.
    resultsTabs_->addTab(trajectoryTab, "Траектории");

    // Регистрируем вкладку полной выборки.
    resultsTabs_->addTab(samplesTab, "Выборка");

    // Кладем заголовок правой панели результатов.
    resultsLayout->addWidget(resultsTitle);

    // Ниже размещаем сам набор вкладок, который должен занять всю оставшуюся высоту.
    resultsLayout->addWidget(resultsTabs_, 1);

    // Добавляем левую панель управления в splitter.
    splitter->addWidget(controlCard);

    // Добавляем правую панель результатов в splitter.
    splitter->addWidget(resultsCard);

    // Вторую панель делаем растягиваемой по приоритету.
    splitter->setStretchFactor(1, 1);

    // Стартовые размеры секций splitter: узкая левая панель и широкая правая.
    splitter->setSizes({480, 1080});

    // Кладем splitter в корневую компоновку окна.
    rootLayout->addWidget(splitter);

    // Завершаем сборку, назначая central как центральный виджет QMainWindow.
    setCentralWidget(central);
}

// ----------------------------------------------------------------------------
// МЕТОД ApplyTheme - ПРИМЕНЕНИЕ ГЛОБАЛЬНОЙ QSS-ТЕМЫ ОКНА
// ----------------------------------------------------------------------------
// Назначение:
// Централизованно задать внешний вид практически всего GUI:
// - фоны и рамки основных карточек;
// - типографику заголовков и подписей;
// - стиль group box, вкладок, таблиц и скроллбаров;
// - стиль primary/secondary-кнопок;
// - оформление карточек графиков и блоков результатов.
//
// Принимает:
// - ничего.
//
// Возвращает:
// - ничего.
// ----------------------------------------------------------------------------
void MainWindow::ApplyTheme()
{
    // Весь стиль задаем одной большой QSS-строкой.
    // Это удобно, потому что вся тема живет в одном месте
    // и ее проще поддерживать как единую систему.
    //
    // Используем raw string literal `R"( ... )"`,
    // чтобы не экранировать кавычки и переводы строк внутри CSS-подобного текста.
    // Внутри этой строки мы можем спокойно писать CSS-комментарии `/* ... */`,
    // и Qt Style Sheets корректно их проигнорирует.
    setStyleSheet(R"(
        /* ----------------------------------------------------------------- */
        /* Общий фон всего окна приложения                                   */
        /* ----------------------------------------------------------------- */
        QMainWindow {
            /* Мягкий серо-голубой фон, на котором белые карточки читаются лучше. */
            background: #eef3f8;
        }

        /* ----------------------------------------------------------------- */
        /* Две большие основные панели: левая и правая                       */
        /* ----------------------------------------------------------------- */
        QWidget#controlCard, QWidget#resultsCard {
            /* Основной фон панели. */
            background: #fbfdff;
            /* Тонкая внешняя рамка панели. */
            border: 1px solid #d8e2ed;
            /* Скругление углов панели. */
            border-radius: 18px;
        }

        /* ----------------------------------------------------------------- */
        /* Ручка горизонтального splitter                                    */
        /* ----------------------------------------------------------------- */
        QSplitter::handle {
            /* Базовый цвет ручки. */
            background: #dbe6f2;
            /* Отступ сверху и снизу делает ручку визуально короче. */
            margin: 20px 0;
            /* Легкое скругление ручки. */
            border-radius: 2px;
        }

        QSplitter::handle:hover {
            /* При наведении делаем ручку заметнее. */
            background: #c5d5e6;
        }

        /* ----------------------------------------------------------------- */
        /* Базовая типографика QLabel                                        */
        /* ----------------------------------------------------------------- */
        QLabel {
            /* Цвет обычного текста по умолчанию. */
            color: #102033;
            /* Базовый размер шрифта большинства подписей. */
            font-size: 15px;
        }

        /* Крупные заголовки двух главных колонок. */
        QLabel#titleLabel, QLabel#resultsTitle {
            /* Темный заголовочный цвет. */
            color: #102033;
            /* Увеличенный размер шрифта. */
            font-size: 24px;
            /* Выраженная жирность. */
            font-weight: 700;
        }

        /* Заголовки карточек таблиц. */
        QLabel#tableTitle {
            /* Цвет заголовка карточки таблицы. */
            color: #102033;
            /* Размер шрифта заголовка карточки таблицы. */
            font-size: 18px;
            /* Жирность заголовка карточки таблицы. */
            font-weight: 700;
        }

        /* ----------------------------------------------------------------- */
        /* Карточки итоговых метрик                                          */
        /* ----------------------------------------------------------------- */
        QFrame#summaryMetricCard {
            /* Рамка карточки метрики. */
            border: 1px solid #dbe5ef;
            /* Скругление карточки метрики. */
            border-radius: 16px;
            /* Фон карточки метрики. */
            background: #fbfdff;
        }

        QLabel#summaryMetricTitle {
            /* Более спокойный цвет заголовка метрики. */
            color: #496073;
            /* Размер подписи метрики. */
            font-size: 14px;
            /* Средняя жирность подписи метрики. */
            font-weight: 500;
        }

        QLabel#summaryMetricValue {
            /* Основной цвет числа метрики. */
            color: #102033;
            /* Крупный размер для числового значения. */
            font-size: 24px;
            /* Вес числа оставляем умеренным, без лишней тяжести. */
            font-weight: 500;
        }

        QLabel#summaryMetricFormula {
            /* Приглушенный цвет формулы под числом. */
            color: #718394;
            /* Размер формульной подписи. */
            font-size: 14px;
            /* Средняя жирность формульной подписи. */
            font-weight: 500;
        }

        QLabel#summaryCaption {
            /* Цвет дополнительных summary-caption подписей. */
            color: #4d647a;
            /* Размер summary-caption. */
            font-size: 15px;
            /* Немного усиленная жирность. */
            font-weight: 600;
        }

        QLabel#summaryValue {
            /* Цвет дополнительных summary-value значений. */
            color: #102033;
            /* Размер дополнительного summary-value. */
            font-size: 15px;
            /* Более сильная жирность числа/значения. */
            font-weight: 700;
        }

        /* ----------------------------------------------------------------- */
        /* Базовые карточки графиков и bundle-контейнеры                     */
        /* ----------------------------------------------------------------- */
        QFrame#plotCard {
            /* Рамка обычной карточки графика/таблицы. */
            border: 1px solid #d9e2ec;
            /* Скругление обычной карточки. */
            border-radius: 16px;
            /* Белый фон карточки. */
            background: #ffffff;
        }

        QFrame#trajectoryCardBottomBorder {
            /* Цвет нижней дополнительной линии под графиком траекторий. */
            background: #d9e2ec;
            /* Собственную рамку выключаем: нужна только линия-фон. */
            border: none;
            /* Минимальная высота ровно в один пиксель. */
            min-height: 1px;
            /* Максимальная высота тоже ровно в один пиксель. */
            max-height: 1px;
        }

        QFrame#embeddedPlotCard {
            /* У встроенного графика дополнительную рамку убираем. */
            border: none;
            /* Скругление тоже убираем, чтобы не дублировать внешний контейнер. */
            border-radius: 0px;
            /* Фон делаем прозрачным, чтобы работала внешняя карточка. */
            background: transparent;
        }

        QFrame#contentBundleCard {
            /* Рамка большой общей bundle-карточки. */
            border: 1px solid #d8e2ed;
            /* Более крупное скругление общей карточки. */
            border-radius: 20px;
            /* Светлый фон bundle-контейнера. */
            background: #fbfdff;
        }

        QLabel#plotTitle {
            /* Цвет заголовка графика. */
            color: #102033;
            /* Размер заголовка графика. */
            font-size: 16px;
            /* Жирность заголовка графика. */
            font-weight: 700;
        }

        QChartView#plotChartView {
            /* Фон chart view прозрачен, чтобы была видна внешняя карточка. */
            background: transparent;
            /* Встроенную рамку chart view убираем. */
            border: none;
        }

        /* ----------------------------------------------------------------- */
        /* GroupBox: общая база и две большие секции слева                   */
        /* ----------------------------------------------------------------- */
        QGroupBox {
            /* Цвет текста group box по умолчанию. */
            color: #102033;
            /* Базовый размер шрифта заголовка/содержимого group box. */
            font-size: 14px;
            /* Базовая жирность group box. */
            font-weight: 700;
            /* Рамка group box. */
            border: 1px solid #d9e2ec;
            /* Скругление group box. */
            border-radius: 14px;
            /* Верхний отступ нужен под выступающий заголовок. */
            margin-top: 12px;
            /* Фон group box. */
            background: #ffffff;
        }

        QGroupBox::title {
            /* Заголовок позиционируется относительно margin-области. */
            subcontrol-origin: margin;
            /* Сдвиг заголовка от левого края. */
            left: 12px;
            /* Внутренние поля заголовка, чтобы он не прилипал к рамке. */
            padding: 0 6px;
        }

        QGroupBox#parametersGroup,
        QGroupBox#theoryGroup {
            /* Для двух крупных секций оставляем увеличенный верхний отступ. */
            margin-top: 18px;
        }

        QGroupBox#parametersGroup::title,
        QGroupBox#theoryGroup::title {
            /* Цвет крупных секционных заголовков. */
            color: #17324d;
            /* Увеличенный размер шрифта заголовков секций. */
            font-size: 26px;
            /* Очень плотная жирность заголовков секций. */
            font-weight: 800;
            /* Чуть больший сдвиг заголовка внутрь. */
            left: 16px;
            /* Специальные внутренние поля для красивой посадки заголовка на рамку. */
            padding: 0 10px 2px 10px;
        }

        QGroupBox#parametersGroup QLabel {
            /* Размер подписей внутри формы параметров. */
            font-size: 15px;
            /* Жирность подписей формы немного повышаем,
               чтобы они лучше держали визуальную структуру. */
            font-weight: 600;
            /* Цвет подписей формы делаем немного мягче чисто черного,
               но все еще достаточно контрастным. */
            color: #20384f;
        }

        QLabel#theoryKey {
            /* Цвет ключей в блоках теории/варианта. */
            color: #294560;
            /* Размер подписи-ключа. */
            font-size: 15px;
            /* Жирность подписи-ключа. */
            font-weight: 700;
        }

        QLabel#theoryValue {
            /* Цвет значений в теоретическом блоке. */
            color: #102033;
            /* Размер значения. */
            font-size: 15px;
            /* Более спокойный вес текста значения. */
            font-weight: 500;
        }

        /* ----------------------------------------------------------------- */
        /* Поля ввода                                                        */
        /* ----------------------------------------------------------------- */
        QLineEdit, QSpinBox {
            /* Размер шрифта полей ввода. */
            font-size: 15px;
            /* Минимальная высота поля. */
            min-height: 36px;
            /* Рамка поля. */
            border: 1px solid #c8d4e2;
            /* Скругление поля. */
            border-radius: 10px;
            /* Внутренние отступы текста. */
            padding: 0 10px;
            /* Фон поля. */
            background: #ffffff;
            /* Цвет текста поля. */
            color: #102033;
            /* Цвет фона выделенного текста. */
            selection-background-color: #2a6df4;
            /* Цвет самого выделенного текста. */
            selection-color: #ffffff;
        }

        QLineEdit:focus, QSpinBox:focus {
            /* При фокусе рамка становится синей. */
            border: 1px solid #2a6df4;
        }

        /* ----------------------------------------------------------------- */
        /* Маленькие кнопки + / - у кастомных spinbox                        */
        /* ----------------------------------------------------------------- */
        QPushButton#spinAdjustButton {
            /* Фон кнопок регулировки значения. */
            background: #f7fbff;
            /* Цвет символов + и -. */
            color: #1e3655;
            /* Рамка кнопки. */
            border: 1px solid #d7e2ee;
            /* Скругление кнопки. */
            border-radius: 8px;
            /* Размер шрифта символа. */
            font-size: 14px;
            /* Жирность символа. */
            font-weight: 700;
            /* Лишний внутренний padding здесь не нужен. */
            padding: 0;
        }

        QPushButton#spinAdjustButton:hover {
            /* При hover фон делаем чуть более голубым. */
            background: #edf5ff;
            /* И усиливаем рамку. */
            border-color: #bdd0e6;
        }

        QPushButton#spinAdjustButton:pressed {
            /* При нажатии еще немного затемняем кнопку. */
            background: #dfeaf8;
        }

        /* ----------------------------------------------------------------- */
        /* Главная кнопка запуска                                             */
        /* ----------------------------------------------------------------- */
        QPushButton#primaryButton {
            /* Синий фон primary-кнопки. */
            background: #2a6df4;
            /* Белый текст на синем фоне. */
            color: #ffffff;
            /* Отдельную рамку не используем. */
            border: none;
            /* Скругление primary-кнопки. */
            border-radius: 12px;
            /* Размер шрифта кнопки. */
            font-size: 15px;
            /* Жирность текста кнопки. */
            font-weight: 700;
            /* Горизонтальные внутренние поля текста. */
            padding: 0 16px;
        }

        QPushButton#primaryButton:hover {
            /* При hover фон становится чуть темнее. */
            background: #1f61e4;
        }

        QPushButton#primaryButton:pressed {
            /* При нажатии фон еще сильнее затемняется. */
            background: #174fc4;
        }

        QPushButton#primaryButton:disabled {
            /* В disabled-состоянии фон сильно ослабляется. */
            background: #b2c8f2;
            /* И текст тоже становится бледнее. */
            color: #eaf1ff;
        }

        /* ----------------------------------------------------------------- */
        /* Вторичные кнопки вроде экспорта PNG                               */
        /* ----------------------------------------------------------------- */
        QPushButton#secondaryButton {
            /* Белый фон вторичной кнопки. */
            background: #ffffff;
            /* Темный текст вторичной кнопки. */
            color: #102033;
            /* Контрастная темная рамка. */
            border: 1px solid #102033;
            /* Скругление вторичной кнопки. */
            border-radius: 10px;
            /* Размер шрифта. */
            font-size: 13px;
            /* Жирность шрифта. */
            font-weight: 700;
            /* Горизонтальные внутренние поля текста. */
            padding: 0 18px;
        }

        QPushButton#secondaryButton:hover:enabled {
            /* При hover вторичная кнопка инвертируется в темную. */
            background: #102033;
            /* Текст становится белым. */
            color: #ffffff;
            /* И рамка совпадает с цветом фона. */
            border-color: #102033;
        }

        QPushButton#secondaryButton:pressed:enabled {
            /* При нажатии используем чуть более глубокий темно-синий. */
            background: #26394c;
            /* Текст оставляем белым. */
            color: #ffffff;
        }

        /* ----------------------------------------------------------------- */
        /* ScrollArea                                                         */
        /* ----------------------------------------------------------------- */
        QScrollArea {
            /* Внешнюю рамку scroll area убираем. */
            border: none;
            /* Фон делаем прозрачным. */
            background: transparent;
        }

        QScrollArea > QWidget > QWidget {
            /* Внутренние Qt-контейнеры scroll area тоже делаем прозрачными,
               чтобы не было случайных фонов поверх bundle-карточек. */
            background: transparent;
        }

        /* ----------------------------------------------------------------- */
        /* Вкладки результатов                                               */
        /* ----------------------------------------------------------------- */
        QTabWidget#resultsTabs::pane {
            /* Рамка вокруг области вкладок не нужна. */
            border: none;
            /* Небольшой отрицательный сдвиг помогает визуально состыковать выбранную вкладку с контентом. */
            top: -1px;
            /* Фон области вкладок прозрачный. */
            background: transparent;
        }

        QTabWidget#resultsTabs::tab-bar {
            /* Сдвигаем весь tab-bar чуть вправо. */
            left: 14px;
        }

        QTabWidget#resultsTabs QTabBar::tab {
            /* Фон невыбранной вкладки. */
            background: #eef3f8;
            /* Цвет текста невыбранной вкладки. */
            color: #46627d;
            /* Рамка вкладки. */
            border: 1px solid #d8e2ed;
            /* Нижнюю часть рамки убираем, чтобы вкладка стыковалась с контентом. */
            border-bottom: none;
            /* Внутренние поля вкладки. */
            padding: 11px 18px;
            /* Зазор справа между вкладками. */
            margin-right: 6px;
            /* Минимальная ширина вкладки. */
            min-width: 120px;
            /* Скругление левого верхнего угла вкладки. */
            border-top-left-radius: 12px;
            /* Скругление правого верхнего угла вкладки. */
            border-top-right-radius: 12px;
            /* Размер шрифта вкладки. */
            font-size: 15px;
            /* Жирность текста вкладки. */
            font-weight: 700;
        }

        QTabWidget#resultsTabs QTabBar::tab:hover {
            /* Hover-фон вкладки. */
            background: #e8f0f8;
            /* Hover-цвет текста вкладки. */
            color: #284560;
        }

        QTabWidget#resultsTabs QTabBar::tab:selected {
            /* Выбранная вкладка становится белой как ее контент. */
            background: #ffffff;
            /* И получает основной темный цвет текста. */
            color: #102033;
            /* Небольшой отрицательный нижний margin помогает спрятать шов. */
            margin-bottom: -1px;
        }

        /* ----------------------------------------------------------------- */
        /* Таблицы                                                            */
        /* ----------------------------------------------------------------- */
        QTableView {
            /* Внешняя рамка таблицы. */
            border: 1px solid #d9e2ec;
            /* Скругление таблицы. */
            border-radius: 12px;
            /* Цвет линий сетки между ячейками. */
            gridline-color: #e5edf5;
            /* Основной цвет текста таблицы. */
            color: #102033;
            /* Цвет фона выбранной строки/ячейки. */
            selection-background-color: #dce8ff;
            /* Цвет текста на выделении. */
            selection-color: #102033;
            /* Цвет чередующихся строк. */
            alternate-background-color: #f7fbff;
            /* Основной фон таблицы. */
            background: #ffffff;
            /* Размер шрифта таблицы. */
            font-size: 15px;
        }

        QHeaderView {
            /* Общий фон header view прозрачен, чтобы стиль задавался секциями. */
            background: transparent;
            /* Отдельную рамку у самого header view выключаем. */
            border: none;
        }

        QHeaderView::section {
            /* Фон секции заголовка. */
            background: #f3f7fb;
            /* Цвет текста заголовка. */
            color: #102033;
            /* Лишнюю общую рамку секции убираем. */
            border: none;
            /* Правая граница секции. */
            border-right: 1px solid #d9e2ec;
            /* Нижняя граница секции. */
            border-bottom: 1px solid #d9e2ec;
            /* Внутренние поля текста заголовка. */
            padding: 8px;
            /* Размер шрифта заголовка. */
            font-size: 15px;
            /* Жирность заголовка. */
            font-weight: 700;
        }

        QHeaderView::section:horizontal:first {
            /* Скругляем левый верхний угол первой горизонтальной секции. */
            border-top-left-radius: 11px;
        }

        QHeaderView::section:horizontal:last {
            /* Скругляем правый верхний угол последней горизонтальной секции. */
            border-top-right-radius: 11px;
            /* У последней секции правую границу убираем, чтобы не было двойной линии. */
            border-right: none;
        }

        QHeaderView::section:vertical {
            /* Вертикальные секции тоже имеют правую границу. */
            border-right: 1px solid #d9e2ec;
            /* И нижнюю границу. */
            border-bottom: 1px solid #d9e2ec;
        }

        QTableCornerButton::section {
            /* Верхний левый угол таблицы оформляем в стиле header-секций. */
            background: #f3f7fb;
            /* Лишнюю рамку выключаем. */
            border: none;
            /* Скругляем угол. */
            border-top-left-radius: 11px;
            /* Правая граница угловой ячейки. */
            border-right: 1px solid #d9e2ec;
            /* Нижняя граница угловой ячейки. */
            border-bottom: 1px solid #d9e2ec;
        }

        /* ----------------------------------------------------------------- */
        /* Вертикальный скроллбар                                             */
        /* ----------------------------------------------------------------- */
        QScrollBar:vertical {
            /* Фон дорожки вертикального скроллбара. */
            background: #eef4fb;
            /* Ширина вертикального скроллбара. */
            width: 12px;
            /* Внешние поля скроллбара. */
            margin: 10px 2px 10px 2px;
            /* Скругление дорожки. */
            border-radius: 6px;
        }

        QScrollBar::handle:vertical {
            /* Цвет ползунка. */
            background: #c7d7e8;
            /* Минимальная высота ползунка. */
            min-height: 36px;
            /* Скругление ползунка. */
            border-radius: 6px;
        }

        QScrollBar::handle:vertical:hover {
            /* Hover-цвет вертикального ползунка. */
            background: #b4c9de;
        }

        QScrollBar::add-line:vertical,
        QScrollBar::sub-line:vertical {
            /* Кнопки по краям вертикального скроллбара скрываем. */
            height: 0px;
            /* Их фон делаем прозрачным,
               чтобы скрытые элементы не оставляли следов в дорожке. */
            background: transparent;
            /* И дополнительно убираем любую рамку этих скрытых кнопок. */
            border: none;
        }

        QScrollBar::add-page:vertical,
        QScrollBar::sub-page:vertical {
            /* Пустые области дорожки между handle и краями. */
            background: transparent;
        }

        /* ----------------------------------------------------------------- */
        /* Горизонтальный скроллбар                                           */
        /* ----------------------------------------------------------------- */
        QScrollBar:horizontal {
            /* Фон дорожки горизонтального скроллбара. */
            background: #eef4fb;
            /* Высота горизонтального скроллбара. */
            height: 12px;
            /* Внешние поля горизонтального скроллбара. */
            margin: 2px 10px 2px 10px;
            /* Скругление дорожки. */
            border-radius: 6px;
        }

        QScrollBar::handle:horizontal {
            /* Цвет горизонтального ползунка. */
            background: #c7d7e8;
            /* Минимальная ширина ползунка. */
            min-width: 36px;
            /* Скругление ползунка. */
            border-radius: 6px;
        }

        QScrollBar::handle:horizontal:hover {
            /* Hover-цвет горизонтального ползунка. */
            background: #b4c9de;
        }

        QScrollBar::add-line:horizontal,
        QScrollBar::sub-line:horizontal {
            /* Кнопки по краям горизонтального скроллбара скрываем. */
            width: 0px;
            /* Фон скрытых кнопок делаем прозрачным. */
            background: transparent;
            /* И рамку у них тоже полностью убираем. */
            border: none;
        }

        QScrollBar::add-page:horizontal,
        QScrollBar::sub-page:horizontal {
            /* Пустые области дорожки делаем прозрачными. */
            background: transparent;
        }
    )");
}

// ----------------------------------------------------------------------------
// МЕТОД SetupSignals - ПОДКЛЮЧЕНИЕ SIGNAL/SLOT-СВЯЗЕЙ ГЛАВНОГО ОКНА
// ----------------------------------------------------------------------------
// Назначение:
// Один раз связать пользовательские действия и асинхронные события
// с соответствующими обработчиками MainWindow.
//
// Принимает:
// - ничего.
//
// Возвращает:
// - ничего.
// ----------------------------------------------------------------------------
void MainWindow::SetupSignals()
{
    // По нажатию основной кнопки запускаем процедуру моделирования.
    connect(runButton_, &QPushButton::clicked, this, &MainWindow::RunSampling);

    // Когда фоновый расчет завершится, QFutureWatcher испустит finished.
    // Через lambda мы забираем уже готовый SamplingResult
    // и передаем его в общий финализатор FinishSampling().
    connect(simulationWatcher_, &QFutureWatcher<SamplingResult>::finished, this, [this]() {
        // result() извлекает результат уже завершившегося future.
        FinishSampling(simulationWatcher_->result());
    });
}

// ----------------------------------------------------------------------------
// МЕТОД ResetOutputs - СБРОС ВСЕХ ПОКАЗЫВАЕМЫХ РЕЗУЛЬТАТОВ
// ----------------------------------------------------------------------------
// Назначение:
// Вернуть интерфейс в "пустое" состояние перед новым запуском моделирования:
// очистить summary-значения, матрицы, таблицы, графики и кеш последнего результата.
//
// Принимает:
// - ничего.
//
// Возвращает:
// - ничего.
// ----------------------------------------------------------------------------
void MainWindow::ResetOutputs()
{
    // Сбрасываем размерность в summary-карточке на заглушку.
    dimensionValue_->setText("—");
    // Сбрасываем отображаемый размер выборки.
    sampleCountValue_->setText("—");
    // Сбрасываем метрику ошибки среднего.
    meanErrorValue_->setText("—");
    // Сбрасываем метрику ошибки ковариации.
    covarianceErrorValue_->setText("—");
    // Сбрасываем F-норму ошибки ковариационной матрицы.
    frobeniusErrorValue_->setText("—");
    // Сбрасываем отображаемый seed.
    seedValue_->setText("—");

    // Если виджет теоретического вектора среднего уже создан,
    // очищаем его матричные данные и заголовки.
    if (theoreticalMeanMatrix_ != nullptr)
    {
        static_cast<MatrixDisplayWidget*>(theoreticalMeanMatrix_)->SetMatrixData({}, {});
    }

    // Аналогично очищаем теоретическую ковариационную матрицу.
    if (theoreticalCovarianceMatrix_ != nullptr)
    {
        static_cast<MatrixDisplayWidget*>(theoreticalCovarianceMatrix_)->SetMatrixData({}, {});
    }

    // Очищаем матрицу фактора Холецкого A.
    if (choleskyMatrix_ != nullptr)
    {
        static_cast<MatrixDisplayWidget*>(choleskyMatrix_)->SetMatrixData({}, {});
    }

    // Очищаем выборочную оценку среднего.
    if (empiricalMeanMatrix_ != nullptr)
    {
        static_cast<MatrixDisplayWidget*>(empiricalMeanMatrix_)->SetMatrixData({}, {});
    }

    // Очищаем выборочную ковариационную матрицу.
    if (empiricalCovarianceMatrix_ != nullptr)
    {
        static_cast<MatrixDisplayWidget*>(empiricalCovarianceMatrix_)->SetMatrixData({}, {});
    }

    // Очищаем выборочную корреляционную матрицу.
    if (empiricalCorrelationMatrix_ != nullptr)
    {
        static_cast<MatrixDisplayWidget*>(empiricalCorrelationMatrix_)->SetMatrixData({}, {});
    }

    // Очищаем компактную таблицу превью первых векторов U и X.
    static_cast<NumericMatrixModel*>(previewTableModel_)->SetMatrixData({}, {});

    // У полной таблицы выборки убираем указатель на выборку совсем,
    // чтобы модель перестала что-либо показывать.
    static_cast<SampleTableModel*>(sampleTableModel_)->SetSamples(nullptr);

    // Последовательно очищаем все карточки гистограмм.
    for (PlotChartWidget* plot : histogramPlots_)
    {
        // На всякий случай проверяем указатель:
        // отдельный график мог еще не быть создан.
        if (plot != nullptr)
        {
            plot->Clear();
        }
    }

    // Очищаем график траекторий первых наблюдений.
    if (sequencePlot_ != nullptr)
    {
        sequencePlot_->Clear();
    }

    // Помечаем, что валидного результата сейчас нет.
    hasResult_ = false;

    // Полностью зануляем кеш последнего результата.
    // Конструкция `SamplingResult {}` вызывает value-initialization:
    // числовые поля обнуляются, контейнеры становятся пустыми,
    // флаги возвращаются к значениям по умолчанию.
    lastResult_ = SamplingResult {};
}

// ----------------------------------------------------------------------------
// МЕТОД RunSampling - ЗАПУСК НОВОГО МОДЕЛИРОВАНИЯ
// ----------------------------------------------------------------------------
// Назначение:
// Проверить входные параметры, собрать SamplingOptions,
// перевести интерфейс в режим ожидания и запустить фоновый расчет.
//
// Принимает:
// - ничего.
//
// Возвращает:
// - ничего.
// ----------------------------------------------------------------------------
void MainWindow::RunSampling()
{
    // Если расчет уже идет, повторный запуск игнорируем,
    // чтобы не запустить две конкурирующие симуляции одновременно.
    if (isSamplingRunning_)
    {
        return;
    }

    // Перед новым запуском очищаем все старые результаты,
    // чтобы пользователь не видел устаревшие матрицы и графики.
    ResetOutputs();

    // Сюда ValidateInput() при необходимости запишет человекочитаемую ошибку.
    QString errorMessage;

    // Если входные параметры некорректны,
    // показываем ошибку и не запускаем фоновые вычисления.
    if (!ValidateInput(errorMessage))
    {
        ShowStatus(errorMessage, false);
        return;
    }

    // Собираем объект опций, который потом уйдет в вычислительное ядро.
    SamplingOptions options;

    // Размер выборки переводим к типу std::size_t,
    // потому что дальше он используется как размер контейнеров и циклов.
    options.sampleSize = static_cast<std::size_t>(sampleSizeEdit_->value());

    // Число интервалов гистограммы тоже переводим к size_t.
    options.histogramBins = static_cast<std::size_t>(binsEdit_->value());

    // Число отображаемых точек превью переводим к size_t по той же причине.
    options.previewCount = static_cast<std::size_t>(previewCountEdit_->value());

    // Seed переводим к 32-битному беззнаковому типу,
    // который ожидает генератор случайных чисел.
    options.seed = static_cast<std::uint32_t>(seedEdit_->value());

    // Блокируем элементы формы и переводим кнопку в режим "идет расчет".
    ApplySamplingRunningState(true);

    // Показываем пользователю текущую стадию работы.
    // Сообщение прямо перечисляет основные математические шаги:
    // вычисление фактора Холецкого A, генерацию независимого U
    // и построение выборочных оценок уже по X.
    ShowStatus("Идёт моделирование. Вычисляю A, генерирую U и строю выборочные оценки.", true);

    // Запускаем вычисление в фоне через QtConcurrent::run,
    // чтобы не блокировать GUI-поток.
    //
    // Lambda захватывает options по значению,
    // поэтому фоновая задача работает со своей собственной копией параметров.
    //
    // Запись `::RunSampling(options)` с глобальным `::`
    // явно вызывает свободную функцию из вычислительного слоя,
    // а не метод MainWindow::RunSampling().
    simulationWatcher_->setFuture(QtConcurrent::run([options]() { return ::RunSampling(options); }));
}

// ----------------------------------------------------------------------------
// МЕТОД ValidateInput - ПРОВЕРКА КОРРЕКТНОСТИ ВВЕДЕННЫХ ПАРАМЕТРОВ
// ----------------------------------------------------------------------------
// Назначение:
// Проверить, что все числовые параметры эксперимента допустимы,
// и при ошибке вернуть подробное сообщение для пользователя.
//
// Принимает:
// - errorMessage : строка, в которую будет записана причина ошибки.
//
// Возвращает:
// - bool : true, если ввод корректен; false, если найдено нарушение.
// ----------------------------------------------------------------------------
bool MainWindow::ValidateInput(QString& errorMessage) const
{
    // Размер выборки должен быть строго положительным:
    // нулевая или отрицательная выборка статистически бессмысленна.
    if (sampleSizeEdit_->value() <= 0)
    {
        errorMessage = "Размер выборки должен быть больше нуля.";
        return false;
    }

    // Для гистограммы нужен хотя бы разбиение на два интервала.
    // При одном интервале распределение теряет смысл как форма.
    if (binsEdit_->value() <= 1)
    {
        errorMessage = "Число интервалов гистограммы должно быть больше единицы.";
        return false;
    }

    // Число показываемых точек тоже должно быть положительным,
    // иначе нечего выводить в таблицу превью и на график траектории.
    if (previewCountEdit_->value() <= 0)
    {
        errorMessage = "Число отображаемых точек должно быть больше нуля.";
        return false;
    }

    // Нельзя просить показать больше точек, чем реально сгенерировано в выборке.
    if (previewCountEdit_->value() > sampleSizeEdit_->value())
    {
        errorMessage = "Число отображаемых точек не должно превышать размер выборки.";
        return false;
    }

    // Seed генератора тоже требуем положительным,
    // чтобы не допускать сомнительных или специальных значений.
    if (seedEdit_->value() <= 0)
    {
        errorMessage = "Seed генератора должен быть положительным.";
        return false;
    }

    // Если ни одно из ограничений не нарушено, ввод считаем корректным.
    return true;
}

// ----------------------------------------------------------------------------
// МЕТОД ApplySamplingRunningState - ПЕРЕКЛЮЧЕНИЕ GUI В РЕЖИМ РАСЧЕТА
// ----------------------------------------------------------------------------
// Назначение:
// Синхронно перевести элементы ввода и кнопку запуска
// в состояние "идет расчет" или обратно в обычный режим.
//
// Принимает:
// - running : true, если моделирование запущено; false, если завершено.
//
// Возвращает:
// - ничего.
// ----------------------------------------------------------------------------
void MainWindow::ApplySamplingRunningState(bool running)
{
    // Сохраняем текущее логическое состояние расчета.
    isSamplingRunning_ = running;

    // Пока моделирование идет, поле размера выборки блокируем.
    SetFieldContainerEnabled(sampleSizeEdit_, !running);

    // Блокируем настройку числа интервалов гистограммы.
    SetFieldContainerEnabled(binsEdit_, !running);

    // Блокируем настройку числа показываемых точек.
    SetFieldContainerEnabled(previewCountEdit_, !running);

    // Блокируем и поле seed, чтобы пользователь не менял параметры на лету.
    SetFieldContainerEnabled(seedEdit_, !running);

    // Основную кнопку во время расчета выключаем.
    runButton_->setEnabled(!running);

    // Текст на кнопке тоже меняем,
    // чтобы по самому контролу было видно текущее состояние.
    runButton_->setText(running ? "Идёт моделирование..." : "Выполнить моделирование");
}

// ----------------------------------------------------------------------------
// МЕТОД FinishSampling - ФИНАЛИЗАЦИЯ ПОСЛЕ ЗАВЕРШЕНИЯ ФОНОВОГО РАСЧЕТА
// ----------------------------------------------------------------------------
// Назначение:
// Получить готовый SamplingResult, снять режим блокировки GUI,
// обработать возможную ошибку и при успехе заполнить все вкладки результатов.
//
// Принимает:
// - result : итог вычислительного ядра.
//
// Возвращает:
// - ничего.
// ----------------------------------------------------------------------------
void MainWindow::FinishSampling(const SamplingResult& result)
{
    // Вне зависимости от исхода расчета возвращаем GUI в обычное состояние.
    ApplySamplingRunningState(false);

    // Если вычислительное ядро сообщило об ошибке,
    // показываем сообщение и дальше интерфейс не заполняем.
    if (!result.success)
    {
        ShowStatus(QString::fromStdString(result.message), false);
        return;
    }

    // Сохраняем успешный результат в кеш,
    // чтобы другие методы и модели могли ссылаться на него позже.
    lastResult_ = result;

    // Помечаем, что валидный результат теперь существует.
    hasResult_ = true;

    // Заполняем summary-карточки.
    PopulateSummary(result);

    // Заполняем матричные виджеты.
    PopulateMatrices(result);

    // Заполняем компактную таблицу превью U и X.
    PopulatePreviewTable(result);

    // Передаем полную выборку в модель большой таблицы.
    PopulateSamplesTable(result);

    // Перестраиваем все графики.
    UpdatePlots(result);

    // В конце показываем итоговый статус успешного выполнения.
    ShowStatus(QString::fromStdString(result.message), true);
}

// ----------------------------------------------------------------------------
// МЕТОД PopulateSummary - ЗАПОЛНЕНИЕ ИТОГОВЫХ SUMMARY-КАРТОЧЕК
// ----------------------------------------------------------------------------
// Назначение:
// Перенести краткие числовые показатели эксперимента
// из SamplingResult в верхние карточки вкладки "Итоги".
//
// Принимает:
// - result : результат моделирования.
//
// Возвращает:
// - ничего.
// ----------------------------------------------------------------------------
void MainWindow::PopulateSummary(const SamplingResult& result)
{
    // Используем русскую локаль,
    // чтобы большие целые значения форматировались привычно для пользователя.
    const QLocale ruLocale(QLocale::Russian, QLocale::Russia);

    // Показываем размерность случайного вектора X.
    dimensionValue_->setText(ruLocale.toString(static_cast<qulonglong>(result.summary.dimension)));

    // Показываем фактический размер выборки.
    sampleCountValue_->setText(ruLocale.toString(static_cast<qulonglong>(result.summary.sampleSize)));

    // Показываем максимум ошибки по компонентам среднего.
    meanErrorValue_->setText(FormatNumber(result.summary.maxMeanError));

    // Показываем максимум ошибки по элементам ковариационной матрицы.
    covarianceErrorValue_->setText(FormatNumber(result.summary.maxCovarianceError));

    // Показываем ошибку ковариационной матрицы в норме Фробениуса.
    frobeniusErrorValue_->setText(FormatNumber(result.summary.frobeniusCovarianceError));

    // Отдельно выводим seed, с которым была сгенерирована текущая выборка.
    seedValue_->setText(ruLocale.toString(static_cast<qulonglong>(result.options.seed)));
}

// ----------------------------------------------------------------------------
// МЕТОД PopulateMatrices - ЗАПОЛНЕНИЕ ВСЕХ МАТРИЧНЫХ КАРТОЧЕК
// ----------------------------------------------------------------------------
// Назначение:
// Передать в виджеты матриц теоретические и выборочные характеристики:
// m_X, K_X, фактор Холецкого A, оценки среднего, ковариации и корреляции.
//
// Принимает:
// - result : результат моделирования.
//
// Возвращает:
// - ничего.
// ----------------------------------------------------------------------------
void MainWindow::PopulateMatrices(const SamplingResult& result)
{
    // Подписи x1, x2, ..., xn понадобятся для объектов,
    // живущих уже в пространстве случайного вектора X.
    const QStringList xHeaders = BuildComponentHeaders("x", Lab2Variant::kDimension);

    // Подписи u1, u2, ..., un нужны для независимого вектора U,
    // из которого затем строится X.
    const QStringList uHeaders = BuildComponentHeaders("u", Lab2Variant::kDimension);

    // Подпись строки для теоретического вектора среднего.
    const QString meanVectorLabel = QStringLiteral("mₓ");

    // Подпись строки для выборочной оценки среднего с "шапочкой".
    const QString empiricalMeanVectorLabel = QStringLiteral("m̂ₓ");

    // Теоретическое среднее храним как вектор,
    // но MatrixDisplayWidget рисует матричную форму,
    // поэтому оборачиваем его в матрицу из одной строки.
    static_cast<MatrixDisplayWidget*>(theoreticalMeanMatrix_)
        ->SetMatrixData(MakeSingleRowMatrix(result.theoreticalMean), xHeaders, {meanVectorLabel});

    // Теоретическую ковариационную матрицу K_X показываем как квадратную матрицу
    // с одними и теми же заголовками по строкам и столбцам.
    static_cast<MatrixDisplayWidget*>(theoreticalCovarianceMatrix_)
        ->SetMatrixData(result.theoreticalCovariance, xHeaders, xHeaders);

    // Фактор Холецкого A связывает независимый вектор U и коррелированный X.
    // В модели вида X = m_X + A * U
    // строки A относятся к компонентам X, а столбцы - к компонентам U.
    static_cast<MatrixDisplayWidget*>(choleskyMatrix_)->SetMatrixData(result.choleskyFactor, uHeaders, xHeaders);

    // Выборочную оценку среднего тоже приводим к форме матрицы-строки.
    static_cast<MatrixDisplayWidget*>(empiricalMeanMatrix_)
        ->SetMatrixData(MakeSingleRowMatrix(result.empiricalMean), xHeaders, {empiricalMeanVectorLabel});

    // Выборочную ковариацию показываем в той же координатной системе x_i.
    static_cast<MatrixDisplayWidget*>(empiricalCovarianceMatrix_)
        ->SetMatrixData(result.empiricalCovariance, xHeaders, xHeaders);

    // И отдельно показываем выборочную корреляционную матрицу.
    static_cast<MatrixDisplayWidget*>(empiricalCorrelationMatrix_)
        ->SetMatrixData(result.empiricalCorrelation, xHeaders, xHeaders);
}

// ----------------------------------------------------------------------------
// МЕТОД PopulatePreviewTable - ЗАПОЛНЕНИЕ КОМПАКТНОЙ ТАБЛИЦЫ ПРЕВЬЮ
// ----------------------------------------------------------------------------
// Назначение:
// Построить и передать в модель таблицы первые строки совместного превью
// для векторов U и X.
//
// Принимает:
// - result : результат моделирования.
//
// Возвращает:
// - ничего.
// ----------------------------------------------------------------------------
void MainWindow::PopulatePreviewTable(const SamplingResult& result)
{
    // Первый столбец - это просто номер строки наблюдения.
    QStringList headers {"№"};

    // Затем добавляем подписи компонент независимого вектора U.
    headers.append(BuildComponentHeaders("u", Lab2Variant::kDimension));

    // После них добавляем подписи компонент преобразованного вектора X.
    headers.append(BuildComponentHeaders("x", Lab2Variant::kDimension));

    // BuildPreviewMatrix(result) собирает компактную матрицу превью:
    // в каждой строке сначала идут компоненты U, затем соответствующие компоненты X.
    static_cast<NumericMatrixModel*>(previewTableModel_)
        ->SetMatrixData(BuildPreviewMatrix(result), headers);
}

// ----------------------------------------------------------------------------
// МЕТОД PopulateSamplesTable - ПЕРЕДАЧА ПОЛНОЙ ВЫБОРКИ В ТАБЛИЦУ X
// ----------------------------------------------------------------------------
// Назначение:
// Подключить полную сгенерированную выборку X к модели большой таблицы.
//
// Принимает:
// - result : результат моделирования.
//
// Возвращает:
// - ничего.
// ----------------------------------------------------------------------------
void MainWindow::PopulateSamplesTable(const SamplingResult& result)
{
    // Здесь сам параметр result не используется напрямую,
    // потому что к этому моменту успешный результат уже сохранен в lastResult_.
    Q_UNUSED(result);

    // Передаем модели указатель на массив выборки из кешированного результата.
    static_cast<SampleTableModel*>(sampleTableModel_)->SetSamples(&lastResult_.samples);
}

// ----------------------------------------------------------------------------
// МЕТОД UpdatePlots - ПЕРЕСТРОЕНИЕ ВСЕХ ГРАФИКОВ ПО РЕЗУЛЬТАТУ
// ----------------------------------------------------------------------------
// Назначение:
// Заполнить карточки гистограмм и график траекторий сериями,
// подготовленными из содержимого SamplingResult.
//
// Принимает:
// - result : результат моделирования.
//
// Возвращает:
// - ничего.
// ----------------------------------------------------------------------------
void MainWindow::UpdatePlots(const SamplingResult& result)
{
    // Каждой компоненте X заранее назначаем собственный цвет,
    // чтобы один и тот же индекс компоненты одинаково выглядел на разных графиках.
    const std::array<QColor, Lab2Variant::kDimension> colors = {
        QColor("#2563eb"),
        QColor("#dc2626"),
        QColor("#059669"),
        QColor("#7c3aed"),
    };

    // Подписи x1, x2, ..., xn понадобятся и для заголовков серий,
    // и для графика траекторий.
    const QStringList xHeaders = BuildComponentHeaders("x", Lab2Variant::kDimension);

    // Обходим карточки гистограмм и соответствующие им подготовленные данные.
    for (std::size_t index = 0; index < histogramPlots_.size() && index < result.histograms.size(); ++index)
    {
        // componentLabel - это человекочитаемое имя текущей компоненты: x1, x2 и т.д.
        const QString componentLabel = xHeaders.value(static_cast<int>(index));

        // meanLabel - математическая подпись m_i для среднего i-й компоненты.
        const QString meanLabel = IndexedSymbol("m", QString::number(index + 1));

        // covarianceLabel - подпись k_ii.
        // Здесь диагональный элемент K_X используется как дисперсия i-й компоненты:
        // для одномерной маргинали X_i имеем распределение N(m_i, k_ii).
        const QString covarianceLabel =
            IndexedSymbol("k", QString("%1%1").arg(index + 1));

        // Для каждой компоненты строим две серии:
        // 1) ступенчатый контур гистограммы по эмпирическим данным;
        // 2) теоретическую нормальную плотность с тем же m_i и k_ii.
        histogramPlots_[index]->SetSeries(
            {
                {QString("Гистограмма %1").arg(componentLabel),
                 // Контур гистограммы строится по уже готовым биннам.
                 BuildHistogramOutline(result.histograms[index].bins),
                 // Цвет серии берем из заранее фиксированной палитры.
                 colors[index],
                 // Для контура гистограммы используем чуть более толстую линию.
                 2.4,
                 // Гистограмма рисуется сплошной линией.
                 Qt::SolidLine,
                 // HoverMode::Step нужен, потому что гистограмма - кусочно-постоянная функция.
                 PlotSeriesData::HoverMode::Step},
                {QString("Теоретическая плотность N(%1, %2)").arg(meanLabel, covarianceLabel),
                 // Теоретическую кривую плотности строим из параметров той же компоненты.
                 BuildNormalDensityCurve(result.histograms[index]),
                 // Для теоретической кривой используем единый темно-синий цвет.
                 QColor("#0f4c81"),
                 // Чуть более тонкая линия, чем у гистограммы.
                 2.0,
                 // Теоретическую плотность делаем пунктирной,
                 // чтобы ее было легко отличить от эмпирического контура.
                 Qt::DashLine},
            });
    }

    // Ниже подготавливаем набор серий для графика первых наблюдений компонентов X.
    QVector<PlotSeriesData> sequenceSeries;

    // Сразу резервируем место под все компоненты,
    // чтобы не делать лишних realocation при push_back().
    sequenceSeries.reserve(static_cast<int>(Lab2Variant::kDimension));

    // Для каждой компоненты X_i строим отдельную временную серию x_i(n).
    for (std::size_t index = 0; index < Lab2Variant::kDimension; ++index)
    {
        sequenceSeries.push_back(
            {xHeaders.value(static_cast<int>(index)),
             // Берем последовательность значений i-й компоненты
             // из первых transformedPreviewSamples.
             BuildSequenceSeries(result.transformedPreviewSamples, index),
             // Цвет сохраняем тем же, что и на остальных графиках этой компоненты.
             colors[index],
             // Толщину линии делаем умеренной,
             // чтобы одновременно помещалось несколько компонент.
             1.9});
    }

    // Передаем все временные серии в график траектории.
    sequencePlot_->SetSeries(sequenceSeries);
}

// ----------------------------------------------------------------------------
// МЕТОД ShowStatus - ПОКАЗ СТАТУСНОГО СООБЩЕНИЯ
// ----------------------------------------------------------------------------
// Назначение:
// Показать пользователю текст статуса и подобрать для него
// зеленое или красное оформление в зависимости от успеха операции.
//
// Принимает:
// - text    : текст сообщения;
// - success : true для успешного/информационного статуса, false для ошибки.
//
// Возвращает:
// - ничего.
// ----------------------------------------------------------------------------
void MainWindow::ShowStatus(const QString& text, bool success)
{
    // Записываем текст сообщения в статусную плашку.
    statusBadge_->setText(text);

    // Разрешаем rich text, чтобы при необходимости сообщение могло содержать HTML-разметку.
    statusBadge_->setTextFormat(Qt::RichText);

    // Для успешного статуса используем зеленую плашку.
    if (success)
    {
        statusBadge_->setStyleSheet(
            // Зеленая рамка и мягкий зеленый фон визуально сообщают об успехе.
            "border: 1px solid #94d7aa; background: #eaf8ef; color: #1d5a32; "
            // Скругление и внутренние поля делают статус похожим на аккуратную badge-карточку.
            "border-radius: 12px; padding: 10px 12px;");
    }
    else
    {
        statusBadge_->setStyleSheet(
            // Для ошибки используем красноватую рамку, светлый розовый фон и темно-красный текст.
            "border: 1px solid #efb7bf; background: #fff1f3; color: #7b2636; "
            // Геометрия плашки остается той же, меняется только цветовое кодирование.
            "border-radius: 12px; padding: 10px 12px;");
    }
}

// ----------------------------------------------------------------------------
// МЕТОД FormatNumber - ЕДИНАЯ ОБЕРТКА НАД ФОРМАТИРОВАНИЕМ ЧИСЕЛ
// ----------------------------------------------------------------------------
// Назначение:
// Пропустить число через общее правило форматирования GUI,
// чтобы summary-значения выглядели так же, как числа в таблицах и матрицах.
//
// Принимает:
// - value : число для форматирования.
//
// Возвращает:
// - QString : готовую строку для показа в интерфейсе.
// ----------------------------------------------------------------------------
QString MainWindow::FormatNumber(double value)
{
    // Делегируем форматирование общей функции из UiFormatting,
    // чтобы во всем приложении был единый числовой стиль.
    return FormatNumericValue(value);
}
