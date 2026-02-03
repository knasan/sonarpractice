#include "databasemanager.h"
#include "importprocessor.h"
#include "importdialog.h"
#include "sonarstructs.h"

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
 * @brief Constructor of the ImportDialog.
 * Creates the UI structure, initializes trees, and connects signals.
 */
ImportDialog::ImportDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle(tr("Organizing and structuring data"));
    resize(1100, 800);

    // Source
    sourceModel_m = new QStandardItemModel(this);

    sourceView_m = new QTreeView(this);
    sourceView_m->setModel(sourceModel_m);

    // Target
    targetModel_m = new QStandardItemModel(this);

    targetView_m = new QTreeView(this);
    targetView_m->setModel(targetModel_m);
    targetView_m->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);

    targetModel_m->clear();
    targetModel_m->setHorizontalHeaderLabels({tr("Target structure")});

    QStandardItem *rootItem = new QStandardItem(style()->standardIcon(QStyle::SP_DirHomeIcon), "SonarPractice");
    rootItem->setData(true, RoleIsFolder);
    rootItem->setData("ROOT", RoleFilePath); // Marking that this is the base
    rootItem->setEditable(false);            // The name "SonarPractice" should not be changeable.

    targetModel_m->appendRow(rootItem);
    targetView_m->expand(rootItem->index());

    // Selection Mode
    sourceView_m->setSelectionMode(QAbstractItemView::ExtendedSelection);
    targetView_m->setSelectionMode(QAbstractItemView::ExtendedSelection);

    auto *layout = new QVBoxLayout(this);

    auto *infoLabel = new QLabel(this);

    infoLabel->setText(tr("<b>Instructions:</b><br>"
                          "1. Create folders for your structure on the right-hand side.<br>"
                          "2. Select files on the left and drag them into a folder using <b>&gt;</b>."));
    infoLabel->setWordWrap(true);
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
    btnNewDir_m = new QPushButton(tr("New directory"), this);
    btnMap_m = new QPushButton(">", this);
    btnUnmap_m = new QPushButton("<", this);

    midBtnLayout->addStretch();
    midBtnLayout->addWidget(btnNewDir_m);
    midBtnLayout->addSpacing(10);
    midBtnLayout->addWidget(btnMap_m);
    midBtnLayout->addWidget(btnUnmap_m);

    midBtnLayout->addStretch();

    // D: The large tree layout (Horizontal: Left | Center | Right)
    auto *hTreeLayout = new QHBoxLayout();
    hTreeLayout->addLayout(leftColumnLayout, 2); // Left side gets a factor of 2
    hTreeLayout->addLayout(midBtnLayout, 0);     // Buttons in the middle are fixed.
    hTreeLayout->addWidget(targetView_m, 2);     // The right side receives a factor of 2.

    // Everything into the main layout of the page
    layout->addLayout(hTreeLayout);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Import"));
    buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    layout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &ImportDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &ImportDialog::reject);

    if (!isConnectionsEstablished_m) {
        isConnectionsEstablished_m = true;
        sideConnection();
    }

    setupTargetRoot();
}

void ImportDialog::setupTargetRoot() {
    targetModel_m->clear();

    targetModel_m->setColumnCount(1);
    targetModel_m->setHorizontalHeaderLabels({tr("Target structure")});

    // Get the information from the database / settings.
    dataBasePath_m = DatabaseManager::instance().getManagedPath();
    isManaged_m = DatabaseManager::instance().getSetting("is_managed", QString("false")) == "true";

    QString rootDisplay;
    if (isManaged_m) {
        QDir rootDir(dataBasePath_m);
        QStandardItem *rootItem = new QStandardItem(style()->standardIcon(QStyle::SP_DirHomeIcon), rootDir.dirName());
        rootItem->setData(true, RoleIsFolder);
        rootItem->setData(dataBasePath_m, RoleFilePath);
        targetModel_m->appendRow(rootItem);

        // Recursively read existing subfolders
        buildExistingDirTree(dataBasePath_m, rootItem);

        targetModel_m->setHorizontalHeaderLabels({tr("Target structure")});
        targetView_m->expand(rootItem->index());

        // Display the actual folder name so the user knows: My files are physically copied here.
        rootDisplay = QDir(dataBasePath_m).absolutePath();
        if(rootDisplay.isEmpty()) rootDisplay = "Library";
    } else {
        // Unmanaged: The user retains their files; we only organize them virtually.
        rootDisplay = "SonarPractice (Virtual)";
    }
}

