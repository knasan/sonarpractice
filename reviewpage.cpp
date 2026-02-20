#include "reviewpage.h"
#include "fileutils.h"
#include "setupwizard.h"
#include "filemanager.h"
#include "filefilterproxymodel.h"
#include "filescanner.h"
#include "uihelper.h"

#include <QVBoxLayout>
#include <QTreeView>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QRadioButton>
#include <QGroupBox>
#include <QApplication>
#include <QMenu>
#include <QTimer>
#include <QMessageBox>
#include <QCheckBox>
#include <QKeyEvent>

// =============================================================================
// --- LIFECYCLE (Constructor / Destructor)
// =============================================================================

ReviewPage::ReviewPage(QWidget *parent) : BasePage(parent) {
    setTitle(tr("Examination"));
    setSubTitle(tr("Choose which files to import."));

    qRegisterMetaType<ReviewStats>("ReviewStats");

    setupLayout();
}

ReviewPage::~ReviewPage() = default;

// =============================================================================
// --- PAGE FLOW (Initialization / Validation)
// =============================================================================

void ReviewPage::initializePage() {
    auto *wiz = qobject_cast<SetupWizard*>(wizard());
    if (!wiz) return;

    // ALL wiz
    static bool connectionsEstablished = false;
    if (!connectionsEstablished) {
        setupConnections();
        connectionsEstablished = true;
    }

    wiz->fileManager()->clearCaches();
    if (wiz->filesModel()) {
        wiz->filesModel()->removeRows(0, wiz->filesModel()->rowCount());
    }

    wiz->prepareScannerWithDatabaseData();

    treeView_m->setModel(wiz->proxyModel());

    // configure header
    treeView_m->header()->setSectionResizeMode(ColName, QHeaderView::Stretch);

    treeView_m->header()->setSectionResizeMode(ColSize, QHeaderView::ResizeToContents);
    treeView_m->header()->setSectionResizeMode(ColStatus, QHeaderView::ResizeToContents);

    treeView_m->header()->setStretchLastSection(true);

    QMetaObject::invokeMethod(wiz->fileScanner(), "doScan",
                              Qt::QueuedConnection,
                              Q_ARG(QStringList, wiz->sourcePaths()),
                              Q_ARG(QStringList, wiz->activeFilters()));


    connect(wiz->fileScanner(), &FileScanner::finished, this, [this](const ReviewStats &workerStats) {
        applySmartCheck();
        statusLabel_m->setText(tr("Scan complete. %1 files found.").arg(workerStats.totalFiles));
    });
}

void ReviewPage::setupLayout() {
    setTitle(tr("Examination"));
    setSubTitle(tr("Choose which files to import."));

    qRegisterMetaType<ReviewStats>("ReviewStats");

    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(10);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setStretchFactor(treeView_m, 1);

    auto *topControlLayout = new QHBoxLayout();

    // --- Middle: The TreeView ---
    treeView_m = new QTreeView(this);
    treeView_m->setAlternatingRowColors(true);
    treeView_m->setSelectionMode(QAbstractItemView::ExtendedSelection);
    treeView_m->setContextMenuPolicy(Qt::CustomContextMenu);
    treeView_m->setEditTriggers(QAbstractItemView::NoEditTriggers);
    treeView_m->setContextMenuPolicy(Qt::CustomContextMenu);
    treeView_m->setIndentation(20);
    treeView_m->setSortingEnabled(true);

    collabsTree_m = new QCheckBox(tr("Open structure"), this);
    expertModeCheck_m = new QCheckBox(tr("Expert mode"), this);
    expertModeCheck_m->setToolTip(tr("Enables file deletion and direct editing of indexed files!"));

    topControlLayout->addWidget(collabsTree_m);
    topControlLayout->addWidget(expertModeCheck_m);

    // Filter (All, Error, Duplicates)
    auto *filterGroupBox = new QGroupBox(tr("Filter"), this);
    auto *filterLayout = new QHBoxLayout();
    radioAll_m = new QRadioButton(tr("All"), this);
    radioErrors_m = new QRadioButton(tr("Errors"), this);
    radioDuplicates_m = new QRadioButton(tr("Duplicates"), this);

    // Default all on
    radioAll_m->setChecked(true);

    // Search
    searchLineEdit_m = new QLineEdit(this);
    searchLineEdit_m->setPlaceholderText(tr("Search for artists, songs, or paths..."));

    filterLayout->addWidget(radioAll_m);
    filterLayout->addWidget(radioErrors_m);
    filterLayout->addWidget(radioDuplicates_m);
    filterLayout->addWidget(searchLineEdit_m);
    filterLayout->addStretch();

    filterGroupBox->setLayout(filterLayout);

    topControlLayout->addWidget(filterGroupBox);

    layout->addLayout(topControlLayout);
    layout->addWidget(treeView_m);
    this->setLayout(layout);


    // --- Below: Statistics bar ---
    // Container-Layout (Vertikal)
    auto *statsContainerLayout = new QVBoxLayout();
    statsContainerLayout->setSpacing(10);

    // Summary & Status (Vertically stacked on top of each other)
    summaryLabel_m = new QLabel(this);
    summaryLabel_m->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    summaryLabel_m->setWordWrap(true);

    statusLabel_m = new QLabel(this);
    statusLabel_m->setText("Status:");

    statsContainerLayout->addWidget(summaryLabel_m);
    statsContainerLayout->addWidget(statusLabel_m);

    // Finally, into your main page layout (which is probably called 'layout')
    layout->addLayout(statsContainerLayout);

    // Signals for filter changes connect
    connect(radioAll_m, &QRadioButton::toggled, this, &ReviewPage::onFilterChanged);
    connect(radioDuplicates_m, &QRadioButton::toggled, this, &ReviewPage::onFilterChanged);
    connect(radioErrors_m, &QRadioButton::toggled, this, &ReviewPage::onFilterChanged);
}

