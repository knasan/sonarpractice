#include "filterpage.h"
#include "fileutils.h"
#include "setupwizard.h"
#include "databasemanager.h"

#include <QCheckBox>
#include <QGroupBox>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardPaths>
#include <QTextBrowser>
#include <QTimer>
#include <QVBoxLayout>
#include <QApplication>

FilterPage::FilterPage(QWidget *parent) : BasePage(parent) {
    setTitle(tr("Configuration"));
    setSubTitle(tr("Settings for your repertoire."));

    setupLayout();
    setupConnections();
}

void FilterPage::setupLayout() {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(30, 20, 30, 20);
    layout->setSpacing(15);

    addHeaderLogo(layout, tr("Filterpage"));

    auto *infoLabel = new QLabel(this);
    infoLabel->setObjectName("infoLabel");
    infoLabel->setContentsMargins(0, 0, 0, 10);
    layout->addWidget(infoLabel);

    // TODO: generate infoLabel with const tr() strings with args.
    infoLabel->setText(tr("<h3>Select Data Management</h3>"
                          "<p>You decide how SonarPractice handles your files:</p>"
                          "<p>Manage option:</p>"
                          "<ul>"
                          "  <li><b>Disabled:</b> Your files will remain exactly where they are now. SonarPractice simply creates a smart link in the database.<br>"
                          "This allows the program to check if the file is still in place and lets you link journal entries directly to your repertoire."
                          "</li>"
                          "  <li><b>Activated:</b>Sonar Practice handles the organization and copies your selection"
                          "into the selected target directory. This helps you to keep things organized and permanently avoid duplicates.</li>"
                          "</ul>"
                          "<p><i>Note: Your progress analysis and journal entries are securely stored in the system's internal database (AppData/Local).</i></p>"));
    infoLabel->setWordWrap(true);

    // --- Data management ---
    auto *dataManagmentLayout = new QHBoxLayout();

    cbManageData_m = new QCheckBox(tr("Manage"));
    cbManageData_m->setObjectName("manageData");
    // TODO: Aktivieren Sie die Dateiverwaltung; standardmäßig werden alle Daten in das Zielverzeichnis kopiert, es sei denn, die Option „Dateien verschieben“ ist aktiviert.
    cbManageData_m->setToolTip(tr("Enable file management; by default, all data will be copied to the target directory unless 'move files' is enabled."));

    cbMoveFiles_m = new QCheckBox(tr("Move files"));
    cbMoveFiles_m->setObjectName("moveFiles");
    // TODO: Aktiviert das Verschieben von Dateien
    cbMoveFiles_m->setToolTip(tr("Enables file moving"));

    dataManagmentLayout->addWidget(cbManageData_m);
    dataManagmentLayout->addWidget(cbMoveFiles_m,1);

    layout->addLayout(dataManagmentLayout);

    cbSkipImport_m = new QCheckBox(tr("Skip import"));
    cbSkipImport_m->setObjectName("skipimport");
    cbSkipImport_m->setToolTip(tr("Skip the file import. An empty library will be created with your chosen path settings so you can add files manually later."));

    layout->addWidget(cbSkipImport_m);

    // Container for path selection
    QHBoxLayout *pathLayout = new QHBoxLayout();
    pathLayout->setContentsMargins(0, 0, 0, 0);
    // SonarPractice is my top level in the tree

    lblTargetPath_m = new QLabel(QDir::homePath() + "/SonarPractice", this);
    btnSelectTargetPath_m = new QPushButton(tr("Change path..."), this);

    // Initial state: Disabled
    lblTargetPath_m->setEnabled(false);
    lblTargetPath_m->setObjectName("targetPathLabel");

    btnSelectTargetPath_m->setEnabled(false);

    pathLayout->addWidget(lblTargetPath_m);
    pathLayout->addWidget(btnSelectTargetPath_m);

    layout->addLayout(pathLayout);

    // --- File types ---
    auto *typeGroup = new QGroupBox(tr("What data should be searched for?"), this);
    auto *gridLayout = new QGridLayout(typeGroup);

    cbGuitarPro_m = new QCheckBox("Guitar Pro (.gp, .gpx, .gtp, etc.)", this);
    cbDoc_m = new QCheckBox("Documents", this);
    cbAudio_m = new QCheckBox("Audio (mp3, wav, aiff, etc.)", this);
    cbVideo_m = new QCheckBox("Video (mp4, avi, mpeg, etc.)", this);

    // Guitar Pro is mandatory (requirement)
    cbGuitarPro_m->setChecked(true);
    cbGuitarPro_m->setEnabled(false);

    // Arrangement in the grid (row, column)
    gridLayout->addWidget(cbGuitarPro_m, 0, 0);
    gridLayout->addWidget(cbDoc_m, 0, 1);
    gridLayout->addWidget(cbAudio_m, 1, 0);
    gridLayout->addWidget(cbVideo_m, 1, 1);

    layout->addWidget(typeGroup);
    layout->addStretch();

    listWidgetSource_m = new QListWidget(this);
    listWidgetSource_m->setMinimumHeight(120);

    layout->addWidget(listWidgetSource_m);

    // Add Button
    btnAddSource_m = new QPushButton(tr("Add folder"), this);

    // Remove Button
    btnRemSource_m = new QPushButton(tr("Remove folder"), this);

    // Default Button State
    btnRemSource_m->setEnabled(false);

    // QHBoxLayout
    auto *btnLayout = new QHBoxLayout();
    btnLayout->addWidget(btnAddSource_m);
    btnLayout->addWidget(btnRemSource_m);
    btnLayout->setSpacing(10);

    layout->addLayout(btnLayout);

    registerField("cbManageData", cbManageData_m);
    registerField("cbSkipImport", cbSkipImport_m);
    registerField("cbMoveFiles", cbMoveFiles_m);
    registerField("cbTargetPath", lblTargetPath_m, "text");
}

