#include "filemanager.h"
#include "fileutils.h"

#include <QStandardItemModel>
#include <QApplication>
#include <QFileInfo>
#include <QStyle>

void FileManager::printModelStructure(QStandardItem *item, int level) const {
    if (!item) return;

    QString indent(level * 2, ' ');
    qDebug() << indent << "Item:" << (item->data(RoleFilePath).toString().isEmpty() ? "Root" : item->data(RoleFilePath).toString())
             << "Type:" << item->data(RoleItemType).toInt()
             << "Status:" << item->data(RoleFileStatus).toInt();

    for (int i = 0; i < item->rowCount(); ++i) {
        QStandardItem *child = item->child(i);
        printModelStructure(child, level + 1);
    }
}

QColor FileManager::getStatusColor(int status) {
    switch (status) {
        case StatusDefect:    return QColor(255, 100, 100);  // Rot
        case StatusReady:     return QColor(100, 255, 100);  // Ready-Green
        case StatusDuplicate: return QColor(255, 140, 0);    // Orange
        case StatusManaged:   return QColor(80, 180, 120);
        case StatusReject:    return QColor(80, 170, 80);
        // case StatusIgnored:   return QColor(80, 80, 80);     // Dark grey
        default:              return QColor(170, 170, 170);  // Grau
    }
}

QString FileManager::getStatusText(int status) {
    static const QString txtReady     = tr("Ready");
    static const QString txtDefect    = tr("Defect");
    static const QString txtDuplicate = tr("Duplicates");
    static const QString txtManaged   = tr("Selected");
    static const QString txtUnknown   = tr("Unknown");
    static const QString txtReject    = tr("Rejected");
    static const QString txtFiles     = tr("Files");

    switch (status) {
    case StatusReady:     return txtReady;
    case StatusDefect:    return txtDefect;
    case StatusDuplicate: return txtDuplicate;
    case StatusManaged:   return txtManaged;
    case StatusFiles:     return txtFiles;
    case StatusReject:    return txtReject;
    default:              return txtUnknown;
    }
}

QStandardItem* FileManager::getFolderItem(const QString &path) const {
    if (path.isEmpty()) return model_m->invisibleRootItem(); // If the path is empty, it is the root level.

    // qDebug() << "[FileManager] getFolderItem path:" << path ;
    QString clean = QDir::cleanPath(path);
    // qDebug() << "[FileManager] getFolderItem cleanPath: " << clean ;
    return pathCache_m.value(path, nullptr);
}