void ImportDialog::buildExistingDirTree(const QString &path, QStandardItem *parentItem) {
    QDir dir(path);
    QFileInfoList list = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

    for (const QFileInfo &fileInfo : std::as_const(list)) {
        QStandardItem *childItem = new QStandardItem(style()->standardIcon(QStyle::SP_DirIcon), fileInfo.fileName());
        childItem->setData(true, RoleIsFolder);
        childItem->setData(fileInfo.absoluteFilePath(), RoleFilePath);

        parentItem->appendRow(childItem);

        buildExistingDirTree(fileInfo.absoluteFilePath(), childItem);
    }
}

// Search
void ImportDialog::applyFilter(const QString &filterText) {
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

bool ImportDialog::filterItemRecursive(QStandardItem *item, const QString &filterText) {
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
void ImportDialog::sideConnection() {
    auto *searchTimer = new QTimer(this);
    searchTimer->setSingleShot(true);
    searchTimer->setInterval(400);

    connect(collabsTree_m, &QPushButton::clicked, this, &ImportDialog::expandAllTree);

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

    connect(btnNewDir_m, &QPushButton::clicked, this, &ImportDialog::addNewDir);
    connect(btnMap_m, &QPushButton::clicked, this, &ImportDialog::mapSelectedItems);
    connect(btnUnmap_m, &QPushButton::clicked, this, &ImportDialog::unmapItem);
}

void ImportDialog::expandAllTree() {
    sourceView_m->expandAll();
    targetView_m->expandAll();
}

// block return by search line (don't delete this)
bool ImportDialog::eventFilter(QObject *obj, QEvent *event) {
    if (obj == searchLineEdit_m && event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);

        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            // Keep the focus in the field so the user can continue typing.
            searchLineEdit_m->selectAll();
            return true; // IMPORTANT: If Qt says "Event complete", it does not pass to the Wizard!
        }
    }
    // All other events will proceed as normal.
    return QDialog::eventFilter(obj, event);
}

/*
 * The "move" logic (mapSelectedItems)
 * To move the files from left to right, you need to check the user's current location in the target tree.
 * If no folder is selected, move it to "SonarPractice" (Root).
*/
QStandardItem* ImportDialog::getTargetFolder() {
    QItemSelectionModel *select = sourceView_m->selectionModel();
    QModelIndexList selected = select->selectedRows();

    QStandardItem *targetFolder = nullptr;
    QModelIndex targetIndex = targetView_m->currentIndex();

    if (!targetIndex.isValid()) {
        targetFolder = targetModel_m->item(ColName);
    } else {
         targetFolder = targetModel_m->itemFromIndex(targetIndex);
    }
    return targetFolder;
}

