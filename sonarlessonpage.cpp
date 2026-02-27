/*
* SonarPractice - Repertoire Management & Practice Journal
* Copyright (C) 2026 [Sandy Marko Knauer alias smk and knasan]
* This program is free software: You may redistribute and/or modify it under the terms
* of the GNU General Public License as published by the Free Software Foundation,
* version 3 of the License.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; even without the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for further details.
*/

#include "sonarlessonpage.h"
#include "collapsiblesection.h"
#include "databasemanager.h"
#include "fileutils.h"
#include "uihelper.h"
#include "songeditdialog.h"

#include <QCalendarWidget>
#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>

#include <QTableWidget>
#include <QTextEdit>
#include <QLCDNumber>
#include <QTimer>
#include <QCompleter>
#include <QMessageBox>
#include <QDebug>
#include <QEvent>
#include <QMouseEvent>

// Move these to a separate header file or namespace
namespace PracticeTable {
constexpr int DEFAULT_ROW_COUNT = 5;
constexpr int COLUMN_COUNT = 6;
constexpr int BOX_WIDTH = 80;
constexpr int LABEL_WIDTH = 120;
constexpr int DEFAULT_BPM = 20;
constexpr int BPM_STEP = 5;
constexpr int MIN_DURATION_MINUTES = 1;

enum PracticeColumn { Date = 0, BeatFrom, BeatTo, BPM, Repetitions, Duration };
} // namespace PracticeTable

SonarLessonPage::SonarLessonPage(DatabaseManager *dbManager, QWidget *parent)
    : QWidget(parent)
    , dbManager_m(dbManager)
    , isDirtyNotes_m(false)
    , isDirtyTable_m(false)
    , isPlaceholderActive_m(false)
    , isTimerRunning_m(false)
    , isConnectionsEstablished_m(false)
    , currentFileId_m(-1)
{

    sourceModel_m = new QStandardItemModel(this);
    proxyModel_m = new QSortFilterProxyModel(this);
    proxyModel_m->setSourceModel(sourceModel_m);
    proxyModel_m->setFilterRole(SelectorRole::PathRole);

    setupTimer();
    setupUI();
    sitesConnects();
    initialLoadFromDb();
    updateButtonState();
    if (songSelector_m->currentIndex() >= 0) {
        onSongChanged(songSelector_m->currentIndex());
    }
    onFilterToggled();
}

void SonarLessonPage::setupUI()
{   
    auto *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(10);

    // 1. Sidebar
    setupSidebar(mainLayout);

    // 2. Main content
    auto *contentLayout = new QVBoxLayout();
    contentLayout->setSpacing(20);

    // Song information
    setupSongInformationSection(contentLayout);

    // Training section
    setupTrainingSection(contentLayout);

    // Notes section
    setupNotesSection(contentLayout);

    // Session table
    setupPracticeTable(contentLayout);

    // Footer
    setupFooterSection(contentLayout);

    mainLayout->addLayout(contentLayout, 3);
}

void SonarLessonPage::setupPracticeTable(QVBoxLayout *contentLayout)
{
    auto *practiceSection = new CollapsibleSection(tr("Practice Table"), true, true, this);

    auto *groupSession = new QGroupBox();
    groupSession->setObjectName("practiceTableGroup");
    auto *sessionLayout = new QVBoxLayout(groupSession);

    practiceTable_m = new QTableWidget(PracticeTable::DEFAULT_ROW_COUNT,
                                      PracticeTable::COLUMN_COUNT,
                                      this);
    practiceTable_m->setObjectName("practiceSessionTable");
    practiceTable_m->setAlternatingRowColors(true);

    QStringList headers = {
        tr("Day"), tr("Takt from"), tr("Takt to"),
        tr("Tempo (BPM)"), tr("Success Streak"), tr("Duration (Min)")
    };

    practiceTable_m->setHorizontalHeaderLabels(headers);
    practiceTable_m->model()->setHeaderData(0, Qt::Horizontal, tr("Date of the exercise session"), Qt::ToolTipRole);
    practiceTable_m->model()->setHeaderData(1, Qt::Horizontal, tr("Starting beat number of the section"), Qt::ToolTipRole);
    practiceTable_m->model()->setHeaderData(2, Qt::Horizontal, tr("End bar number of the section"), Qt::ToolTipRole);
    practiceTable_m->model()->setHeaderData(3, Qt::Horizontal, tr("Set speed in beats per minute"), Qt::ToolTipRole);
    practiceTable_m->model()->setHeaderData(4, Qt::Horizontal,
                                            tr("Maximum number of consecutive error-free repetitions.\n"
                                                "This number shows how confidently you have mastered the section."),
                                            Qt::ToolTipRole);

    practiceTable_m->model()->setHeaderData(5, Qt::Horizontal, tr("Total duration of this exercise unit"), Qt::ToolTipRole);

    practiceTable_m->horizontalHeader()->setObjectName("practiceTableHeader");
    practiceTable_m->horizontalHeader()->setStretchLastSection(true);

    practiceTable_m->verticalHeader()->setVisible(false);
    practiceTable_m->verticalHeader()->setObjectName("practiceTableVerticalHeader");

    practiceTable_m->setEditTriggers(QAbstractItemView::DoubleClicked
                                    | QAbstractItemView::EditKeyPressed);

    practiceTable_m->setSelectionBehavior(QAbstractItemView::SelectRows);

    QHeaderView* header = practiceTable_m->horizontalHeader();
    header->setSectionResizeMode(QHeaderView::Interactive);
    header->setSectionResizeMode(4, QHeaderView::Stretch);
    header->setSectionResizeMode(5, QHeaderView::Stretch);

    // 4. Buttons
    auto *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    auto *addBtn = new QPushButton(QIcon::fromTheme("list-add"), tr("Add Row"), this);
    addBtn->hide();
    addBtn->setObjectName("addTableRowButton");

    auto *removeBtn = new QPushButton(QIcon::fromTheme("list-remove"), tr("Remove Row"), this);
    removeBtn->hide();
    removeBtn->setObjectName("removeTableRowButton");

    buttonLayout->addWidget(addBtn);
    buttonLayout->addWidget(removeBtn);

    sessionLayout->addWidget(practiceTable_m);
    sessionLayout->addLayout(buttonLayout);
    practiceSection->addContentWidget(groupSession);
    contentLayout->addWidget(practiceSection);

    connect(addBtn, &QPushButton::clicked, this, &SonarLessonPage::addTableRow);
    connect(removeBtn, &QPushButton::clicked, this, &SonarLessonPage::removeTableRow);
}

void SonarLessonPage::updatePracticeTable(const QList<PracticeSession>& sessions) {
    practiceTable_m->setRowCount(0);

    for (const auto &s : sessions) {
        int row = practiceTable_m->rowCount();
        practiceTable_m->insertRow(row);

        practiceTable_m->setItem(row, 0, new QTableWidgetItem(s.date.toString("dd.MM.yyyy")));
        practiceTable_m->setItem(row, 1, new QTableWidgetItem(QString("%1 - %2").arg(s.startBar).arg(s.endBar)));
        practiceTable_m->setItem(row, 2, new QTableWidgetItem(QString::number(s.bpm)));
        practiceTable_m->setItem(row, 4, new QTableWidgetItem(QString::number(s.reps)));
        practiceTable_m->setItem(row, 5, new QTableWidgetItem(QString::number(s.streaks)));

        for(int i = 0; i < practiceTable_m->columnCount(); ++i) {
            if(auto item = practiceTable_m->item(row, i))
                item->setTextAlignment(Qt::AlignCenter);
        }
    }
}

