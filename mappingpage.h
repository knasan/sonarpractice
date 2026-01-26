#ifndef MAPPINGPAGE_H
#define MAPPINGPAGE_H

#include "basepage.h"
#include "importprocessor.h"
#include <QTreeView>
#include <QStandardItemModel>
#include <QItemSelection>
#include <QCheckBox>

class QLineEdit;
class QPushButton;

class MappingPage : public BasePage {
    Q_OBJECT
public:
    explicit MappingPage(QWidget *parent = nullptr);
    void initializePage() override;

private:
    // Model-Handling
    void fillMappingSourceFromModel(QStandardItem* sourceParent, QStandardItem* targetParent);
    void cleanupEmptyFolders(QStandardItem *parent);
    void sideConnection();
    void collectTasksFromModel(QStandardItem* parent, QString currentCategoryPath, QList<ImportTask>& tasks);

    [[nodiscard]] QStandardItem* deepCopyItem(QStandardItem* item);
    [[nodiscard]] QStandardItem* getTargetFolder();
    [[nodiscard]] QStandardItem* reconstructPathInSource(const QString &fullPath);
    [[nodiscard]] bool validatePage() override;
    [[nodiscard]] int countFiles(QStandardItem* item);


    QTreeView *sourceView_m;
    QTreeView *targetView_m;

    QStandardItemModel *sourceModel_m;
    QStandardItemModel *targetModel_m;

    QPushButton *btnMap_m;
    QPushButton *btnUnmap_m;
    QPushButton *btnNewGroup_m;
    QPushButton *btnReset_m;
    QLineEdit *searchLineEdit_m;
    QCheckBox *collabsTree_m;

    bool isConnectionsEstablished_m;

private slots:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void expandAllTree();
    void mapSelectedItems();
    void addNewGroup();
    void unmapItem();
    void resetMapping();
    void applyFilter(const QString &filterText);
    [[nodiscard]] bool filterItemRecursive(QStandardItem *item, const QString &filterText);
};

#endif
