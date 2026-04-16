// ============================================================================
// ФАЙЛ TABLEEXPORT.CPP - СОХРАНЕНИЕ ТАБЛИЦ LAB_2 В PNG
// ============================================================================
// В этом файле реализован экспорт табличных карточек интерфейса в PNG.
//
// Общая идея такая:
// 1) взять уже существующий QTableView из интерфейса;
// 2) построить его временную "экспортную" копию без скроллбаров;
// 3) растянуть эту копию так, чтобы влезла вся таблица целиком;
// 4) отрендерить готовую карточку в QImage;
// 5) сохранить изображение в exports/tables.
//
// То есть файл отвечает не за данные таблицы и не за GUI-логику окна,
// а за отдельную служебную задачу: красиво и предсказуемо выгрузить
// визуальное представление таблицы в PNG-файл.
// ============================================================================

// Подключаем собственный заголовок с публичным интерфейсом экспорта.
#include "TableExport.h"

// QAbstractItemView нужен для режима редактирования и поведения таблицы.
#include <QAbstractItemView>
// QDateTime нужен для метки времени в имени файла.
#include <QDateTime>
// QDialog используется для аккуратных сообщений об успехе или ошибке.
#include <QDialog>
// QDir нужен для построения путей и создания каталога exports/tables.
#include <QDir>
// QFrame используется как временная карточка, которую мы затем рендерим в PNG.
#include <QFrame>
// QHeaderView нужен для доступа к размерам и настройкам заголовков таблицы.
#include <QHeaderView>
// QHBoxLayout нужен для выравнивания кнопки OK внизу диалога.
#include <QHBoxLayout>
// QImage - итоговый контейнер, в который рисуется экспортируемая карточка.
#include <QImage>
// QLabel нужен для заголовков и текстов сообщений в диалогах.
#include <QLabel>
// QPainter выполняет фактический рендер виджета в изображение.
#include <QPainter>
// QPushButton - кнопка подтверждения в диалоге результата экспорта.
#include <QPushButton>
// QRegularExpression нужен для очистки заголовка перед использованием в имени файла.
#include <QRegularExpression>
// QTableView - основной источник табличного содержимого.
#include <QTableView>
// QVBoxLayout собирает вертикальную структуру экспортной карточки и диалогов.
#include <QVBoxLayout>
// QWidget нужен для базового доступа к родителю и его свойствам устройства.
#include <QWidget>

// std::max используется для безопасного выбора минимально допустимых размеров.
#include <algorithm>

