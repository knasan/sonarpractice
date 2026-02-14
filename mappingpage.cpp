#include "databasemanager.h"
#include "importprocessor.h"
#include "mappingpage.h"
#include "setupwizard.h"

#include <QEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QSqlDatabase>
#include <QStandardPaths>
#include <QTextBrowser>
#include <QTimer>
#include <QVBoxLayout>

/**
 * @brief Constructor of the MappingPage.
 * Creates the UI structure, initializes trees, and connects signals.
 */
MappingPage::MappingPage(QWidget *parent) : BasePage(parent) {
    setTitle(tr("Organizing and structuring data"));

    // Source
    sourceModel_m = new QStandardItemModel(this);

    sourceView_m = new QTreeView(this);

    sourceView_m->setModel(sourceModel_m);

    // Target
    targetModel_m = new QStandardItemModel(this);

    targetView_m = new QTreeView(this);
    targetView_m->setModel(targetModel_m);
    targetView_m->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);

    // Selection Mode
    sourceView_m->setSelectionMode(QAbstractItemView::ExtendedSelection);
    targetView_m->setSelectionMode(QAbstractItemView::ExtendedSelection);

    auto *layout = new QVBoxLayout(this);
    addHeaderLogo(layout, tr("Mapping"));

    auto *infoLabel = new QLabel(this);

    infoLabel->setText(tr("<b>Instructions:</b><br>"
                          "1. Create folders for your structure on the right-hand side.<br>"
                          "2. Select files on the left and drag them into a folder using <b>&gt;</b>."));
    infoLabel->setWordWrap(true);
    styleInfoLabel(infoLabel);
    layout->addWidget(infoLabel);

    searchLineEdit_m = new QLineEdit(this);
    searchLineEdit_m->setPlaceholderText(tr("Search..."));
    searchLineEdit_m->setClearButtonEnabled(true);
    searchLineEdit_m->installEventFilter(this);

    collabsTree_m = new QCheckBox(tr("Open structure"), this);

    // --- LAYOUT ---

    // A: Search + Button (H-Box)
    auto *searchRowLayout = new QHBoxLayout();
    searchRowLayout->addWidget(searchLineEdit_m, 1);
    searchRowLayout->addWidget(collabsTree_m, 0);

    // B: Left Column (Search via Tree)
    auto *leftColumnLayout = new QVBoxLayout();
    leftColumnLayout->addLayout(searchRowLayout);
    leftColumnLayout->addWidget(sourceView_m);

    // C: Medium Buttons (V-Box)
    auto *midBtnLayout = new QVBoxLayout();
    btnNewGroup_m = new QPushButton(tr("New group"), this);
    btnMap_m = new QPushButton(">", this);
    btnUnmap_m = new QPushButton("<", this);
    btnReset_m = new QPushButton(tr("Reset"), this);

    midBtnLayout->addStretch();
    midBtnLayout->addWidget(btnNewGroup_m);
    midBtnLayout->addSpacing(10);
    midBtnLayout->addWidget(btnMap_m);
    midBtnLayout->addWidget(btnUnmap_m);
    midBtnLayout->addSpacing(30);
    midBtnLayout->addWidget(btnReset_m);
    midBtnLayout->addStretch();

    // D: The large tree layout (Horizontal: Left | Center | Right)
    auto *hTreeLayout = new QHBoxLayout();
    hTreeLayout->addLayout(leftColumnLayout, 2); // Left side gets a factor of 2
    hTreeLayout->addLayout(midBtnLayout, 0);     // Buttons in the middle are fixed.
    hTreeLayout->addWidget(targetView_m, 2);     // The right side receives a factor of 2.

    // Everything into the main layout of the page
    layout->addLayout(hTreeLayout);
}

// Search
void MappingPage::applyFilter(const QString &filterText) {
    // UI prepare
    sourceView_m->setUpdatesEnabled(false);

    QString lowerFilter = filterText.toLower();

    // Actual work (recursion)
    bool ok = filterItemRecursive(sourceModel_m->invisibleRootItem(), lowerFilter);

    // UI Follow-up
    if (ok) {
        if (!lowerFilter.isEmpty()) {
            sourceView_m->expandAll();
        } else {
            sourceView_m->collapseAll();
        }
    }

    sourceView_m->setUpdatesEnabled(true);
}

