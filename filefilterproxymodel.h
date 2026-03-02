#ifndef FILEFILTERPROXYMODEL_H
#define FILEFILTERPROXYMODEL_H

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
    [[nodiscard]] ReviewStats calculateCurrentStats() const;

protected:
    [[nodiscard]] bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
    [[nodiscard]] bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;

    void updateStatsRecursive(const QModelIndex &parent, ReviewStats &stats) const;

private:
    FilterMode currentMode_m;
    QPointer<SetupWizard> wizard_m;
};


#endif // FILEFILTERPROXYMODEL_H