void SonarLessonPage::setupSidebar(QHBoxLayout *mainLayout)
{
    auto *sidebarLayout = new QVBoxLayout();
    sidebarLayout->setSpacing(10);

    // 1. Calendar section
    auto *calSection = new CollapsibleSection(tr("Calendar"), true, true, this);
    calendar_m = new QCalendarWidget(this);
    calendar_m->setObjectName("lessonCalendar");
    calSection->addContentWidget(calendar_m);

    // 2. Reminder section
    auto *remSection = new CollapsibleSection(tr("Today's Reminders"), true, true, this);
    remSection->setObjectName("sidebarSectionLabel");

    // Reminder-Table initialize
    reminderTable_m = new QTableWidget(this);
    reminderTable_m->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    reminderTable_m->setObjectName("reminderTable");
    reminderTable_m->setColumnCount(4);
    reminderTable_m->setHorizontalHeaderLabels({tr("Song"), tr("Range"), tr("BPM"), tr("Status")});
    reminderTable_m->setContextMenuPolicy(Qt::CustomContextMenu);

    // UI refinements for the sidebar
    reminderTable_m->verticalHeader()->setVisible(false);
    reminderTable_m->horizontalHeader()->setStretchLastSection(true);
    reminderTable_m->setSelectionBehavior(QAbstractItemView::SelectRows);
    reminderTable_m->setEditTriggers(QAbstractItemView::NoEditTriggers);

    remSection->addContentWidget(reminderTable_m);

    sidebarLayout->addWidget(calSection);
    sidebarLayout->addWidget(remSection);

    connect(remSection, &CollapsibleSection::toggled, this, [sidebarLayout, remSection](bool expanded) {
        if (expanded) {
            sidebarLayout->setStretchFactor(remSection, 0);
        } else {
            sidebarLayout->setStretchFactor(remSection, 0);
        }
    });

    connect(calSection, &CollapsibleSection::toggled, this, [sidebarLayout, calSection](bool expanded) {
        if (expanded) {
            sidebarLayout->setStretchFactor(calSection, 0);
        } else {
            sidebarLayout->setStretchFactor(calSection, 0);
        }
    });

    mainLayout->addLayout(sidebarLayout, 1);
}

void SonarLessonPage::setupSongInformationSection(QVBoxLayout *contentLayout)
{
    // --- CREATE COLLAPSIBLE SECTION ---
    auto *songInfoSection = new CollapsibleSection(tr("Song Information"),
                                                   true, // collapsible
                                                   true, // expandedByDefault
                                                   this);

    songInfoSection->setObjectName("songInfoSection");

    // --- SECTION CONTENTS ---
    auto *formLayout = new QGridLayout();
    formLayout->setSpacing(20);

    // Line 0: Song Selector
    QLabel *songLabel = new QLabel(tr("Selected Repertoire:"), this);
    songLabel->setObjectName("songInfoLabel");
    formLayout->addWidget(songLabel, 0, 0, 1, 1);

    songSelector_m = new QComboBox(this);
    songSelector_m->setObjectName("songSelectorComboBox");
    songSelector_m->setEditable(true);
    songSelector_m->setInsertPolicy(QComboBox::NoInsert);

    songSelector_m->setModel(proxyModel_m);
    songSelector_m->setModelColumn(0);

    QCompleter *completer = new QCompleter(songSelector_m->model(), this);
    completer->setFilterMode(Qt::MatchContains);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setCompletionMode(QCompleter::PopupCompletion);

    songSelector_m->setCompleter(completer);

    auto setupFilterButton = [](QToolButton* btn, const QString& iconPath, const QString& tip) {
        btn->setCheckable(true);
        btn->setFixedSize(28, 28);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setToolTip(tip);
        btn->setIcon(QIcon(iconPath));
    };

    btnFilterGp_m = new QToolButton(this);
    btnFilterGp_m->setObjectName("filterToolButton");
    btnFilterGp_m->setCheckable(true);

    btnFilterAudio_m = new QToolButton(this);
    btnFilterAudio_m->setObjectName("filterToolButton");
    btnFilterAudio_m->setCheckable(true);

    btnFilterVideo_m = new QToolButton(this);
    btnFilterVideo_m->setObjectName("filterToolButton");
    btnFilterVideo_m->setCheckable(true);

    btnFilterDocument_m = new QToolButton(this);
    btnFilterDocument_m->setObjectName("filterToolButton");
    btnFilterDocument_m->setCheckable(true);

    setupFilterButton(btnFilterGp_m, ":gp.png", tr("GuitarPro Filter"));
    setupFilterButton(btnFilterAudio_m, ":audio.png", tr("Audio Filter"));
    setupFilterButton(btnFilterVideo_m, ":video.png", tr("Video Filter"));
    setupFilterButton(btnFilterDocument_m, ":doc.png", tr("Dokument Filter"));

    // Enable GP by default
    btnFilterGp_m->setChecked(true);

    auto *filterLayout = new QHBoxLayout();
    filterLayout->setSpacing(2);
    filterLayout->setContentsMargins(0, 0, 0, 0);
    filterLayout->addWidget(btnFilterGp_m);
    filterLayout->addWidget(btnFilterAudio_m);
    filterLayout->addWidget(btnFilterVideo_m);
    filterLayout->addWidget(btnFilterDocument_m);

    formLayout->addWidget(songSelector_m, 0, 1, 1, 1);

    formLayout->addLayout(filterLayout, 0, 2, 1, 2, Qt::AlignLeft);

    // Line 1: Artist
    QLabel *artistLabel = new QLabel(tr("Artist:"), this);
    artistLabel->setObjectName("songInfoLabel");
    formLayout->addWidget(artistLabel, 1, 0, 1, 1);

    artist_m = new QLabel(this);
    artist_m->setObjectName("songDataLabel");
    formLayout->addWidget(artist_m, 1, 1, 1, 3);

    // Line 2: Title
    QLabel *titleLabel = new QLabel(tr("Title:"));
    titleLabel->setObjectName("songInfoLabel");
    formLayout->addWidget(titleLabel, 2, 0, 1, 1);

    title_m = new QLabel(this);
    title_m->setObjectName("songDataLabel");
    formLayout->addWidget(title_m, 2, 1, 1, 3);

    // Line 3: Tempo
    QLabel *tempoLabel = new QLabel(tr("Tempo:"));
    tempoLabel->setObjectName("songInfoLabel");
    formLayout->addWidget(tempoLabel, 3, 0, 1, 1);

    tempo_m = new QLabel(this);
    tempo_m->setObjectName("songDataLabel");
    formLayout->addWidget(tempo_m, 3, 1, 1, 1);

    // Line 4: Tuning
    QLabel *tuningLabel = new QLabel(tr("Tuning:"));
    tuningLabel->setObjectName("songInfoLabel");
    formLayout->addWidget(tuningLabel, 4, 0, 1, 1);

    tuningLabel_m = new QLabel(this);
    tuningLabel_m->setObjectName("songDataLabel");
    formLayout->addWidget(tuningLabel_m, 4, 1, 1, 1);

    // Line 5: Open Song Button
    btnGpIcon_m = new QPushButton(tr("Open Media"), this);
    btnGpIcon_m->setObjectName("songOpenButton");
    btnGpIcon_m->setIcon(style()->standardIcon(QStyle::SP_FileIcon));
    formLayout->addWidget(btnGpIcon_m, 5, 0, 1, 1);

    auto *btnEditDialog = new QPushButton(tr("Edit Entry"), this);
    btnEditDialog->setObjectName("songEditDialog");
    formLayout->addWidget(btnEditDialog, 5, 1, 1, 1);

    connect(btnEditDialog, &QPushButton::clicked, this, &SonarLessonPage::onEditSongClicked);

    auto *containerWidget = new QWidget(this);
    containerWidget->setLayout(formLayout);
    songInfoSection->addContentWidget(containerWidget);

    contentLayout->addWidget(songInfoSection);
}

void SonarLessonPage::onEditSongClicked() {
    // 1. Retrieve data for the currently selected song
    int songId = getCurrentSongId();
    QString title = title_m->text();
    QString artist = artist_m->text();
    QString tuning = tuningLabel_m->text();
    int bpm = tempo_m->text().toInt();

    // 2. Dialog
    SongEditDialog dialog(this);

    // 3. Retrieve lists from the database for the dropdowns.
    QStringList artists = dbManager_m->getAllArtists();
    QStringList tunings = dbManager_m->getAllTunings();

    // 4. Pass data to dialog
    dialog.setSongData(title, artist, tuning, bpm, artists, tunings);

    // 5. Show dialog
    if (dialog.exec() == QDialog::Accepted) {
        QString newTitle = dialog.title();
        QString newArtistName = dialog.artist();
        QString newTuningName = dialog.tuning();
        int newBpm = dialog.bpm();

        // 6. Store in the database
        // First, get IDs for Artist/Tuning (creates new ones if none exist)
        int artistId = dbManager_m->getOrCreateArtist(newArtistName);
        int tuningId = dbManager_m->getOrCreateTuning(newTuningName);

        // Call up the update function in the DatabaseManager
        if(!dbManager_m->updateSong(songId, newTitle, artistId, tuningId, newBpm)) {
            statusLabel_m->setText(savedMessageFailed_m);
        } else {
            showSaveMessage(savedMessage_m);
            // load DB and reselect the Song
            initialLoadFromDb();
            selectSongByExternalId(songId);
        }
    }
}