void ReviewPage::onScanProgressUpdated(const ReviewStats &stats) {
    summaryLabel_m->setText(tr("Found: %1 Files | Duplicate: %2 | Error: %3")
                                .arg(stats.totalFiles)
                                .arg(stats.duplicates)
                                .arg(stats.defects));
}

void ReviewPage::cleanupPage() {
    auto *wiz = qobject_cast<SetupWizard*>(wizard());
    if (wiz && wiz->fileScanner()) {
        wiz->fileScanner()->abort();
    }
}

void ReviewPage::onFilterChanged() {
    auto *wiz = qobject_cast<SetupWizard*>(wizard());
    if (!wiz || !treeView_m->model()) {
        return;
    }

    auto *proxy = qobject_cast<FileFilterProxyModel*>(treeView_m->model());
    if (!proxy) return;

    if (radioDuplicates_m->isChecked()) {
        proxy->setFilterMode(FileFilterProxyModel::ModeDuplicates);
    } else if (radioErrors_m->isChecked()) {
        proxy->setFilterMode(FileFilterProxyModel::ModeErrors);
    } else {
        proxy->setFilterMode(FileFilterProxyModel::ModeAll);
    }

    ReviewStats visibleStats = proxy->calculateVisibleStats();
    updateUIStats(visibleStats);
    emit completeChanged();
}

// =============================================================================
// --- GUI SLOTS (Events & Menus)
// =============================================================================

void ReviewPage::showContextMenu(const QPoint &pos) {
    QModelIndex proxyIndex = treeView_m->indexAt(pos);
    if (!proxyIndex.isValid()) {
        return;
    }
    // TODO: Context menu for summaryLabel
    showTreeContextMenu(pos, proxyIndex);
}

void ReviewPage::showSummaryContextMenu(const QPoint &pos) {
    qInfo() << "ShowSummaryContextMenu planed";
}

void ReviewPage::showTreeContextMenu(const QPoint &pos, const QModelIndex &proxyIndex) {
    if (!proxyIndex.isValid()) {
        return;
    }

    QModelIndex sourceIndex = wiz()->proxyModel()->mapToSource(proxyIndex);

    QModelIndex nameIndex = sourceIndex.siblingAtColumn(ColName);
    QString currentHash = nameIndex.data(RoleFileHash).toString();
    QString currentPath = nameIndex.data(RoleFilePath).toString();

    QMenu menu(this);

    // Section 1: File & Ignore Paths
    addFileActionsSectionToMenu(&menu, proxyIndex, currentPath);

    // Section 2: Duplicates
    addDuplicateSectionToMenu(&menu, nameIndex, currentHash, currentPath);

    // Section 3: Standard Actions
    addStandardActionsToMenu(&menu);

    menu.exec(treeView_m->viewport()->mapToGlobal(pos));
}

