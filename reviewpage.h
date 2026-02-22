#ifndef REVIEWPAGE_H
#define REVIEWPAGE_H

#include "basepage.h"
#include <QProgressBar>

// Forward Declarations (beschleunigt die Kompilierung)
class QTreeView;
class QLabel;
class QLineEdit;
class QRadioButton;
class QCheckBox;
class QStandardItem;
class QModelIndex;
class SetupWizard;

class ReviewPage : public BasePage {
    Q_OBJECT

public:
    explicit ReviewPage(QWidget *parent = nullptr);
    ~ReviewPage() override;
    void initializePage() override;
    void cleanupPage() override;
    [[nodiscard]] bool isComplete() const override;

private slots:
    void showContextMenu(const QPoint &pos);
    void handleItemChanged(QStandardItem *item);
    void onFilterChanged();

    void showSummaryContextMenu(const QPoint &pos);
    void showTreeContextMenu(const QPoint &pos, const QModelIndex &proxyIndex);
    void addDuplicateSectionToMenu(QMenu *menu, const QModelIndex &nameIndex, const QString &currentHash, const QString &currentPath);

    void addJumpToDuplicateActions(QMenu *jumpMenu, const QString &currentHash, const QString &currentPath);
    [[nodiscard]] QModelIndexList findDuplicatePartners(const QString &hash);
    void jumpToDuplicate(const QModelIndex &sourceIndex);
    void setCheckStateRecursive(QStandardItem* item, Qt::CheckState state);

    void onItemChanged(QStandardItem *item);
    bool eventFilter(QObject *obj, QEvent *event) override;

    void setAllCheckStates(Qt::CheckState state);

    void applySmartCheck();

private:
    void setupLayout();
    void setupConnections();
    void updateUIStats();

    void addFileActionsSectionToMenu(QMenu *menu, const QModelIndex &proxyIndex, const QString &currentPath);
    void addStandardActionsToMenu(QMenu *menu);

    void discardItemFromModel(const QModelIndex &proxyIndex);
    void collectHashesRecursive(QStandardItem* parent, QStringList &hashes);
    void refreshDuplicateStatus(const QString &hash);

    void collectItemsByHashRecursive(QStandardItem* parent, const QString &hash, QList<QStandardItem*> &result);
    void deleteItemPhysically(const QModelIndex &proxyIndex);
    [[nodiscard]] QStringList getUnrecognizedFiles(const QString &folderPath);

    QTreeView* treeView_m{nullptr};
    QLabel* summaryLabel_m{nullptr};
    QLabel* statusLabel_m{nullptr};
    QLineEdit* searchLineEdit_m{nullptr};

    QProgressBar* progressBar_m{nullptr};

    // Filter-Buttons
    QRadioButton* radioAll_m{nullptr};
    QRadioButton* radioErrors_m{nullptr};
    QRadioButton* radioDuplicates_m{nullptr};

    QCheckBox* collabsTree_m{nullptr};
    QCheckBox* expertModeCheck_m{nullptr};
};

#endif // REVIEWPAGE_H
