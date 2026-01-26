#include "fileselectiondialog.h"
#include "fileutils.h"
#include "uihelper.h"

#include <QVBoxLayout>
#include <QSqlQuery>
#include <QVariant>
#include <QFileInfo>
#include <QLineEdit>
#include <QPushButton>
#include <QButtonGroup>
#include <QMenu>

FileSelectionDialog::FileSelectionDialog(int excludeId, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Select files to link"));
    setMinimumSize(450, 600);

    auto *mainLayout = new QVBoxLayout(this);

    // Search
    searchEdit_m = new QLineEdit(this);
    searchEdit_m->setPlaceholderText(tr("Search by name..."));
    mainLayout->addWidget(searchEdit_m);

    // Filter-Buttons (Horizontal)
    auto *filterLayout = new QHBoxLayout();
    QStringList categories = { tr("All"), tr("Audio"), tr("Video"), tr("Guitar Pro"), tr("PDF") };

    // QButtonGroup ensures that only one filter is active at a time (like RadioButtons)
    auto *filterGroup = new QButtonGroup(this);
    for (int i = 0; i < categories.size(); ++i) {
        auto *btn = new QPushButton(categories[i], this);
        btn->setCheckable(true);
        if (i == 0) btn->setChecked(true); //"All" is standard
        filterGroup->addButton(btn, i);
        filterLayout->addWidget(btn);
    }
    mainLayout->addLayout(filterLayout);

    // The list
    listWidget_m = new QListWidget(this);
    listWidget_m->setSelectionMode(QAbstractItemView::ExtendedSelection);
    mainLayout->addWidget(listWidget_m);
    listWidget_m->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(listWidget_m, &QListWidget::customContextMenuRequested,
            this, &FileSelectionDialog::showContextMenu);

    // Dialog buttons (OK/Cancel)
    auto *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(btns, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(btns);

    // --- LOAD DATA ---
    loadFiles(excludeId);

    // --- Linking Logic---
    connect(searchEdit_m, &QLineEdit::textChanged, this, &FileSelectionDialog::updateFilter);
    connect(filterGroup, &QButtonGroup::idClicked, this, &FileSelectionDialog::updateFilter);
}

QList<int> FileSelectionDialog::getSelectedFileIds() const {
    QList<int> ids;
    for (auto *item : listWidget_m->selectedItems()) {
        ids << item->data(Qt::UserRole).toInt();
    }
    return ids;
}

void FileSelectionDialog::loadFiles(int excludeId) {
    QSqlQuery query;
    query.prepare("SELECT id, file_path FROM media_files "
                  "WHERE id != :currentId " // <--- Verhindert, dass die Datei sich selbst findet
                  "AND id NOT IN ("
                  "  SELECT file_id_b FROM file_relations WHERE file_id_a = :currentId "
                  "  UNION "
                  "  SELECT file_id_a FROM file_relations WHERE file_id_b = :currentId "
                  ")");
    query.bindValue(":currentId", excludeId);

    if (query.exec()) {
        while (query.next()) {
            QString path = query.value("file_path").toString();
            QString fileName = QFileInfo(path).fileName();

            auto *item = new QListWidgetItem(fileName, listWidget_m);
            item->setData(Qt::UserRole, query.value("id").toInt());
            item->setData(Qt::ToolTipRole, path); // The path appears on hover.

            item->setData(Qt::UserRole + 1, getCategoryForFile(fileName));
            item->setData(Qt::UserRole + 2, path);

            listWidget_m->addItem(item);
        }
    }
}

void FileSelectionDialog::updateFilter() {
    QString searchText = searchEdit_m->text().toLower();

    QString activeCategory = "";
    for(auto *btn : findChildren<QPushButton*>()) {
        if(btn->isCheckable() && btn->isChecked()) {
            activeCategory = btn->text();
            break;
        }
    }

    for (int i = 0; i < listWidget_m->count(); ++i) {
        auto *item = listWidget_m->item(i);
        QString itemCategory = item->data(Qt::UserRole + 1).toString();

        bool nameMatches = item->text().toLower().contains(searchText);
        bool categoryMatches = (activeCategory == tr("All") || itemCategory == activeCategory);

        item->setHidden(!(nameMatches && categoryMatches));
    }
}

QString FileSelectionDialog::getCategoryForFile(const QString &fileName) {
    QString ext = "*." + QFileInfo(fileName).suffix().toLower();

    if (FileUtils::getAudioFormats().contains(ext))     return tr("Audio");
    if (FileUtils::getVideoFormats().contains(ext))     return tr("Video");
    if (FileUtils::getGuitarProFormats().contains(ext))  return tr("Guitar Pro");
    if (FileUtils::getPdfFormats().contains(ext))       return tr("PDF");

    return tr("Other");
}

void FileSelectionDialog::showContextMenu(const QPoint &pos) {
    QListWidgetItem *item = listWidget_m->itemAt(pos);
    if (!item) return;

    QString fullPath = item->data(Qt::UserRole + 2).toString();
    if (fullPath.isEmpty()) return;

    QMenu menu(this);
    QAction *openAction = menu.addAction(tr("Open file for review"));

    // ADD Icon
    openAction->setIcon(style()->standardIcon(QStyle::SP_DesktopIcon));

    QAction *selected = menu.exec(listWidget_m->viewport()->mapToGlobal(pos));
    if (selected == openAction) {
        UIHelper::openFileWithFeedback(this, fullPath);
    }
}
