#include "filefilterproxymodel.h"
#include <QDir>
#include <QDebug>

FileFilterProxyModel::FileFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent) {
    setRecursiveFilteringEnabled(true);
}

/**
 * @brief FileFilterProxyModel::setFilterMode
 * @param mode
 */
void FileFilterProxyModel::setFilterMode(FilterMode mode) {
    // 1. Filter-Änderung ankündigen
    beginFilterChange();

    // 3. Den neuen Modus setzen
    currentMode_m = mode;

    // 4. Filter-Änderung abschließen -> Das löst filterAcceptsRow für alle Zeilen aus
    endFilterChange();
}

/**
 * @brief FileFilterProxyModel::filterAcceptsRow
 * @param source_row
 * @param source_parent
 * @return
 */
bool FileFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const {
    // 1. Zugriff auf das Item
    QModelIndex index = sourceModel()->index(sourceRow, ColName, sourceParent);

    // 2. TEXT-FILTER (Deine Search Bar)
    // Wenn ein Suchtext eingegeben wurde, nutzen wir die Standard-Logik von Qt
    if (!filterRegularExpression().pattern().isEmpty()) {
        return QSortFilterProxyModel::filterAcceptsRow(sourceRow, sourceParent);
    }

    // 3. STATUS-FILTER (Optional)
    // Wenn du z.B. einen Schalter hättest "Nur Duplikate anzeigen"
    if (currentMode_m == ModeDuplicates) {
        int status = index.data(RoleFileStatus).toInt();
        return (status == StatusDuplicate);
    }

    if (currentMode_m == ModeErrors) {
        int status = index.data(RoleFileStatus).toInt();
        return (status == StatusDefect);
    }

    return true; // Ansonsten: Alles anzeigen
}

/**
 * @brief FileFilterProxyModel::lessThan
 * @param source_left
 * @param source_right
 * @return
 */
bool FileFilterProxyModel::lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const {
    QModelIndex leftDataIdx = source_left.siblingAtColumn(ColName);
    QModelIndex rightDataIdx = source_right.siblingAtColumn(ColName);

    int column = source_left.column();

    if (column == ColSize) { // Nutze dein Enum oder die Zahl 1
        qint64 leftSize = sourceModel()->data(leftDataIdx, RoleFileSizeRaw).toLongLong();
        qint64 rightSize = sourceModel()->data(rightDataIdx, RoleFileSizeRaw).toLongLong();
        return leftSize < rightSize;
    }
    else if (column == ColStatus) { // Nutze dein Enum oder die Zahl 2
        int leftStat = sourceModel()->data(leftDataIdx, RoleFileStatus).toInt();
        int rightStat = sourceModel()->data(rightDataIdx, RoleFileStatus).toInt();
        return leftStat < rightStat;
    }
    return QSortFilterProxyModel::lessThan(source_left, source_right);
}

bool FileFilterProxyModel::hasStatusChild(const QModelIndex &parent, FileStatus targetStatus) const {
    QAbstractItemModel* src = sourceModel();
    int rowCount = src->rowCount(parent);

    for (int r = 0; r < rowCount; ++r) {
        QModelIndex childIdx = src->index(r, 0, parent);

        // 1. Prüfe den Status des direkten Kindes
        if (src->data(childIdx, RoleFileStatus).toInt() == targetStatus) {
            return true;
        }

        // 2. Wenn das Kind ein Ordner ist, geh tiefer (Rekursion)
        if (src->hasChildren(childIdx)) {
            if (hasStatusChild(childIdx, targetStatus)) {
                return true;
            }
        }
    }
    return false;
}

// QStringList FileFilterProxyModel::getAllPaths(bool onlyChecked) const {
//     QStringList paths;
//     QAbstractItemModel* src = sourceModel();
//     if (!src) return paths;

//     collectPathsRecursive(QModelIndex(), paths, onlyChecked);
//     return paths;
// }

void FileFilterProxyModel::collectPathsRecursive(const QModelIndex &parent, QStringList &paths, bool onlyChecked) const {
    QAbstractItemModel* src = sourceModel();
    for (int r = 0; r < src->rowCount(parent); ++r) {
        QModelIndex idx = src->index(r, 0, parent);
        QString filePath = src->data(idx, RoleFilePath).toString();

        if (src->hasChildren(idx)) {
            collectPathsRecursive(idx, paths, onlyChecked);
        } else {
            if (!onlyChecked || src->data(idx, Qt::CheckStateRole).toInt() == Qt::Checked) {
                if (!filePath.isEmpty()) paths << filePath;
            }
        }
    }
}

ReviewStats FileFilterProxyModel::calculateVisibleStats(const QModelIndex &parent) {
    ReviewStats stats;
    int rows = rowCount(parent);

    for (int i = 0; i < rows; ++i) {
        // Wir erzwingen den Index auf Spalte 0 (wo unsere Metadaten liegen)
        QModelIndex proxyIndex = index(i, ColName, parent);

        // Jetzt prüfen wir, ob es ein Ordner ist
        if (hasChildren(proxyIndex)) {
            stats.add(calculateVisibleStats(proxyIndex));
            continue; // WICHTIG: Ordner nicht als Datei zählen
        }

        // Jetzt die Daten explizit aus Spalte 0 ziehen
        // qDebug() << "[FileFilterProxyModel] calculateVisibleStats proxyIndex RoleFileStatus: " << proxyIndex.data(RoleFileStatus);
        int status = proxyIndex.data(RoleFileStatus).toInt();

        // Ab hier sind wir sicher bei einer Datei
        // int status = data(proxyIndex, RoleFileStatus).toInt();
        qint64 size = data(proxyIndex, RoleFileSizeRaw).toLongLong();

        // Debug Log um zu sehen was ankommt
        // qDebug() << "[FileFilterProxyModel] calculateVisibleStats / Datei gefunden:" << data(proxyIndex, Qt::DisplayRole).toString() << "Status:" << status;
        // qDebug() << "[FileFilterProxyModel] calculateVisibleStats / Status: " << status;
        // qDebug() << "[FileFilterProxyModel] calculateVisibleStats / size: " << size;

        if (status == StatusDuplicate) {
            // qDebug() << "[FileFilterProxyModel] calculateVisibleStats found StatusDuplicate and stats.duplicates_m++";
            stats.duplicates_m++;
        } else if (status == StatusDefect) {
            // qDebug() << "[FileFilterProxyModel] calculateVisibleStats found StatusDefect and stats.defects_m++";
            stats.defects_m++;
        }

        stats.totalFiles_m++;
        stats.totalBytes_m += size;

        if (data(proxyIndex, Qt::CheckStateRole).toInt() == Qt::Checked) {
            stats.selectedFiles_m++;
            stats.savedBytes_m += size;
        }
    }
    return stats;
}
