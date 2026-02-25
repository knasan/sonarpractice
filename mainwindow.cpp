#include "mainwindow.h"
#include "databasemanager.h"
#include "librarypage.h"
#include "sonarlessonpage.h"
#include "sonarmenuhelper.h"
#include "importdialog.h"
#include "fnv1a.h"
#include "filescanner.h"

#include <QMainWindow>
#include <QGuiApplication>
#include <QScreen>
#include <QHBoxLayout>
#include <QListWidget>
#include <QStackedWidget>
#include <QLabel>
#include <QWidget>
#include <QScreen>
#include <QPushButton>
#include <QStandardPaths>
#include <QProgressDialog>
#include <QThread>
#include <QShortcut>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    this->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint | Qt::WindowMinimizeButtonHint);

    // Set window size
    setMinimumSize(600, 500);
    showMaximized();

    // Database
    dbManager_m = new DatabaseManager(this);

    // Prepare page
    lessonPage_m = new SonarLessonPage(dbManager_m, this);
    libraryPage_m = new LibraryPage();

    // Create a central widget and layout
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    QVBoxLayout *layout = new QVBoxLayout(centralWidget);
    SonarMenuHelper::setupMainWindowMenu(this);

    // StackedWidget for page switching (Home, Library, etc.)
    stackedWidget_m = new QStackedWidget(this);

    // --- NAVIGATION ---
    QHBoxLayout *navLayout = new QHBoxLayout();

    QPushButton *btnHome = new QPushButton(this);
    btnHome->setObjectName("homeButton");
    btnHome->setIcon(QIcon(":/house.svg"));
    btnHome->setCheckable(true);
    btnHome->setAutoExclusive(true);
    btnHome->setToolTip(tr("Exercise (Home)"));

    QPushButton *btnLibrary = new QPushButton(this);
    btnLibrary->setObjectName("libraryButton");
    btnLibrary->setIcon(QIcon(":/book.svg"));
    btnLibrary->setCheckable(true);
    btnLibrary->setAutoExclusive(true);
    btnLibrary->setToolTip(tr("Library"));

    navLayout->addWidget(btnHome);
    navLayout->addWidget(btnLibrary);
    navLayout->addStretch();

    // Add the navigation layout to the main layout (above the QStackedWidget)
    layout->addLayout(navLayout);

    // --- Linking Logic (Signals and Slots) ---
    connect(btnHome, &QPushButton::clicked, this, [this]() {
        stackedWidget_m->setCurrentIndex(0); // page home
    });

    connect(btnLibrary, &QPushButton::clicked, this, [this]() {
        stackedWidget_m->setCurrentIndex(1); // page library
    });

    stackedWidget_m->addWidget(lessonPage_m);   // Index 0
    stackedWidget_m->addWidget(libraryPage_m);  // Index 1

    // connect(this, &MainWindow::dataChanged, lessonPage_m, &SonarLessonPage::loadData);

    connect(this, &MainWindow::dataChanged, libraryPage_m, &LibraryPage::markAsDirty);

    layout->addWidget(stackedWidget_m);

    // the default setting is to show the Lesson Page (your main page).
    stackedWidget_m->setCurrentWidget(lessonPage_m);

    QAction* importFile = this->findChild<QAction*>("actionImportFile");
    if (importFile) {
        connect(importFile, &QAction::triggered, this, &MainWindow::onImportFileTriggered);
    }

    QAction* importDirectory = this->findChild<QAction*>("actionImportDirectory");
    if (importFile) {
        connect(importDirectory, &QAction::triggered, this, &MainWindow::onImportDirectoryTriggered);
    }

    QShortcut *shortcut = new QShortcut(QKeySequence(Qt::Key_F5), this);
    connect(shortcut, &QShortcut::activated, this, &MainWindow::reloadStyle);
}

MainWindow::~MainWindow() {}

/*void MainWindow::reloadStyle() {
    qDebug() << "Load Style";
    QFile file(":/base.qss"); // Pfad zu deiner Ressource
    if (file.open(QFile::ReadOnly)) {
        QString styleSheet = QLatin1String(file.readAll());
        qApp->setStyleSheet(styleSheet);
        file.close();
        qDebug() << "Style reloaded!";
    }
}*/

void MainWindow::reloadStyle() {
    // 1. Pfad zu deiner echten Datei auf der Festplatte (NICHT die Ressource)
    // Ersetze den Pfad durch deinen tatsächlichen Pfad zur Datei!
    QString diskPath = "C:/Users/smk/Develop/03_Projects/SonarPractice/styles/base.qss";

    QFile file(diskPath);

    // Falls die Datei auf der Platte nicht gefunden wird (z.B. beim User),
    // nimm die eingebaute Ressource als Fallback
    if (!file.exists()) {
        file.setFileName(":/base.qss");
    }

    if (file.open(QFile::ReadOnly)) {
        QString styleSheet = QLatin1String(file.readAll());
        qApp->setStyleSheet(styleSheet);
        file.close();
        qDebug() << "Style reloaded from:" << file.fileName();
    }
}

