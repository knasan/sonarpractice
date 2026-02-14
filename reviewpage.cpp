#include "reviewpage.h"
#include "setupwizard.h"
#include "filemanager.h"
#include "fileutils.h"
#include "uihelper.h"
#include "filefilterproxymodel.h"
#include "filescanner.h"
#include "sonarstructs.h"
#include "reviewstruct.h"

#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>

#include <QVBoxLayout>
#include <QTreeView>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QWizard>
#include <QRadioButton>
#include <QGroupBox>
#include <QTimer>
#include <QEvent>
#include <QKeyEvent>
#include <QPushButton>
#include <QCache>
#include <QCheckBox>
#include <QApplication>
#include <QMessageBox>
#include <QLineEdit>

// =============================================================================
// --- LIFECYCLE (Constructor / Destructor)
// =============================================================================

ReviewPage::ReviewPage(QWidget *parent) : BasePage(parent)
{
    setTitle(tr("Examination"));
    setSubTitle(tr("Choose which files to import."));

    qRegisterMetaType<ReviewStats>("ReviewStats");

    // ContextMenu find partners (Duplicates)
    pathPartsCache_m = new QCache<QString, QStringList>(10000);

    auto *layout = new QVBoxLayout(this);
    auto *topControlLayout = new QHBoxLayout();

    // TreeView Setup
    treeView_m = new QTreeView(this);
    treeView_m->setAlternatingRowColors(true);
    treeView_m->setSelectionMode(QAbstractItemView::ExtendedSelection);
    treeView_m->setContextMenuPolicy(Qt::CustomContextMenu);
    treeView_m->setEditTriggers(QAbstractItemView::NoEditTriggers);

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
    radioDup_m = new QRadioButton(tr("Duplicates"), this);

    // Default all on
    radioAll_m->setChecked(true);

    // Search
    searchLineEdit_m = new QLineEdit(this);
    searchLineEdit_m->setPlaceholderText(tr("Search for artists, songs, or paths..."));

    filterLayout->addWidget(radioAll_m);
    filterLayout->addWidget(radioErrors_m);
    filterLayout->addWidget(radioDup_m);
    filterLayout->addWidget(searchLineEdit_m);

    filterGroupBox->setLayout(filterLayout);

    topControlLayout->addWidget(filterGroupBox);

    layout->addLayout(topControlLayout);

    layout->addWidget(treeView_m);

    // Container-Layout (Vertikal)
    auto *statsContainerLayout = new QVBoxLayout();
    statsContainerLayout->setSpacing(10);

    // Summary & Status (Vertically stacked on top of each other)
    summaryLabel_m = new QLabel(this);
    statusLabel_m = new QLabel(this);
    statsContainerLayout->addWidget(summaryLabel_m);
    statsContainerLayout->addWidget(statusLabel_m);

    // Button-Layout (Horizontal)
    auto *actionHBox = new QHBoxLayout();

    // Putting it all together
    statsContainerLayout->addLayout(actionHBox);

    // Finally, into your main page layout (which is probably called 'layout')
    layout->addLayout(statsContainerLayout);
}

// =============================================================================
// --- PAGE FLOW (Initialization / Validation)
// =============================================================================

