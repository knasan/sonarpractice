#ifndef IMPORTDIALOG_H
#define IMPORTDIALOG_H

#include "importprocessor.h"
#include "sonarstructs.h"

#include <QTreeView>
#include <QStandardItemModel>
#include <QItemSelection>
#include <QCheckBox>
#include <QDialog>

class QLineEdit;
class QPushButton;

class ImportDialog : public QDialog {
    Q_OBJECT
public:
    explicit ImportDialog(QWidget *parent = nullptr);
    void setImportData(const QList<ScanBatch>& batches);

private:
    void setupTargetRoot();
    void buildExistingDirTree(const QString &path, QStandardItem *parentItem);

    // Model-Handling
    void fillMappingSource();
    void fillRecursive(QStandardItem* sourceParent, QStandardItem* targetParent);
    void cleanupEmptyFolders(QStandardItem *parent);
    void sideConnection();
    void collectTasksFromModel(QStandardItem* parent, QString currentDirPath, QList<ImportTask>& tasks);
    void accept() override;

    [[nodiscard]] QStandardItem* deepCopyItem(QStandardItem* item);
    [[nodiscard]] QStandardItem* deepCopyItemForUnmap(QStandardItem* item);
    [[nodiscard]] QStandardItem* getTargetFolder();
    [[nodiscard]] QStandardItem* reconstructPathInSource(const QString &fullPath);
    [[nodiscard]] int countFiles(QStandardItem* item);

    void updateImportButtonState();
    bool hasCheckedItems(QStandardItem* item);

    QTreeView *sourceView_m;
    QTreeView *targetView_m;

    QStandardItemModel *sourceModel_m;
    QStandardItemModel *targetModel_m;

    QPushButton *btnMap_m;
    QPushButton *btnUnmap_m;
    QPushButton *btnNewDir_m;

    QPushButton *btnImport_m;
    QLineEdit *searchLineEdit_m;
    QCheckBox *collabsTree_m;

    QString dataBasePath_m;
    bool isManaged_m;

    bool isConnectionsEstablished_m{false};

private slots:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void expandAllTree();
    void mapSelectedItems();
    void addNewDir();
    void unmapItem();
    void applyFilter(const QString &filterText);
    [[nodiscard]] bool filterItemRecursive(QStandardItem *item, const QString &filterText);
};

#endif // IMPORTDIALOG_H
