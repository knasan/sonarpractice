#include "setupwizard.h"
#include "filemanager.h"
#include "filescanner.h"
#include "welcomepage.h"
#include "filterpage.h"
#include "reviewpage.h"
#include "databasemanager.h"
#include "mappingpage.h"
#include "filefilterproxymodel.h"

#include <QScreen>
#include <QGuiApplication>
#include <QMessageBox>
#include <QProcess>

SetupWizard::SetupWizard(QWidget *parent)
    : QWizard(parent)
{
    fileScanner_m = new FileScanner(); // without this, no parent use in a thread
    scannerThread_m = new QThread(this);
    fileScanner_m->moveToThread(scannerThread_m);

    setupModels();
    setupUiLayout();
    createPages();
    setupConnections();
}

SetupWizard::~SetupWizard() = default;

void SetupWizard::setupModels() {
    filesModel_m = new QStandardItemModel(this);
    fileManager_m = new FileManager(this);
    fileManager_m->setModel(filesModel_m);

    filesModel_m->setColumnCount(4);
    setProxyModelHeader();

    proxyModel_m = new FileFilterProxyModel(this);
    proxyModel_m->setSourceModel(filesModel_m);
    proxyModel_m->setDynamicSortFilter(true);
}

void SetupWizard::setupUiLayout() {
    setWindowTitle(tr("SonarPractice Setup Assistant"));
    setWindowFlags(windowFlags() | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);

    setOption(QWizard::HaveHelpButton, true);
    setWizardStyle(QWizard::ModernStyle);
    setButtonText(QWizard::HelpButton, tr("About Qt"));

    // Responsive design: Window size based on screen
    if (const auto *screen = QGuiApplication::primaryScreen()) {
        const QSize availableSize = screen->availableGeometry().size();
        resize(availableSize.width() * 0.6, availableSize.height() * 0.7);
    }
}

void SetupWizard::createPages() {
    // Instantiation
    welcomePage_m = new WelcomePage(this);
    filterPage_m  = new FilterPage(this);
    reviewPage_m  = new ReviewPage(this);
    mappingPage_m = new MappingPage(this);

    setPage(Page_Welcome, welcomePage_m);
    setPage(Page_Filter,  filterPage_m);
    setPage(Page_Review,  reviewPage_m);
    setPage(Page_Mapping, mappingPage_m);
}

void SetupWizard::setupConnections() {
    // Help button (About Qt)
    connect(this, &QWizard::helpRequested, this, [] {
        QMessageBox::aboutQt(nullptr);
    });

    connect(scannerThread_m, &QThread::finished, fileScanner_m, &QObject::deleteLater);
    scannerThread_m->start();
}

void SetupWizard::setProxyModelHeader() {
    filesModel_m->setHorizontalHeaderLabels({
        tr("Name"), tr("Size"), tr("Status"), tr("Group")
    });
}

void SetupWizard::prepareScannerWithDatabaseData() {
    // If the database exists, load hashes
    auto &db = DatabaseManager::instance();
    QSet<QString> knwonHahes = db.getAllFileHashes();
    if (fileScanner_m) {
        fileManager_m->setExistingHashes(knwonHahes);
    }
}

void SetupWizard::restartApp() {
    QString appPath = QCoreApplication::applicationFilePath();
    QStringList arguments = QCoreApplication::arguments();

    if (!arguments.isEmpty()) {
        arguments.removeFirst();
    }


    if (scannerThread_m && scannerThread_m->isRunning()) {
        scannerThread_m->quit();
        scannerThread_m->wait(500);
    }

    bool success = QProcess::startDetached(appPath, arguments);

    if (success) {
        QCoreApplication::quit();
    } else {
        QMessageBox::critical(nullptr, "Error", "Could not restart application.");
    }
}