void SonarLessonPage::onFilterToggled() {
    QStringList allAllowedExtensions;

    // 1. Collect formats from FileUtils when the respective button is active.
    if (btnFilterGp_m->isChecked())       allAllowedExtensions << FileUtils::getGuitarProFormats();
    if (btnFilterAudio_m->isChecked())    allAllowedExtensions << FileUtils::getAudioFormats();
    if (btnFilterVideo_m->isChecked())    allAllowedExtensions << FileUtils::getVideoFormats();
    if (btnFilterDocument_m->isChecked()) allAllowedExtensions << FileUtils::getDocFormats();

    if (allAllowedExtensions.isEmpty()) {
        // If nothing is selected, we display nothing at all.
        proxyModel_m->setFilterRegularExpression(QRegularExpression("^$"));
        return;
    }

    // 2. Converting "*.gp3" to regex-compatible extensions "\.gp3$"
    QStringList regexPatterns;
    for (QString ext : std::as_const(allAllowedExtensions)) {
        ext.remove('*');
        regexPatterns << QRegularExpression::escape(ext) + "$";
    }

    QString finalPattern = regexPatterns.join("|");

    proxyModel_m->setFilterRegularExpression(QRegularExpression(finalPattern, QRegularExpression::CaseInsensitiveOption));
}

void SonarLessonPage::selectSongById(int targetId) {
    // 1. Search in the SOURCE model (everything is included there)
    QModelIndexList items = sourceModel_m->match(
        sourceModel_m->index(0, 0),
        Qt::UserRole,
        targetId,
        1,
        Qt::MatchExactly
        );

    if (!items.isEmpty()) {
        QModelIndex sourceIndex = items.first();

        // 2. Map the source index to the proxy index
        QModelIndex proxyIndex = proxyModel_m->mapFromSource(sourceIndex);

        if (!proxyIndex.isValid()) {
            auto details = dbManager_m->getSongDetails(targetId);
            updateFilterButtonsForFile(details.fullPath);
            proxyIndex = proxyModel_m->mapFromSource(sourceIndex);
        }

        songSelector_m->setCurrentIndex(proxyIndex.row());
    }
}

void SonarLessonPage::setupTrainingSection(QVBoxLayout *contentLayout)
{
    QGroupBox *trainingGroup = new QGroupBox(this);
    trainingGroup->setObjectName("trainingGroup");

    QVBoxLayout *groupLayout = new QVBoxLayout(trainingGroup);
    groupLayout->setContentsMargins(0, 0, 0, 0);
    groupLayout->setSpacing(0);

    QWidget *headerWidget = new QWidget(this);
    QHBoxLayout *headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(10, 5, 10, 5);

    // Titel-Label
    QLabel *titleLabel = new QLabel(tr("Training"), this);
    titleLabel->setObjectName("sectionHeaderLabel");

    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();

    // 4. Training area content
    QWidget *contentWidget = new QWidget(this);
    QHBoxLayout *trainingLayout = new QHBoxLayout(contentWidget);
    trainingLayout->setAlignment(Qt::AlignCenter);
    trainingLayout->setContentsMargins(10, 10, 10, 10);

    // 4.1. Timer area
    timerBtn_m = new QPushButton(tr("Start Timer"), this);
    timerBtn_m->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));

    lcdNumber_m = new QLCDNumber(this);
    lcdNumber_m->setObjectName("trainingTimerDisplay");
    lcdNumber_m->display("00:00");
    refreshTimer_m = new QTimer(this);

    trainingLayout->addSpacerItem(new QSpacerItem(250, 0, QSizePolicy::Fixed));
    trainingLayout->addWidget(lcdNumber_m);

    // 4.2. Beat and tempo settings
    beatOf_m = new QSpinBox(this);
    beatOf_m->setObjectName("beatFromSpinBox");
    beatOf_m->setMinimum(1);
    beatOf_m->setMaximum(9999);

    beatTo_m = new QSpinBox(this);
    beatTo_m->setObjectName("beatToSpinBox");
    beatTo_m->setMinimum(1);
    beatTo_m->setMaximum(9999);

    practiceBpm_m = new QSpinBox(this);
    practiceBpm_m->setObjectName("bpmSpinBox");
    practiceBpm_m->setMinimum(20);  // For slow lick study
    practiceBpm_m->setMaximum(300); // Provides a buffer above a realistic maximum.
    practiceBpm_m->setValue(60);    // Good starting value
    practiceBpm_m->setSingleStep(5);
    practiceBpm_m->setSuffix(" BPM");

    btnAddReminder_m = new QPushButton(tr("Add Reminder"));
    btnAddReminder_m->setObjectName("addReminderButton");
    btnAddReminder_m->setFlat(true);

    trainingLayout->addWidget(new QLabel(tr("Beat of:")));
    trainingLayout->addWidget(beatOf_m);
    trainingLayout->addWidget(new QLabel(tr("Beat to:")));
    trainingLayout->addWidget(beatTo_m);
    trainingLayout->addWidget(new QLabel(tr("Tempo:")));
    trainingLayout->addWidget(practiceBpm_m);

    trainingLayout->addWidget(timerBtn_m);
    trainingLayout->addWidget(btnAddReminder_m);
    trainingLayout->addStretch();

    groupLayout->addWidget(headerWidget);
    groupLayout->addWidget(contentWidget);
    contentLayout->addWidget(trainingGroup);
}

void SonarLessonPage::onAddReminderClicked()
{
    int currentSongId = getCurrentSongId();
    if (currentSongId <= 0)
        return;

    QString songName;
    QString rawTitle = title_m->text().trimmed();

    if (rawTitle.isEmpty() || rawTitle == "Unknown" || rawTitle == "Unknown Title") {
        songName = songSelector_m->currentText();
    } else {
        QString artist = artist_m->text().trimmed();
        if (!artist.isEmpty() && artist != "Unknown") {
            songName = QString("%1 - %2").arg(artist, rawTitle);
        } else {
            songName = rawTitle;
        }
    }

    ReminderDialog dlg(this, beatOf_m->value(), beatTo_m->value(), practiceBpm_m->value());

    dlg.setTargetSong(currentSongId, songName);

    if (dlg.exec() == QDialog::Accepted) {
        auto res = dlg.getResults();

        if (dbManager_m->addReminder(currentSongId,
                                     res.startBar,
                                     res.endBar,
                                     res.targetBpm,
                                     res.isDaily,
                                     res.isWeekly,
                                     res.isMonthly,
                                     res.weekday,
                                     res.reminderDate)) {
            updateReminderTable(calendar_m->selectedDate());
        };
    }
}

void SonarLessonPage::onEditReminder(int reminderId, const QString title) {
    // 1. Hol die Daten aus der Liste/DB
    ReminderDialog::ReminderData oldData = dbManager_m->getReminder(reminderId);

    ReminderDialog dlg(this);
    dlg.setWindowTitle(tr("Edit Reminder"));

    dlg.setReminderData(oldData);

    dlg.setTargetSong(oldData.songId, title);

    if (dlg.exec() == QDialog::Accepted) {
        ReminderDialog::ReminderData newData = dlg.getResults();

        if(!dbManager_m->updateReminder(reminderId, newData)) {
            showSaveMessage(savedMessageFailed_m);
        } else {
            showSaveMessage(savedMessage_m);
        }

        updateReminderTable(calendar_m->selectedDate());
    }
}