bool MappingPage::filterItemRecursive(QStandardItem *item, const QString &filterText) {
    bool anyChildVisible = false;

    for (int i = 0; i < item->rowCount(); ++i) {
        QStandardItem *child = item->child(i);

        if (child->hasChildren()) {
            // If it's a folder, check children
            bool folderHasMatch = filterItemRecursive(child, filterText);

            // Show folder if a child matches or the folder name itself matches
            bool folderMatches = child->text().toLower().contains(filterText);
            bool visible = folderHasMatch || folderMatches;

            sourceView_m->setRowHidden(i, item->index(), !visible);
            if (visible) anyChildVisible = true;
        } else {
            // If it's a file
            bool matches = child->text().toLower().contains(filterText);
            sourceView_m->setRowHidden(i, item->index(), !matches);
            if (matches) anyChildVisible = true;
        }
    }
    return anyChildVisible;
}
// Search end

/**
 * @brief Prepare the page before it is displayed.
 * Loads the found files from the wizard's memory into the left tree.
 */
void MappingPage::initializePage() {
    sourceModel_m->clear();
    targetModel_m->clear();

    // Set header
    sourceModel_m->setHorizontalHeaderLabels({tr("Source (verified)")});
    targetModel_m->setHorizontalHeaderLabels({tr("Target structure")});

    // Initialize the target tree with a root folder
    QStandardItem *root = new QStandardItem(style()->standardIcon(QStyle::SP_DirHomeIcon), "SonarPractice");
    targetModel_m->appendRow(root);

    // Copy only the "Checked" items from the Wizard model to our local sourceModel_m.
    fillMappingSourceFromModel(wiz()->filesModel()->invisibleRootItem(), sourceModel_m->invisibleRootItem());

    sourceView_m->expandAll();
    targetView_m->expandAll();

    if (!isConnectionsEstablished_m) {
        isConnectionsEstablished_m = true;
        sideConnection();
    }

}

void MappingPage::sideConnection() {
    auto *searchTimer = new QTimer(this);
    searchTimer->setSingleShot(true);
    searchTimer->setInterval(400);

    connect(collabsTree_m, &QPushButton::clicked, this, &MappingPage::expandAllTree);

    connect(searchLineEdit_m, &QLineEdit::textChanged, this, [this, searchTimer](const QString &text) {
        searchTimer->start();
    });

    connect(searchTimer, &QTimer::timeout, this, [this]() {
        applyFilter(searchLineEdit_m->text());

        // Automatically expand when searching.
        if (!searchLineEdit_m->text().isEmpty()) {
            sourceView_m->expandAll();
        }
    });

    connect(btnNewGroup_m, &QPushButton::clicked, this, &MappingPage::addNewGroup);
    connect(btnMap_m, &QPushButton::clicked, this, &MappingPage::mapSelectedItems);
    connect(btnReset_m, &QPushButton::clicked, this, &MappingPage::resetMapping);
    connect(btnUnmap_m, &QPushButton::clicked, this, &MappingPage::unmapItem);
}

void MappingPage::expandAllTree() {
    sourceView_m->expandAll();
    targetView_m->expandAll();
}

// block return by search line (don't delete this)
bool MappingPage::eventFilter(QObject *obj, QEvent *event) {
    // qDebug() << "[ReviewPage] eventFilter START";
    if (obj == searchLineEdit_m && event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);

        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            // Run search immediately
            wiz()->proxyModel_m->setFilterFixedString(searchLineEdit_m->text());

            // Keep the focus in the field so the user can continue typing.
            searchLineEdit_m->selectAll();

            return true; // IMPORTANT: If Qt says "Event complete", it does not pass to the Wizard!
        }
    }
    // All other events will proceed as normal.
    return QWizardPage::eventFilter(obj, event);
}

void MappingPage::fillMappingSourceFromModel(QStandardItem* sourceParent, QStandardItem* targetParent) {
    for (int i = 0; i < sourceParent->rowCount(); ++i) {
        QStandardItem* sourceItem = sourceParent->child(i, 0);
        if (!sourceItem) continue;

        int status = sourceItem->data(RoleFileStatus).toInt();
        bool isChecked = (sourceItem->checkState() == Qt::Checked || status == StatusManaged);

        if (sourceItem->hasChildren()) {
            QStandardItem* newFolder = new QStandardItem(sourceItem->icon(), sourceItem->text());

            // --- Give the folder its path too! ---
            // So that we know where this folder is located in the file system.
            newFolder->setData(sourceItem->data(RoleFilePath), RoleFilePath);
            newFolder->setData(true, RoleIsFolder);

            fillMappingSourceFromModel(sourceItem, newFolder);

            if (newFolder->rowCount() > 0) {
                targetParent->appendRow(newFolder);
            } else {
                delete newFolder;
            }
        }
        else if (isChecked) {
            QStandardItem* fileItem = new QStandardItem(sourceItem->icon(), sourceItem->text());
            fileItem->setData(sourceItem->data(RoleFilePath), RoleFilePath);
            fileItem->setData(sourceItem->data(RoleFileHash), RoleFileHash);
            fileItem->setData(false, RoleIsFolder); // Es ist eine Datei
            targetParent->appendRow(fileItem);
        }
    }
}

