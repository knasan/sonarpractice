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
#include "reminderdialog.h"
#include "uihelper.h"

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
    , isLoading_m(false)
    , isDirtyNotes_m(false)
    , isDirtyTable_m(false)
    , isPlaceholderActive_m(false)
    , isTimerRunning_m(false)
    , isConnectionsEstablished_m(false)
    , currentFileId_m(-1)
{
    setupTimer();
    setupUI();
    sitesConnects();
    updateButtonState();
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

    // Notes section (JETZT ÜBER der Tabelle)
    setupNotesSection(contentLayout);

    // Session table (JETZT UNTER den Notizen)
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

    sessionTable_m = new QTableWidget(PracticeTable::DEFAULT_ROW_COUNT,
                                      PracticeTable::COLUMN_COUNT,
                                      this);
    sessionTable_m->setObjectName("practiceSessionTable");
    sessionTable_m->setAlternatingRowColors(true);
    sessionTable_m->setHorizontalHeaderLabels({tr("Day"),
                                               tr("Takt from"),
                                               tr("Takt to"),
                                               tr("Tempo (BPM)"),
                                               tr("Repetitions"),
                                               tr("Duration (Min)")});
    sessionTable_m->horizontalHeader()->setObjectName("practiceTableHeader");
    sessionTable_m->horizontalHeader()->setStretchLastSection(true);
    sessionTable_m->verticalHeader()->setVisible(false);
    sessionTable_m->verticalHeader()->setObjectName("practiceTableVerticalHeader");
    sessionTable_m->setEditTriggers(QAbstractItemView::DoubleClicked
                                    | QAbstractItemView::EditKeyPressed);
    sessionTable_m->setSelectionBehavior(QAbstractItemView::SelectRows);

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

    sessionLayout->addWidget(sessionTable_m);
    sessionLayout->addLayout(buttonLayout);
    practiceSection->addContentWidget(groupSession);
    contentLayout->addWidget(practiceSection);

    connect(addBtn, &QPushButton::clicked, this, &SonarLessonPage::addTableRow);
    connect(removeBtn, &QPushButton::clicked, this, &SonarLessonPage::removeTableRow);
}