void ReviewPage::handleItemChanged(QStandardItem *item) {
    if (!item || item->column() != ColName) return;

    // 1. When a user selects a duplicate
    if (item->checkState() == Qt::Checked) {
        QString hash = item->data(RoleFileHash).toString();

        // If it is indeed a duplicate (hash present and not empty)
        if (!hash.isEmpty() && hash != "0") {
            QList<QStandardItem*> duplicates;
            auto *wiz = qobject_cast<SetupWizard*>(wizard());
            collectItemsByHashRecursive(wiz->filesModel()->invisibleRootItem(), hash, duplicates);

            // Logic: "Only one file may be selected at any one time"
            for (QStandardItem* dup : std::as_const(duplicates)) {
                if (dup != item && dup->checkState() == Qt::Checked) {
                    // Another file with this hash is already active!
                    item->setCheckState(Qt::Unchecked);
                    QMessageBox::warning(this, tr("Duplicate protection"),
                                         tr("You have already selected a copy of this file.\n"
                                            "Only one duplicate can be imported per group."));
                    return;
                }
            }
        }
    }

    // Update UI statistics after every change
    updateUIStats(calculateGlobalStats());
    emit completeChanged();
}

void ReviewPage::setAllCheckStates(Qt::CheckState state) {
    if (!wiz()->filesModel()) return;
    wiz()->filesModel()->blockSignals(true);

    // Iterate directly over the source model (including filtered items!).
    for (int row = 0; row < wiz()->filesModel()->rowCount(); ++row) {
        QStandardItem* item = wiz()->filesModel()->item(row, ColName);
        if (item) {
            setCheckStateRecursive(item, state);
        }
    }

    wiz()->filesModel()->blockSignals(false);

    treeView_m->viewport()->update();
    emit completeChanged();
}

void ReviewPage::applySmartCheck() {
    auto *wiz = qobject_cast<SetupWizard*>(wizard());
    if (!wiz) return;

    QStandardItemModel* model = wiz->filesModel();
    QSet<QString> seenHashes;

    // Block the signal to prevent handleItemChanged from running amok.
    model->blockSignals(true);

    // recursively go through all items
    auto applyRecursive = [&](auto self, QStandardItem* parent) -> void {
        for (int i = 0; i < parent->rowCount(); ++i) {
            QStandardItem* item = parent->child(i, ColName);
            if (!item) continue;

            QString hash = item->data(RoleFileHash).toString();
            int status = item->data(RoleFileStatus).toInt();

            if (status == StatusDuplicate) {
                if (seenHashes.contains(hash)) {
                    item->setCheckState(Qt::Unchecked);
                } else {
                    item->setCheckState(Qt::Checked);
                    seenHashes.insert(hash);
                }
            } else if (status == StatusReady) {
                item->setCheckState(Qt::Checked);
            }

            if (item->hasChildren()) self(self, item);
        }
    };

    applyRecursive(applyRecursive, model->invisibleRootItem());
    model->blockSignals(false);

    updateUIStats(calculateGlobalStats());
}

// block return by search line (don't delete this)
bool ReviewPage::eventFilter(QObject *obj, QEvent *event) {
    if (obj == searchLineEdit_m && event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);

        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            // Run search immediately
            wiz()->proxyModel()->setFilterFixedString(searchLineEdit_m->text());

            // Keep the focus in the field so the user can continue typing.
            searchLineEdit_m->selectAll();

            return true;
        }
    }
    // All other events will proceed as normal.
    return QWizardPage::eventFilter(obj, event);
}

// --- private --
void ReviewPage::setupConnections() {
    auto *wiz = qobject_cast<SetupWizard*>(wizard());
    if(!wiz) return;

    // Live search: Filter directly by proxy model
    connect(searchLineEdit_m, &QLineEdit::textChanged, this, [wiz](const QString &text) {
        wiz->proxyModel()->setFilterFixedString(text);
    });

    // Item changes (checkboxes)
    connect(wiz->filesModel(), &QStandardItemModel::itemChanged,
            this, &ReviewPage::handleItemChanged);

    // Instead of filtering immediately, start only after 400ms of inactivity in the typing flow.
    auto *searchTimer = new QTimer(this);
    searchTimer->setSingleShot(true);
    searchTimer->setInterval(400);

    connect(searchLineEdit_m, &QLineEdit::returnPressed, this, [this, wiz, searchTimer]() {
        searchTimer->stop();
        wiz->proxyModel()->setFilterFixedString(this->searchLineEdit_m->text());
    });

    // Connect search
    connect(searchTimer, &QTimer::timeout, this, [this, wiz]() {
        wiz->proxyModel()->setFilterFixedString(searchLineEdit_m->text());
    });

    connect(searchLineEdit_m, &QLineEdit::textChanged, searchTimer, qOverload<>(&QTimer::start));

    connect(wiz->filesModel(), &QStandardItemModel::itemChanged,
            this, &ReviewPage::onItemChanged);

    // TreeView
    connect(treeView_m, &QTreeView::customContextMenuRequested,
            this, &ReviewPage::showContextMenu);

    connect(collabsTree_m, &QCheckBox::toggled, this, [this](bool checked) {
        if (!treeView_m) return;

        treeView_m->setUpdatesEnabled(false);

        if (checked) {
            treeView_m->expandAll();
        } else {
            treeView_m->collapseAll();
        }

        treeView_m->setUpdatesEnabled(true);
    });

    // 3. Connect scanner signals (Qt::UniqueConnection prevents multiple connections)
    connect(wiz->fileScanner(), &FileScanner::batchesFound, wiz->fileManager(), &FileManager::addBatchesToModel, Qt::UniqueConnection);

    // Progressbar or show found files.
    connect(wiz->fileScanner(), &FileScanner::batchesFound, this, [this](const QList<ScanBatch> &batches) {
        statusLabel_m->setText(tr("Scanning: %1").arg(batches.last().info.absoluteFilePath()));
    });

    connect(wiz->fileScanner(), &FileScanner::progressStats,
            this, &ReviewPage::onScanProgressUpdated, Qt::UniqueConnection);

    connect(wiz->fileScanner(), &FileScanner::finished, this, [this]() {
        emit completeChanged();
    });
}