void SonarLessonPage::updateReminderTable(const QDate &date)
{
    reminderTable_m->setRowCount(0);
    auto reminders = dbManager_m->getRemindersForDate(date);

    for (const auto &ref : std::as_const(reminders)) {
        QVariantMap r = ref.toMap();
        int row = reminderTable_m->rowCount();
        reminderTable_m->insertRow(row);

        auto *itemSong = new QTableWidgetItem(r["title"].toString());
        auto *itemRange = new QTableWidgetItem(r["range"].toString());
        auto *itemBpm = new QTableWidgetItem(r["bpm"].toString());

        bool done = r["is_done"].toBool();
        auto *itemStatus = new QTableWidgetItem(done ? tr("Done") : tr("Pending"));

        itemSong->setData(ReminderRole::ReminderIdRole, r["id"].toInt());
        itemSong->setData(ReminderRole::ReminderFileIdRole, r["songId"].toInt());
        itemSong->setData(ReminderRole::ReminderSongTitle, r["title"].toString());

        itemStatus->setData(Qt::UserRole, done);

        reminderTable_m->setItem(row, 0, itemSong);
        reminderTable_m->setItem(row, 1, itemRange);
        reminderTable_m->setItem(row, 2, itemBpm);
        reminderTable_m->setItem(row, 3, itemStatus);
    }

    reminderTable_m->viewport()->update();
}

void SonarLessonPage::setupNotesSection(QVBoxLayout *contentLayout)
{
    // --- CREATE COLLAPSIBLE SECTION ---
    auto *notesSection = new CollapsibleSection(tr("Notice"),
                                                true,  // collapsible
                                                false, // expandedByDefault
                                                this);
    notesSection->setObjectName("notesSection");

    // --- TOOLBAR (Formatting buttons) ---
    auto *toolbarLayout = new QHBoxLayout();
    toolbarLayout->setSpacing(2);

    btnBold_m = createFormattingButton("notesBoldButton",
                                       ":/bold.svg",
                                       QKeySequence("Ctrl+B"),
                                       tr("Bold (Ctrl+B)"));
    btnItalic_m = createFormattingButton("notesItalicButton",
                                         ":/italic.svg",
                                         QKeySequence("Ctrl+I"),
                                         tr("Italic (Ctrl+I)"));
    btnHeader1_m = createFormattingButton("notesHeader1Button",
                                          ":/heading-1.svg",
                                          QKeySequence("Ctrl+1"),
                                          tr("Heading 1 (Ctrl+1)"));
    btnHeader2_m = createFormattingButton("notesHeader2Button",
                                          ":/heading-2.svg",
                                          QKeySequence("Ctrl+2"),
                                          tr("Heading 2 (Ctrl+2)"));
    btnList_m = createFormattingButton("notesListButton",
                                       ":/minus.svg",
                                       QKeySequence("Ctrl+L"),
                                       tr("List (Ctrl+L)"));
    btnCheck_m = createFormattingButton("notesCheckButton",
                                        ":/brackets.svg",
                                        QKeySequence(Qt::CTRL | Qt::Key_Return),
                                        tr("Task List (Ctrl+Enter)"));

    toolbarLayout->addWidget(btnBold_m);
    toolbarLayout->addWidget(btnItalic_m);
    toolbarLayout->addWidget(btnHeader1_m);
    toolbarLayout->addWidget(btnHeader2_m);
    toolbarLayout->addWidget(btnList_m);
    toolbarLayout->addWidget(btnCheck_m);
    toolbarLayout->addStretch();

    // --- NOTES-EDITOR ---
    notesEdit_m = new QTextEdit();
    notesEdit_m->setObjectName("notesTextEdit");
    notesEdit_m->setPlaceholderText(tr("Write your practice notes here..."));
    notesEdit_m->setAcceptRichText(false);
    notesEdit_m->installEventFilter(this);

    auto *notesLayout = new QVBoxLayout();
    notesLayout->setSpacing(5);
    notesLayout->addLayout(toolbarLayout);
    notesLayout->addWidget(notesEdit_m);

    auto *containerWidget = new QWidget(this);
    containerWidget->setLayout(notesLayout);
    notesSection->addContentWidget(containerWidget);

    contentLayout->addWidget(notesSection);
}

QPushButton *SonarLessonPage::createFormattingButton(const QString &objectName,
                                                     const QString &iconPath,
                                                     const QKeySequence &shortcut,
                                                     const QString &tooltip)
{
    auto *button = new QPushButton(this);
    button->setObjectName(objectName);
    button->setCheckable(true);
    button->setIcon(QIcon(iconPath));
    button->setShortcut(shortcut);
    button->setToolTip(tooltip);
    button->setFixedWidth(35);
    return button;
}

bool SonarLessonPage::eventFilter(QObject *obj, QEvent *event) {
    if(isLoading_m) return false;

    if (obj == notesEdit_m && event->type() == QEvent::FocusIn) {
        if (notesEdit_m->isReadOnly()) {
            notesEdit_m->setReadOnly(false);
        }
    }
    return QWidget::eventFilter(obj, event);
}

void SonarLessonPage::setupFooterSection(QVBoxLayout *contentLayout)
{
    auto *footerLayout = new QHBoxLayout();

    // Resource buttons
    setupResourceButtons();

    // Status label
    statusLabel_m = new QLabel(this);
    statusLabel_m->setObjectName("statusMessageLabel");

    // Save button
    saveBtn_m = new QPushButton(tr("Save"));
    saveBtn_m->setObjectName("saveButton");
    saveBtn_m->setEnabled(false);

    footerLayout->addLayout(resourceLayout_m);
    footerLayout->addWidget(statusLabel_m);
    footerLayout->addWidget(saveBtn_m);

    contentLayout->addLayout(footerLayout);
}

void SonarLessonPage::setupResourceButtons()
{
    resourceLayout_m = new QHBoxLayout();

    btnPdfIcon_m = createResourceButton("docsButton",
                                        ":/file-stack.svg",
                                        tr("Open linked documents and files"));
    btnVideoIcon_m = createResourceButton("videosButton",
                                          ":/list-video.svg",
                                          tr("Open linked video files"));
    btnAudioIcon_m = createResourceButton("audiosButton",
                                          ":/audio-lines.svg",
                                          tr("Open linked audio tracks"));

    // Initially disabled
    btnPdfIcon_m->setEnabled(false);
    btnVideoIcon_m->setEnabled(false);
    btnAudioIcon_m->setEnabled(false);

    resourceLayout_m->addWidget(btnPdfIcon_m);
    resourceLayout_m->addWidget(btnVideoIcon_m);
    resourceLayout_m->addWidget(btnAudioIcon_m);
    resourceLayout_m->addStretch();
}

QPushButton *SonarLessonPage::createResourceButton(const QString &objectName,
                                                   const QString &iconPath,
                                                   const QString &tooltip)
{
    auto *button = new QPushButton(this);
    button->setObjectName(objectName);
    button->setIcon(QIcon(iconPath));
    button->setCheckable(true);
    button->setAutoExclusive(true);
    button->setToolTip(tooltip);
    return button;
}

void SonarLessonPage::setupTimer()
{
    refreshTimer_m = new QTimer(this);
    refreshTimer_m->setInterval(1000);

    lcdNumber_m = new QLCDNumber();
    lcdNumber_m->setDigitCount(5);
    lcdNumber_m->display("00:00");
    lcdNumber_m->setSegmentStyle(QLCDNumber::Filled);

    timerBtn_m = new QPushButton(tr("Start Timer"));
    timerBtn_m->setObjectName("trainingStartStopButton");
    timerBtn_m->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
}

