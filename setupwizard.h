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
    // Definition der IDs (so weiß jede Seite, wer sie ist)
    enum PageId {
        Page_Welcome,
        Page_Filter,
        Page_Processing,
        Page_Review,
        Page_Mapping
    };

    // Hilfsfunktionen, um von den Seiten aus auf andere Seiten zuzugreifen
    [[nodiscard]] ReviewPage* reviewPage() const { return reviewPage_m; }

    explicit SetupWizard(QWidget *parent = nullptr);
    ~SetupWizard();

    QStringList activeFilters_m;      // Zentraler Speicher für die Endungen
    QStringList sourcePaths_m;        // Zentraler Speicher für die Suchordner

    QStringList finalImportList_m;    // Zentraler Speicher für Übernahmeliste
    QStringList realDuplicates_m;     // Zentraler Speicher für Duplikate
    long long totalSizeBytes_m;       // Zentraler Speicher für Größe

    FileFilterProxyModel *proxyModel_m;

    [[nodiscard]] ReviewStats getFinalStats() const { // in reviewpage UI Status get state!
        qDebug() << "[SetupWizard]: getFinalStatus.";
        // Falls der Proxy existiert, berechne LIVE.
        // Das ist syntaktisch sauberer als eine veraltete Variable.
        if (proxyModel_m) {
            return proxyModel_m->calculateVisibleStats();
        }
        return ReviewStats();
    }

    // Zugriff auf das zentrale Model
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
