#include "filefilterproxymodel.h"
#include <QDir>
#include <QDebug>

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
    QModelIndex index = sourceModel()->index(sourceRow, ColName, sourceParent);
    if (!filterRegularExpression().pattern().isEmpty()) {
        return QSortFilterProxyModel::filterAcceptsRow(sourceRow, sourceParent);
    }

    if (currentMode_m == ModeDuplicates) {
        int status = index.data(RoleFileStatus).toInt();
        return (status == StatusDuplicate);
    }

    if (currentMode_m == ModeErrors) {
        int status = index.data(RoleFileStatus).toInt();
        return (status == StatusDefect);
    }

    return true;
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

/**
* @brief Recursively searches all child elements for a specific status.
* Performs a depth-first search through the model tree to check
* if the specified 'targetStatus' exists in at least one descendant.
* @param parent The starting point (index) for the search in the source model.
* @param targetStatus The file status to be searched for (e.g., StatusDuplicate or StatusDefect).
* @return true if a matching element is found in the subtree, otherwise false.
*/
bool FileFilterProxyModel::hasStatusChild(const QModelIndex &parent, FileStatus targetStatus) const {
    QAbstractItemModel* src = sourceModel();
    int rowCount = src->rowCount(parent);

    for (int r = 0; r < rowCount; ++r) {
        QModelIndex childIdx = src->index(r, 0, parent);

        if (src->data(childIdx, RoleFileStatus).toInt() == targetStatus) {
            return true;
        }

        if (src->hasChildren(childIdx)) {
            if (hasStatusChild(childIdx, targetStatus)) {
                return true;
            }
        }
    }
    return false;
}

/**
* @brief Recursively collects file paths from the model tree.
* Traverses the tree in depth-first search and adds found paths to the 'paths' list.
* - For folders (elements with children), the path is further descended.
* - For files (leaf elements), the path is added if it is valid.
* @param parent The starting point of the search in the source model.
* @param paths Reference to a string list in which the paths are collected.
* @param onlyChecked If true, only elements whose CheckState is 'Qt::Checked' are considered.
*/
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

/**
* @brief Calculates statistics for all files currently visible in the view.
* Recursively traverses the (filtered) model tree and aggregates:
* - Number of duplicates and defects.
* - Total number of files and their file sizes.
* - Number of checked files and the amount of storage space saved.
* Because the function uses the 'rowCount' and 'index' of the proxy model,
* hidden rows are automatically ignored.
* @param parent The starting index for the statistical analysis.
* @return A ReviewStats object containing the accumulated values.
*/
ReviewStats FileFilterProxyModel::calculateVisibleStats(const QModelIndex &parent) {
    ReviewStats stats;
    int rows = rowCount(parent);

    for (int i = 0; i < rows; ++i) {
        QModelIndex proxyIndex = index(i, ColName, parent);

        if (hasChildren(proxyIndex)) {
            stats.add(calculateVisibleStats(proxyIndex));
            continue;
        }

        int status = proxyIndex.data(RoleFileStatus).toInt();
        qint64 size = data(proxyIndex, RoleFileSizeRaw).toLongLong();

        if (status == StatusDuplicate) {
            stats.duplicates_m++;
        } else if (status == StatusDefect) {
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