void MainWindow::onImportFileTriggered() {

    QString audioFilter = formatFilter(tr("Audio"), FileUtils::getAudioFormats());
    QString gpFilter = formatFilter(tr("Guitar Pro"), FileUtils::getGuitarProFormats());
    QString pdfFilter = formatFilter(tr("PDF"), FileUtils::getPdfFormats());
    QString videoFilter = formatFilter(tr("Video"), FileUtils::getVideoFormats());
    QString allFilter = tr("All files (*.*)");

    QString allSupported = formatFilter(tr("All Supported"), FileUtils::getAudioFormats() + FileUtils::getGuitarProFormats() + FileUtils::getPdfFormats() + FileUtils::getVideoFormats());

    // Combine everything into one complete filter string
    QString combinedFilter = QString("%1;;%2;;%3;;%4;;%5;;%6")
                                 .arg(allSupported, audioFilter, gpFilter, pdfFilter, videoFilter, allFilter);

    QStringList selectedFiles = QFileDialog::getOpenFileNames(
        this,
        tr("Select files for import"),
        QStandardPaths::writableLocation(QStandardPaths::HomeLocation),
        combinedFilter
        );

    if (selectedFiles.isEmpty()) {
        return;
    }

    QSet<QString> dbHashes = dbManager_m->getAllFileHashes();

    QList<ScanBatch> batches;
    for (const QString &filePath : std::as_const(selectedFiles)) {
        ScanBatch batch;
        batch.info = QFileInfo(filePath);
        batch.hash = FNV1a::calculate(filePath);
        if (!dbHashes.contains(batch.hash)) {
            batch.status = StatusReady;
        } else {
            batch.status = StatusAlreadyInDatabase;
        }

        batches.append(batch);
    }

    ImportDialog dlg(this);
    dlg.setImportData(batches); // This method populates sourceModel_m

    if (dlg.exec() == QDialog::Accepted) {
        emit dataChanged();
    } else {
        qDebug() << "Canceled";
    }
}

void MainWindow::onImportDirectoryTriggered() {

    QString dirPath = QFileDialog::getExistingDirectory(this, tr("Select folder for import"));
    if (dirPath.isEmpty()) return;

    QProgressDialog *progress = new QProgressDialog(tr("Scanning and hashing files..."), tr("Cancel"), 0, 0, this);
    progress->setWindowModality(Qt::WindowModal);

    QSet<QString> dbHashes = dbManager_m->getAllFileHashes();

    FileScanner *scanner = new FileScanner();
    scanner->setExistingHashes(dbHashes);

    QThread *thread = new QThread();
    scanner->moveToThread(thread);

    QStringList filters = FileUtils::getAudioFormats() + FileUtils::getGuitarProFormats() +
                          FileUtils::getPdfFormats() + FileUtils::getVideoFormats();

    // ProgressDialog
    int totalFound = 0;
    connect(scanner, &FileScanner::batchesFound, this, [progress, totalFound](const QList<ScanBatch>& batches) mutable {
        totalFound += batches.size();
        progress->setLabelText(tr("%1 files processed...").arg(totalFound));
    });

    // abort logic
    connect(progress, &QProgressDialog::canceled, this, [scanner, progress]() {
        scanner->abort();

        progress->setLabelText(tr("Aborting scan... Please wait."));
        progress->setEnabled(false);
    });

    connect(scanner, &FileScanner::finishWithAllBatches, this, [this, progress](const QList<ScanBatch>& all){
        progress->close();
        progress->deleteLater();

        ImportDialog dlg;
        dlg.setImportData(all);

        if (dlg.exec() == QDialog::Accepted) {
            emit dataChanged();
        } else {
            qDebug() << "Canceled";
        }
    });

    // Cleanup
    connect(thread, &QThread::started, [scanner, dirPath, filters](){
        scanner->doScan({dirPath}, filters);
    });

    connect(scanner, &FileScanner::finished, thread, &QThread::quit);
    connect(scanner, &FileScanner::finished, scanner, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);

    thread->start();
    progress->show();
}

// Helper function to build a QFileDialog filter from the list
QString MainWindow::formatFilter(const QString& description, const QStringList& extensions) {
    return QString("%1 (%2)").arg(description, extensions.join(" "));
}