// Анонимное пространство имен скрывает внутренние helper-функции в пределах файла.
namespace
{
// ----------------------------------------------------------------------------
// ФУНКЦИЯ ProjectTablesDirectoryPath
// ----------------------------------------------------------------------------
// Назначение:
// Получить абсолютный путь до папки, в которую складываются PNG-экспорты таблиц.
//
// Если папки еще нет, функция создает ее автоматически.
//
// Принимает:
// - ничего.
//
// Возвращает:
// - QString : абсолютный путь до каталога exports/tables.
// ----------------------------------------------------------------------------
QString ProjectTablesDirectoryPath()
{
    // CMake подставляет сюда абсолютный путь к корню текущего проекта.
    const QString projectRoot = QString::fromUtf8(MODELING_METHODS_LAB2_SOURCE_DIR);

    // Относительно корня проекта формируем каталог exports/tables.
    const QString tablesPath = QDir(projectRoot).absoluteFilePath("exports/tables");

    // Гарантируем существование каталога, чтобы save() не упал из-за отсутствующего пути.
    QDir().mkpath(tablesPath);

    // Возвращаем готовый абсолютный путь.
    return tablesPath;
}

// ----------------------------------------------------------------------------
// ФУНКЦИЯ BuildTableExportFileName
// ----------------------------------------------------------------------------
// Назначение:
// Построить безопасное и читаемое имя PNG-файла по заголовку таблицы.
//
// Здесь важно:
// - убрать HTML/ rich-text фрагменты;
// - оставить только буквы и цифры;
// - ограничить длину имени;
// - добавить timestamp, чтобы файлы не перетирали друг друга.
//
// Принимает:
// - title : исходный заголовок таблицы или карточки.
//
// Возвращает:
// - QString : безопасное имя PNG-файла без конфликтов по времени.
// ----------------------------------------------------------------------------
QString BuildTableExportFileName(const QString& title)
{
    // Начинаем с исходного заголовка карточки.
    QString stem = title;

    // Если в заголовке были HTML-теги, удаляем их полностью.
    stem.remove(QRegularExpression("<[^>]*>"));

    // Все непрерывные группы символов, кроме букв и цифр, заменяем на "_".
    stem.replace(QRegularExpression("[^\\p{L}\\p{N}]+"), "_");

    // Убираем пробелы по краям после всех замен.
    stem = stem.trimmed();

    // Если имя началось с "_", удаляем такие артефакты слева.
    while (stem.startsWith('_'))
    {
        stem.remove(0, 1);
    }

    // Аналогично убираем висящие "_" справа.
    while (stem.endsWith('_'))
    {
        stem.chop(1);
    }

    // Если после очистки от заголовка ничего не осталось, даем безопасное имя по умолчанию.
    if (stem.isEmpty())
    {
        stem = "table";
    }

    // Излишне длинное имя файла бессмысленно, поэтому ограничиваем длину основы.
    if (stem.size() > 64)
    {
        stem = stem.left(64);
    }

    // Финальное имя строится как:
    // очищенный_заголовок__YYYYMMDD_HHMMSS.png
    return QString("%1__%2.png").arg(stem, QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
}

// ----------------------------------------------------------------------------
// ФУНКЦИЯ ShowExportDialog
// ----------------------------------------------------------------------------
// Назначение:
// Показать стилизованный модальный диалог с результатом экспорта.
//
// Один и тот же helper используется и для успешного завершения,
// и для любых ошибок сохранения.
//
// Принимает:
// - parent  : родительский виджет Qt;
// - title   : заголовок диалога;
// - message : основной текст сообщения;
// - success : флаг, определяющий визуальное оформление диалога.
//
// Возвращает:
// - void : функция только показывает диалог и завершает работу после его закрытия.
// ----------------------------------------------------------------------------
void ShowExportDialog(QWidget* parent, const QString& title, const QString& message, bool success)
{
    // Создаем локальный диалог, привязанный к родительскому окну.
    QDialog dialog(parent);

    // Заголовок окна нужен для системной рамки диалога.
    dialog.setWindowTitle(title);

    // Диалог модальный: пока пользователь не подтвердит сообщение,
    // управление обратно в основное окно не возвращается.
    dialog.setModal(true);

    // ObjectName нужен для адресного применения QSS только к этому диалогу.
    dialog.setObjectName("exportDialog");

    // Задаем минимальную ширину, чтобы длинные пути не выглядели совсем сломанно.
    dialog.setMinimumWidth(520);

    // Весь QSS собирается здесь, чтобы диалог выглядел в стиле проекта.
    // Цвет кнопки зависит от success: синий для успеха, оранжево-красный для ошибки.
    dialog.setStyleSheet(QString(R"(
        QDialog#exportDialog {
            background: #fbfdff;
        }

        QLabel#exportDialogTitle {
            color: #102033;
            font-size: 20px;
            font-weight: 800;
        }

        QLabel#exportDialogMessage {
            color: #294560;
            font-size: 15px;
            font-weight: 600;
            line-height: 130%;
        }

        QPushButton#exportDialogButton {
            background: %1;
            color: #ffffff;
            border: none;
            border-radius: 10px;
            font-size: 15px;
            font-weight: 700;
            padding: 8px 22px;
        }

        QPushButton#exportDialogButton:hover {
            background: %2;
        }
    )")
                             .arg(success ? "#2a6df4" : "#c2410c", success ? "#1f61e4" : "#9a3412"));

    // Основной вертикальный layout для содержимого диалога.
    auto* layout = new QVBoxLayout(&dialog);

    // Делаем внутренние поля диалога аккуратными и визуально "воздушными".
    layout->setContentsMargins(22, 20, 22, 18);
    layout->setSpacing(14);

    // Верхняя жирная строка - заголовок результата.
    auto* titleLabel = new QLabel(title, &dialog);
    titleLabel->setObjectName("exportDialogTitle");

    // Основной текст сообщения: сюда может попадать и путь к файлу, и причина ошибки.
    auto* messageLabel = new QLabel(message, &dialog);
    messageLabel->setObjectName("exportDialogMessage");

    // Разрешаем перенос строк, иначе длинный текст будет ломать диалог по ширине.
    messageLabel->setWordWrap(true);

    // Разрешаем выделять текст мышью, чтобы пользователь мог скопировать путь к файлу.
    messageLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    // Отдельный layout под кнопку подтверждения.
    auto* buttonLayout = new QHBoxLayout();

    // Слегка опускаем кнопку относительно текста сообщения.
    buttonLayout->setContentsMargins(0, 4, 0, 0);

    // Кнопка закрытия диалога.
    auto* okButton = new QPushButton("OK", &dialog);
    okButton->setObjectName("exportDialogButton");

    // По нажатию закрываем диалог штатным accept().
    QObject::connect(okButton, &QPushButton::clicked, &dialog, &QDialog::accept);

    // Собираем финальную структуру окна.
    layout->addWidget(titleLabel);
    layout->addWidget(messageLabel);
    buttonLayout->addStretch();
    buttonLayout->addWidget(okButton);
    layout->addLayout(buttonLayout);

    // Показываем диалог и ждем решения пользователя.
    dialog.exec();
}