void SonarLessonPage::setupSidebar(QHBoxLayout *mainLayout)
{
    auto *sidebarLayout = new QVBoxLayout();
    sidebarLayout->setSpacing(10);

    // 1. Kalender-Sektion
    auto *calSection = new CollapsibleSection(tr("Calendar"), true, true, this);
    calendar_m = new QCalendarWidget(this);
    calendar_m->setObjectName("lessonCalendar");
    calSection->addContentWidget(calendar_m);

    // 2. Reminder-Sektion
    auto *remSection = new CollapsibleSection(tr("Today's Reminders"), true, true, this);
    remSection->setObjectName("sidebarSectionLabel");

    // Reminder-Tabelle initialisieren
    reminderTable_m = new QTableWidget(this);
    reminderTable_m->setObjectName("reminderTable");
    reminderTable_m->setColumnCount(4);
    reminderTable_m->setHorizontalHeaderLabels({tr("Song"), tr("Range"), tr("BPM"), tr("Status")});
    reminderTable_m->setContextMenuPolicy(Qt::CustomContextMenu);

    // UI-Feinschliff für die Sidebar
    reminderTable_m->verticalHeader()->setVisible(false);
    reminderTable_m->horizontalHeader()->setStretchLastSection(true);
    reminderTable_m->setSelectionBehavior(QAbstractItemView::SelectRows);
    reminderTable_m->setEditTriggers(QAbstractItemView::NoEditTriggers);

    remSection->addContentWidget(reminderTable_m);

    sidebarLayout->addWidget(calSection);
    sidebarLayout->addWidget(remSection);
    sidebarLayout->addStretch();

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
    QLabel *songLabel = new QLabel(tr("Song (GuitarPro):"), this);
    songLabel->setObjectName("songInfoLabel");
    formLayout->addWidget(songLabel, 0, 0, 1, 1);

    songSelector_m = new QComboBox(this);
    songSelector_m->setObjectName("songSelectorComboBox");
    formLayout->addWidget(songSelector_m, 0, 1, 1, 3);

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
    btnGpIcon_m = new QPushButton(tr("Open song"), this);
    btnGpIcon_m->setObjectName("songOpenButton");
    btnGpIcon_m->setIcon(style()->standardIcon(QStyle::SP_FileIcon));
    formLayout->addWidget(btnGpIcon_m, 5, 0, 1, 1);

    auto *containerWidget = new QWidget(this);
    containerWidget->setLayout(formLayout);
    songInfoSection->addContentWidget(containerWidget);

    contentLayout->addWidget(songInfoSection);
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
    uiRefreshTimer_m = new QTimer(this);

    trainingLayout->addSpacerItem(new QSpacerItem(250, 0, QSizePolicy::Fixed));
    trainingLayout->addWidget(timerBtn_m);
    trainingLayout->addWidget(lcdNumber_m);

    // 4.2. Beat and tempo settings
    beatOf_m = new QSpinBox(this);
    beatOf_m->setObjectName("beatFromSpinBox");
    beatOf_m->setMinimum(1);

    beatTo_m = new QSpinBox(this);
    beatTo_m->setObjectName("beatToSpinBox");
    beatTo_m->setMinimum(1);

    practiceBpm_m = new QSpinBox(this);
    practiceBpm_m->setObjectName("bpmSpinBox");
    practiceBpm_m->setMinimum(20);  // For slow lick study
    practiceBpm_m->setMaximum(300); // Provides a buffer above a realistic maximum.
    practiceBpm_m->setValue(60);    // Good starting value
    practiceBpm_m->setSingleStep(5);
    practiceBpm_m->setSuffix(" BPM");

    btnAddReminder_m = new QPushButton(tr("Add Reminder"));
    btnAddReminder_m->setObjectName("addReminderButton");

    trainingLayout->addWidget(new QLabel(tr("Beat of:")));
    trainingLayout->addWidget(beatOf_m);
    trainingLayout->addWidget(new QLabel(tr("Beat to:")));
    trainingLayout->addWidget(beatTo_m);
    trainingLayout->addWidget(new QLabel(tr("Tempo:")));
    trainingLayout->addWidget(practiceBpm_m);

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

    ReminderDialog dlg = ReminderDialog(this);

    dlg.setTargetSong(currentSongId, songName);

    if (dlg.exec() == QDialog::Accepted) {
        auto res = dlg.getResults();

        if (dbManager_m->addReminder(currentSongId,
                                     res.startBar,
                                     res.endBar,
                                     res.targetBpm,
                                     res.isDaily,
                                     res.isMonthly,
                                     res.weekday,
                                     res.reminderDate)) {
            updateReminderTable(calendar_m->selectedDate());
        };
    }
}