void FilterPage::setupConnections() {
    // Connection to the slot addPath
    connect(btnAddSource_m, &QPushButton::clicked, this, &FilterPage::addSourcePath);

    // Connection to the slot removePath
    connect(btnRemSource_m, &QPushButton::clicked, this, &FilterPage::removeSourcePath);

    // --- LOGIC LINK ---

    // Linking: Checkbox -> Activation skip import
    connect(cbSkipImport_m, &QCheckBox::toggled, this, &FilterPage::onSettingsChanged);

    // Linking: Checkbox -> Activation of path selection
    connect(cbManageData_m, &QCheckBox::toggled, this, &FilterPage::onSettingsChanged);

    connect(cbManageData_m, &QCheckBox::toggled, lblTargetPath_m, &QLabel::setEnabled);
    connect(cbManageData_m, &QCheckBox::toggled, btnSelectTargetPath_m, &QPushButton::setEnabled);

    // Visual update (change style)
    connect(cbManageData_m, &QCheckBox::toggled, this, [this](bool checked) {
        updateTargetPathStyle(checked);
    });

    // Path selection
    connect(btnSelectTargetPath_m, &QPushButton::clicked, this, &FilterPage::addTargetPath);

    // Connection to the slot updateRemoveButtonState when click and selected
    connect(listWidgetSource_m, &QListWidget::itemSelectionChanged, this, &FilterPage::updateRemoveSourceButtonState);
}

void FilterPage::onSettingsChanged() {
    const bool isSkipActive = cbSkipImport_m->isChecked();
    listWidgetSource_m->setEnabled(!isSkipActive);
    btnAddSource_m->setEnabled(!isSkipActive);
    emit completeChanged();
}

bool FilterPage::validatePage() {
    auto *wiz = qobject_cast<SetupWizard*>(wizard());
    if (!wiz) return false;

    // 1. Special case: Skip Import
    if (cbSkipImport_m && cbSkipImport_m->isChecked()) {
        return handleSkipImport(); // Internally, wizard()->accept() is called here.
    }

    // 2.Transfer data via setter to the wizard
    wiz->setActiveFilters(getActiveFilters());

    QStringList paths;
    for(int i = 0; i < listWidgetSource_m->count(); ++i) {
        paths << listWidgetSource_m->item(i)->text();
    }

    wiz->setSourcePaths(paths);

    return true;
}

bool FilterPage::isComplete() const {
    // If import is skipped, the page is always "complete".
    if (cbSkipImport_m && cbSkipImport_m->isChecked()) {
        return true;
    }

    // When "Manage Data" is active, a path must be selected.
    if (cbManageData_m && cbManageData_m->isChecked()) {
        if (lblTargetPath_m->text().isEmpty() || lblTargetPath_m->text() == tr("No path selected")) {
            return false;
        }
    }

    // There must be files in the list.
    return listWidgetSource_m && listWidgetSource_m->count() > 0;
}

bool FilterPage::handleSkipImport() {
    static bool isProcessing = false;
    if (isProcessing) return true;

    isProcessing = true;

    QString appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(appDataDir);

    // TODO: Define a Global Makro or function (DatabaseManager getDatabaseStorePath
    #ifdef QT_DEBUG
            QString finalDbPath = appDataDir + "/sonar_practice_debug.db";
    #else
            QString finalDbPath = appDataDir + "/sonar_practice.db";
    #endif

    if (!DatabaseManager::instance().initDatabase(finalDbPath)) {
            QMessageBox::critical(this, "Error", "The database could not be initialized.");
            return false;
    }

    auto &db = DatabaseManager::instance();

    // C++20: Use structured bindings or clear logic for DB writes
    bool success = true;
    success &= db.setSetting("is_managed", false);
    success &= db.setSetting("last_import_date", QDateTime::currentDateTime().toString(Qt::ISODate));

    // If 'Manage Data' was checked but skipped:
    if (field("cbManageData").toBool()) {
        success &= db.setSetting("managed_path", field("cbTargetPath").toString());
    }

    db.closeDatabase();

    if (success) {
        // wizard()->accept(); // Successfully completes the wizard
        wiz()->restartApp();
    }

    return success;
}

