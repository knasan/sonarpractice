#ifndef FILESELECTIONDIALOG_H
#define FILESELECTIONDIALOG_H

#include <QDialog>
#include <QListWidget>
#include <QDialogButtonBox>

class FileSelectionDialog : public QDialog {
    Q_OBJECT
public:
    explicit FileSelectionDialog(int excludeId, QWidget *parent = nullptr);
    [[nodiscard]] QList<int> getSelectedFileIds() const;

private:
    void loadFiles(int excludeId);
    void updateFilter();
    void showContextMenu(const QPoint &pos);

    [[nodiscard]] QString getCategoryForFile(const QString &fileName);

    QListWidget *listWidget_m;
    QLineEdit* searchEdit_m;
};

#endif // FILESELECTIONDIALOG_H