void FileManager::addBatchesToModel(const QList<ScanBatch> &batches) {
    if (batches.isEmpty()) return;

    QStandardItem* root = model_m->invisibleRootItem();

    for (const ScanBatch &batch : batches) {
        QFileInfo fileInfo(batch.info);
        QString fullFolderPath = QDir::cleanPath(fileInfo.absolutePath());
        QStringList pathParts = fullFolderPath.split('/', Qt::SkipEmptyParts);

        QStandardItem* currentParent = root;
        QString currentAccumulatedPath;

        // Folder hierarchy with cache support
        for (const QString &part : std::as_const(pathParts)) {
            // Build the path step by step for the cache key
            if (currentAccumulatedPath.isEmpty() && fileInfo.absolutePath().startsWith("/")) {
                // Linux/Unix Root
                currentAccumulatedPath = QDir(currentAccumulatedPath).filePath(part);
            } else if (currentAccumulatedPath.isEmpty()) {
                if (part.contains(":") && part.length() == 2) {
                    currentAccumulatedPath = part + "/";
                } else {
                    currentAccumulatedPath = part;
                }
            } else {
                currentAccumulatedPath = QDir::cleanPath(currentAccumulatedPath + "/" + part);
            }

            QString cleanKey = QDir::cleanPath(currentAccumulatedPath);

            // Search in cache (faster than the for loop!)
            if (pathCache_m.contains(cleanKey)) {
                currentParent = pathCache_m.value(cleanKey);
            } else {
                QStandardItem* folderItem = new QStandardItem(part);
                folderItem->setData(cleanKey, RoleFilePath);
                folderItem->setData(ColFolderType, RoleItemType);
                folderItem->setEditable(false);

                // ALWAYS define visible columns for the row.
                QList<QStandardItem*> folderRow;
                folderRow << folderItem                  // Spalte 0: Name/Tree
                          << new QStandardItem("")       // Spalte 1: Size
                          << new QStandardItem("")       // Spalte 2: Status
                          << new QStandardItem("");      // Spalte 3: ID

                currentParent->appendRow(folderRow);

                // Add to cache (We only cache the first item of the row)
                pathCache_m.insert(cleanKey, folderItem);
                currentParent = folderItem;
            }
        }

        // --- ADD FILE ---
        QList<QStandardItem*> row;
        QStandardItem *nameItem = new QStandardItem(fileInfo.fileName());
        nameItem->setData(batch.info.absoluteFilePath(), RoleFilePath);
        nameItem->setData(batch.status, RoleFileStatus);
        nameItem->setData(batch.info.size(), RoleFileSizeRaw);
        nameItem->setData(batch.hash, RoleFileHash);
        nameItem->setData(batch.groupId, RoleDuplicateId);
        nameItem->setData(ColFileType, RoleItemType);
        nameItem->setCheckable(true);

        if (batch.info.size() > 0) {
            nameItem->setCheckState(Qt::Checked);
            nameItem->setToolTip(batch.hash);
            nameItem->setEnabled(true);
        } else {
            // 0-byte files: Uncheck and disable
            nameItem->setCheckState(Qt::Unchecked);
            nameItem->setEnabled(false);
            static QString toolTipStr = tr("0-byte files cannot be imported.");
            nameItem->setToolTip(toolTipStr);
        }

        QStandardItem *sizeItem = new QStandardItem(FileUtils::formatBytes(batch.info.size()));
        QStandardItem *statusItem = new QStandardItem(getStatusText(batch.status));
        QStandardItem *groupIdItem = new QStandardItem(QString::number(batch.groupId));

        row << nameItem << sizeItem << statusItem << groupIdItem;
        currentParent->appendRow(row);

        QString fileKey = QDir::cleanPath(batch.info.absoluteFilePath());
        pathCache_m.insert(fileKey, nameItem);
    }
}

void FileManager::clearCaches() {
    pathCache_m.clear();
    groupHeaderCache_m.clear();
}

// INFO: Here you can also set the colors for duplicates, defects, etc.
void FileManager::updateStatuses(const QList<ScanBatch> &allBatches) {
    qDebug() << "[FileManager] updateStatuses START";
    model_m->blockSignals(true);

    duplicateGroups_m.clear();

    for (const ScanBatch &batch : allBatches) {
        QString pathKey = QDir::cleanPath(batch.info.absoluteFilePath());

        if (pathCache_m.contains(pathKey)) {
            QStandardItem* nameItem = pathCache_m.value(pathKey);

            QColor baseColor = getStatusColor(batch.status);
            baseColor.setAlpha(10);

            // Set status to 2 (Duplicate)
            // This is where the color of duplicates is set for the first time.
            if (batch.status == StatusDuplicate) {
                // qDebug() << "[FileManager] updateStatuses pathKey: " << pathKey;
                duplicateGroups_m[batch.groupId].append(pathKey);
                nameItem->setData(batch.status, RoleFileStatus);
                nameItem->setData(batch.groupId, RoleDuplicateId);
                nameItem->setCheckState(Qt::Unchecked);

                // Change text in column 2 to "Duplicate".
                QStandardItem* parent = nameItem->parent() ? nameItem->parent() : model_m->invisibleRootItem();
                QStandardItem* statusCell = parent->child(nameItem->row(), StatusDuplicate);

                if (statusCell) {
                    statusCell->setText(getStatusText(batch.status));
                }
            }

            // Add color for defects and duplicates - ready remains transparent.
            if (batch.status  == StatusDefect || batch.status == StatusDuplicate || batch.status == StatusManaged) {
                // qDebug() << "updateStatuses set color for status: " << getStatusText(batch.status);
                nameItem->setBackground(baseColor);
            }
        }
    }
    model_m->blockSignals(false);
}
