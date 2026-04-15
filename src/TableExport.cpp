// ============================================================================
// ФАЙЛ TABLEEXPORT.CPP - СОХРАНЕНИЕ ТАБЛИЦ LAB_2 В PNG
// ============================================================================

#include "TableExport.h"

#include <QAbstractItemView>
#include <QDateTime>
#include <QDialog>
#include <QDir>
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QRegularExpression>
#include <QTableView>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>

namespace
{
QString ProjectTablesDirectoryPath()
{
    const QString projectRoot = QString::fromUtf8(MODELING_METHODS_LAB2_SOURCE_DIR);
    const QString tablesPath = QDir(projectRoot).absoluteFilePath("exports/tables");
    QDir().mkpath(tablesPath);
    return tablesPath;
}

QString BuildTableExportFileName(const QString& title)
{
    QString stem = title;
    stem.remove(QRegularExpression("<[^>]*>"));
    stem.replace(QRegularExpression("[^\\p{L}\\p{N}]+"), "_");
    stem = stem.trimmed();
    while (stem.startsWith('_'))
    {
        stem.remove(0, 1);
    }
    while (stem.endsWith('_'))
    {
        stem.chop(1);
    }

    if (stem.isEmpty())
    {
        stem = "table";
    }

    if (stem.size() > 64)
    {
        stem = stem.left(64);
    }

    return QString("%1__%2.png").arg(stem, QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
}

void ShowExportDialog(QWidget* parent, const QString& title, const QString& message, bool success)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(title);
    dialog.setModal(true);
    dialog.setObjectName("exportDialog");
    dialog.setMinimumWidth(520);
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

    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(22, 20, 22, 18);
    layout->setSpacing(14);

    auto* titleLabel = new QLabel(title, &dialog);
    titleLabel->setObjectName("exportDialogTitle");

    auto* messageLabel = new QLabel(message, &dialog);
    messageLabel->setObjectName("exportDialogMessage");
    messageLabel->setWordWrap(true);
    messageLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->setContentsMargins(0, 4, 0, 0);

    auto* okButton = new QPushButton("OK", &dialog);
    okButton->setObjectName("exportDialogButton");
    QObject::connect(okButton, &QPushButton::clicked, &dialog, &QDialog::accept);

    layout->addWidget(titleLabel);
    layout->addWidget(messageLabel);
    buttonLayout->addStretch();
    buttonLayout->addWidget(okButton);
    layout->addLayout(buttonLayout);

    dialog.exec();
}

int FullTableWidth(const QTableView* table)
{
    if (table == nullptr)
    {
        return 0;
    }

    const int verticalHeaderWidth = table->verticalHeader()->isVisible() ? table->verticalHeader()->width() : 0;
    return verticalHeaderWidth + table->horizontalHeader()->length() + 2 * table->frameWidth();
}

int FullTableHeight(const QTableView* table)
{
    if (table == nullptr || table->model() == nullptr)
    {
        return 0;
    }

    const int rowCount = table->model()->rowCount();
    const int rowHeight = rowCount > 0 ? table->rowHeight(0) : table->verticalHeader()->defaultSectionSize();
    return table->horizontalHeader()->height() + rowCount * rowHeight + 2 * table->frameWidth();
}
} // namespace

void ExportTableViewPng(QWidget* parent, QTableView* sourceTable, const QString& title)
{
    if (parent == nullptr || sourceTable == nullptr || sourceTable->model() == nullptr ||
        sourceTable->model()->rowCount() == 0)
    {
        ShowExportDialog(parent, "PNG не сохранён", "Таблица пока пуста. Сначала выполните моделирование.", false);
        return;
    }

    QFrame exportCard;
    exportCard.setObjectName("plotCard");
    exportCard.setAttribute(Qt::WA_DontShowOnScreen, true);
    if (QWidget* window = parent->window())
    {
        exportCard.setStyleSheet(window->styleSheet());
    }

    auto* exportLayout = new QVBoxLayout(&exportCard);
    exportLayout->setContentsMargins(12, 12, 12, 12);
    exportLayout->setSpacing(10);

    auto* exportTitle = new QLabel(title, &exportCard);
    exportTitle->setObjectName("tableTitle");
    exportTitle->setTextFormat(Qt::RichText);

    auto* exportTable = new QTableView(&exportCard);
    exportTable->setModel(sourceTable->model());
    exportTable->setAlternatingRowColors(sourceTable->alternatingRowColors());
    exportTable->setSelectionBehavior(sourceTable->selectionBehavior());
    exportTable->setSelectionMode(sourceTable->selectionMode());
    exportTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    exportTable->setWordWrap(sourceTable->wordWrap());
    exportTable->setTextElideMode(sourceTable->textElideMode());
    exportTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    exportTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    exportTable->verticalHeader()->setVisible(sourceTable->verticalHeader()->isVisible());
    exportTable->verticalHeader()->setDefaultAlignment(Qt::AlignCenter);
    exportTable->verticalHeader()->setDefaultSectionSize(sourceTable->rowHeight(0));
    exportTable->horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);
    exportTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    const int exportWidth = std::max(sourceTable->width(), 900);
    exportTable->setMinimumWidth(exportWidth);
    exportTable->setMaximumWidth(exportWidth);
    exportTable->resize(exportWidth, sourceTable->height());
    exportTable->ensurePolished();
    exportTable->horizontalHeader()->resizeSections(QHeaderView::Stretch);

    const int exportHeight = std::max(FullTableHeight(exportTable), 160);
    exportTable->setMinimumSize(exportWidth, exportHeight);
    exportTable->setMaximumSize(exportWidth, exportHeight);

    exportLayout->addWidget(exportTitle);
    exportLayout->addWidget(exportTable);

    exportCard.ensurePolished();
    exportLayout->activate();

    const int finalWidth = exportWidth + 24;
    const int finalHeight = exportHeight + std::max(exportTitle->sizeHint().height(), 28) + 34;
    exportCard.resize(finalWidth, finalHeight);
    exportLayout->setGeometry(QRect(0, 0, finalWidth, finalHeight));
    exportLayout->activate();

    const qreal devicePixelRatio = parent->devicePixelRatioF();
    const qint64 pixelCount = static_cast<qint64>(finalWidth * devicePixelRatio) *
                              static_cast<qint64>(finalHeight * devicePixelRatio);
    constexpr qint64 kMaxPngPixels = 220000000;
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

    QImage image((QSizeF(finalWidth, finalHeight) * devicePixelRatio).toSize(), QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(devicePixelRatio);
    image.fill(Qt::white);

    QPainter painter(&image);
    exportCard.render(&painter);
    painter.end();

    const QString filePath = QDir(ProjectTablesDirectoryPath()).absoluteFilePath(BuildTableExportFileName(title));
    if (!image.save(filePath, "PNG"))
    {
        ShowExportDialog(parent, "PNG не сохранён", "Не удалось сохранить PNG таблицы.", false);
        return;
    }

    ShowExportDialog(parent, "PNG сохранён", QString("Файл сохранён:\n%1").arg(filePath), true);
}