void SonarLessonPage::sitesConnects() {
    if (isConnectionsEstablished_m) {
        return;
    }

    // Establish connection: When a song is changed in the ComboBox
    isConnectionsEstablished_m = true;

    connect(btnGpIcon_m, &QPushButton::clicked, this, [this]() {
        currentSongPath_m = QDir::cleanPath(
            songSelector_m->itemData(songSelector_m->currentIndex(), PathRole).toString());
        QString fullPath = QDir::cleanPath(currentSongPath_m);
        UIHelper::openFileWithFeedback(this, fullPath);
    });

    connect(songSelector_m,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &SonarLessonPage::onSongChanged);

    connect(notesEdit_m, &QTextEdit::textChanged, this, [this]() {
        isDirtyNotes_m = true;
        updateButtonState();
    });

    // To make the grey text disappear when the user clicks again:
    connect(notesEdit_m, &QTextEdit::selectionChanged, this, [this]() {
        static bool blockSignal = false;
        if (blockSignal)
            return;

        blockSignal = true;

        if (isPlaceholderActive_m && !notesEdit_m->toMarkdown().isEmpty()) {
            notesEdit_m->clear();
            notesEdit_m->setAcceptRichText(false);
            isPlaceholderActive_m = false;
        } else if (!isPlaceholderActive_m && notesEdit_m->toMarkdown().isEmpty()) {
            dailyNotePlaceholder();
            isPlaceholderActive_m = true;
        }

        blockSignal = false;
    });

    connect(notesEdit_m, &QTextEdit::selectionChanged, this, [this]() {
        if (isLoading_m || !notesEdit_m->hasFocus()) return;

        if (isPlaceholderActive_m) {
            isPlaceholderActive_m = false;
            notesEdit_m->clear();
            notesEdit_m->setReadOnly(false);

            // QTextCharFormat format;
            // format.setForeground(palette().windowText());
            // notesEdit_m->setCurrentCharFormat(format);
        }
    });

    connect(notesEdit_m,
            &QTextEdit::currentCharFormatChanged,
            this,
            [=, this](const QTextCharFormat &format) {
                btnBold_m->setChecked(format.fontWeight() == QFont::Bold);
                btnItalic_m->setChecked(format.fontItalic());
                notesEdit_m->setFocus();
            });

    connect(practiceTable_m, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *item) {

        int row = item->row();
        bool rowComplete = true;

        // Check all columns to see if they contain text.
        for (int col = 0; col < practiceTable_m->columnCount(); ++col) {
            QTableWidgetItem *checkItem = practiceTable_m->item(row, col);
            if (!checkItem || checkItem->text().trimmed().isEmpty()) {
                rowComplete = false;
                break;
            }
        }

        if (rowComplete) {
            isDirtyTable_m = true;
            updateButtonState();
        }
    });

    practiceTable_m->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(practiceTable_m, &QTableWidget::customContextMenuRequested, this, [this](const QPoint &pos){
        QMenu menu(this);
        QAction* fullHistory = menu.addAction(tr("Load full history"));

        QAction* selected = menu.exec(practiceTable_m->mapToGlobal(pos));
        if (selected == fullHistory) {
            auto allSessions = dbManager_m->getLastSessions(getCurrentSongId(), 0);
            updatePracticeTable(allSessions);
        }
    });

    connect(saveBtn_m, &QPushButton::pressed, this, &SonarLessonPage::onSaveClicked);

    connect(calendar_m, &QCalendarWidget::selectionChanged, this, [this]() {
        // Retrieve the newly selected date from the calendar.
        QDate selectedDate = calendar_m->selectedDate();

        // Get the current song ID from the selector.
        int songId = songSelector_m->currentData().toInt();

        // load the data for this combination
        // notesEdit_m->clear();
        loadJournalForDay(songId, selectedDate);
        updateReminderTable(selectedDate);
    });

    calendar_m->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(calendar_m,
            &QCalendarWidget::customContextMenuRequested,
            this,
            [this](const QPoint &pos) {
                QDate selectedDate = calendar_m->selectedDate();
                QMap<int, QString> songs = dbManager_m->getPracticedSongsForDay(selectedDate);

                if (songs.isEmpty())
                    return;

                // Retrieve currently selected song ID in the UI
                int currentActiveSongId = getCurrentSongId();

                QMenu menu(this);
                menu.addSection(tr("Training sessions at %1").arg(selectedDate.toString("dd.MM.")));

                for (auto it = songs.begin(); it != songs.end(); ++it) {
                    int songId = it.key();
                    QString songTitle = it.value();

                    QAction *action = menu.addAction(songTitle);

                    // HIGHLIGHT: If this song is currently active in the main window
                    if (songId == currentActiveSongId) {
                        action->setCheckable(true);
                        action->setChecked(true);
                        // Make the text bold
                        QFont font = action->font();
                        font.setBold(true);
                        action->setFont(font);
                    }

                    connect(action, &QAction::triggered, this, [this, songId]() {
                        int index = songSelector_m->findData(songId, FileIdRole);
                        if (index != -1) {
                            songSelector_m->setCurrentIndex(index);
                            calendar_m->setSelectedDate(QDate::currentDate());
                        }
                    });

                    connect(action, &QAction::triggered, this, [this, songId]() {
                        // 1. Search in the SOURCE MODE (RAM), as EVERYTHING is located there, regardless of whether it's filtered or not.
                        QModelIndexList matches = sourceModel_m->match(
                            sourceModel_m->index(0, 0),
                            SelectorRole::FileIdRole,
                            songId,
                            1,
                            Qt::MatchExactly
                            );

                        if (!matches.isEmpty()) {
                            QModelIndex sourceIndex = matches.first();

                            // 2. Check if the song is currently visible through the proxy.
                            QModelIndex proxyIndex = proxyModel_m->mapFromSource(sourceIndex);

                            if (!proxyIndex.isValid()) {
                                // Song is hidden! Retrieve the path from RAM and enable filter buttons.
                                QString filePath = sourceIndex.data(SelectorRole::PathRole).toString();
                                updateFilterButtonsForFile(filePath); // Diese Methode musst du implementieren (siehe unten)

                                // After the filter update, the ProxyIndex is now valid.
                                proxyIndex = proxyModel_m->mapFromSource(sourceIndex);
                            }

                            // 3. Select securely now
                            if (proxyIndex.isValid()) {
                                songSelector_m->setCurrentIndex(proxyIndex.row());
                                calendar_m->setSelectedDate(QDate::currentDate());
                            }
                        }
                    });
                }
                menu.exec(calendar_m->mapToGlobal(pos));
            });

    // connect with local objects
    // Bold Button
    connect(btnBold_m, &QPushButton::clicked, this, [this]() {
        QTextCharFormat format;
        format.setFontWeight(notesEdit_m->fontWeight() == QFont::Bold ? QFont::Normal : QFont::Bold);
        notesEdit_m->mergeCurrentCharFormat(format);
        notesEdit_m->setFocus();
    });

    // Italic Button
    connect(btnItalic_m, &QPushButton::clicked, this, [this]() {
        QTextCharFormat format;
        format.setFontItalic(!notesEdit_m->fontItalic());
        notesEdit_m->mergeCurrentCharFormat(format);
        notesEdit_m->setFocus();
    });

    // Heading Buttons
    // --- HEADER 1 (# ) ---
    connect(btnHeader1_m, &QPushButton::clicked, this, [this]() {
        QTextCursor cursor = notesEdit_m->textCursor();
        cursor.movePosition(QTextCursor::StartOfLine);
        cursor.insertText("# ");
        notesEdit_m->setFocus();
    });

    // --- HEADER 2 (## ) ---
    connect(btnHeader2_m, &QPushButton::clicked, this, [this]() {
        QTextCursor cursor = notesEdit_m->textCursor();
        cursor.movePosition(QTextCursor::StartOfLine);
        cursor.insertText("## ");
        notesEdit_m->setFocus();
    });

    // --- LISTING (- ) ---
    connect(btnList_m, &QPushButton::clicked, this, [this]() {
        QTextCursor cursor = notesEdit_m->textCursor();
        cursor.movePosition(QTextCursor::StartOfLine);
        cursor.insertText("- ");
        notesEdit_m->setFocus();
    });

    // --- CHECKLIST ([] ) ---
    connect(btnCheck_m, &QPushButton::clicked, this, [this]() {
        QTextCursor cursor = notesEdit_m->textCursor();
        cursor.movePosition(QTextCursor::StartOfLine);
        cursor.insertText("- [ ] ");
        notesEdit_m->setFocus();
    });

    connect(timerBtn_m, &QPushButton::clicked, this, &SonarLessonPage::onTimerButtonClicked);

    connect(refreshTimer_m, &QTimer::timeout, this, &SonarLessonPage::updateTimerDisplay);

    connect(btnAddReminder_m, &QPushButton::clicked, this, &SonarLessonPage::onAddReminderClicked);

    connect(reminderTable_m, &QTableWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        QTableWidgetItem *item = reminderTable_m->itemAt(pos);
        if (!item) return;

        int row = item->row();
        QTableWidgetItem *firstColItem = reminderTable_m->item(row, 0);

        // Retrieve IDs from the reminder data
        int reminderId = firstColItem->data(ReminderIdRole).toInt();
        int fileId     = firstColItem->data(ReminderFileIdRole).toInt();

        QString songTitle = firstColItem->data(ReminderSongTitle).toString();

        QMenu menu(this);
        QAction *loadAction = menu.addAction(tr("Select Entry"));
        QAction *openAction = menu.addAction(tr("Open Entry"));
        menu.addSeparator();
        QAction* editAct = menu.addAction(tr("Edit Reminder"));
        menu.addSeparator();
        QAction *deleteAction = menu.addAction(tr("Delete Reminder"));

        QAction *selectedAction = menu.exec(reminderTable_m->mapToGlobal(pos));
        if (!selectedAction) return;

        if(selectedAction == editAct) {
            onEditReminder(reminderId, songTitle);
        }
        menu.addSeparator();

        if (selectedAction == loadAction || selectedAction == openAction) {
            // (Toggles filter and selects in ComboBox)
            selectSongByExternalId(fileId);

            if (selectedAction == openAction) {
                QString path = songSelector_m->itemData(songSelector_m->currentIndex(), SelectorRole::PathRole).toString();
                if (!path.isEmpty()) {
                    UIHelper::openFileWithFeedback(this, QDir::cleanPath(path));
                }
            }
        } else if (selectedAction == deleteAction) {
            auto res = QMessageBox::warning(this, tr("Confirm deletion"),
                                            tr("Do you really want to delete this reminder?"),
                                            QMessageBox::Yes | QMessageBox::No);
            if(res == QMessageBox::Yes) {
                if (dbManager_m->deleteReminder(reminderId)) {
                    updateReminderTable(calendar_m->selectedDate());
                }
            }
        }
    });

    QList<QToolButton*> filterButtons = {
        btnFilterGp_m,
        btnFilterAudio_m,
        btnFilterVideo_m,
        btnFilterDocument_m
    };

    for (QToolButton* btn : filterButtons) {
        connect(btn, &QToolButton::toggled, this, &SonarLessonPage::onFilterToggled);
    }
}

