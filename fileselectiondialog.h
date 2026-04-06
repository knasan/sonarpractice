#ifndef FILESELECTIONDIALOG_H
#define FILESELECTIONDIALOG_H

#include "databasemanager.h"

#include <QDialog>
#include <QListWidget>
#include <QDialogButtonBox>


class FileSelectionDialog : public QDialog {
    Q_OBJECT
public:
    explicit FileSelectionDialog(int excludeId, QWidget *parent = nullptr,  DatabaseManager *db = nullptr);
    virtual ~FileSelectionDialog();
    [[nodiscard]] QList<int> getSelectedFileIds() const;

private:
    void loadFiles(int excludeId);
    void updateFilter();
    void showContextMenu(const QPoint &pos);

    [[nodiscard]] QString getCategoryForFile(const QString &fileName);

    QListWidget *listWidget_m;
    QLineEdit *searchEdit_m;

    DatabaseManager *dbManager_m;

};

#endif // FILESELECTIONDIALOG_H