bool ReviewPage::isComplete() const {
    auto *wiz = qobject_cast<SetupWizard*>(wizard());
    if (!wiz || !wiz->filesModel()) return false;
    return wiz && !wiz->fileScanner()->isScanning();

    QMap<QString, int> selectionCounts;
    int totalSelected = 0;
    bool collisionFound = false;

    auto checkRecursive = [&](auto self, QStandardItem* parent) -> void {
        for (int i = 0; i < parent->rowCount(); ++i) {
            QStandardItem* item = parent->child(i, ColName);
            if (!item) continue;

            if (item->checkState() == Qt::Checked) {
                totalSelected++;
                QString hash = item->data(RoleFileHash).toString();
                if (!hash.isEmpty() && hash != "0") {
                    selectionCounts[hash]++;
                    if (selectionCounts[hash] > 1) {
                        collisionFound = true;
                    }
                }
            }
            if (item->hasChildren()) self(self, item);
        }
    };

    checkRecursive(checkRecursive, wiz->filesModel()->invisibleRootItem());
    return (totalSelected > 0 && !collisionFound);

}

void ReviewPage::updateUIStats(const ReviewStats &stats) {
    if (!wiz()) {
        qDebug() << "[ReviewPage] updateUIStats : wiz not ready";
        return;
    }
    if (!wiz()->fileManager()) {
        qDebug() << "[ReviewPage] updateUIStats : fileManager not ready";
        return;
    }
    if (!wiz()->filesModel()) {
        qDebug() << "[ReviewPage] updateUIStats : filesModel not ready";
        return;
    }

    // --- COLORS ---
    QColor colDefect = wiz()->fileManager()->getStatusColor(StatusDefect);
    colDefect = colDefect.toHsl();
    colDefect.setHsl(colDefect.hue(), 180, colDefect.lightness());
    const QString strColDefect = colDefect.name(QColor::HexArgb);

    QColor colReady = wiz()->fileManager()->getStatusColor(StatusReady);
    const QString strColReady = colReady.name(QColor::HexArgb);

    QColor strColDup = wiz()->fileManager()->getStatusColor(StatusDuplicate);

    // --- HTML LABEL UPDATE ---
    // %1 StatusFiles (text)
    // %2 TotalFiles
    // %3 StatusManaged (text)
    // %4 Color ready
    // %5 Selected files
    // %6 TotalBytes
    // %7 SavedBytes
    // %8 StatusDefect
    // %9 Color defects
    // %10 Defect number
    // %11 StatusDuplicate
    // %12 Color Duplicate
    // %13 Duplicate counter
    QString labelHtml = QString("<html><b>%1:</b>  <span style='color:%4;'>%2 (%6)</span> | <b>%3:</b> <span style='color:%4;'><b>%5</b> (%7)</span> | <b>%8:</b> <span style='color:%9;'>%10</span> | <b>%11:</b> <span style='color:%12;'>%13</span></html>");

    labelHtml = labelHtml.arg(wiz()->fileManager()->getStatusText(StatusFiles), QString::number(stats.totalFiles));

    labelHtml = labelHtml.arg(wiz()->fileManager()->getStatusText(StatusManaged),
                              strColReady,
                              QString::number(stats.selectedFiles),
                              FileUtils::formatBytes(stats.totalBytes),
                              FileUtils::formatBytes(stats.savedBytes));

    labelHtml = labelHtml.arg(wiz()->fileManager()->getStatusText(StatusDefect), strColDefect, QString::number(stats.defects));

    // Here, stats.duplicates remains (the constant number of duplicates found).
    labelHtml = labelHtml.arg(wiz()->fileManager()->getStatusText(StatusDuplicate), strColDup.name(), QString::number(stats.duplicates));

    summaryLabel_m->setText(labelHtml);
}