void ReviewPage::initializePage() {
    treeView_m->setModel(wiz()->proxyModel_m);
    treeView_m->setSortingEnabled(false);

    // reset file model
    wiz()->filesModel()->clear();
    wiz()->setProxyModelHeader();

    wiz()->proxyModel_m->setSourceModel(wiz()->filesModel());
    wiz()->proxyModel_m->setDynamicSortFilter(true);

    wiz()->proxyModel_m->setFilterMode(FileFilterProxyModel::ModeAll);
    wiz()->proxyModel_m->setFilterFixedString(QString());
    wiz()->proxyModel_m->setFilterKeyColumn(0); //
    wiz()->proxyModel_m->setFilterCaseSensitivity(Qt::CaseInsensitive); //

    // Column optimization (important for visibility)
    treeView_m->header()->setSectionResizeMode(QHeaderView::Interactive);
    treeView_m->setColumnWidth(ColName, 1200); //
    treeView_m->header()->setSectionResizeMode(ColName, QHeaderView::Interactive);
    treeView_m->header()->setCascadingSectionResizes(true);
    treeView_m->header()->setSectionResizeMode(ColSize, QHeaderView::Stretch);
    treeView_m->header()->setSectionResizeMode(ColStatus, QHeaderView::Stretch);

    treeView_m->setEnabled(false);

    // Tell the search field that this class (this) is monitoring the events
    searchLineEdit_m->installEventFilter(this);
    searchLineEdit_m->setClearButtonEnabled(true); // add x in QLineEdit

    if (!isConnectionsEstablished_m) {
        isConnectionsEstablished_m = true;
        QThread* scanThread = new QThread(this);
        FileScanner* worker = new FileScanner();
        worker->moveToThread(scanThread);

        // --- CONNECTIONS ---
        sideConnections();

        // Start signal
        connect(this, &ReviewPage::requestScanStart, worker, &FileScanner::doScan);

        connect(worker, &FileScanner::batchesFound, this, [this](const QList<ScanBatch> &batches) {
            wiz()->filesModel()->blockSignals(true);
            wiz()->fileManager()->addBatchesToModel(batches);
            wiz()->filesModel()->blockSignals(false);
            summaryLabel_m->setText(tr("Index: %1").arg(batches.last().info.absoluteFilePath()));
            statusLabel_m->setText(tr("Scanning ..."));
        });

        connect(worker, &FileScanner::progressStats, this, [this](const ReviewStats &stats) {
            // qDebug() << "[ReviewPage] progressStats";
            treeView_m->setEnabled(false);
            statusLabel_m->setText(tr("%1 Data scanned ... ").arg(stats.totalFiles()));
        });

        connect(worker, &FileScanner::finished, this, [this, scanThread, worker](const ReviewStats &workerStats) {
            // Copy the data from the worker to the wizard's global stats.
            // ReviewStats has an assignment operator operator=
            qDebug() << "[ReviewPage] Worker reports files:" << workerStats.totalFiles();

            // Transfer data to the wizard
            // wiz()->getFinalStats() When a reference is returned, we use the operator=
            wiz()->getFinalStats() = workerStats;

            auto *effector = new QGraphicsOpacityEffect(statusLabel_m);
            statusLabel_m->setGraphicsEffect(effector);

            // UI update with the now (hopefully) populated wizard stats
            int count = workerStats.totalFiles();
            statusLabel_m->setText(tr("Scan complete. %1 files found.").arg(count));

            // A "delayed" fade-out: Wait 4 seconds, then fade for 1 second.
            QTimer::singleShot(4000, this, [effector]() {
                auto *anim = new QPropertyAnimation(effector, "opacity");
                anim->setDuration(1000); // The fade lasts 1 second.
                anim->setStartValue(1.0);
                anim->setEndValue(0.0);
                anim->setEasingCurve(QEasingCurve::OutCubic); // Gentle exit

                //After the animation ends, really clear the text.
                connect(anim, &QPropertyAnimation::finished, [anim]() {
                    anim->deleteLater();
                });

                anim->start(QAbstractAnimation::DeleteWhenStopped);
            });

            // Unlock UI elements
            wiz()->filesModel()->layoutChanged();
            treeView_m->blockSignals(false);
            updateUIStats();

            treeView_m->setEnabled(true);
            // treeView_m->expandAll();

            // Cleanly close the thread
            scanThread->quit();
            scanThread->deleteLater();
            worker->deleteLater();
            isConnectionsEstablished_m = false;
        });

        connect(worker, &FileScanner::finishWithAllBatches, this, [this, scanThread](const QList<ScanBatch> &allBatches, const ReviewStats &stats) {
            this->totalDuplicatesCount_m = stats.duplicates();
            this->totalDefectsCount_m = stats.defects();
            wiz()->fileManager()->updateStatuses(allBatches);
        });

        // Cleanup
        connect(scanThread, &QThread::finished, worker, &QObject::deleteLater);
        connect(scanThread, &QThread::finished, scanThread, &QObject::deleteLater);

        scanThread->start();
        emit requestScanStart(wiz()->getSelectedPaths(), wiz()->getFileFilters(), wiz());
    }
}