/*
 * The "move" logic (mapSelectedItems)
 * To move the files from left to right, you need to check the user's current location in the target tree.
 * If no folder is selected, move it to "SonarPractice" (Root).
*/
QStandardItem* MappingPage::getTargetFolder() {
    QItemSelectionModel *select = sourceView_m->selectionModel();
    QModelIndexList selected = select->selectedRows();

    if (selected.isEmpty()) {
        qDebug() << "[MappingPage] getTargetFolder: selected is empty";
    }

    QStandardItem *targetFolder = nullptr;
    QModelIndex targetIndex = targetView_m->currentIndex();

    if (!targetIndex.isValid()) {
        targetFolder = targetModel_m->item(ColName);
    } else {
         targetFolder = targetModel_m->itemFromIndex(targetIndex);
    }
    return targetFolder;
}

void MappingPage::mapSelectedItems() {
    QItemSelectionModel *select = sourceView_m->selectionModel();
    QModelIndexList selected = select->selectedRows();

    if (selected.isEmpty()) return;

    QStandardItem *targetFolder = getTargetFolder();

    QGuiApplication::setOverrideCursor(Qt::WaitCursor);

    // IMPORTANT: Sort the indices in descending order so that deleting items in the source model does not shift the indices of the items still waiting.
    std::sort(selected.begin(), selected.end(), [](const QModelIndex &a, const QModelIndex &b) {
        return a.row() > b.row();
    });

    for (const QModelIndex &index : std::as_const(selected)) {
        QStandardItem *sourceItem = sourceModel_m->itemFromIndex(index);
        if (!sourceItem) continue;

        // Create a deep copy (whether file or folder with 1000 children)
        QStandardItem *newItem = deepCopyItem(sourceItem);
        targetFolder->appendRow(newItem);

        // Remove from the source tree
        sourceModel_m->removeRow(index.row(), index.parent());
    }

    cleanupEmptyFolders(sourceModel_m->invisibleRootItem());

    QGuiApplication::restoreOverrideCursor();

    sourceView_m->viewport()->update();
}

void MappingPage::addNewGroup() {
    QModelIndex index = targetView_m->currentIndex();
    QStandardItem *parentItem = targetModel_m->itemFromIndex(index);

    if (!parentItem) parentItem = targetModel_m->item(0);

    QStandardItem *newDir = new QStandardItem(style()->standardIcon(QStyle::SP_DirIcon), tr("New group"));
    newDir->setData(true, RoleIsFolder);
    parentItem->appendRow(newDir);

    targetView_m->edit(newDir->index());
}

void MappingPage::unmapItem() {
    QItemSelectionModel *select = targetView_m->selectionModel();
    QModelIndexList selected = select->selectedRows();
    if (selected.isEmpty()) return;

    sourceView_m->setUpdatesEnabled(false);
    sourceModel_m->blockSignals(true);

    // Work from bottom to top (Important when deleting!)
    std::sort(selected.begin(), selected.end(), [](const QModelIndex &a, const QModelIndex &b) {
        return a.row() > b.row();
    });

    for (const QModelIndex &index : std::as_const(selected)) {
        QStandardItem *item = targetModel_m->itemFromIndex(index);
        if (!item || item == targetModel_m->item(0)) continue;

        QString fullPath = item->data(RoleFilePath).toString();
        QStandardItem *targetParentForReturn = nullptr;

        if (item->data(RoleIsFolder).toBool()) {
            // CASE A: An entire FOLDER is pushed back
            // We search for the parent folder of the folder in the source tree
            QFileInfo dirInfo(fullPath);
            targetParentForReturn = reconstructPathInSource(dirInfo.path());
        } else {
            // CASE B: A single FILE is pushed back
            targetParentForReturn = reconstructPathInSource(fullPath);
        }

        // Deep Copy (copies files or entire folder trees)
        QStandardItem *returnItem = deepCopyItem(item);
        targetParentForReturn->appendRow(returnItem);

        // Remove from the right side of the target tree
        targetModel_m->removeRow(index.row(), index.parent());
    }

    sourceModel_m->blockSignals(false);
    sourceModel_m->layoutChanged();
    sourceView_m->setUpdatesEnabled(true);
}