// ----------------------------------------------------------------------------
// ФУНКЦИЯ FullTableWidth
// ----------------------------------------------------------------------------
// Назначение:
// Посчитать полную ширину таблицы с учетом заголовка строк и рамки.
//
// Это нужно не для отображаемой на экране таблицы, а именно для ее полной
// экспортной копии без горизонтальной прокрутки.
//
// Принимает:
// - table : таблицу, для которой требуется вычислить итоговую ширину.
//
// Возвращает:
// - int : полную ширину таблицы в пикселях.
// ----------------------------------------------------------------------------
int FullTableWidth(const QTableView* table)
{
    // Если таблицы нет, возвращать осмысленную ширину невозможно.
    if (table == nullptr)
    {
        return 0;
    }

    // Ширина вертикального заголовка учитывается только если он реально видим.
    const int verticalHeaderWidth = table->verticalHeader()->isVisible() ? table->verticalHeader()->width() : 0;

    // Итоговая ширина:
    // заголовок строк + суммарная длина всех столбцов + толщина рамки слева и справа.
    return verticalHeaderWidth + table->horizontalHeader()->length() + 2 * table->frameWidth();
}

// ----------------------------------------------------------------------------
// ФУНКЦИЯ FullTableHeight
// ----------------------------------------------------------------------------
// Назначение:
// Посчитать полную высоту таблицы так, чтобы при экспорте влезли все строки.
//
// Принимает:
// - table : таблицу, для которой требуется вычислить итоговую высоту.
//
// Возвращает:
// - int : полную высоту таблицы в пикселях.
// ----------------------------------------------------------------------------
int FullTableHeight(const QTableView* table)
{
    // Без таблицы или без модели высоту считать бессмысленно.
    if (table == nullptr || table->model() == nullptr)
    {
        return 0;
    }

    // Узнаем общее количество строк в модели.
    const int rowCount = table->model()->rowCount();

    // Если строки существуют, берем реальную высоту первой строки.
    // Если строк еще нет, используем стандартную высоту секции вертикального заголовка.
    const int rowHeight = rowCount > 0 ? table->rowHeight(0) : table->verticalHeader()->defaultSectionSize();

    // Итоговая высота:
    // высота горизонтального заголовка + все строки + рамка сверху и снизу.
    return table->horizontalHeader()->height() + rowCount * rowHeight + 2 * table->frameWidth();
}
} // namespace

