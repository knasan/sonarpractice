#ifndef LIBRARYPAGE_H
#define LIBRARYPAGE_H

#include "fileutils.h"
#include "databasemanager.h"

#include <QListWidget>
#include <QStandardItemModel>
#include <QTreeView>

class QListView;
class QSqlTableModel;
class QLabel;
class QVBoxLayout;
class QHBoxLayout;
class QGroupBox;
class QListWidgetItem;
class QStyledItemDelegate;
class QLineEdit;

class LibraryPage : public QWidget {
    Q_OBJECT
public:
    explicit LibraryPage(QWidget *parent = nullptr, DatabaseManager *db = nullptr);

private slots:
    void onItemSelected(const QModelIndex &index);
    void onAddRelationClicked();

private:

    enum CustomRoles {
        FileIdRole = Qt::UserRole + 1,    // 257
        FilePathRole,
        SongIdRole,
    };

    void setupCatalog();
    void setupUI();
    void onRemoveRelationClicked();
    void refreshRelatedFilesList();
    void loadCatalogFromDatabase();
    void showCatalogContextMenu(const QPoint &pos);

    [[nodiscard]] int getCurrentSongId();

    DatabaseManager *dbManager_m;

    QSqlTableModel* masterModel_m;

    QListWidget* relatedFilesListWidget_m;
    QTreeView* catalogTreeView_m;

    QStandardItemModel* catalogModel_m;

    QWidget* detailWidget_m;
    QLabel* detailTitleLabel_m;
    QVBoxLayout* detailLayout_m;

    QGroupBox* mediaGroup_m;
    QHBoxLayout* mediaLayout_m;

    QGroupBox* linksGroup_m;
    QVBoxLayout* linksListLayout_m;

    QLineEdit* searchEdit_m;

    QPushButton* addBtn_m;
    QPushButton* removeBtn_m;
};

#endif // LIBRARYPAGE_H