bool ReviewPage::validatePage() {
    qDebug() << "[ReviewPage] validatePage START";
    return finishDialog();
}

// =============================================================================
// --- GUI SLOTS (Events & Menus)
// =============================================================================

void ReviewPage::showContextMenu(const QPoint &pos) {
    qDebug() << "[ReviewPage] showContextMenu START";
    QModelIndex proxyIndex = treeView_m->indexAt(pos);
    if (!proxyIndex.isValid()) {
        qDebug() << "index from model not valid";
        return;
    }
    // TODO: context menü für summaryLabel
    showTreeContextMenu(pos, proxyIndex);
}

void ReviewPage::showSummaryContextMenu(const QPoint &pos) {
    qDebug() << "ShowSummaryContextMenu planed";
}

void ReviewPage::showTreeContextMenu(const QPoint &pos, const QModelIndex &proxyIndex) {
    if (!proxyIndex.isValid()) {
        qDebug() << "index from model not valid";
        return;
    }
    qDebug() << "[ReviewPage] showTreeContextMenu START";
    QModelIndex sourceIndex = wiz()->proxyModel_m->mapToSource(proxyIndex);

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
    if (item->column() != ColName) return;

    static bool isUpdating = false;
    if (isUpdating) return;

    isUpdating = true;

    int groupId = item->data(RoleDuplicateId).toInt();

    Qt::CheckState currentState = item->checkState();

    if (currentState == Qt::Checked) {
        // Checkmark set -> Managed (Blue)
        item->setData(StatusManaged, RoleFileStatus);
        updateItemVisuals(item, StatusManaged);

        // PARTNER SEARCH: It must see deep through the tree
        if (groupId > 0) {
            QList<QStandardItem*> results;
            // The search starts at the root to catch all levels.
            searchGroupRecursive(wiz()->filesModel()->invisibleRootItem(), groupId, results);

            for (QStandardItem* other : std::as_const(results)) {
                if (other != item) {
                    other->setCheckState(Qt::Unchecked);
                    other->setData(StatusReject, RoleFileStatus);
                    if (other->column() != ColName && other->data(Qt::CheckStateRole).isValid()) {
                        qWarning() << "ERROR: other (item) handleItemChanged: in column: " << other->column()
                        << "has a checkbox! Path:" << other->data(RoleFilePath).toString();
                    }
                    updateItemVisuals(other, StatusReject);
                }
            }
        }
    } else {
        // Check removed -> Back to original
        int currentStatus = item->data(RoleFileStatus).toInt();
        int newStatus;

        if (currentStatus == StatusReject) {
            // If it was already rejected, we'll make it a "normal" duplicate again.
            newStatus = (groupId > 0) ? StatusDuplicate : StatusReady;
        } else {
            // Otherwise, we will mark it as rejected.
            newStatus = StatusReject;
        }

        item->setData(newStatus, RoleFileStatus);
        updateItemVisuals(item, newStatus);
    }

    // RECURSION: Children (Folder Logic)
    if (item->hasChildren()) {
        recursiveCheckChilds(item, currentState);
    }

    debugItemInfo(item, QString("handleItemChanged"));

    // Statistics Connect recalculates everything.
    wiz()->filesModel()->dataChanged(item->index(), item->index());
    isUpdating = false;
}

void ReviewPage::setAllCheckStates(Qt::CheckState state) {
    if (!wiz()->filesModel()) return;
    qDebug() << "[ReviewPage] setAllCheckStates START";
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
}

// block return by search line (don't delete this)
bool ReviewPage::eventFilter(QObject *obj, QEvent *event) {
    if (obj == searchLineEdit_m && event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);

        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            // Run search immediately
            wiz()->proxyModel_m->setFilterFixedString(searchLineEdit_m->text());

            // Keep the focus in the field so the user can continue typing.
            searchLineEdit_m->selectAll();

            return true;
        }
    }
    // All other events will proceed as normal.
    return QWizardPage::eventFilter(obj, event);
}

