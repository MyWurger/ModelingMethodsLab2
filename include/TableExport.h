// ============================================================================
// ФАЙЛ TABLEEXPORT.H - ЭКСПОРТ ТАБЛИЦ LAB_2 В PNG
// ============================================================================

#pragma once

class QString;
class QTableView;
class QWidget;

void ExportTableViewPng(QWidget* parent, QTableView* sourceTable, const QString& title);
