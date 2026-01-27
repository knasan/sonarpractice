#include "filterpage.h"
#include "fileutils.h"
#include "setupwizard.h"

#include <QCheckBox>
#include <QGroupBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QTextBrowser>
#include <QTimer>
#include <QVBoxLayout>

FilterPage::FilterPage(QWidget *parent) : BasePage(parent) {
    setTitle(tr("Configuration"));
    setSubTitle(tr("Settings for your repertoire."));

    auto *layout = new QVBoxLayout(this);
    addHeaderLogo(layout, tr("Filterpage"));

    auto *infoLabel = new QLabel(this);
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

    styleInfoLabel(infoLabel);

    layout->addWidget(infoLabel);

    // --- Data management ---
    cbManageData_m = new QCheckBox(tr("Manage"), this);
    layout->addWidget(cbManageData_m);

    // Container for path selection
    QHBoxLayout *pathLayout = new QHBoxLayout();
    // SonarPractice is my top level in the tree
    lblTargetPath_m = new QLabel(QDir::homePath() + "/SonarPractice", this);

    btnSelectTargetPath_m = new QPushButton(tr("Change path..."), this);

    // Initial state: Disabled
    lblTargetPath_m->setEnabled(false);
    btnSelectTargetPath_m->setEnabled(false);
    // Visual feedback: Slightly gray out or frame the label
    lblTargetPath_m->setStyleSheet("color: gray; border: 1px solid #444; padding: 5px;");

    pathLayout->addWidget(lblTargetPath_m);
    pathLayout->addWidget(btnSelectTargetPath_m);
    layout->addLayout(pathLayout);

    // --- File types ---
    auto *typeGroup = new QGroupBox(tr("What data should be searched for?"), this);
    auto *gridLayout = new QGridLayout(typeGroup);

    cbGuitarPro_m = new QCheckBox("Guitar Pro (.gp, .gpx, .gtp)", this);
    cbPdf_m = new QCheckBox("PDF Documents", this);
    cbAudio_m = new QCheckBox("Audio (MP3, WAV)", this);
    cbVideo_m = new QCheckBox("Video (MP4, AVI)", this);

    // Guitar Pro is mandatory (requirement)
    cbGuitarPro_m->setChecked(true);
    cbGuitarPro_m->setEnabled(false);

    // Arrangement in the grid (row, column)
    gridLayout->addWidget(cbGuitarPro_m, 0, 0);
    gridLayout->addWidget(cbPdf_m, 0, 1);
    gridLayout->addWidget(cbAudio_m, 1, 0);
    gridLayout->addWidget(cbVideo_m, 1, 1);

    layout->addWidget(typeGroup);
    layout->addStretch();

    listWidgetSource_m = new QListWidget(this);
    layout->addWidget(listWidgetSource_m);

    // Add Button
    btnAddSource_m = new QPushButton(tr("Add folder"), this);
    // button style add
    stylePushButton(btnAddSource_m);

    // Remove Button
    btnRemSource_m = new QPushButton(tr("Remove folder"), this);
    // button style rem
    stylePushButton(btnRemSource_m);

    // Default Button State
    btnRemSource_m->setEnabled(false);

    // QHBoxLayout
    auto *btnLayout = new QHBoxLayout();
    btnLayout->addWidget(btnAddSource_m);
    btnLayout->addWidget(btnRemSource_m);
    btnLayout->setSpacing(10);

    layout->addLayout(btnLayout);

    if (!isConnectionsEstablished_m) {
        isConnectionsEstablished_m = true;

        // Connection to the slot addPath
        connect(btnAddSource_m, &QPushButton::clicked, this, &FilterPage::addSourcePath);

        // Connection to the slot removePath
        connect(btnRemSource_m, &QPushButton::clicked, this, &FilterPage::removeSourcePath);

        // Connection to the slot updateRemoveButtonState when click and selected
        connect(listWidgetSource_m, &QListWidget::itemSelectionChanged, this, &FilterPage::updateRemoveSourceButtonState);

        // --- LOGIC LINK ---
        // Linking: Checkbox -> Activation of path selection
        connect(cbManageData_m, &QCheckBox::toggled, lblTargetPath_m, &QLabel::setEnabled);
        connect(cbManageData_m, &QCheckBox::toggled, btnSelectTargetPath_m, &QPushButton::setEnabled);

        // Pairing: Button -> Call slot
        connect(btnSelectTargetPath_m, &QPushButton::clicked, this, &FilterPage::addTargetPath);

        // Visual update (change style)
        connect(cbManageData_m, &QCheckBox::toggled, this, [this](bool checked) {
            updateTargetPathStyle(checked);
        });
    }

    registerField("manageData", cbManageData_m);
    registerField("targetPath", lblTargetPath_m, "text");
}

void FilterPage::initializePage() {
    bool managed = field("manageData").toBool();
    updateTargetPathStyle(managed);
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
            // Der alte Pfad ist jetzt redundant. Wir löschen ihn.
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

    if (cbPdf_m->isChecked()) {
        filters << FileUtils::getPdfFormats();
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

// The "Next" button is only active if the list is not empty.
bool FilterPage::isComplete() const {
    return listWidgetSource_m->count() > 0;
}

bool FilterPage::validatePage() {
    if (wiz()) {
        wiz()->activeFilters_m = getActiveFilters();
        wiz()->sourcePaths_m.clear();
        for(int i = 0; i < listWidgetSource_m->count(); ++i) {
            wiz()->sourcePaths_m << listWidgetSource_m->item(i)->text();
        }
        wiz()->setButtonsState(false, false, true);
    }

    QStringList paths = wiz()->getSelectedPaths();
    QStringList filters = wiz()->getFileFilters();

    wiz()->fileManager()->clearCaches();

    wiz()->button(QWizard::NextButton)->setEnabled(false);
    return true;
}
