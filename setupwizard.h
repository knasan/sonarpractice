#ifndef SETUPWIZARD_H
#define SETUPWIZARD_H

#include "filemanager.h"
#include <QWizard>
#include <QFileInfoList>
#include <QLabel>
#include <QStandardItemModel>
#include "filefilterproxymodel.h"

class WelcomePage;
class FilterPage;
class ProcessingPage;
class ReviewPage;
class MappingPage;
class FileManager;

class SetupWizard : public QWizard
{
    Q_OBJECT
    friend class ReviewPageTest;
public:
    // Definition of the IDs (so each page knows who it is)
    enum PageId {
        Page_Welcome,
        Page_Filter,
        Page_Processing,
        Page_Review,
        Page_Mapping
    };

    // Helper functions to access other pages from within the pages
    [[nodiscard]] ReviewPage* reviewPage() const { return reviewPage_m; }

    explicit SetupWizard(QWidget *parent = nullptr);
    ~SetupWizard();

    QStringList activeFilters_m; // Central storage for the endings
    QStringList sourcePaths_m;   // Central storage for search folders

    QStringList finalImportList_m; // Central storage for takeover list
    QStringList realDuplicates_m;  // Central storage for duplicates
    long long totalSizeBytes_m;    // Central storage for size

    FileFilterProxyModel *proxyModel_m;

    [[nodiscard]] ReviewStats getFinalStats() const
    {
        if (proxyModel_m) {
            return proxyModel_m->calculateVisibleStats();
        }
        qDebug() << "[SetupWizard]: getFinalStatus.";
        return ReviewStats();
    }

    // Access to the central model
    [[nodiscard]] FileManager* fileManager() { return fileManager_m; }
    [[nodiscard]] QStandardItemModel* filesModel() const { return filesModel_m; }

    void setButtonsState(bool backEnabled, bool nextEnabled, bool cancelEnabled);

    [[nodiscard]] bool isDuplicateHash(const QString &hash) const {
        return realDuplicates_m.contains(hash);
    }

    [[nodiscard]] QStringList getSelectedPaths() const;
    [[nodiscard]] QStringList getFileFilters() const;

    [[nodiscard]] QList<QList<QStandardItem*>> takeFromIgnore(const QString &path);

    void setProxyModelHeader();

private:
    WelcomePage*    welcomePage_m;
    FilterPage*     filterPage_m;
    ProcessingPage* processingPage_m;
    ReviewPage*     reviewPage_m;
    MappingPage*    mappingPage_m;

    QStandardItemModel* filesModel_m;
    FileManager*   fileManager_m;
};

#endif // SETUPWIZARD_H
