#include "filefilterproxymodel.h"
#include "sonarstructs.h"

#include <QDir>

FileFilterProxyModel::FileFilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent) {
    setRecursiveFilteringEnabled(true);
}

/* Sets the filter mode for the proxy model.
 *
 * This function updates the current filter mode and triggers a filter change cycle
 * to refresh the filtered data accordingly.
 * 
 * \param mode The FilterMode to set as the current filtering mode  
 * \see FilterMode
 * \see beginFilterChange()
 * \see endFilterChange()
 */
void FileFilterProxyModel::setFilterMode(FilterMode mode) {
    beginFilterChange();
    currentMode_m = mode;
    endFilterChange();
}

/**
* @brief Determines whether a row is displayed based on the current mode and filter rules.
* * Filtering is performed in priority levels:
* 1. If a search term (RegEx) is specified, Qt's standard filter logic is used.
* 2. In 'Duplicates' mode, only rows with the status 'StatusDuplicate' are displayed.
* 3. In 'Errors' mode, only rows with the status 'StatusDefect' are displayed.
* 4. Otherwise (Default/All), all rows are accepted.
* @param sourceRow The row number in the source model.
* @param sourceParent The parent index in the source model.
* @return true if the row meets the criteria, otherwise false.
*/
bool FileFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const {
    QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
    if (!filterRegularExpression().pattern().isEmpty()) {
        return QSortFilterProxyModel::filterAcceptsRow(sourceRow, sourceParent);
    }

    int status = sourceModel()->data(index, RoleFileStatus).toInt();

    // 1. Dateien, die bereits in der DB sind, immer ausblenden
    if (status == StatusAlreadyInDatabase) {
        return false;
    }

    // 2. Deine bestehende Filter-Logik nach Modus
    if (currentMode_m == ModeDuplicates) {
        return (status == ModeDuplicates);
    } else if (currentMode_m == ModeErrors) {
        return (status == ModeErrors);
    }

    // 3. Suche/RegExp (falls gesetzt)
    return QSortFilterProxyModel::filterAcceptsRow(sourceRow, sourceParent);
}

/**
* @brief Compares two entries for sorting based on the column.
* This method implements specialized sorting logic:
* - Column 'ColSize': Numerical comparison of file size (raw data).
* - Column 'ColStatus': Numerical comparison of the file status enum.
* - All other columns: Uses the standard sorting (e.g., alphabetical).
* @param source_left Index of the leftmost element in the source model.
* @param source_right Index of the rightmost element in the source model.
* @return true if the leftmost element is smaller than the rightmost element, otherwise false.
*/
bool FileFilterProxyModel::lessThan(const QModelIndex &source_left, const QModelIndex &source_right) const {
    QModelIndex leftDataIdx = source_left.siblingAtColumn(ColName);
    QModelIndex rightDataIdx = source_right.siblingAtColumn(ColName);

    int column = source_left.column();

    if (column == ColSize) {
        qint64 leftSize = sourceModel()->data(leftDataIdx, RoleFileSizeRaw).toLongLong();
        qint64 rightSize = sourceModel()->data(rightDataIdx, RoleFileSizeRaw).toLongLong();
        return leftSize < rightSize;
    }
    else if (column == ColStatus) {
        int leftStat = sourceModel()->data(leftDataIdx, RoleFileStatus).toInt();
        int rightStat = sourceModel()->data(rightDataIdx, RoleFileStatus).toInt();
        return leftStat < rightStat;
    }
    return QSortFilterProxyModel::lessThan(source_left, source_right);
}

ReviewStats FileFilterProxyModel::calculateCurrentStats() const {
    ReviewStats stats;
    updateStatsRecursive(QModelIndex(), stats);
    return stats;
}

void FileFilterProxyModel::updateStatsRecursive(const QModelIndex &parent, ReviewStats &stats) const {
    QAbstractItemModel* src = sourceModel();
    for (int r = 0; r < src->rowCount(parent); ++r) {
        QModelIndex idx = src->index(r, 0, parent);
        if (src->hasChildren(idx)) {
            updateStatsRecursive(idx, stats);
        } else {
            qint64 size = src->data(idx, RoleFileSizeRaw).toLongLong(); // qint64 and long long are the same on almost all systems - best compromise
            int status = src->data(idx, RoleFileStatus).toInt();
            bool checked = (src->data(idx, Qt::CheckStateRole).toInt() == Qt::Checked);

            stats.addFile(size, (status == StatusDuplicate), (status == StatusDefect));
            if (checked) {
                stats.selectedFiles++;
                stats.selectedBytes += size;
            }
        }
    }
}