// --- private --

void ReviewPage::sideConnections() {
    // Signals connect
    connect(treeView_m, &QTreeView::customContextMenuRequested,
            this, &ReviewPage::showContextMenu);

    connect(collabsTree_m, &QCheckBox::checkStateChanged, this, [this](int state) {
        if (state == Qt::Checked) {
            treeView_m->expandAll();
        } else {
            treeView_m->collapseAll();
        }
    });

    connect(wiz()->filesModel(), &QStandardItemModel::itemChanged,
            this, &ReviewPage::onItemChanged);

    // Observe model changes, Qt::QueuedConnection -> is executed when Tree has time.
    connect(wiz()->filesModel(), &QStandardItemModel::dataChanged, this, &ReviewPage::updateUIStats, Qt::QueuedConnection);

    // Responds to file deletion (ignore, delete 0-bytes)
    connect(wiz()->filesModel(), &QStandardItemModel::rowsRemoved,
            this, &ReviewPage::updateUIStats, Qt::QueuedConnection);

    // If the entire list is ever reloaded
    connect(wiz()->filesModel(), &QStandardItemModel::modelReset,
            this, &ReviewPage::updateUIStats, Qt::QueuedConnection);

    // Filters
    // Connection for "Show all"
    connect(radioAll_m, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) {
            wiz()->proxyModel_m->setFilterMode(FileFilterProxyModel::ModeAll);
        }
    });

    // Connection for "Show errors only"
    connect(radioErrors_m, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) {
            wiz()->proxyModel_m->setFilterMode(FileFilterProxyModel::ModeErrors);
        }
    });

    // Connection for "Show only duplicates"
    connect(radioDup_m, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) {
            wiz()->proxyModel_m->setFilterMode(FileFilterProxyModel::ModeDuplicates);
        }
    });

    // Checkbox logik
    connect(wiz()->filesModel(), &QStandardItemModel::itemChanged,
            this, &ReviewPage::handleItemChanged);

    // Instead of filtering immediately, start only after 400ms of inactivity in the typing flow.
    auto *searchTimer = new QTimer(this);
    searchTimer->setSingleShot(true);
    searchTimer->setInterval(400);

    // intercept return
    connect(searchLineEdit_m, &QLineEdit::returnPressed, this, [this, searchTimer]() {
        searchTimer->stop();
        wiz()->proxyModel_m->setFilterFixedString(searchLineEdit_m->text());
    });

    // Connect search
    connect(searchTimer, &QTimer::timeout, this, [this]() {
        wiz()->proxyModel_m->setFilterFixedString(searchLineEdit_m->text());
    });

    connect(searchLineEdit_m, &QLineEdit::textChanged, searchTimer, qOverload<>(&QTimer::start));
}