int FilterPage::nextId() const {
    if (cbSkipImport_m->isChecked()) {
        return -1; // -1 signals: "This is the last page"
    }
    return SetupWizard::Page_Review;
}

void FilterPage::addTargetPath() {
    QString chosenPath = FileUtils::getCleanDirectory(this, tr("Select target directory"));

    QString systemPath = QDir::fromNativeSeparators(chosenPath);

    if (!systemPath.isEmpty()) {
        lblTargetPath_m->setText(systemPath);
    }
}

void FilterPage::addSourcePath() {
    QString dir = QDir::cleanPath(QFileDialog::getExistingDirectory(this, tr("Select folder")));
    if (dir.isEmpty()) return;

    listWidgetSource_m->blockSignals(true);

    // Normalization for comparison (always with / at the end)
    auto ensureSlash = [](QString p) {
        return p.endsWith('/') ? p : p + "/";
    };

    QString newPathSlash = ensureSlash(dir);
    bool shouldAdd = true;

    // Go through the list backwards to ensure deletion.
    for (int i = listWidgetSource_m->count() - 1; i >= 0; --i) {
        QString existing = listWidgetSource_m->item(i)->text();
        QString existingSlash = ensureSlash(existing);

        // Case A: The newly elected UPPER FOLDER is a subfolder of an existing one.
        // Example: New=F:/, Old=F:/Guitar/Pdf
        if (newPathSlash.startsWith(existingSlash)) {
            flashItem(listWidgetSource_m->item(i)); // Hier blinkt es jetzt!
            shouldAdd = false;
            break;
        }

        // Case B: Newly selected paths include existing paths
        // Example: New=F:/Guitar, Old=F:/Guitar/Pdf
        if (existingSlash.startsWith(newPathSlash)) {
            delete listWidgetSource_m->takeItem(i);
        }
    }

    if (shouldAdd) {
        listWidgetSource_m->addItem(dir);
    }

    emit completeChanged();
    listWidgetSource_m->blockSignals(false);
}

// flash source path
void FilterPage::flashItem(QListWidgetItem *item) {
    if (!item) return;

    // static counter or a lambda timer
    int *count = new int(0);
    QTimer *timer = new QTimer(this);

    connect(timer, &QTimer::timeout, [=]() {
        (*count)++;

        // Switch color (blinking effect)
        if ((*count) % 2 == 1) {
            item->setBackground(Qt::red);
            item->setForeground(Qt::white);
        } else {
            item->setBackground(Qt::transparent);
            item->setForeground(Qt::white);
        }

        // Stop after flashing 4 times (2 times on, 2 times off).
        if (*count >= 4) {
            timer->stop();
            timer->deleteLater();
            delete count;
        }
    });

    timer->start(250); // Switch every 250ms
}

void FilterPage::removeSourcePath() {
    listWidgetSource_m->blockSignals(true);

    int currentRow = listWidgetSource_m->currentRow();

    if (currentRow >= 0) {
        delete listWidgetSource_m->takeItem(currentRow);
    }

    updateRemoveSourceButtonState();

    listWidgetSource_m->blockSignals(false);
}

// Logic function removeButton on/off (slot)
void FilterPage::updateRemoveSourceButtonState() {
    if (!listWidgetSource_m || !btnRemSource_m) return;

    listWidgetSource_m->blockSignals(true);

    bool hasItems = listWidgetSource_m->count() > 0;

    bool hasSelection = listWidgetSource_m->currentItem() != nullptr;

    btnRemSource_m->setEnabled(hasItems && hasSelection);

    listWidgetSource_m->blockSignals(false);
}

// Merging in FilterPage
QStringList FilterPage::getActiveFilters() {
    QStringList filters;

    // Guitar Pro is always with me
    filters << FileUtils::getGuitarProFormats();

    if (cbDoc_m->isChecked()) {
        filters << FileUtils::getDocFormats();
    }

    if (cbAudio_m->isChecked()) {
        filters << FileUtils::getAudioFormats();
    }

    if (cbVideo_m->isChecked()) {
        filters << FileUtils::getVideoFormats();
    }

    return filters;
}

// Style code for enabled/disabled
void FilterPage::updateTargetPathStyle(bool checked) {
    if(checked) {
        lblTargetPath_m->setStyleSheet("color: #aaccff; border: 1px solid #666; padding: 5px;");
    } else {
        lblTargetPath_m->setStyleSheet("color: gray; border: 1px solid #444; padding: 5px;");
    }
}
