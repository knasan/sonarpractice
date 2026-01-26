#include "librarypage.h"
#include "databasemanager.h"
#include "uihelper.h"
#include "fileselectiondialog.h"
#include "uihelper.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPushButton>
#include <QSqlTableModel>
#include <QScrollArea>
#include <QSplitter>
#include <QSqlQuery>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QListWidget>
#include <QComboBox>
#include <QCheckBox>
#include <QMessageBox>
#include <QListWidgetItem>
#include <QMenu>
#include <QTimer>
#include <QCoreApplication>
#include <QProgressDialog>

LibraryPage::LibraryPage(QWidget *parent, DatabaseManager *dbManager)
    : QWidget(parent), dbManager_m(dbManager)
{
    // UI initialisieren
    setupUI();

    // Place the master model on media_files
    masterModel_m = new QSqlTableModel(this, dbManager_m->database());
    masterModel_m->setTable("media_files");
    masterModel_m->setEditStrategy(QSqlTableModel::OnManualSubmit);
    masterModel_m->select();

    masterModel_m->setHeaderData(masterModel_m->fieldIndex("file_path"), Qt::Horizontal, tr("Path"));

    // Connections (Signals and Slots)

    // Search on Master Model
    connect(searchEdit_m, &QLineEdit::textChanged, this, [this](const QString &text) {
        for (int i = 0; i < catalogModel_m->rowCount(); ++i) {
            bool match = catalogModel_m->item(i)->text().contains(text, Qt::CaseInsensitive);
            catalogTreeView_m->setRowHidden(i, QModelIndex(), !match);
        }
    });

    QTimer::singleShot(100, this, &LibraryPage::setupCatalog);
}

void LibraryPage::setupUI() {
    auto *mainLayout = new QVBoxLayout(this);
    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);

    // --- LEFT SIDE ---
    auto *masterContainer = new QWidget();
    auto *masterLayout = new QVBoxLayout(masterContainer);

    searchEdit_m = new QLineEdit(this);
    searchEdit_m->setPlaceholderText(tr("Search for songs, videos, or PDFs..."));
    catalogTreeView_m = new QTreeView(this); // Main media list
    catalogTreeView_m->setMouseTracking(true); // Reacts faster
    catalogTreeView_m->setToolTipDuration(5000);
    catalogTreeView_m->setSelectionMode(QAbstractItemView::SingleSelection);
    catalogTreeView_m->setEditTriggers(QAbstractItemView::NoEditTriggers);

    catalogTreeView_m->setContextMenuPolicy(Qt::CustomContextMenu);

    masterLayout->addWidget(searchEdit_m);
    masterLayout->addWidget(catalogTreeView_m);

    // --- RIGHT SIDE ---
    detailWidget_m = new QWidget();
    detailLayout_m = new QVBoxLayout(detailWidget_m);

    // Titel
    detailTitleLabel_m = new QLabel(tr("Choose content"), this);
    detailTitleLabel_m->setStyleSheet("font-size: 18px; font-weight: bold; color: #ecf0f1;");
    detailLayout_m->addWidget(detailTitleLabel_m);

    // Linked files (The GroupBox with the list)
    mediaGroup_m = new QGroupBox(tr("Linked files"), this);
    auto *vboxMedia = new QVBoxLayout(mediaGroup_m);

    relatedFilesListWidget_m = new QListWidget(this);
    relatedFilesListWidget_m->setMinimumHeight(200);
    relatedFilesListWidget_m->setSelectionMode(QAbstractItemView::ExtendedSelection);
    vboxMedia->addWidget(relatedFilesListWidget_m);

    // Buttons directly below the list in the same GroupBox
    auto *btnLayout = new QHBoxLayout();

    addBtn_m = new QPushButton("+", this);
    addBtn_m->setToolTip(tr("Select files and link them to the currently selected medium."));
    addBtn_m->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Plus));

    removeBtn_m = new QPushButton("-", this);
    removeBtn_m->setToolTip(tr("Remove selected links from the list."));
    removeBtn_m->setShortcut(QKeySequence(Qt::Key_Delete));

    addBtn_m->setEnabled(false);
    removeBtn_m->setEnabled(false);

    QString buttonStyle =
        "QPushButton:disabled { background-color: #333333; color: #666666; border: 1px solid #444444; }"
        "QPushButton:enabled { background-color: #2c3e50; color: white; border: 1px solid #34495e; }"
                          "QPushButton:hover { background-color: #34495e; }";

    addBtn_m->setStyleSheet(buttonStyle);
    removeBtn_m->setStyleSheet(buttonStyle);

    btnLayout->addWidget(addBtn_m);
    btnLayout->addWidget(removeBtn_m);
    btnLayout->addStretch();
    vboxMedia->addLayout(btnLayout);

    detailLayout_m->addWidget(mediaGroup_m);

    // Online resources
    // TODO: No way to edit these has been created yet.
    linksGroup_m = new QGroupBox(tr("Online resources / course portals"), this);
    linksListLayout_m = new QVBoxLayout(linksGroup_m);
    detailLayout_m->addWidget(linksGroup_m);

    detailLayout_m->addStretch();

    // Splitter & Main Layout
    splitter->addWidget(masterContainer);
    splitter->addWidget(detailWidget_m);
    mainLayout->addWidget(splitter);

    // Connections for the buttons
    connect(addBtn_m, &QPushButton::clicked, this, &LibraryPage::onAddRelationClicked);
    connect(removeBtn_m, &QPushButton::clicked, this, &LibraryPage::onRemoveRelationClicked);

    connect(relatedFilesListWidget_m, &QListWidget::itemSelectionChanged, this, [this]() {
        // The delete button only becomes active if at least one item is selected on the right.
        bool hasRightSelection = !relatedFilesListWidget_m->selectedItems().isEmpty();
        removeBtn_m->setEnabled(hasRightSelection);
    });

    connect(catalogTreeView_m, &QTreeView::customContextMenuRequested,
            this, &LibraryPage::showCatalogContextMenu);
}