void SonarLessonPage::updateReminderTable(const QDate &date)
{
    reminderTable_m->setRowCount(0);
    auto reminders = DatabaseManager::instance().getRemindersForDate(date);

    for (const auto &ref : std::as_const(reminders)) {
        QVariantMap r = ref.toMap();
        int row = reminderTable_m->rowCount();
        reminderTable_m->insertRow(row);

        auto *itemSong = new QTableWidgetItem(r["title"].toString());
        auto *itemRange = new QTableWidgetItem(r["range"].toString());
        auto *itemBpm = new QTableWidgetItem(r["bpm"].toString());

        bool done = r["is_done"].toBool();
        auto *itemStatus = new QTableWidgetItem(done ? "Done" : "Pending");

        itemSong->setData(Qt::UserRole, r["id"].toInt());
        itemSong->setData(Qt::UserRole + 1, r["songId"].toInt());
        itemStatus->setData(Qt::UserRole, done);

        reminderTable_m->setItem(row, 0, itemSong);
        reminderTable_m->setItem(row, 1, itemRange);
        reminderTable_m->setItem(row, 2, itemBpm);
        reminderTable_m->setItem(row, 3, itemStatus);

        reminderTable_m->style()->unpolish(reminderTable_m);
        reminderTable_m->style()->polish(reminderTable_m);
    }
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

    // --- NOTIZEN-EDITOR ---
    notesEdit_m = new QTextEdit();
    notesEdit_m->setObjectName("notesTextEdit");
    notesEdit_m->setPlaceholderText(tr("Write your practice notes here..."));
    notesEdit_m->setAcceptRichText(false);

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
                                        tr("Open linked PDF files"));
    btnVideoIcon_m = createResourceButton("videoButton",
                                          ":/list-video.svg",
                                          tr("Open linked video files"));
    btnAudioIcon_m = createResourceButton("audioButton",
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
    uiRefreshTimer_m = new QTimer(this);
    uiRefreshTimer_m->setInterval(1000);

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
        if (isLoading_m)
            return;
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
            isPlaceholderActive_m = false;
        } else if (!isPlaceholderActive_m && notesEdit_m->toMarkdown().isEmpty()) {
            dailyNotePlaceholder();
            isPlaceholderActive_m = true;
        }

        blockSignal = false;
    });

    // In the setup area:
    connect(sessionTable_m, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *item) {
        if (!item || isLoading_m)
            return;

        int row = item->row();
        bool rowComplete = true;

        // Check all columns to see if they contain text.
        for (int col = 0; col < sessionTable_m->columnCount(); ++col) {
            QTableWidgetItem *checkItem = sessionTable_m->item(row, col);
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

    connect(saveBtn_m, &QPushButton::pressed, this, &SonarLessonPage::onSaveClicked);

    connect(calendar_m, &QCalendarWidget::selectionChanged, this, [this]() {
        // Retrieve the newly selected date from the calendar.
        QDate selectedDate = calendar_m->selectedDate();

        // Get the current song ID from the selector.
        int songId = songSelector_m->currentData().toInt();

        // load the data for this combination
        loadJournalForDay(songId, selectedDate);
        updateReminderTable(selectedDate);
    });

    calendar_m->setContextMenuPolicy(Qt::CustomContextMenu);

    // Context menu-Slot
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
                }
                menu.exec(calendar_m->mapToGlobal(pos));
            });

    // connect with local objects
    // Bold Button
    connect(btnBold_m, &QPushButton::clicked, this, [this]() {
        QTextCharFormat format;
        format.setFontWeight(notesEdit_m->fontWeight() == QFont::Bold ? QFont::Normal : QFont::Bold);
        notesEdit_m->mergeCurrentCharFormat(format);
    });

    // Italic Button
    connect(btnItalic_m, &QPushButton::clicked, this, [this]() {
        QTextCharFormat format;
        format.setFontItalic(!notesEdit_m->fontItalic());
        notesEdit_m->mergeCurrentCharFormat(format);
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

    connect(notesEdit_m,
            &QTextEdit::currentCharFormatChanged,
            this,
            [=, this](const QTextCharFormat &format) {
                btnBold_m->setChecked(format.fontWeight() == QFont::Bold);
                btnItalic_m->setChecked(format.fontItalic());
            });

    connect(timerBtn_m, &QPushButton::clicked, this, &SonarLessonPage::onTimerButtonClicked);

    connect(uiRefreshTimer_m, &QTimer::timeout, this, &SonarLessonPage::updateTimerDisplay);

    connect(btnAddReminder_m, &QPushButton::clicked, this, &SonarLessonPage::onAddReminderClicked);

    connect(reminderTable_m,
            &QTableWidget::customContextMenuRequested,
            this,
            [this](const QPoint &pos) {
                QTableWidgetItem *item = reminderTable_m->itemAt(pos);
                if (!item)
                    return;
                int row = item->row();
                QTableWidgetItem *firstColItem = reminderTable_m->item(row, 0);
                int reminderId = firstColItem->data(Qt::UserRole).toInt();
                int songId = firstColItem->data(Qt::UserRole + 1).toInt();

                QMenu menu(this);
                QAction *loadData = menu.addAction("Select Song");
                menu.addSeparator();
                QAction *openSongAction = menu.addAction(tr("Open Song"));
                menu.addSeparator();
                QAction *deleteAction = menu.addAction(tr("Delete Reminder"));
                QAction *selectedAction = menu.exec(reminderTable_m->mapToGlobal(pos));

                if (selectedAction == openSongAction) {
                    int index = songSelector_m->findData(songId, FileIdRole);
                    if (index != -1) {
                        QString path = songSelector_m->itemData(index, PathRole).toString();
                        QString fullPath = QDir::cleanPath(path);
                        songSelector_m->setCurrentIndex(index);
                        UIHelper::openFileWithFeedback(this, fullPath);
                    }
                } else if (selectedAction == deleteAction) {
                    if (dbManager_m->deleteReminder(reminderId)) {
                        updateReminderTable(calendar_m->selectedDate());
                    }
                } else if (selectedAction == loadData) {
                    int index = songSelector_m->findData(songId, FileIdRole);
                    songSelector_m->setCurrentIndex(index);
                }
            });
}

void SonarLessonPage::onTimerButtonClicked() {
    if (!isTimerRunning_m) {
        // --- START ---
        elapsedTimer_m.start();
        uiRefreshTimer_m->start(1000);
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

// The logic: Check files and activate icons.
void SonarLessonPage::onSongChanged(int index)
{
    if (isLoading_m || !dbManager_m) {
        return;
    }

    try {
        isLoading_m = true;
        isPlaceholderActive_m = true;

        currentSongPath_m = songSelector_m->itemData(index, PathRole).toString();

        QDate selectedDate = calendar_m->selectedDate(); // Keep current calendar day

        auto songDetails = dbManager_m->getSongDetails(getCurrentSongId());

        artist_m->setText(songDetails.artist);
        title_m->setText(songDetails.title);
        tempo_m->setText(QString::number(songDetails.bpm));
        tuningLabel_m->setText(songDetails.tuning);

        if (songDetails.practice_bpm > 0) {
            practiceBpm_m->setValue(songDetails.practice_bpm);
        } else {
            practiceBpm_m->setValue(20);
        }

        loadJournalForDay(getCurrentSongId(), selectedDate);

        // Keep lists for the file types
        QList<DatabaseManager::RelatedFile> pdfFiles, videoFiles, audioFiles;

        // Get linked files
        QList<DatabaseManager::RelatedFile> related = dbManager_m->getFilesByRelation(
            getCurrentSongId());

        for (const auto &file : std::as_const(related)) {
            QString ext = QFileInfo(file.fileName).suffix().toLower();

            DatabaseManager::RelatedFile correctedFile = file;
            // file.fileName is the relative or absolut path to the file
            if (dbManager_m->getManagedPath().isEmpty()) {
                correctedFile.relativePath = QDir::cleanPath(file.fileName);
            } else {
                correctedFile.relativePath = QDir::cleanPath(dbManager_m->getManagedPath() + "/"
                                                             + file.fileName);
            }

            if (FileUtils::getPdfFormats().contains("*." + ext))
                pdfFiles.append(correctedFile);
            else if (FileUtils::getVideoFormats().contains("*." + ext))
                videoFiles.append(correctedFile);
            else if (FileUtils::getAudioFormats().contains("*." + ext))
                audioFiles.append(correctedFile);
        }

        // Helper function for filling in the buttons
        setupResourceButton(btnPdfIcon_m, pdfFiles);
        setupResourceButton(btnVideoIcon_m, videoFiles);
        setupResourceButton(btnAudioIcon_m, audioFiles);
        isLoading_m = false;
        updateButtonState();
        updateCalendarHighlights();
        updateReminderTable(calendar_m->selectedDate());
    } catch (const std::exception &e) {
        qCritical() << "[SonarLessonPage] onSongChanged error: " << e.what();
        statusLabel_m->setText(tr("Error loading song data"));
    }
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
    QString fullPath = QDir::cleanPath(file.relativePath);
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

        QString fullPath = QDir::cleanPath(file.relativePath);

        connect(action, &QAction::triggered, this, [this, fullPath]() {
            qInfo() << "[SonarLessonPage] setupResourceButton - fullPath: " << fullPath;
            UIHelper::openFileWithFeedback(this, fullPath);
        });
    }
    btn->setMenu(menu);
}

void SonarLessonPage::loadData() {
    if (!dbManager_m) return;

    isLoading_m = true;

    songSelector_m->clear();

    auto songs = dbManager_m->loadData();

    for (const DatabaseManager::SongDetails &song : std::as_const(songs)) {
        songSelector_m->addItem(song.filePath);
        int currentIndex = songSelector_m->count() - 1;

        songSelector_m->setItemData(currentIndex, song.songId, FileIdRole);
        songSelector_m->setItemData(currentIndex, song.fullPath, PathRole);

        // Optional: Save additional data
        // songSelector_m->setItemData(currentIndex, song.artist, ArtistRole);
        // songSelector_m->setItemData(currentIndex, song.tuning, TuningRole);
    }

    if (!songs.isEmpty()) {
        const DatabaseManager::SongDetails &firstSong = songs.first();
        artist_m->setText(firstSong.artist);
        title_m->setText(firstSong.title);
        tempo_m->setText(QString::number(firstSong.bpm));
        tuningLabel_m->setText(firstSong.tuning);
    }

    isLoading_m = false;
}

// Triggers when changes have been made.
void SonarLessonPage::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    loadData(); // Reload the ComboBox
    // Update icons if a song is selected
    if (songSelector_m->currentIndex() != -1) {
        onSongChanged(songSelector_m->currentIndex());
    }
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
        if (tableSuccess) isDirtyTable_m = false;
    }

    if (isDirtyNotes_m) {
        notesSuccess = dbManager_m->updateSongNotes(songId, notesEdit_m->toMarkdown(), selectedDate);
        if (notesSuccess) {
            isDirtyNotes_m = false;
            showSaveMessage();
        }
    }

    updateButtonState();
    updateCalendarHighlights();
    updateReminderTable(calendar_m->selectedDate());
}