// ----------------------------------------------------------------------------
// ФУНКЦИЯ ExportTableViewPng
// ----------------------------------------------------------------------------
// Назначение:
// Сохранить полное визуальное представление QTableView в PNG-файл.
//
// Сценарий работы:
// 1) проверить входные данные;
// 2) построить временную off-screen карточку;
// 3) поместить в нее заголовок и полную таблицу без прокрутки;
// 4) отрендерить карточку в QImage;
// 5) сохранить QImage на диск и показать результат пользователю.
//
// Принимает:
// - parent      : родительский виджет для диалогов результата;
// - sourceTable : исходную таблицу, которую требуется экспортировать;
// - title       : заголовок экспортной карточки и основа имени файла.
//
// Возвращает:
// - void : функция либо сохраняет PNG, либо показывает сообщение об ошибке.
// ----------------------------------------------------------------------------
void ExportTableViewPng(QWidget* parent, QTableView* sourceTable, const QString& title)
{
    // Если нет родителя, таблицы, модели или строк, экспорт невозможен.
    if (parent == nullptr || sourceTable == nullptr || sourceTable->model() == nullptr ||
        sourceTable->model()->rowCount() == 0)
    {
        ShowExportDialog(parent, "PNG не сохранён", "Таблица пока пуста. Сначала выполните моделирование.", false);
        return;
    }

    // Создаем временную карточку, которая никогда не показывается на экране,
    // а существует только как источник рендера в PNG.
    QFrame exportCard;

    // Даем ей тот же objectName, что и обычным карточкам проекта,
    // чтобы reused-стили применились автоматически.
    exportCard.setObjectName("plotCard");

    // Явно помечаем карточку как off-screen: она нужна только для render().
    exportCard.setAttribute(Qt::WA_DontShowOnScreen, true);

    // Если у родительского окна есть общий styleSheet,
    // переносим его и на экспортную карточку, иначе PNG выглядел бы "сырым".
    if (QWidget* window = parent->window())
    {
        exportCard.setStyleSheet(window->styleSheet());
    }

    // Основной layout экспортной карточки: наверху заголовок, ниже таблица.
    auto* exportLayout = new QVBoxLayout(&exportCard);

    // Внутренние отступы карточки подбираются так, чтобы PNG выглядел
    // максимально близко к реальному интерфейсу.
    exportLayout->setContentsMargins(12, 12, 12, 12);
    exportLayout->setSpacing(10);

    // Создаем заголовок экспортируемой таблицы.
    auto* exportTitle = new QLabel(title, &exportCard);

    // Используем тот же objectName, что и у реальных table-карточек.
    exportTitle->setObjectName("tableTitle");

    // Разрешаем rich text, потому что в заголовках могут использоваться
    // HTML- или Unicode-обозначения.
    exportTitle->setTextFormat(Qt::RichText);

    // Создаем отдельную временную таблицу для экспорта.
    auto* exportTable = new QTableView(&exportCard);

    // Подключаем к ней ту же модель данных, что у исходной таблицы.
    exportTable->setModel(sourceTable->model());

    // Переносим режим чередования цветов строк из исходной таблицы,
    // чтобы PNG визуально совпадал с тем, что видит пользователь в окне.
    exportTable->setAlternatingRowColors(sourceTable->alternatingRowColors());

    // Повторяем поведение выбора ячеек/строк,
    // чтобы экспортная копия наследовала ту же конфигурацию представления.
    exportTable->setSelectionBehavior(sourceTable->selectionBehavior());

    // Переносим режим выделения,
    // даже если в off-screen-таблице он напрямую не используется при взаимодействии.
    exportTable->setSelectionMode(sourceTable->selectionMode());

    // Экспортная копия не должна входить в режим редактирования.
    exportTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // Переносим настройку word wrap,
    // чтобы длинные подписи и значения отображались с теми же правилами переноса.
    exportTable->setWordWrap(sourceTable->wordWrap());

    // Переносим режим elide,
    // чтобы поведение обрезки текста совпадало с исходной таблицей.
    exportTable->setTextElideMode(sourceTable->textElideMode());

    // Вертикальный скроллбар отключаем полностью,
    // потому что в PNG должна попасть вся таблица сразу, а не только видимая область.
    exportTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // Горизонтальный скроллбар тоже отключаем,
    // потому что ширина экспортной копии будет рассчитана под все столбцы целиком.
    exportTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // Повторяем факт видимости вертикального заголовка,
    // чтобы экспорт не потерял подписи строк, если они были включены.
    exportTable->verticalHeader()->setVisible(sourceTable->verticalHeader()->isVisible());

    // Выравниваем подписи вертикального заголовка по центру,
    // чтобы нумерация и обозначения строк выглядели аккуратно.
    exportTable->verticalHeader()->setDefaultAlignment(Qt::AlignCenter);

    // Повторяем высоту секций вертикального заголовка по исходной таблице,
    // чтобы высота строк в экспортной копии не отличалась визуально.
    exportTable->verticalHeader()->setDefaultSectionSize(sourceTable->rowHeight(0));

    // Горизонтальные заголовки также центрируем,
    // чтобы подписи столбцов выглядели симметрично относительно содержимого.
    exportTable->horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);

    // Режим Stretch заставляет все столбцы перераспределить ширину по доступной области,
    // что важно для цельной и аккуратной экспортной карточки.
    exportTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    // Экспортируемая таблица должна быть не уже исходной
    // и при этом не слишком узкой даже для компактных карточек.
    const int exportWidth = std::max(sourceTable->width(), 900);

    // Нижней границей фиксируем минимальную ширину,
    // чтобы layout не смог неожиданно сузить таблицу.
    exportTable->setMinimumWidth(exportWidth);

    // Верхней границей фиксируем ту же ширину,
    // чтобы таблица оставалась строго заданного размера без самопроизвольного растяжения.
    exportTable->setMaximumWidth(exportWidth);

    // Даем таблице первичный размер, близкий к размеру исходного виджета.
    exportTable->resize(exportWidth, sourceTable->height());

    // Принудительно полируем стили перед расчетом геометрии.
    exportTable->ensurePolished();

    // После полировки окончательно пересчитываем ширины секций заголовка.
    exportTable->horizontalHeader()->resizeSections(QHeaderView::Stretch);

    // Вычисляем полную высоту таблицы без прокрутки,
    // но не даем ей стать подозрительно маленькой.
    const int exportHeight = std::max(FullTableHeight(exportTable), 160);

    // Фиксируем таблице полный размер, которого достаточно для всего содержимого.
    exportTable->setMinimumSize(exportWidth, exportHeight);
    exportTable->setMaximumSize(exportWidth, exportHeight);

    // Добавляем заголовок и таблицу в карточку.
    exportLayout->addWidget(exportTitle);
    exportLayout->addWidget(exportTable);

    // Принудительно применяем стили ко всей карточке перед рендером.
    exportCard.ensurePolished();

    // Активируем layout, чтобы он реально разложил дочерние виджеты.
    exportLayout->activate();

    // Финальная ширина карточки учитывает еще и ее собственные поля.
    const int finalWidth = exportWidth + 24;

    // Финальная высота складывается из высоты таблицы, высоты заголовка и полей карточки.
    const int finalHeight = exportHeight + std::max(exportTitle->sizeHint().height(), 28) + 34;

    // Устанавливаем итоговый размер off-screen карточки.
    exportCard.resize(finalWidth, finalHeight);

    // Явно задаем геометрию layout'у по размеру карточки.
    exportLayout->setGeometry(QRect(0, 0, finalWidth, finalHeight));

    // Повторно активируем layout уже на финальной геометрии.
    exportLayout->activate();

    // Учитываем DPR монитора/системы, чтобы PNG не получился мыльным на Retina.
    const qreal devicePixelRatio = parent->devicePixelRatioF();

    // Считаем общее количество пикселей будущего изображения.
    // Это нужно как предохранитель от гигантских PNG, которые могут не поместиться в память.
    const qint64 pixelCount = static_cast<qint64>(finalWidth * devicePixelRatio) *
                              static_cast<qint64>(finalHeight * devicePixelRatio);

    // Верхний технический лимит на размер экспортируемой картинки.
    constexpr qint64 kMaxPngPixels = 220000000;

    // Если таблица слишком большая, честно предупреждаем пользователя
    // и не пытаемся создать заведомо тяжелое изображение.
    if (pixelCount > kMaxPngPixels)
    {
        ShowExportDialog(
            parent,
            "PNG не сохранён",
            QString("Полная таблица слишком большая для одного PNG: %1 строк. Уменьшите размер выборки N.")
                .arg(sourceTable->model()->rowCount()),
            false);
        return;
    }

    // Создаем итоговое изображение нужного физического размера.
    QImage image((QSizeF(finalWidth, finalHeight) * devicePixelRatio).toSize(), QImage::Format_ARGB32_Premultiplied);

    // Сообщаем изображению его логический DPR, чтобы Qt корректно интерпретировал размеры.
    image.setDevicePixelRatio(devicePixelRatio);

    // Заливаем фон белым, чтобы PNG не оказался прозрачным или грязным по краям.
    image.fill(Qt::white);

    // Создаем painter, который будет рисовать exportCard прямо в QImage.
    QPainter painter(&image);

    // Рендерим всю подготовленную карточку как обычный QWidget.
    exportCard.render(&painter);

    // Явно завершаем рисование.
    painter.end();

    // Формируем полный путь сохранения:
    // exports/tables + безопасное имя файла.
    const QString filePath = QDir(ProjectTablesDirectoryPath()).absoluteFilePath(BuildTableExportFileName(title));

    // Пытаемся сохранить изображение на диск в формате PNG.
    if (!image.save(filePath, "PNG"))
    {
        ShowExportDialog(parent, "PNG не сохранён", "Не удалось сохранить PNG таблицы.", false);
        return;
    }

    // При успехе показываем пользователю путь к сохраненному файлу.
    ShowExportDialog(parent, "PNG сохранён", QString("Файл сохранён:\n%1").arg(filePath), true);
}