void ReviewPage::updateItemVisuals(QStandardItem* nameItem, int status) {
    if (!nameItem) return;

    QStandardItemModel* model = wiz()->filesModel();
    QStandardItem* parent = nameItem->parent() ? nameItem->parent() : model->invisibleRootItem();
    QStandardItem* statusItem = parent->child(nameItem->row(), ColStatus);

    if (!statusItem) return;

    // Basic reset
    nameItem->setBackground(Qt::transparent);
    statusItem->setText(FileManager::getStatusText(status));
    statusItem->setForeground(QApplication::palette().text());

    // Deletes the stored state, causing the box to disappear from the view.
    statusItem->setData(QVariant(), Qt::CheckStateRole);
    statusItem->setCheckable(false);

    if (status == StatusManaged) {
        QColor baseColor = FileManager::getStatusColor(StatusManaged);
        statusItem->setForeground(baseColor);

        QColor bgColor = baseColor;
        bgColor.setAlpha(30);
        nameItem->setBackground(bgColor);
    }
    else if (status == StatusReject) {
        // Grey and neutral for discarded items
        statusItem->setForeground(Qt::gray);
    }
    else if (status == StatusDuplicate) {
        // Warning color orange, if it is still an "unprocessed" duplicate
        statusItem->setForeground(FileManager::getStatusColor(StatusDuplicate));
    }
}

// =============================================================================
// --- Core logic (calculations & data processing)
// =============================================================================

ReviewStats ReviewPage::calculateGlobalStats() {
    ReviewStats stats;
    QStandardItemModel* model = wiz()->filesModel();

    // Use a recursive helper function to count all items in the source model.
    countItemsRecursive(model->invisibleRootItem(), stats);

    return stats;
}

void ReviewPage::countItemsRecursive(QStandardItem* parent, ReviewStats &stats) const {
    for (int row = 0; row < parent->rowCount(); ++row) {
        QStandardItem* item = parent->child(row, ColName);
        if (!item) continue;

        if (item->hasChildren()) {
            // It's a folder -> further down
            countItemsRecursive(item, stats);
        } else {
            // It's a file -> read values
            int status = item->data(RoleFileStatus).toInt();
            qint64 size = item->data(RoleFileSizeRaw).toLongLong();

            stats.totalFiles++;
            stats.totalBytes += size;

            // The most important part: Counting what is ready for import.
            if (item->checkState() == Qt::Checked || status == StatusManaged) {
                stats.selectedFiles++;
                stats.savedBytes += size;
            }
        }
    }
}

void ReviewPage::collectHashesRecursive(QStandardItem* parent, QStringList &hashes) {
    if (!parent) return;

    for (int i = 0; i < parent->rowCount(); ++i) {
        QStandardItem* child = parent->child(i, ColName);
        if (!child) continue;

        // If it is a file, it normally has no children (rowCount == 0)
        // But to be safe, check the hash again.
        QString h = child->data(RoleFileHash).toString();
        if (!h.isEmpty()) {
            hashes << h;
        }

        // If it's a folder, go deeper
        if (child->rowCount() > 0) {
            collectHashesRecursive(child, hashes);
        }
    }
}

// // Delete Guard
QStringList ReviewPage::getUnrecognizedFiles(const QString &folderPath) {
    QStringList unrecognized;
    QDir dir(folderPath);

    QStringList knownFilters = wiz()->activeFilters();

    QDirIterator it(folderPath, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);

    while (it.hasNext()) {
        it.next();
        QFileInfo info = it.fileInfo();
        QString suffix = "*." + info.suffix().toLower();

        // Check if the ending is in the known filters.
        if (!knownFilters.contains(suffix, Qt::CaseInsensitive)) {
            unrecognized << info.fileName();
        }
    }

    return unrecognized;
}

// bool ReviewPage::finishDialog() {
//     ReviewStats stats = wiz()->proxyModel_m->calculateVisibleStats();
//     QStringList unresolvedDups = getUnresolvedDuplicateNames();

//     // The message text (HTML for better readability)
//     QString msg = QString(
//                       "<h3>" + tr("Import summary") + "</h3>"
//                                                                    "<p><b>" + tr("In total:") + "</b> %1 (%2)</p>"
//                                         "<p><b>" + tr("Take over:") + "</b> <span style='color:green;'>%3 (%4)</span></p>"
//                                             "<p><b>" + tr("Defect:") + "</b> <span style='color:red;'>%5</span></p>"
//                                         "<p><b>" + tr("Duplicates:") + "</b> %6</p>"
//                       ).arg(QString::number(stats.totalFiles()),
//                            FileUtils::formatBytes(stats.totalBytes()),
//                            QString::number(stats.selectedFiles()),
//                            FileUtils::formatBytes(stats.savedBytes()),
//                            QString::number(stats.defects()),
//                            QString::number(totalDuplicatesCount_m));