void SonarLessonPage::onTimerButtonClicked() {
    if (!isTimerRunning_m) {
        // --- START ---
        elapsedTimer_m.start();
        refreshTimer_m->start(1000);
        isTimerRunning_m = true;

        timerBtn_m->setText(tr("Stop Timer"));
        timerBtn_m->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
        timerBtn_m->setStyleSheet("background-color: #e74c3c; color: white; font-weight: bold;");
    } else {
        // --- STOP ---
        qint64 ms = elapsedTimer_m.elapsed();
        // Conversion to minutes (rounded up, as 0 minutes makes little sense in the table)
        int minutes = qMax(1, static_cast<int>(std::round(ms / 60000.0)));

        isTimerRunning_m = false;
        timerBtn_m->setText(tr("Start Timer"));
        timerBtn_m->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
        timerBtn_m->setStyleSheet("background-color: transparent;");

        int startBar = beatOf_m->value();
        int endBar = beatTo_m->value();
        int currentBpm = practiceBpm_m->value();

        syncCurrentSessionToTable(startBar, endBar, currentBpm, minutes);

        lcdNumber_m->display("00:00");
    }
}

void SonarLessonPage::updateTimerDisplay() {
    if (!isTimerRunning_m) return;

    qint64 ms = elapsedTimer_m.elapsed();
    int totalSeconds = static_cast<int>(ms / 1000);
    int minutes = totalSeconds / 60;
    int seconds = totalSeconds % 60;

    // Formatting as MM:SS
    QString text = QString("%1:%2")
                       .arg(minutes, 2, 10, QChar('0'))
                       .arg(seconds, 2, 10, QChar('0'));

    lcdNumber_m->display(text);
}

void SonarLessonPage::onSongChanged(int index) {
    if (index < 0 || isLoading_m) return;

    // Data directly from the ProxyModel (which points to the SourceModel)
    QModelIndex proxyIndex = proxyModel_m->index(index, 0);

    // Different roles to read data directly from RAM
    // (You must populate these roles during the initial load!)
    QString artist = proxyModel_m->data(proxyIndex, ArtistRole).toString();
    QString title  = proxyModel_m->data(proxyIndex, TitleRole).toString();
    QString tempo  = proxyModel_m->data(proxyIndex, TempoRole).toString();
    QString tuning = proxyModel_m->data(proxyIndex, TuningRole).toString();

    artist_m->setText(artist);
    title_m->setText(title);
    tempo_m->setText(tempo);
    tuningLabel_m->setText(tuning);

    // File path for the "Open Song" button
    currentSongPath_m = proxyModel_m->data(proxyIndex, PathRole).toString();
    btnGpIcon_m->setEnabled(!currentSongPath_m.isEmpty());

    QModelIndex sourceIndex = proxyModel_m->mapToSource(proxyIndex);

    int sId = 0;

    if (sourceIndex.isValid()) {
        sId = sourceIndex.data(SelectorRole::SongIdRole).toInt();
        auto recentSessions = dbManager_m->getLastSessions(sId, 3);
        updatePracticeTable(recentSessions);
    }

    QList<DatabaseManager::RelatedFile> allFiles = dbManager_m->getFilesByRelation(sId);
    QList<DatabaseManager::RelatedFile> gpFiles, audioFiles, videoFiles, pdfFiles;

    for (const auto& file : std::as_const(allFiles)) {
        QString ext = "*." + QFileInfo(file.fileName).suffix().toLower();
        if (FileUtils::getGuitarProFormats().contains(ext)) gpFiles << file;
        else if (FileUtils::getAudioFormats().contains(ext)) audioFiles << file;
        else if (FileUtils::getVideoFormats().contains(ext)) videoFiles << file;
        else if (FileUtils::getDocFormats().contains(ext)) pdfFiles << file;
    }

    setupResourceButton(btnPdfIcon_m, pdfFiles);
    setupResourceButton(btnAudioIcon_m, audioFiles);
    setupResourceButton(btnVideoIcon_m, videoFiles);

    updateReminderTable(QDate::currentDate());
    loadJournalForDay(getCurrentSongId(), calendar_m->selectedDate());

    updateEmptyTableMessage();
}

void SonarLessonPage::setupResourceButton(QPushButton *btn,
                                          const QList<DatabaseManager::RelatedFile> &files)
{
    // Clean up existing menu if any
    if (btn->menu()) {
        btn->menu()->clear();
        btn->menu()->deleteLater();
    }

    // Disconnect only our own connections
    btn->disconnect(this);

    if (files.isEmpty()) {
        btn->setEnabled(false);
        return;
    }

    btn->setEnabled(true);

    if (files.size() == 1) {
        setupSingleFileButton(btn, files.first());
    } else {
        setupMultiFileButton(btn, files);
    }
}

void SonarLessonPage::setupSingleFileButton(QPushButton *btn,
                                            const DatabaseManager::RelatedFile &file)
{
    QString fullPath = QDir::cleanPath(file.absolutePath);
    connect(btn, &QPushButton::clicked, this, [this, fullPath]() {
        UIHelper::openFileWithFeedback(this, fullPath);
    });
}

void SonarLessonPage::setupMultiFileButton(QPushButton *btn,
                                           const QList<DatabaseManager::RelatedFile> &files)
{
    QMenu *menu = new QMenu(this);
    for (const auto &file : files) {
        QString cleanName = QFileInfo(file.fileName).baseName();
        QAction *action = menu->addAction(cleanName);

        QString fullPath = QDir::cleanPath(file.absolutePath);

        connect(action, &QAction::triggered, this, [this, fullPath]() {
            qInfo() << "[SonarLessonPage] setupResourceButton - fullPath: " << fullPath;
            UIHelper::openFileWithFeedback(this, fullPath);
        });
    }
    btn->setMenu(menu);
}