QStandardItem* MappingPage::reconstructPathInSource(const QString &fullPath) {
    if (fullPath.isEmpty()) {
        return sourceModel_m->invisibleRootItem();
    }

    QFileInfo fileInfo(fullPath);

    // If it's a file, we'll use the folder path. If it's already a path, we'll use that.
    QString relativePath = fileInfo.isDir() ? fullPath : fileInfo.path();
    
    // Normalization for Windows/Linux
    relativePath = QDir::fromNativeSeparators(relativePath);

    QStandardItem *currentParent = sourceModel_m->invisibleRootItem();
    QStringList parts = relativePath.split('/', Qt::SkipEmptyParts);

    for (const QString &part : std::as_const(parts)) {
        bool found = false;
        // Check if the folder already exists at this level.
        for (int i = 0; i < currentParent->rowCount(); ++i) {
            QStandardItem *child = currentParent->child(i);
            if (child->text() == part && child->data(RoleIsFolder).toBool()) {
                currentParent = child;
                found = true;
                break;
            }
        }

        // If not found, recreate folder
        if (!found) {
            QStandardItem *newDir = new QStandardItem(style()->standardIcon(QStyle::SP_DirIcon), part);
            newDir->setData(true, RoleIsFolder);
            currentParent->appendRow(newDir);
            currentParent = newDir;
        }
    }
    return currentParent;
}

void MappingPage::resetMapping() {
    auto res = QMessageBox::question(this, tr("Reset mapping"),
                                     tr("Do you want to delete the entire structure and move all the files back to the left side?"),
                                     QMessageBox::Yes | QMessageBox::No);

    if (res == QMessageBox::Yes) {
        initializePage();
    }
}

QStandardItem* MappingPage::deepCopyItem(QStandardItem* item) {
    QStandardItem* copy = item->clone(); // Creates a copy of the item with text, icon, and all data (roles).

    // Clone all custom roles (clone() usually only copies standard roles)
    copy->setData(item->data(RoleFilePath), RoleFilePath);
    copy->setData(item->data(RoleFileHash), RoleFileHash);
    copy->setData(item->data(RoleIsFolder), RoleIsFolder);
    copy->setData(item->data(RoleFileStatus), RoleFileStatus);

    // Copy all children recursively
    for (int i = 0; i < item->rowCount(); ++i) {
        copy->appendRow(deepCopyItem(item->child(i)));
    }
    return copy;
}

void MappingPage::cleanupEmptyFolders(QStandardItem *parent) {
    if (!parent) return;

    // Go through the list backwards so that deleting elements does not affect the indices of the elements still to be checked.
    for (int i = parent->rowCount() - 1; i >= 0; --i) {
        QStandardItem *child = parent->child(i);

        if (child->hasChildren()) {
            cleanupEmptyFolders(child);
        }

        // After cleaning up the subfolders, check:
        // Is the folder now empty AND is it a folder?
        // (Files never have children, but we want to keep them)
        if (child->rowCount() == 0 && child->data(RoleIsFolder).toBool()) {
            parent->removeRow(i);
        }
    }
}