void ReviewPage::updateUIStats() {
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

    // qDebug() << "[ReviewPage] updateUIStats START";
    // Wir holen die Basis-Stats (für totalFiles, defects, etc.)
    // ReviewStats stats = wiz()->fileManager()->calculateStats(wiz()->filesModel());
    // ReviewStats stats = wiz()->proxyModel_m->calculateVisibleStats();
    ReviewStats stats = calculateGlobalStats();

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

    labelHtml = labelHtml.arg(wiz()->fileManager()->getStatusText(StatusFiles), QString::number(stats.totalFiles()));

    labelHtml = labelHtml.arg(wiz()->fileManager()->getStatusText(StatusManaged),
                              strColReady,
                              QString::number(stats.selectedFiles()),
                              FileUtils::formatBytes(stats.totalBytes()),
                              FileUtils::formatBytes(stats.savedBytes()));

    labelHtml = labelHtml.arg(wiz()->fileManager()->getStatusText(StatusDefect), strColDefect, QString::number(totalDefectsCount_m));

    // Here, stats.duplicates remains (the constant number of duplicates found).
    labelHtml = labelHtml.arg(wiz()->fileManager()->getStatusText(StatusDuplicate), strColDup.name(), QString::number(totalDuplicatesCount_m));

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

            stats.totalFiles_m++;
            stats.totalBytes_m += size;

            // The most important part: Counting what is ready for import.
            if (item->checkState() == Qt::Checked || status == StatusManaged) {
                stats.selectedFiles_m++;
                stats.savedBytes_m += size;
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

// Delete Guard
QStringList ReviewPage::getUnrecognizedFiles(const QString &folderPath) {
    QStringList unrecognized;
    QDir dir(folderPath);

    QStringList knownFilters = wiz()->getFileFilters();

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

bool ReviewPage::finishDialog() {
    ReviewStats stats = wiz()->proxyModel_m->calculateVisibleStats();
    QStringList unresolvedDups = getUnresolvedDuplicateNames();

    // The message text (HTML for better readability)
    QString msg = QString(
                      "<h3>" + tr("Import summary") + "</h3>"
                                                                   "<p><b>" + tr("In total:") + "</b> %1 (%2)</p>"
                                        "<p><b>" + tr("Take over:") + "</b> <span style='color:green;'>%3 (%4)</span></p>"
                                            "<p><b>" + tr("Defect:") + "</b> <span style='color:red;'>%5</span></p>"
                                        "<p><b>" + tr("Duplicates:") + "</b> %6</p>"
                      ).arg(QString::number(stats.totalFiles()),
                           FileUtils::formatBytes(stats.totalBytes()),
                           QString::number(stats.selectedFiles()),
                           FileUtils::formatBytes(stats.savedBytes()),
                           QString::number(stats.defects()),
                           QString::number(totalDuplicatesCount_m));

    // Add warning for unresolved duplicates
    if (!unresolvedDups.isEmpty()) {
        msg += "<div style='margin-top:10px; padding:5px; border:1px solid orange;'>"
               "<b>" + tr("Attention: No duplicate was selected in %1 group(s):").arg(unresolvedDups.size()) + "</b><br/>"
                                                                                                          "<i>" + unresolvedDups.join(", ") + "</i></div>";
    }

    msg += "<p><br/>" + tr("Do you want to accept the selection as is?") + "</p>";

    // Show Dialog
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("Confirm import"));
    msgBox.setTextFormat(Qt::RichText);
    msgBox.setText(msg);
    msgBox.setIcon(QMessageBox::Question);

    QPushButton *confirmBtn = msgBox.addButton(tr("Take over"), QMessageBox::AcceptRole);
    msgBox.addButton(tr("Correct"), QMessageBox::RejectRole);

    msgBox.exec();

    if (msgBox.clickedButton() == confirmBtn) {
        return true;
    }

    return false;
}

void ReviewPage::onItemChanged(QStandardItem *item) {
    if (!item) return;

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
    QModelIndex proxyIndex = wiz()->proxyModel_m->mapFromSource(sourceIndex);
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
        QStandardItem* lastOne = remainingItems.first(); // Das ist ColName (0)

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

        debugItemInfo(lastOne, QString("refreshDuplicateStatus"));
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


QStringList ReviewPage::getUnresolvedDuplicateNames() {
    QMap<int, bool> groupHasSelection;
    QMap<int, QString> groupExampleName;

    // Start the recursion at the root element.
    checkFolderRecursive(wiz()->filesModel()->invisibleRootItem(), groupHasSelection, groupExampleName);

    QStringList unresolved;
    for (auto it = groupExampleName.begin(); it != groupExampleName.end(); ++it) {
        if (!groupHasSelection.value(it.key(), false)) {
            unresolved << it.value();
        }
    }
    return unresolved;
}


// =========================================================================
// --- RECURSIVE HELPERS & UTILS
// =========================================================================

void ReviewPage::discardItemFromModel(const QModelIndex &proxyIndex) {
    QModelIndex sourceIndex = wiz()->proxyModel_m->mapToSource(proxyIndex);
    QStandardItem *item = wiz()->filesModel()->itemFromIndex(sourceIndex);
    if (!item) return;

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

    updateUIStats();
}

void ReviewPage::deleteItemPhysically(const QModelIndex &proxyIndex) {
    // Retrieve source index (since we are using a proxy model)
    QModelIndex sourceIndex = wiz()->proxyModel_m->mapToSource(proxyIndex);
    QStandardItem *item = wiz()->filesModel()->itemFromIndex(sourceIndex);

    QString rawPath = item->data(RoleFilePath).toString();
    QString cleanPath = QDir::cleanPath(rawPath);

    if (!item) return;

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
    menu->addAction(tr("Select all"), this, [this]() { setAllCheckStates(Qt::Checked); });
    menu->addAction(tr("Clear selection"), this, [this]() { setAllCheckStates(Qt::Unchecked); });
}

// =============================================================================
// --- UTILS (Recursive Helpers)
// =============================================================================

void ReviewPage::recursiveCheckChilds(QStandardItem* parentItem, Qt::CheckState state) {
    for (int i = 0; i < parentItem->rowCount(); ++i) {
        QStandardItem* child = parentItem->child(i, ColName);
        if (child) {
            child->setCheckState(state);

            // Child status update
            int gId = child->data(RoleDuplicateId).toInt();
            int status = (state == Qt::Checked) ? StatusManaged :
                             (gId > 0 ? StatusDuplicate : StatusReady);

            child->setData(status, RoleFileStatus);

            debugItemInfo(child, QString("recursiveCheckChilds"));

            updateItemVisuals(child, status);

            // Go deeper if the child is a folder again
            if (child->hasChildren()) {
                recursiveCheckChilds(child, state);
            }
        }
    }
}

void ReviewPage::setCheckStateRecursive(QStandardItem* item, Qt::CheckState state) {
    if (!item) return;

    if (item->column() == ColName) {
        if (item->data(RoleFileStatus).toInt() != StatusDefect) {
            item->setCheckState(state);
        }
    } else {
        // If we end up here, we'll clear the column just to be safe.
        item->setData(QVariant(), Qt::CheckStateRole);
        item->setCheckable(false);

        debugItemInfo(item,  QString("setCheckStateRecursive"));
    }

    for (int row = 0; row < item->rowCount(); ++row) {
        QStandardItem* child = item->child(row, ColName);
        if (child) {
            setCheckStateRecursive(child, state);
        }
    }
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

void ReviewPage::checkFolderRecursive(QStandardItem* parentItem,
                                      QMap<int, bool>& groupHasSelection,
                                      QMap<int, QString>& groupExampleName) {
    if (!parentItem) return;

    for (int i = 0; i < parentItem->rowCount(); ++i) {
        QStandardItem* item = parentItem->child(i, ColName);
        if (!item) continue;

        int gId = item->data(RoleDuplicateId).toInt();

        if (gId > 0) {
            if (!groupExampleName.contains(gId)) {
                groupExampleName[gId] = item->text();
            }
            int status = item->data(RoleFileStatus).toInt();
            if (item->checkState() == Qt::Checked || status == StatusManaged) {
                groupHasSelection[gId] = true;
            }
        }

        // Dive deeper into the tree (folders have children)
        if (item->hasChildren()) {
            checkFolderRecursive(item, groupHasSelection, groupExampleName);
        }
    }
}

QStringList ReviewPage::getPathParts(const QString& fullPath) {
    QStringList* cached = pathPartsCache_m->object(fullPath);
    if (cached) return *cached;

    QStringList parts = fullPath.split('/', Qt::SkipEmptyParts);
    pathPartsCache_m->insert(fullPath, new QStringList(parts));
    return parts;
}

bool ReviewPage::isRootExpanded() {
    return treeView_m->isExpanded(treeView_m->model()->index(0, 0));
}

// =============================================================================
// --- Debugging
// =============================================================================

void ReviewPage::debugRoles(QModelIndex indexAtPos) {
    // 1. Mappe zum Source-Model
    QModelIndex srcIdx = wiz()->proxyModel_m->mapToSource(indexAtPos);
    if (!srcIdx.isValid()) {
        qDebug() << "index from model not valid";
        return;
    }

    QModelIndex realName  = srcIdx.siblingAtColumn(ColName);
    QModelIndex realSize = srcIdx.siblingAtColumn(ColSize);
    QModelIndex realStatus = srcIdx.siblingAtColumn(ColStatus);

    QStandardItem* itemRealName = wiz()->filesModel()->itemFromIndex(realName);
    
    /* if (!itemRealName) {
        qDebug() << "item ColName does not exist";
        qDebug() << "-------------------------------------------------------------------------";
    } else {
        qDebug() << "ColName / RoleFileInfo : " << itemRealName->data(RoleFileInfo);
        qDebug() << "ColName / RoleFileSizeRaw : " << itemRealName->data(RoleFileSizeRaw);
        qDebug() << "ColName / RoleFilePath : " << itemRealName->data(RoleFilePath);
        qDebug() << "ColName / RoleFileStatus : " << itemRealName->data(RoleFileStatus);
        qDebug() << "ColName / RoleFileHash : " << itemRealName->data(RoleFileHash);
        qDebug() << "ColName / RoleIsFolder : " << itemRealName->data(RoleIsFolder);
        qDebug() << "ColName / RoleItemType : " << itemRealName->data(RoleItemType);
        qDebug() << "-------------------------------------------------------------------------";
    } */

    QStandardItem* itemRealSize = wiz()->filesModel()->itemFromIndex(realSize);
    /* if (!itemRealSize) {
        qDebug() << "item ColSize does not exist";
        qDebug() << "-------------------------------------------------------------------------";
    } else {
        qDebug() << "ColSize / RoleFileInfo : " << itemRealSize->data(RoleFileInfo);
        qDebug() << "ColSize / RoleFileSizeRaw : " << itemRealSize->data(RoleFileSizeRaw);
        qDebug() << "ColSize / RoleFilePath : " << itemRealSize->data(RoleFilePath);
        qDebug() << "ColSize / RoleFileStatus : " << itemRealSize->data(RoleFileStatus);
        qDebug() << "ColSize / RoleFileHash : " << itemRealSize->data(RoleFileHash);
        qDebug() << "ColSize / RoleIsFolder : " << itemRealSize->data(RoleIsFolder);
        qDebug() << "ColSize / RoleItemType : " << itemRealSize->data(RoleItemType);
        qDebug() << "-------------------------------------------------------------------------";
    } */

    QStandardItem* itemRealStatus = wiz()->filesModel()->itemFromIndex(realStatus);
    /* if (!itemRealStatus) {
        qDebug() << "item ColStatus does not exist";
        qDebug() << "-------------------------------------------------------------------------";
    } else {
        qDebug() << "ColStatus / RoleFileInfo : " << itemRealStatus->data(RoleFileInfo);
        qDebug() << "ColStatus / RoleFileSizeRaw : " << itemRealStatus->data(RoleFileSizeRaw);
        qDebug() << "ColStatus / RoleFilePath : " << itemRealStatus->data(RoleFilePath);
        qDebug() << "ColStatus / RoleFileStatus : " << itemRealStatus->data(RoleFileStatus);
        qDebug() << "ColStatus / RoleFileHash : " << itemRealStatus->data(RoleFileHash);
        qDebug() << "ColStatus / RoleIsFolder : " << itemRealStatus->data(RoleIsFolder);
        qDebug() << "ColStatus / RoleItemType : " << itemRealStatus->data(RoleItemType);
        qDebug() << "-------------------------------------------------------------------------";
    } */
}

void ReviewPage::debugItemInfo(QStandardItem* item, QString fromFunc) {
    if (item->column() != ColName && item->data(Qt::CheckStateRole).isValid()) {
        qWarning() << "ERROR: in (" << fromFunc << "): in column " << item->column()
        << "has a checkbox! Path: " << item->data(RoleFilePath).toString();
    }
}
