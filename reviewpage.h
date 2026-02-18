#ifndef REVIEWPAGE_H
#define REVIEWPAGE_H

#include "basepage.h"
#include "filescanner.h"

// Forward Declarations for faster compilation times
class QCheckBox;
class QComboBox;
class QLineEdit;
class QModelIndex;
class QProgressBar;
class QSortFilterProxyModel;
class QTreeView;
class QLabel;
class QStandardItem;
class QSpinBox;

class ReviewPage : public BasePage {
    Q_OBJECT

public:
    // Lifecycle (Constructor/Destructor)
    explicit ReviewPage(QWidget *parent = nullptr);
    ~ReviewPage();

    // Reimplementations of base classes (interfaces)
    void initializePage() override;
    [[nodiscard]] bool validatePage() override;

signals:
    void requestScanStart(const QStringList &paths, const QStringList &filters, SetupWizard* wizard);

private slots:
    void showContextMenu(const QPoint &pos);
    void showSummaryContextMenu(const QPoint &pos);
    void showTreeContextMenu(const QPoint &pos, const QModelIndex &proxyIndex);
    void handleItemChanged(QStandardItem *item);
    void setAllCheckStates(Qt::CheckState state);

    bool eventFilter(QObject *obj, QEvent *event) override;

private:

    // Initialization & UI Update
    void sideConnections();
    void updateUIStats();
    void updateItemVisuals(QStandardItem* nameItem, int status);

    void cleanupScanResources();

    // Core logic (calculations & data processing)
    [[nodiscard]] ReviewStats calculateGlobalStats();
    void countItemsRecursive(QStandardItem* parent, ReviewStats &stats) const;
    void collectHashesRecursive(QStandardItem* parent, QStringList &hashes);
    [[nodiscard]] QStringList getUnrecognizedFiles(const QString &folderPath);
    [[nodiscard]] bool finishDialog();
    void onItemChanged(QStandardItem *item);

    // Duplicate handling (Thematically grouped)
    void addDuplicateSectionToMenu(QMenu *menu, const QModelIndex &nameIndex, const QString &currentHash, const QString &currentPath);
    void addJumpToDuplicateActions(QMenu *jumpMenu, const QString &currentHash, const QString &currentPath);
    [[nodiscard]] QModelIndexList findDuplicatePartners(const QString &hash);
    void jumpToDuplicate(const QModelIndex &sourceIndex);
    void refreshDuplicateStatus(const QString &hash);
    void collectItemsByHashRecursive(QStandardItem* parent, const QString &hash, QList<QStandardItem*> &result);
    [[nodiscard]] QStringList getUnresolvedDuplicateNames();

    // File & Model Operations
    void discardItemFromModel(const QModelIndex &proxyIndex);
    void deleteItemPhysically(const QModelIndex &proxyIndex);
    void addFileActionsSectionToMenu(QMenu *menu, const QModelIndex &proxyIndex, const QString &currentPath);
    void addStandardActionsToMenu(QMenu *menu);

    // Auxiliary functions (Recursive / Utility)
    void recursiveCheckChilds(QStandardItem* parent, Qt::CheckState state);
    void setCheckStateRecursive(QStandardItem* item, Qt::CheckState state);
    void searchGroupRecursive(QStandardItem* parent, int groupId, QList<QStandardItem*>& results);
    void checkFolderRecursive(QStandardItem* parentItem,
                              QMap<int, bool>& groupHasSelection,
                              QMap<int, QString>& groupExampleName);
    [[nodiscard]] QStringList getPathParts(const QString& fullPath);
    [[nodiscard]] bool isRootExpanded();

    // Debugging
    void debugRoles(QModelIndex indexAtPos);
    void debugItemInfo(QStandardItem* item, QString fromFunc);

    // Member variables
    QLineEdit* searchLineEdit_m;
    QRadioButton* radioAll_m;
    QRadioButton* radioErrors_m;
    QRadioButton* radioDup_m;
    QCheckBox* collabsTree_m;
    QCheckBox* expertModeCheck_m;
    QTreeView* treeView_m;
    QLabel* summaryLabel_m;
    QLabel* statusLabel_m;

    QCache<QString, QStringList>* pathPartsCache_m;
    int totalDuplicatesCount_m{0};
    int totalDefectsCount_m{0};
    bool isConnectionsEstablished_m{false};

    bool pageInitialized{false};
    bool scanInProgress_m{false};
    QThread *scanThread_m = nullptr;
    FileScanner *worker_m = nullptr;

};

#endif // REVIEWPAGE_H