//     // Add warning for unresolved duplicates
//     if (!unresolvedDups.isEmpty()) {
//         msg += "<div style='margin-top:10px; padding:5px; border:1px solid orange;'>"
//                "<b>" + tr("Attention: No duplicate was selected in %1 group(s):").arg(unresolvedDups.size()) + "</b><br/>"
//                                                                                                           "<i>" + unresolvedDups.join(", ") + "</i></div>";
//     }

//     msg += "<p><br/>" + tr("Do you want to accept the selection as is?") + "</p>";

//     // Show Dialog
//     QMessageBox msgBox(this);
//     msgBox.setWindowTitle(tr("Confirm import"));
//     msgBox.setTextFormat(Qt::RichText);
//     msgBox.setText(msg);
//     msgBox.setIcon(QMessageBox::Question);

//     QPushButton *confirmBtn = msgBox.addButton(tr("Take over"), QMessageBox::AcceptRole);
//     msgBox.addButton(tr("Correct"), QMessageBox::RejectRole);

//     msgBox.exec();

//     if (msgBox.clickedButton() == confirmBtn) {
//         return true;
//     }

//     return false;
// }

void ReviewPage::onItemChanged(QStandardItem *item) {
    if (!item) return;
    qDebug() << "[ReviewPage::onItemChanged] START";

    wiz()->filesModel()->blockSignals(true);

    Qt::CheckState newState = item->checkState();

    setCheckStateRecursive(item, newState);
    wiz()->filesModel()->blockSignals(false);
}

// =============================================================================
// --- DUPLICATE LOGIC (Hashing & Comparisons)
// =============================================================================

void ReviewPage::addDuplicateSectionToMenu(QMenu *menu, const QModelIndex &nameIndex,
                                           const QString &currentHash, const QString &currentPath) {
    if (currentHash.isEmpty())  {
        return;
    }

    QString fileName = nameIndex.data(Qt::DisplayRole).toString();

    // "Jump to Duplicate" submenu
    QMenu *jumpMenu = menu->addMenu(tr("Jump to duplicate..."));
    addJumpToDuplicateActions(jumpMenu, currentHash, currentPath);

    menu->addSeparator();
}

void ReviewPage::addJumpToDuplicateActions(QMenu *jumpMenu, const QString &currentHash,
                                           const QString &currentPath) {
    QModelIndexList partners = findDuplicatePartners(currentHash);
    if (partners.isEmpty()) return;

    for (const QModelIndex &pSrcIndex : std::as_const(partners)) {
        if (pSrcIndex.data(RoleFilePath).toString() == currentPath) continue;

        QString path = pSrcIndex.data(RoleFilePath).toString();
        jumpMenu->addAction(path, [this, pSrcIndex]() {
            jumpToDuplicate(pSrcIndex);
        });
    }
}


QModelIndexList ReviewPage::findDuplicatePartners(const QString &hash) {
    return wiz()->filesModel()->match(
        wiz()->filesModel()->index(0, 0),
        RoleFileHash,
        hash,
        -1,
        Qt::MatchExactly | Qt::MatchRecursive
        );
}