// -------------
bool MappingPage::validatePage() {
    int remaining = countFiles(sourceModel_m->invisibleRootItem());

    if (remaining > 0) {
        auto res = QMessageBox::warning(this, tr("files left over"),
                                        tr("There are %1 files remaining in the list. These will not be imported..\n\n"
                                           "Do you want to continue?").arg(remaining),
                                        QMessageBox::Yes | QMessageBox::No);

        auto ok = (res == QMessageBox::Yes);
        if (!ok) {
            return ok;
        }
    }

    this->setEnabled(false);

    QString appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(appDataDir);

    #ifdef QT_DEBUG
        QString finalDbPath = appDataDir + "/sonar_practice_debug.db";
    #else
        QString finalDbPath = appDataDir + "/sonar_practice.db";
    #endif

    QString tempDbPath = finalDbPath + ".tmp";

    // Path for the music files (chosen by the user)
    QString musicBasePath = wiz()->field("targetPath").toString();
    bool isManaged = wiz()->field("manageData").toBool();

    // Collect tasks
    QList<ImportTask> tasks;
    collectTasksFromModel(targetModel_m->item(ColName), "", tasks);

    if (tasks.isEmpty()) {
        return QMessageBox::question(this, tr("Empty import"),
                                     tr("No files were selected for import. Continue?")) == QMessageBox::Yes;
    }

    // Connection to temporary database & table creation
    // initDatabase automatically calls createInitialTables() if version < 1
    if (!DatabaseManager::instance().initDatabase(tempDbPath)) {
        QMessageBox::critical(this, "Error", "Database initialization failed.");
        return false;
    }

    // Preparing for dialogue
    QProgressDialog progress(tr("Import files..."), tr("Cancel"), 0, tasks.size(), this);
    progress.setWindowModality(Qt::WindowModal);
    progress.show();

    bool success = false;
    {
        // Open connection
        if (!DatabaseManager::instance().initDatabase(tempDbPath)) return false;

        // Create processor locally
        ImportProcessor processor;
        connect(&processor, &ImportProcessor::progressUpdated, &progress, &QProgressDialog::setValue);

        // Carry out
        success = processor.executeImport(tasks, musicBasePath, isManaged);
    }

    if (success) {
        DatabaseManager::instance().closeDatabase();

        if (QFile::exists(finalDbPath)) QFile::remove(finalDbPath);

        if (QFile::rename(tempDbPath, finalDbPath)) {
            qApp->exit(1337);
            return true;
        }
    } else {
        DatabaseManager::instance().closeDatabase();
        if (QFile::exists(tempDbPath)) QFile::remove(tempDbPath);
    }

    // In case of an error, also close
    DatabaseManager::instance().closeDatabase();
    return false;
}


void MappingPage::collectTasksFromModel(QStandardItem* parent, QString currentCategoryPath, QList<ImportTask>& tasks) {
    if (!parent) {
        qWarning() << "[MappingPage] collectTasksFromModel: parent null pointer";
        return;
    }

    for (int i = 0; i < parent->rowCount(); ++i) {
        QStandardItem* child = parent->child(i);

        if (child->data(RoleIsFolder).toBool()) {
            // Folder: Expand path and dig deeper
            QString nextPath = currentCategoryPath.isEmpty() ? child->text() : currentCategoryPath + "/" + child->text();
            collectTasksFromModel(child, nextPath, tasks);
        } else {

            QString sPath = child->data(RoleFilePath).toString();
            qint64 sSize = QFileInfo(sPath).size(); // Determine size

            // VALIDATION:
            // 1. Is the path empty? (Folder remnants)
            // 2.Is the file 0 bytes in size? (Corrupted/Empty)
            if (sPath.trimmed().isEmpty() || sSize == 0) {
                qDebug() << "[MappingPage] Skip invalid item (path empty or 0 bytes):" << child->text();
                continue;
            }

            // File found: Create task
            ImportTask t;
            t.sourcePath = child->data(RoleFilePath).toString();
            t.itemName = child->text();
            t.fileHash = child->data(RoleFileHash).toString();
            t.fileSize = QFileInfo(t.sourcePath).size();
            t.fileSuffix = QFileInfo(t.sourcePath).suffix();
            t.categoryPath = currentCategoryPath; // Hier steckt die Ordner-Struktur drin

            t.relativePath = currentCategoryPath + "/" + t.itemName;

            /* qDebug() << "[MappingPage] collectTasksFromModel t.sourcePath: " << t.sourcePath;
            qDebug() << "[MappingPage] collectTasksFromModel t.itemName: " << t.itemName;
            qDebug() << "[MappingPage] collectTasksFromModel t.fileHash: " << t.fileHash;
            qDebug() << "[MappingPage] collectTasksFromModel t.fileSize: " << t.fileSize;
            qDebug() << "[MappingPage] collectTasksFromModel t.fileSuffix: " << t.fileSuffix;
            qDebug() << "[MappingPage] collectTasksFromModel t.categoryPath: " << t.categoryPath;
            qDebug() << "[MappingPage] collectTasksFromModel t.relativePath: " << t.relativePath;
            qDebug() << "[MappingPage] collectTasksFromModel currentCategoryPath: " << currentCategoryPath;
            qDebug() << "------------------------------------------------------------------------------------"; */

            tasks.append(t);
        }
    }
}

int MappingPage::countFiles(QStandardItem* item) {
    int count = 0;
    for (int i = 0; i < item->rowCount(); ++i) {
        QStandardItem* child = item->child(i);

        // If it's a folder, go deeper.
        if (child->hasChildren()) {
            count += countFiles(child);
        }
        // Only count if it is a file (RoleIsFolder is false or not set)
        else if (!child->data(RoleIsFolder).toBool()) {
            count++;
        }
    }
    return count;
}