void LibraryPage::setupCatalog() {
    catalogModel_m = new QStandardItemModel(this);
    catalogModel_m->setHorizontalHeaderLabels({tr("Media catalog")});

    catalogTreeView_m->setModel(catalogModel_m);

    connect(catalogTreeView_m->selectionModel(), &QItemSelectionModel::currentRowChanged,
            this, &LibraryPage::onItemSelected);

    loadCatalogFromDatabase();
}

void LibraryPage::onItemSelected(const QModelIndex &current) {
    bool hasMasterSelection = current.isValid();

    // Activate the panel on the right
    detailWidget_m->setEnabled(hasMasterSelection);

    if (!hasMasterSelection) {
        detailTitleLabel_m->setText(tr("Choose content"));
        return;
    }

    detailTitleLabel_m->setText(tr("Selected: %1").arg(current.data(Qt::DisplayRole).toString()));

    // + Button is now ready, since we have a goal.
    addBtn_m->setEnabled(true);

    // - The button remains deactivated until something is selected on the right.
    removeBtn_m->setEnabled(false);

    refreshRelatedFilesList();
}

// Fill catalog
void LibraryPage::loadCatalogFromDatabase() {
    // Preparation: Pause UI updates for speed
    catalogTreeView_m->setUpdatesEnabled(false);
    catalogModel_m->clear();
    catalogModel_m->setHorizontalHeaderLabels({tr("Media catalog")});

    // Execute query
    QSqlQuery query("SELECT id, file_path FROM media_files ORDER BY file_path ASC", dbManager_m->database());

    // Initialize the progress dialog
    int totalFiles = 0;
    if (query.last()) { // Jump to the end briefly to count the number
        totalFiles = query.at() + 1;
        query.first(); // Back to the beginning
        query.previous(); // Preparation for the while(query.next()) loop
    }

    QProgressDialog progress(tr("Media catalog is loading..."), tr("Cancel"), 0, totalFiles, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(500); // Only appears if it lasts longer than 0.5 seconds.
    progress.setValue(0);

    int count = 0;
    while (query.next()) {
        count++;
        if (count % 100 == 0) { // Update only every 100 items for better performance
            progress.setValue(count);
            QCoreApplication::processEvents(); // Important: To prevent the UI from freezing
            if (progress.wasCanceled()) break;
        }

        int id = query.value("id").toInt();
        QString path = query.value("file_path").toString();

        QStandardItem* item = new QStandardItem(QFileInfo(path).fileName());
        item->setData(id, LibraryPage::FileIdRole);
        item->setData(path, Qt::ToolTipRole);
        item->setData(path, LibraryPage::FilePathRole);
        item->setIcon(style()->standardIcon(QStyle::SP_FileIcon));

        catalogModel_m->appendRow(item);
    }

    // Diploma
    progress.setValue(totalFiles);
    catalogTreeView_m->setUpdatesEnabled(true);

    qDebug() << "[LibraryPage] Catalog loaded. Entries:" << catalogModel_m->rowCount();
}

void LibraryPage::onAddRelationClicked() {
    // Retrieve the ID of the "master file" marked on the left in the catalog.
    int currentId = getCurrentSelectedFileId();
    if (currentId <= 0) {
        QMessageBox::warning(this, tr("No choice"),
                             tr("Please first select a file from the catalog to be linked."));
        return;
    }

    // Create the dialog (it excludes existing links)
    FileSelectionDialog dlg(currentId, this);
    dlg.setWindowModality(Qt::WindowModal);

    //When the user clicks "OK"
    if (dlg.exec() == QDialog::Accepted) {
        // IDs retrieve the files selected in the dialog
        QList<int> selectedIds = dlg.getSelectedFileIds();

        // Link each selection individually in the database.
        for (int targetId : std::as_const(selectedIds)) {
            if(!dbManager_m->addFileRelation(currentId, targetId)) {
                qCritical() << "[LibraryPage] Link failed for ID:" << targetId;
            };
        }

        // Update the view on the right immediately
        refreshRelatedFilesList();
    }
}

void LibraryPage::onRemoveRelationClicked() {
    // Get the list of selected items FROM THE WIDGET
    QList<QListWidgetItem*> selected = relatedFilesListWidget_m->selectedItems();

    qDebug() << "[LibraryPage] onRemoveRelationClicked: " << selected;

    if (selected.isEmpty()) return;

    int currentId = getCurrentSelectedFileId();
    if (currentId <= 0) return;

    auto res = QMessageBox::question(this, tr("Disconnect"),
                                     tr("Do you really want to remove the link to the highlighted %1 files?").arg(selected.size()));

    if (res == QMessageBox::Yes) {
        // Iterate DIRECTLY over the local list 'selected'
        for (auto* item : std::as_const(selected)) {
            // Access the item that is stored in the list.
            int targetId = item->data(Qt::UserRole).toInt();
            auto ok = dbManager_m->removeRelation(currentId, targetId);
            if (!ok) {
                qCritical() << "dbManager removeRelation failed";
            }
        }

        //  UI update
        refreshRelatedFilesList();
    }
}

int LibraryPage::getCurrentSelectedFileId() {
    QModelIndex index = catalogTreeView_m->currentIndex();

    if (!index.isValid()) {
        qDebug() << "[LibraryPage] No valid index found";
        return -1;
    }

    qDebug() << "--- Index Debug Start ---";
    qDebug() << "Zeile:" << index.row() << "Spalte:" << index.column();
    qDebug() << "DisplayRole (Name):" << index.data(Qt::DisplayRole).toString();

    // Examinations of various roles
    qDebug() << "UserRole (Standard):" << index.data(Qt::UserRole).toInt();
    qDebug() << "UserRole + 1 (PATH):" << index.data(Qt::UserRole + 1).toString();

    // check specific ID role
    QVariant idData = index.data(LibraryPage::FileIdRole);
    qDebug() << "LibraryPage::FileIdRole value:" << idData.toInt()
             << " (Typ:" << idData.typeName() << ")";

    qDebug() << "FileIdRole constant is:" << static_cast<int>(LibraryPage::FileIdRole);
    qDebug() << "--- Index Debug Ende ---";

    int id = index.data(LibraryPage::FileIdRole).toInt();
    qDebug() << "[LibraryPage] Determined ID for selection:" << id;

    return id;
}

void LibraryPage::refreshRelatedFilesList() {
    // UI prepare
     relatedFilesListWidget_m->clear();

    // Get the current song ID
    int currentId = getCurrentSelectedFileId();
    if (currentId <= 0) return;

    // Retrieve linked files from the database
    QList<DatabaseManager::RelatedFile> related = dbManager_m->getFilesByRelation(currentId);

    // Fill in the list
    for (const auto &file : std::as_const(related)) {
        QString fileName = QFileInfo(file.fileName).fileName();

        auto *item = new QListWidgetItem(fileName, relatedFilesListWidget_m);

        // IMPORTANT: Save the ID of the linked file,
        // so that it can be found for deletion (discarding).
        item->setData(Qt::UserRole, file.id);

        relatedFilesListWidget_m->addItem(item);
    }
}

void LibraryPage::showCatalogContextMenu(const QPoint &pos) {
    QModelIndex index = catalogTreeView_m->indexAt(pos);
    if (!index.isValid()) return;

    QString fullPath = index.data(LibraryPage::FilePathRole).toString();

    qDebug() << "[LibraryPage] Right-click path:" << fullPath;

    if (fullPath.isEmpty()) {
        qDebug() << "[LibraryPage] Error: Path role is empty!";
        return;
    }

    QMenu menu(this);
    QAction *openAction = menu.addAction(tr("Open file in default player"));

    QAction *selected = menu.exec(catalogTreeView_m->viewport()->mapToGlobal(pos));
    if (selected == openAction) {
        UIHelper::openFileWithFeedback(this, fullPath);
    }
}