void SonarLessonPage::selectSongByExternalId(int fileId) {
    QModelIndexList matches = sourceModel_m->match(
        sourceModel_m->index(0, 0),
        SelectorRole::FileIdRole,
        fileId,
        1,
        Qt::MatchExactly);

    if (!matches.isEmpty()) {
        QModelIndex sourceIndex = matches.first();

        QString path = sourceModel_m->data(sourceIndex, SelectorRole::PathRole).toString();
        updateFilterButtonsForFile(path);

        QModelIndex proxyIndex = proxyModel_m->mapFromSource(sourceIndex);

        if (proxyIndex.isValid()) {
            songSelector_m->setCurrentIndex(proxyIndex.row());
        }

        updateFilterButtonsForFile(path);
    }
}

void SonarLessonPage::updateFilterButtonsForFile(const QString& filePath) {
    if (filePath.isEmpty()) return;

    QString ext = "*." + QFileInfo(filePath).suffix().toLower();

    // Block signals to prevent the filter from being recalculated with every click.
    const QList<QToolButton*> buttons = {btnFilterGp_m, btnFilterAudio_m, btnFilterVideo_m, btnFilterDocument_m};
    for (auto* btn : buttons) btn->blockSignals(true);

    if (FileUtils::getGuitarProFormats().contains(ext)) {
        btnFilterGp_m->setChecked(true);
    }
    else if (FileUtils::getAudioFormats().contains(ext)) {
        btnFilterAudio_m->setChecked(true);
    }
    else if (FileUtils::getVideoFormats().contains(ext)) {
        btnFilterVideo_m->setChecked(true);
    }
    else if (FileUtils::getDocFormats().contains(ext)) {
        btnFilterDocument_m->setChecked(true);
    }

    // Release signals again
    for (auto* btn : buttons) btn->blockSignals(false);

    // Update the proxy filter once
    onFilterToggled();
}

/*
 * Important: The table must be saved first, followed by the notes.
 * The notes are stored in the `practice_journal` table.
 * However, to avoid a daily loop of many entries, the current row must always be deleted and a new one created.
 * This means that if the notes were saved first, the message would be lost.
 */
void SonarLessonPage::onSaveClicked() {
    int songId = getCurrentSongId();
    if (songId <= 0) {
        qDebug() << "[SonarLessonPage] onSaveClicked wrong songId: " << songId;
        return;
    }

    bool notesSuccess = false;
    bool tableSuccess = false;
    QDate selectedDate = calendar_m->selectedDate();

    if (isDirtyTable_m) {
        tableSuccess = saveTableRowsToDatabase();
        if (tableSuccess) {
            isDirtyTable_m = false;
            showSaveMessage(savedMessage_m);
        } else {
            showSaveMessage(savedMessageFailed_m);
        }
    }

    if (isDirtyNotes_m) {
        QString dataToSave = notesEdit_m->toMarkdown();
        dataToSave.replace("\\#", "#");
        dataToSave.replace("\\-", "-");
        dataToSave.replace("\\[", "[");
        dataToSave.replace("\\]", "]");
        notesSuccess = dbManager_m->updateSongNotes(songId, dataToSave, selectedDate);
        if (notesSuccess) {
            isDirtyNotes_m = false;
            showSaveMessage(savedMessage_m);
        } else {
            showSaveMessage(savedMessageFailed_m);
        }
    }

    updateButtonState();
    updateCalendarHighlights();
    updateReminderTable(calendar_m->selectedDate());
    loadJournalForDay(songId, calendar_m->selectedDate());
}

void SonarLessonPage::showSaveMessage(QString message) {
    statusLabel_m->setText(message);
    // show for 10 secons a saved message
    QTimer::singleShot(10000, this, [this]() { statusLabel_m->clear(); });
}

bool SonarLessonPage::saveTableRowsToDatabase() {
    int songId = getCurrentSongId();
    if (songId <= 0) return false;

    auto sessions = collectTableData();

    bool allOk = dbManager_m->saveTableSessions(songId, calendar_m->selectedDate(), sessions);
    if (allOk) {
        isDirtyTable_m = false;
        showSaveMessage(savedMessage_m);
        updateButtonState();
    } else {
        showSaveMessage(savedMessageFailed_m);
    }

    return allOk;
}

void SonarLessonPage::updateButtonState() {
    saveBtn_m->setEnabled(isDirtyNotes_m || isDirtyTable_m);
}

void SonarLessonPage::dailyNotePlaceholder() {
    static QString questionOne = tr("What did you achieve today?");
    static QString questionTwo = tr("What came easily to you?");
    static QString questionThree = tr("What didn't work so well?");
    static QString questionFour = tr("What will you pay attention to tomorrow?");

    QString markdownList = "- " + questionOne + "\n" +
                           "- " + questionTwo + "\n" +
                           "- " + questionThree + "\n" +
                           "- " + questionFour;
    notesEdit_m->clear();
    notesEdit_m->setAcceptRichText(false);
    notesEdit_m->setMarkdown(markdownList);
    notesEdit_m->setReadOnly(true);
    isPlaceholderActive_m = true;
}

void SonarLessonPage::loadJournalForDay(int songId, QDate date) {
    if (songId <= 0) return;

    QString dailyNote = dbManager_m->getNoteForDay(songId, date);
    notesEdit_m->setAcceptRichText(false);

    if (!dailyNote.isEmpty()) {
        isPlaceholderActive_m = false;
        notesEdit_m->setMarkdown(dailyNote);
        notesEdit_m->setReadOnly(false);
    } else {
        dailyNotePlaceholder();
    }

    loadTableDataForDay(songId, date);

    isDirtyNotes_m = false;
    isDirtyTable_m = false;
    saveBtn_m->setEnabled(false);
}

/* Logic of collectTableData
 * This function reads the current values ​​from the QTableWidget and converts them into a list of PracticeSession objects.
 * This list is then passed to the DatabaseManager for permanent storage.
 */
QList<PracticeSession> SonarLessonPage::collectTableData() {
    QList<PracticeSession> sessions;
    for (int i = 0; i < practiceTable_m->rowCount(); ++i) {
        auto itemStart = practiceTable_m->item(i, PracticeTable::PracticeColumn::BeatFrom); // beat of
        auto itemEnd = practiceTable_m->item(i, PracticeTable::PracticeColumn::BeatTo); // beat until
        auto itemBpm = practiceTable_m->item(i, PracticeTable::PracticeColumn::BPM);    // BPM
        auto itemReps = practiceTable_m
                            ->item(i, PracticeTable::PracticeColumn::Repetitions); // Repetitions
        auto itemStreaks = practiceTable_m
                               ->item(i, PracticeTable::PracticeColumn::Duration); // Length of time

        if (itemStart && !itemStart->text().isEmpty() && itemEnd && !itemEnd->text().isEmpty()) {
            PracticeSession s;
            s.date = calendar_m->selectedDate();
            s.startBar = itemStart->text().toInt();
            s.endBar   = itemEnd->text().toInt();
            s.bpm = itemBpm->text().toInt();
            s.reps = itemReps->text().toInt();
            s.streaks = itemStreaks->text().toInt();
            sessions.append(s);
        }
    }
    return sessions;
}

int SonarLessonPage::getCurrentSongId() {
    int currentIndex = songSelector_m->currentIndex();
    return songSelector_m->itemData(currentIndex, FileIdRole).toInt();
}

// Calendar update Overload
void SonarLessonPage::updateCalendarHighlights() {
    QTextCharFormat hasDataFormat;
    hasDataFormat.setBackground(QColor(60, 100, 60)); // Dark green for experienced days
    hasDataFormat.setFontWeight(QFont::Bold);

    // Retrieve all days on which exercise entries exist.
    QList<QDate> allDates = dbManager_m->getAllPracticeDates();

    for (const QDate &date : std::as_const(allDates)) {
        // Generate the tooltip content
        QString summary = dbManager_m->getPracticeSummaryForDay(date);

        // Copy format and set tooltip
        QTextCharFormat dayFormat = hasDataFormat;
        dayFormat.setToolTip(summary);

        // Add to the calendar for this date
        calendar_m->setDateTextFormat(date, dayFormat);
    }
}

void SonarLessonPage::loadTableDataForDay(int songId, QDate date) {
    if (songId <= 0) return;

    currentSessions_m = dbManager_m->getSessionsForDay(songId, date);

    // Logic: If it's not today and "Show All" is off, only show the last 2 entries for reference.
    referenceSessions_m = dbManager_m->getLastSessions(songId, 2);

    refreshTableDisplay(date);
}