void ReviewPage::jumpToDuplicate(const QModelIndex &sourceIndex) {
    QModelIndex proxyIndex = wiz()->proxyModel()->mapFromSource(sourceIndex);
    if (!proxyIndex.isValid()) return;

    treeView_m->expand(proxyIndex.parent());
    treeView_m->scrollTo(proxyIndex, QAbstractItemView::PositionAtCenter);
    treeView_m->setCurrentIndex(proxyIndex);

    // Visueller Effekt
    QStandardItem* item = wiz()->filesModel()->itemFromIndex(sourceIndex);
    if (item) {
        QTimer::singleShot(200, [this]() { treeView_m->clearSelection(); });
        QTimer::singleShot(400, [this, proxyIndex]() {
            treeView_m->selectionModel()->select(proxyIndex,
                                                 QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        });
    }
}

void ReviewPage::refreshDuplicateStatus(const QString &hash) {
    if (hash.isEmpty()) return;

    QList<QStandardItem*> remainingItems;
    collectItemsByHashRecursive(wiz()->filesModel()->invisibleRootItem(), hash, remainingItems);

    if (remainingItems.size() == 1) {
        QStandardItem* lastOne = remainingItems.first();

        lastOne->setData(StatusReady, RoleFileStatus);

        QStandardItem* parent = lastOne->parent() ? lastOne->parent() : wiz()->filesModel()->invisibleRootItem();
        QStandardItem* statusItem = parent->child(lastOne->row(), ColStatus);

        if (statusItem) {
            statusItem->setCheckable(false);
            statusItem->setData(QVariant(), Qt::CheckStateRole);
            statusItem->setText(FileManager::getStatusText(StatusReady));
        }

        lastOne->setData(QBrush(Qt::black), Qt::ForegroundRole);
        QFont font = lastOne->font();
        font.setBold(false);
        lastOne->setFont(font);

        lastOne->setCheckState(Qt::Checked);
    }
}

void ReviewPage::collectItemsByHashRecursive(QStandardItem* parent, const QString &hash, QList<QStandardItem*> &result) {
    for (int i = 0; i < parent->rowCount(); ++i) {
        QStandardItem* child = parent->child(i, ColName);
        if (!child) continue;

        if (child->data(RoleFileHash).toString() == hash) {
            result.append(child);
        }

        if (child->rowCount() > 0) {
            collectItemsByHashRecursive(child, hash, result);
        }
    }
}

// =========================================================================
// --- RECURSIVE HELPERS & UTILS
// =========================================================================

void ReviewPage::discardItemFromModel(const QModelIndex &proxyIndex) {
    QModelIndex sourceIndex = wiz()->proxyModel()->mapToSource(proxyIndex);
    QStandardItem *item = wiz()->filesModel()->itemFromIndex(sourceIndex);
    if (!item) return;

    qDebug() << "[ReviewPage::discardItemFromModel] START";

    QString name = item->text();
    bool isFolder = QFileInfo(item->data(RoleFilePath).toString()).isDir();

    // --- Security check ---
    auto res = QMessageBox::question(this, tr("Remove from list"),
                                     tr("Do you really want to remove %1 <b>'%2'</b> from this view?<br><br>"
                                        "<small>The files remain on the hard drive, but are ignored during this import process.</small>")
                                         .arg(isFolder ? tr("the folder") : tr("the file"), name),
                                     QMessageBox::Yes | QMessageBox::No);

    if (res != QMessageBox::Yes) return;

    // --- execution ---
    // Collect hashes for status healing (optional, but recommended for accuracy)
    QStringList affectedHashes;
    collectHashesRecursive(item, affectedHashes);

    // Remove from the model
    if (sourceIndex.parent().isValid()) {
        wiz()->filesModel()->removeRow(sourceIndex.row(), sourceIndex.parent());
    } else {
        wiz()->filesModel()->removeRow(sourceIndex.row());
    }

    // Status healing of the remaining duplicates
    for (const QString &h : std::as_const(affectedHashes)) {
        refreshDuplicateStatus(h);
    }
    emit completeChanged();
}

void ReviewPage::deleteItemPhysically(const QModelIndex &proxyIndex) {
    // Retrieve source index (since we are using a proxy model)
    QModelIndex sourceIndex = wiz()->proxyModel()->mapToSource(proxyIndex);
    QStandardItem *item = wiz()->filesModel()->itemFromIndex(sourceIndex);

    if (!item) return;

    QString rawPath = item->data(RoleFilePath).toString();
    QString cleanPath = QDir::cleanPath(rawPath);

    QString fileHash = item->data(RoleFileHash).toString();
    QFileInfo fileInfo(cleanPath);
    bool isFolder = fileInfo.isDir();

    if (isFolder) {
        QStringList unknown = getUnrecognizedFiles(cleanPath);

        if (!unknown.isEmpty()) {
            // Show the first 5-10 files as examples
            QString listPreview = unknown.mid(0, 10).join("\n");
            if (unknown.size() > 10) listPreview += "\n...";

            auto res = QMessageBox::warning(this, tr("Data loss warning"),
                                            tr("The folder contains %1 files that were NOT captured by the scanner (e.g., ZIP files, images, text):\n\n"
                                               "%2\n\n"
                                               "Do you really want to delete the folder and ALL the files it contains?").arg(unknown.size()).arg(listPreview),
                                            QMessageBox::Yes | QMessageBox::No);

            if (res != QMessageBox::Yes) return;
        }
    } else {
        // Security check
        auto res = QMessageBox::warning(this, tr("Confirm deletion"),
                                        tr("Do you really want to move %1 to the trash?\n\n%2")
                                            .arg(isFolder ? "this folder" : "this file", cleanPath),
                                        QMessageBox::Yes | QMessageBox::No);
        if (res != QMessageBox::Yes) return;
    }

    // Delete to hard drive (Recycle Bin)
    if (QFile::moveToTrash(cleanPath)) {
        // Remove from the model if it is a root item (level 0):
        if (!item->parent()) {
            wiz()->filesModel()->removeRow(sourceIndex.row());
        } else {
            // If it is a child:
            item->parent()->removeRow(sourceIndex.row());
        }

        if (!isFolder) {
            refreshDuplicateStatus(fileHash);
        }

        qInfo() << "Successfully deleted: " << cleanPath;
    } else {
        QMessageBox::critical(this, tr("Error"), tr("The file could not be deleted."));
    }
}

void ReviewPage::addFileActionsSectionToMenu(QMenu *menu, const QModelIndex &proxyIndex,
                                             const QString &currentPath) {
    if (currentPath.isEmpty()) return;

    QFileInfo fileInfo(currentPath);
    if (fileInfo.isDir()) {
        menu->addAction(tr("Open directory: %1").arg(currentPath), [this, currentPath]() {
            UIHelper::openFileWithFeedback(this, currentPath);
        });
    } else {
        menu->addAction(tr("Open file: %1").arg(currentPath), [this, currentPath]() {
            UIHelper::openFileWithFeedback(this, currentPath);
        });
    }

    menu->addSeparator();

    QString displayName = QFileInfo(currentPath).fileName();
    menu->setToolTip(tr("Removes the entry only from this view. The file itself is not deleted."));
    menu->addAction(tr("Remove from list: %1").arg(displayName), this, [this, proxyIndex]() {
        discardItemFromModel(proxyIndex);
    });

    if (expertModeCheck_m->isChecked()) {
        int status = proxyIndex.data(RoleFileStatus).toInt();
        QString hash = proxyIndex.data(RoleFileHash).toString();
        if (status == StatusDuplicate) {
            menu->addSeparator();
            menu->addAction(tr("Delete duplicate %1").arg(currentPath), this, [this, proxyIndex](){
                deleteItemPhysically(proxyIndex);
            });
        } else if (hash.isEmpty()) {
            menu->addSeparator();
            menu->addAction(tr("Delete directory %1").arg(currentPath), this, [this, proxyIndex](){
                deleteItemPhysically(proxyIndex);
            });
        }
        menu->addSeparator();
    }
}

void ReviewPage::addStandardActionsToMenu(QMenu *menu) {
    menu->addAction(tr("Select all"), this, [this]() {
        setAllCheckStates(Qt::Checked);
    });
    menu->addAction(tr("Clear selection"), this, [this]() {
        setAllCheckStates(Qt::Unchecked);
    });
}

// =============================================================================
// --- UTILS (Recursive Helpers)
// =============================================================================

void ReviewPage::recursiveCheckChilds(QStandardItem* parentItem, Qt::CheckState state) {
    if (!parentItem)
        return;

    for (int i = 0; i < parentItem->rowCount(); ++i) {
        QStandardItem* child = parentItem->child(i, ColName);
        if (child) {
            child->setCheckState(state);

            // Child status update
            int gId = child->data(RoleDuplicateId).toInt();
            int status = (state == Qt::Checked) ? StatusManaged :
                             (gId > 0 ? StatusDuplicate : StatusReady);

            child->setData(status, RoleFileStatus);

            updateItemVisuals(child, status);

            // Go deeper if the child is a folder again
            if (child->hasChildren()) {
                recursiveCheckChilds(child, state);
            }
        }
    }
    emit completeChanged();
}

void ReviewPage::setCheckStateRecursive(QStandardItem* item, Qt::CheckState state) {
    if (!item) return;

    if(item->data(RoleIsFolder).toBool()) {
        item->setCheckable(false);
    }

    if (item->column() == ColName) {
        if (item->data(RoleFileStatus).toInt() != StatusDefect) {
            item->setCheckState(state);
        }
    } else {
        // If we end up here, we'll clear the column just to be safe.
        item->setData(QVariant(), Qt::CheckStateRole);
        item->setCheckable(false);
    }

    for (int row = 0; row < item->rowCount(); ++row) {
        QStandardItem* child = item->child(row, ColName);
        if (child) {
            setCheckStateRecursive(child, state);
        }
    }
    emit completeChanged();
}

void ReviewPage::searchGroupRecursive(QStandardItem* parent, int groupId, QList<QStandardItem*>& results) {
    for (int i = 0; i < parent->rowCount(); ++i) {
        // always search in the name column (ColName).
        QStandardItem* child = parent->child(i, ColName);
        if (!child) continue;

        if (child->data(RoleDuplicateId).toInt() == groupId) {
            results.append(child);
        }

        if (child->hasChildren()) {
            searchGroupRecursive(child, groupId, results);
        }
    }
}
