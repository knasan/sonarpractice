#ifndef SETUPWIZARD_H
#define SETUPWIZARD_H

#include <QWizard>
#include <QStandardItemModel>

class FileManager;
class FileScanner;
class FileFilterProxyModel;
class WelcomePage;
class FilterPage;
class ReviewPage;
class MappingPage;

class SetupWizard : public QWizard {
    Q_OBJECT

public:
    // Definition der Seiten-IDs für Typsicherheit
    enum PageId {
        Page_Welcome = 0,
        Page_Filter = 1,
        Page_Review = 2,
        Page_Mapping = 3
    };

    explicit SetupWizard(QWidget *parent = nullptr);
    ~SetupWizard() override;

    [[nodiscard]] QStringList activeFilters() const { return activeFilters_m; }
    void setActiveFilters(const QStringList &filters) { activeFilters_m = filters; }

    [[nodiscard]] QStringList sourcePaths() const { return sourcePaths_m; }
    void setSourcePaths(const QStringList &paths) { sourcePaths_m = paths; }

    FileScanner* fileScanner() const { return fileScanner_m; } // scan discs etc.

    // Access to the data model (source)
    [[nodiscard]] QStandardItemModel* filesModel() const { return filesModel_m; }

    // Accessing the proxy model (filter/search)
    [[nodiscard]] FileFilterProxyModel* proxyModel() const { return proxyModel_m; }

    // Access to the manager
    [[nodiscard]] FileManager* fileManager() const { return fileManager_m; }

    void setScanResults(const QStringList &results) { scanResults_m = results; }
    [[nodiscard]] QStringList scanResults() const { return scanResults_m; }

    void prepareScannerWithDatabaseData();

public slots:
    void restartApp();

private:
    void setupUiLayout();
    void setupModels();
    void createPages();
    void setupConnections();
    void setProxyModelHeader();

    QStandardItemModel* filesModel_m{nullptr};

    FileManager* fileManager_m{nullptr};
    FileScanner* fileScanner_m{nullptr};

    QThread* scannerThread_m{nullptr};

    FileFilterProxyModel* proxyModel_m{nullptr};

    // Pointers to the pages for direct access
    WelcomePage* welcomePage_m{nullptr};
    FilterPage* filterPage_m{nullptr};
    ReviewPage* reviewPage_m{nullptr};
    MappingPage* mappingPage_m{nullptr};

    QStringList sourcePaths_m;
    QStringList activeFilters_m;

    QStringList scanResults_m;
};

#endif