void SonarLessonPage::refreshTableDisplay(QDate date) {
    practiceTable_m->setRowCount(0);
    bool isToday = (date == QDate::currentDate());

    // First, the references (the last two, if they are not from today)
    for (const auto& s : std::as_const(referenceSessions_m)) {
        if (s.date < date) {
            addSessionToTable(s, true); // Write-protected
        }
    }

    // Then the current sessions for that day
    for (const auto& s : std::as_const(currentSessions_m)) {
        addSessionToTable(s, false);
    }

    // If selected today: The empty input line
    if (isToday) {
        int lastRow = practiceTable_m->rowCount();
        practiceTable_m->insertRow(lastRow);

        auto* itemDate = new QTableWidgetItem(date.toString("dd.MM.yyyy"));
        itemDate->setFlags(itemDate->flags() & ~Qt::ItemIsEditable);
        practiceTable_m->setItem(lastRow, 0, itemDate);
    }
}

void SonarLessonPage::addSessionToTable(const PracticeSession &s, bool isReadOnly) {
    int row = practiceTable_m->rowCount();
    practiceTable_m->insertRow(row);

    auto* itemDate = new QTableWidgetItem(s.date.toString("dd.MM.yyyy"));
    itemDate->setFlags(itemDate->flags() & ~Qt::ItemIsEditable); // Date never editable

    if (isReadOnly) {
        itemDate->setForeground(Qt::gray);
    }
    practiceTable_m->setItem(row, PracticeTable::PracticeColumn::Date, itemDate);

    QList<QString> values = {QString::number(s.startBar),
                             QString::number(s.endBar),
                             QString::number(s.bpm),
                             QString::number(s.reps),
                             QString::number(s.streaks)};

    for (int i = 0; i < values.size(); ++i) {
        auto* item = new QTableWidgetItem(values[i]);
        if (isReadOnly) {
            item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            item->setForeground(Qt::gray);
        }
        practiceTable_m->setItem(row, i + 1, item); // i+1 because of the date in column 0
    }
}

void SonarLessonPage::syncCurrentSessionToTable(int startBar, int endBar, int bpm, int minutes)
{
    const int targetRow = findOrCreateEmptyTableRow();
    if (targetRow == -1) {
        qWarning() << "No empty row found in practice table";
        return;
    }

    updateTableRow(targetRow, startBar, endBar, bpm, minutes);
    isDirtyTable_m = true;
    updateButtonState();
}

void SonarLessonPage::updateTableRow(int targetRow, int startBar, int endBar, int bpm, int minutes)
{
    for (int i = 0; i < practiceTable_m->rowCount(); ++i) {
        QTableWidgetItem* itemDuration = practiceTable_m
                                             ->item(i, PracticeTable::PracticeColumn::Duration);
        if (!itemDuration || itemDuration->text().isEmpty()) {
            targetRow = i;
            break;
        }
    }

    if(practiceTable_m->rowCount() > 0 ) {
        QTableWidgetItem *firstItem = practiceTable_m->item(0, PracticeTable::Date);
        if (firstItem) {
            QDate checkDate = QDate::fromString(firstItem->text(), "dd.MM.yyyy");
            if (!checkDate.isValid()) {
                practiceTable_m->clearSpans();
                practiceTable_m->setRowCount(0);
                addTableRow();
            }
        }
    }

    QString today = QDate::currentDate().toString("dd.MM.yyyy");
    practiceTable_m->setItem(targetRow, PracticeTable::Date, new QTableWidgetItem(today));

    practiceTable_m->setItem(targetRow,
                            PracticeTable::PracticeColumn::BeatFrom,
                            new QTableWidgetItem(QString::number(beatOf_m->value())));
    practiceTable_m->setItem(targetRow,
                            PracticeTable::PracticeColumn::BeatTo,
                            new QTableWidgetItem(QString::number(beatTo_m->value())));
    practiceTable_m->setItem(targetRow,
                            PracticeTable::PracticeColumn::BPM,
                            new QTableWidgetItem(QString::number(practiceBpm_m->value())));

    auto *itemDuration = new QTableWidgetItem(QString::number(minutes));
    itemDuration->setTextAlignment(Qt::AlignCenter);
    practiceTable_m->setItem(targetRow, PracticeTable::PracticeColumn::Duration, itemDuration);

    if (!practiceTable_m->item(targetRow, PracticeTable::PracticeColumn::Repetitions)) {
        practiceTable_m->setItem(targetRow,
                                PracticeTable::PracticeColumn::Repetitions,
                                new QTableWidgetItem(""));
    }

    practiceTable_m->setCurrentCell(targetRow, PracticeTable::PracticeColumn::Repetitions);
    practiceTable_m->editItem(practiceTable_m->item(targetRow, PracticeTable::PracticeColumn::Repetitions));
}

int SonarLessonPage::findOrCreateEmptyTableRow()
{
    for (int i = 0; i < practiceTable_m->rowCount(); ++i) {
        if (isRowEmpty(i)) {
            return i;
        }
    }

    // Create new row if none empty
    practiceTable_m->insertRow(practiceTable_m->rowCount());
    return practiceTable_m->rowCount() - 1;
}

bool SonarLessonPage::isRowEmpty(int row)
{
    for (int col = 0; col < practiceTable_m->columnCount(); ++col) {
        QTableWidgetItem* item = practiceTable_m->item(row, col);
        if (item && !item->text().trimmed().isEmpty()) {
            return false;
        }
    }
    return true;
}

void SonarLessonPage::addTableRow()
{
    if (!practiceTable_m)
        return;

    int newRow = practiceTable_m->rowCount();
    practiceTable_m->insertRow(newRow);

    practiceTable_m->setItem(newRow, 0, new QTableWidgetItem(QDate::currentDate().toString()));
}

void SonarLessonPage::removeTableRow()
{
    if (!practiceTable_m)
        return;
    practiceTable_m->removeRow(practiceTable_m->currentRow());
}

void SonarLessonPage::initialLoadFromDb() {
    isLoading_m = true;
    sourceModel_m->clear();

    auto allSongs = dbManager_m->getFilteredFiles(true, true, true, true, false);

    for (const auto &song : std::as_const(allSongs)) {
        QString display;

        // 1. Check if the artist and title fields are filled in appropriately.
        // bool hasArtist = !song.artist.isEmpty() && song.artist.toLower() != "unknown artist";
        // bool hasTitle  = !song.title.isEmpty()  && song.title.toLower()  != "unknown";
        // if (hasArtist && hasTitle) {
        //     display = song.artist + " - " + song.title + " (" + QFileInfo(song.fullPath).fileName() + ")";
        // } else {
        //     // 2.Fallback: If everything is "Unknown", we take the filename.
        //     display = QFileInfo(song.fullPath).fileName();
        // }

        display = QFileInfo(song.fullPath).fileName();

        QStandardItem* item = new QStandardItem(display);
        item->setData(QVariant::fromValue(song.id), SelectorRole::FileIdRole);
        item->setData(QVariant::fromValue(song.fullPath), SelectorRole::PathRole);
        item->setData(QVariant::fromValue(song.artist), SelectorRole::ArtistRole);
        item->setData(QVariant::fromValue(song.title), SelectorRole::TitleRole);
        item->setData(QVariant::fromValue(song.bpm), SelectorRole::TempoRole);
        item->setData(QVariant::fromValue(song.tuning), SelectorRole::TuningRole);
        item->setData(QVariant::fromValue(song.songId), SelectorRole::SongIdRole);

        sourceModel_m->appendRow(item);
    }

    if (proxyModel_m->rowCount() > 0) {
        songSelector_m->setCurrentIndex(0);
        onSongChanged(0);
    }

    isLoading_m = false;
}

void SonarLessonPage::updateEmptyTableMessage() {
    if (practiceTable_m->rowCount() == 0) {
        practiceTable_m->setRowCount(1);
        practiceTable_m->setSpan(0, 0, 1, practiceTable_m->columnCount());

        QTableWidgetItem *item = new QTableWidgetItem(tr("No data available. Press 'Start Timer' to begin..."));
        item->setTextAlignment(Qt::AlignCenter);
        item->setForeground(Qt::gray);
        item->setFlags(Qt::NoItemFlags);

        practiceTable_m->setItem(0, 0, item);
    }
}
