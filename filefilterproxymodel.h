#ifndef FILEFILTERPROXYMODEL_H
#define FILEFILTERPROXYMODEL_H

#include "sonarstructs.h"
#include "reviewstruct.h"

#include <QSortFilterProxyModel>
#include <QFileInfo>
#include <QPointer>

class SetupWizard;
class ReviewPage;

class FileFilterProxyModel : public QSortFilterProxyModel {
    Q_OBJECT
    friend class FileFilterProxyModelTest;
public:
    enum FilterMode { ModeAll, ModeErrors, ModeDuplicates };

    explicit FileFilterProxyModel(QObject *parent = nullptr);

    void setFilterMode(FilterMode mode);

    [[nodiscard]] ReviewStats calculateVisibleStats(const QModelIndex &parent = QModelIndex());

protected:
    [[nodiscard]] bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
    [[nodiscard]] bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;

    void collectPathsRecursive(const QModelIndex &parent, QStringList &paths, bool onlyChecked) const;
    [[nodiscard]] bool hasStatusChild(const QModelIndex &parent, FileStatus targetStatus) const;

private:
    FilterMode currentMode_m;
    QPointer<SetupWizard> wizard_m;
};


#endif // FILEFILTERPROXYMODEL_H