void ImportDialog::mapSelectedItems() {
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

void ImportDialog::addNewDir() {
    QModelIndex index = targetView_m->currentIndex();
    QStandardItem *parentItem = nullptr;

    // 1. Check if a valid selection has been made.
    if (index.isValid()) {
        parentItem = targetModel_m->itemFromIndex(index);
    }

    // 2. Fallback: If nothing is selected or the index is invalid, use the root item.
    if (!parentItem) {
        parentItem = targetModel_m->invisibleRootItem();
    }

    // 3. Create new directory item
    QStandardItem *newDir = new QStandardItem(style()->standardIcon(QStyle::SP_DirIcon), tr("New directory"));

    // Important: Set metadata
    newDir->setData(true, RoleIsFolder);
    // Optional: An empty path or a special marker, since it does not yet physically exist.
    newDir->setData("", RoleFilePath);

    // 4. Add item
    parentItem->appendRow(newDir);

    // 5. UX: Immediately open the new folder and enter edit mode.
    targetView_m->expand(parentItem->index());
    targetView_m->edit(newDir->index());
}

void ImportDialog::unmapItem() {
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

QStandardItem* ImportDialog::reconstructPathInSource(const QString &fullPath) {
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

QStandardItem* ImportDialog::deepCopyItem(QStandardItem* item) {
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

void ImportDialog::cleanupEmptyFolders(QStandardItem *parent) {
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

void ImportDialog::collectTasksFromModel(QStandardItem* parent, QString currentDirPath, QList<ImportTask>& tasks) {
    if (!parent) {
        qWarning() << "[ImportDialog] collectTasksFromModel: parent null pointer";
        return;
    }

    for (int i = 0; i < parent->rowCount(); ++i) {
        QStandardItem* child = parent->child(i, 0);

        if (child->data(RoleIsFolder).toBool()) {
            // Folder found: Expand path and dig deeper
            QString nextPath = currentDirPath.isEmpty() ? child->text() : currentDirPath + "/" + child->text();
            collectTasksFromModel(child, nextPath, tasks);
        } else {

            QString sPath = child->data(RoleFilePath).toString();
            qint64 sSize = QFileInfo(sPath).size(); // Größe ermitteln

            // VALIDATION:
            // 1. Is the path empty? (Folder remnants)
            // 2. Is the file 0 bytes in size? (Corrupted/Empty)
            if (sPath.trimmed().isEmpty() || sSize == 0) {
                continue;
            }

            // File found: Create task
            ImportTask t;
            t.sourcePath = child->data(RoleFilePath).toString();
            t.itemName = child->text();
            t.fileHash = child->data(RoleFileHash).toString();
            t.fileSize = QFileInfo(t.sourcePath).size();
            t.fileSuffix = QFileInfo(t.sourcePath).suffix();
            t.categoryPath = currentDirPath; // The folder structure is contained within this.

            if (isManaged_m) {
                t.relativePath = QDir::cleanPath(dataBasePath_m + "/" + currentDirPath + "/" + t.itemName);
            } else {
                t.relativePath = QDir::cleanPath(currentDirPath + "/" + t.itemName);
            }

            // qDebug() << "[ImportDialog] collectTasksFromModel t.sourcePath: " << t.sourcePath;      // absolute sourcePath
            // qDebug() << "[ImportDialog] collectTasksFromModel t.itemName: " << t.itemName;          // FileName
            // qDebug() << "[ImportDialog] collectTasksFromModel t.fileHash: " << t.fileHash;          // not in Application, only in wizrad
            // qDebug() << "[ImportDialog] collectTasksFromModel t.fileSize: " << t.fileSize;          // Size of
            // qDebug() << "[ImportDialog] collectTasksFromModel t.fileSuffix: " << t.fileSuffix;      // file suffix <- must have Directory from target
            // qDebug() << "[ImportDialog] collectTasksFromModel t.categoryPath: " << t.categoryPath;  // BUG: always empty
            // qDebug() << "[ImportDialog] collectTasksFromModel t.relativePath: " << t.relativePath;  // new full path by managed
            // qDebug() << "[ImportDialog] collectTasksFromModel currentDirPath: " << currentDirPath;  // Empty
            // qDebug() << "------------------------------------------------------------------------------------";

            tasks.append(t);
        }
    }
}

void ImportDialog::accept() {
    // 1. Check for remaining files
    int remaining = countFiles(sourceModel_m->invisibleRootItem());
    if (remaining > 0) {
        auto res = QMessageBox::warning(this, tr("files left over"),
                                        tr("There are %1 files remaining in the list. These will not be imported.\n\nDo you want to continue?").arg(remaining),
                                        QMessageBox::Yes | QMessageBox::No);
        if (res == QMessageBox::No) return;
    }

    // 2. Collect tasks
    QList<ImportTask> tasks;

    collectTasksFromModel(targetModel_m->item(0), "", tasks);

    if (tasks.isEmpty()) {
        QMessageBox::information(this, tr("Empty import"), tr("No files were selected for import."));
        return;
    }

    QProgressDialog progress(tr("Files are being imported...."), tr("Cancel"), 0, tasks.size(), this);
    progress.setWindowModality(Qt::WindowModal);
    progress.show();

    // 5. Perform import
    ImportProcessor processor;
    connect(&processor, &ImportProcessor::progressUpdated, &progress, &QProgressDialog::setValue);

    bool success = processor.executeImport(tasks, dataBasePath_m, isManaged_m);

    if (success) {
        // If everything worked, close the dialog and report the result to MainWindow.
        QDialog::accept();
    } else {
        QMessageBox::critical(this, tr("Error"), tr("The import process failed."));
    }
}

int ImportDialog::countFiles(QStandardItem* item) {
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

void ImportDialog::setImportData(const QList<ScanBatch>& batches) {
    sourceModel_m->clear();
    sourceModel_m->setHorizontalHeaderLabels({tr("Source (verified)")});

    for (const auto& batch : batches) {
        // Add only valid files (no duplicates/defects)
        if (batch.status == StatusReady) {
            QStandardItem* fileItem = new QStandardItem(batch.info.fileName());
            fileItem->setData(batch.info.absoluteFilePath(), RoleFilePath);
            fileItem->setData(batch.hash, RoleFileHash);
            fileItem->setData(false, RoleIsFolder);

            // Use the reconstructPathInSource method to build the folder structure on the left.
            QStandardItem* parent = reconstructPathInSource(batch.info.path());
            parent->appendRow(fileItem);
        }
    }
    sourceView_m->expandAll();
}

void ImportDialog::fillMappingSource() {
    targetModel_m->clear();
    targetModel_m->setHorizontalHeaderLabels({tr("Target Structure (Managed)")});

    // 2.Start the recursive process
    fillRecursive(sourceModel_m->invisibleRootItem(), targetModel_m->invisibleRootItem());
}

void ImportDialog::fillRecursive(QStandardItem* sourceParent, QStandardItem* targetParent) {
    for (int i = 0; i < sourceParent->rowCount(); ++i) {
        QStandardItem* sourceItem = sourceParent->child(i, 0);
        if (!sourceItem) continue;

        // Check if file is selected/valid
        int status = sourceItem->data(RoleFileStatus).toInt();
        bool isChecked = (sourceItem->checkState() == Qt::Checked || status == StatusReady); // StatusReady instead of StatusManaged

        if (sourceItem->hasChildren()) {
            // Folder logic
            QStandardItem* newFolder = new QStandardItem(sourceItem->icon(), sourceItem->text());
            newFolder->setData(sourceItem->data(RoleFilePath), RoleFilePath);
            newFolder->setData(true, RoleIsFolder);

            fillRecursive(sourceItem, newFolder);

            if (newFolder->rowCount() > 0) {
                targetParent->appendRow(newFolder);
            } else {
                delete newFolder;
            }
        }
        else if (isChecked) {
            // File logic
            QStandardItem* fileItem = new QStandardItem(sourceItem->icon(), sourceItem->text());
            fileItem->setData(sourceItem->data(RoleFilePath), RoleFilePath);
            fileItem->setData(sourceItem->data(RoleFileHash), RoleFileHash);
            fileItem->setData(sourceItem->data(RoleFileSizeRaw), RoleFileSizeRaw);
            //fileItem->setData(sourceItem->data(RoleFileSuffix), RoleFileSuffix);
            fileItem->setData(false, RoleIsFolder);
            targetParent->appendRow(fileItem);
        }
    }
}