void SonarLessonPage::showSaveMessage() {
    static QString msgSucess = tr("Successfully saved");
    statusLabel_m->setText(msgSucess);
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
        showSaveMessage();
        updateButtonState();
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
    notesEdit_m->setMarkdown(markdownList);
}

void SonarLessonPage::loadJournalForDay(int songId, QDate date) {
    if (songId <= 0) return;

    isLoading_m = true;

    // Retrieve a note for this specific day from the database
    QString dailyNote = dbManager_m->getNoteForDay(songId, date);
    notesEdit_m->setMarkdown(dailyNote);

    if (!dailyNote.isEmpty()) {
        notesEdit_m->setTextColor(Qt::white);
        notesEdit_m->setMarkdown(dailyNote);
        isPlaceholderActive_m = false;
    } else {
        dailyNotePlaceholder();
    }

    // Load table data
    loadTableDataForDay(songId, date);

    // Reset status
    isDirtyNotes_m = false;
    isDirtyTable_m = false;
    saveBtn_m->setEnabled(false);

    isLoading_m = false;
}

/* Logic of collectTableData
 * This function reads the current values ​​from the QTableWidget and converts them into a list of PracticeSession objects.
 * This list is then passed to the DatabaseManager for permanent storage.
 */
QList<PracticeSession> SonarLessonPage::collectTableData() {
    QList<PracticeSession> sessions;
    for (int i = 0; i < sessionTable_m->rowCount(); ++i) {
        auto itemStart = sessionTable_m->item(i, PracticeTable::PracticeColumn::BeatFrom); // beat of
        auto itemEnd = sessionTable_m->item(i, PracticeTable::PracticeColumn::BeatTo); // beat until
        auto itemBpm = sessionTable_m->item(i, PracticeTable::PracticeColumn::BPM);    // BPM
        auto itemReps = sessionTable_m
                            ->item(i, PracticeTable::PracticeColumn::Repetitions); // Repetitions
        auto itemStreaks = sessionTable_m
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
    isLoading_m = true;

    currentSessions_m = dbManager_m->getSessionsForDay(songId, date);

    // Logic: If it's not today and "Show All" is off, only show the last 2 entries for reference.
    referenceSessions_m = dbManager_m->getLastSessions(songId, 2);

    refreshTableDisplay(date);

    isLoading_m = false;
}

void SonarLessonPage::refreshTableDisplay(QDate date) {
    sessionTable_m->setRowCount(0);
    bool isToday = (date == QDate::currentDate());

    // First, the references (the last two, if they are not from today)
    for (const auto& s : std::as_const(referenceSessions_m)) {
        if (s.date < date) {
            addSessionToTable(s, true); // Write-protected
        }
    }

    // Then the current sessions for that day
    for (const auto& s : std::as_const(currentSessions_m)) {
        addSessionToTable(s, false); // Editierbar
    }

    // If selected today: The empty input line
    if (isToday) {
        int lastRow = sessionTable_m->rowCount();
        sessionTable_m->insertRow(lastRow);

        auto* itemDate = new QTableWidgetItem(date.toString("dd.MM.yyyy"));
        itemDate->setFlags(itemDate->flags() & ~Qt::ItemIsEditable);
        sessionTable_m->setItem(lastRow, 0, itemDate);
    }
}

void SonarLessonPage::addSessionToTable(const PracticeSession &s, bool isReadOnly) {
    int row = sessionTable_m->rowCount();
    sessionTable_m->insertRow(row);

    auto* itemDate = new QTableWidgetItem(s.date.toString("dd.MM.yyyy"));
    itemDate->setFlags(itemDate->flags() & ~Qt::ItemIsEditable); // Date never editable

    if (isReadOnly) {
        itemDate->setForeground(Qt::gray);
    }
    sessionTable_m->setItem(row, PracticeTable::PracticeColumn::Date, itemDate);

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
        sessionTable_m->setItem(row, i + 1, item); // i+1 because of the date in column 0
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
    for (int i = 0; i < sessionTable_m->rowCount(); ++i) {
        QTableWidgetItem *itemDuration = sessionTable_m
                                             ->item(i, PracticeTable::PracticeColumn::Duration);
        if (!itemDuration || itemDuration->text().isEmpty()) {
            targetRow = i;
            break;
        }
    }

    if (targetRow < 0) {
        targetRow = sessionTable_m->rowCount();
        sessionTable_m->insertRow(targetRow);

        auto *itemDate = new QTableWidgetItem(QDate::currentDate().toString("dd.MM.yyyy"));
        itemDate->setFlags(itemDate->flags() & ~Qt::ItemIsEditable);
        sessionTable_m->setItem(targetRow, PracticeTable::PracticeColumn::Date, itemDate);
    }

    sessionTable_m->setItem(targetRow,
                            PracticeTable::PracticeColumn::BeatFrom,
                            new QTableWidgetItem(QString::number(beatOf_m->value())));
    sessionTable_m->setItem(targetRow,
                            PracticeTable::PracticeColumn::BeatTo,
                            new QTableWidgetItem(QString::number(beatTo_m->value())));
    sessionTable_m->setItem(targetRow,
                            PracticeTable::PracticeColumn::BPM,
                            new QTableWidgetItem(QString::number(practiceBpm_m->value())));

    auto *itemDuration = new QTableWidgetItem(QString::number(minutes));
    itemDuration->setTextAlignment(Qt::AlignCenter);
    sessionTable_m->setItem(targetRow, PracticeTable::PracticeColumn::Duration, itemDuration);

    if (!sessionTable_m->item(targetRow, PracticeTable::PracticeColumn::Repetitions)) {
        sessionTable_m->setItem(targetRow,
                                PracticeTable::PracticeColumn::Repetitions,
                                new QTableWidgetItem(""));
    }

    sessionTable_m->setCurrentCell(targetRow, PracticeTable::PracticeColumn::Repetitions);
    sessionTable_m->editItem(
        sessionTable_m->item(targetRow, PracticeTable::PracticeColumn::Repetitions));
}

int SonarLessonPage::findOrCreateEmptyTableRow()
{
    for (int i = 0; i < sessionTable_m->rowCount(); ++i) {
        if (isRowEmpty(i)) {
            return i;
        }
    }

    // Create new row if none empty
    sessionTable_m->insertRow(sessionTable_m->rowCount());
    return sessionTable_m->rowCount() - 1;
}

bool SonarLessonPage::isRowEmpty(int row)
{
    for (int col = 0; col < sessionTable_m->columnCount(); ++col) {
        QTableWidgetItem *item = sessionTable_m->item(row, col);
        if (item && !item->text().trimmed().isEmpty()) {
            return false;
        }
    }
    return true;
}

void SonarLessonPage::addTableRow()
{
    if (!sessionTable_m)
        return;

    int newRow = sessionTable_m->rowCount();
    sessionTable_m->insertRow(newRow);

    sessionTable_m->setItem(newRow, 0, new QTableWidgetItem(QDate::currentDate().toString()));
}

void SonarLessonPage::removeTableRow()
{
    if (!sessionTable_m)
        return;
    sessionTable_m->removeRow(sessionTable_m->currentRow());
}
