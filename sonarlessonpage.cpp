/*
 * SonarPractice - Repertoire-Management & Übungs-Journal
 * Copyright (C) 2026 [Sandy Marko Knauer alias smk and knasan]
 * Dieses Programm ist freie Software: Sie können es unter den Bedingungen
 * der GNU General Public License, wie von der Free Software Foundation,
 * Version 3 der Lizenz, veröffentlicht, weiterverteilen und/oder modifizieren.
 *
 * Dieses Programm wird in der Hoffnung verteilt, dass es nützlich sein wird,
 * aber OHNE JEDE GEWÄHRLEISTUNG; sogar ohne die implizite Gewährleistung der
 * MARKTFÄHIGKEIT oder EIGNUNG FÜR EINEN BESTIMMTEN ZWECK.
 * Siehe die GNU General Public License für weitere Details.
 */

#include "sonarlessonpage.h"
#include "databasemanager.h"
#include "fileutils.h"
#include "uihelper.h"

#include <QCalendarWidget>
#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QSqlQuery>
#include <QTableWidget>
#include <QTextEdit>

SonarLessonPage::SonarLessonPage(QWidget * parent, DatabaseManager *dbManager) : QWidget(parent), dbManager_m(dbManager), isLoading_m(false) {
    setupUI();
    loadData();

    // Initials Stand load (first song, today)
    if (songSelector_m->count() > 0) {
        // qDebug() << "SonarLessionPage load firstSongId: " << songSelector_m->currentData(FileIdRole);
        int firstSongId = songSelector_m->currentData(FileIdRole).toInt();
        loadJournalForDay(firstSongId, calendar_m->selectedDate());
        loadTableDataForDay(firstSongId, calendar_m->selectedDate());
    }
}

void SonarLessonPage::setupUI() {
    auto *layout = new QHBoxLayout(this);

    // --- SIDEBAR (left) ---
    auto *sidebar = new QVBoxLayout();
    calendar_m = new QCalendarWidget(this);
    // Style note: Use QSS here for the dark look of the calendar.
    sidebar->addWidget(new QLabel(tr("Daily Journal")));
    sidebar->addWidget(calendar_m);

    auto *journalList = new QListWidget(this);
    sidebar->addWidget(journalList);
    layout->addLayout(sidebar, 1);

    // --- MAIN (right) ---
    auto *contentLayout = new QVBoxLayout();

    // Header: Practice Entry
    auto *headerLabel = new QLabel(tr("Practice Entry - %1").arg(QDate::currentDate().toString()));
    headerLabel->setStyleSheet("font-size: 18px; font-weight: bold;");
    contentLayout->addWidget(headerLabel);

    // Form: Exercise & Song Details
    auto *formGroup = new QGroupBox(tr("Song information"));
    auto *formLayout = new QGridLayout(formGroup);

    songSelector_m = new QComboBox(this);
    tuningLabel_m = new QLabel(tr("Tuning: - "));
    tempoSpin_m = new QSpinBox(this);
    tempoSpin_m->setRange(40, 250);
    tempoSpin_m->setDisabled(true);
    tempoSpin_m->setToolTip("Automatic file extraction from Guitar Pro is planned. Coming in future versions.");
    tempoSpin_m->setSuffix(" bpm");

    formLayout->addWidget(new QLabel(tr("Song (GuitarPro):")), 0, 0);
    formLayout->addWidget(songSelector_m, 0, 1);
    formLayout->addWidget(new QLabel(tr("Tempo:")), 0, 2);
    formLayout->addWidget(tempoSpin_m, 0, 3);
    formLayout->addWidget(tuningLabel_m, 1, 0, 1, 2); // Tuning under the song selector

    gpIcon_m = new QPushButton(tr("Open song"));
    formLayout->addWidget(gpIcon_m, 1, 3);
    contentLayout->addWidget(formGroup);

    // Notes section
    contentLayout->addWidget(new QLabel(tr("Notes:")));

    notesEdit_m = new QTextEdit(this);
    contentLayout->addWidget(notesEdit_m);

    // Table: Practice session (time from/to, tempo, duration, repetitions)
    sessionTable_m = new QTableWidget(5, 6, this);
    sessionTable_m->setHorizontalHeaderLabels({tr("Day"), tr("Takt from"), tr("Takt to"), tr("Tempo (BPM)"), tr("Repetitions"), tr("Duration (Min)")});
    contentLayout->addWidget(sessionTable_m);

    showAllSessions_m = new QCheckBox(tr("Show all entries"), this);
    showAllSessions_m->setCheckState(Qt::Unchecked);
    showAllSessions_m->hide(); // TODO: Hide it until properly integrated, or check if this makes sense. It's rather optional anyway – you'd have to test it once in a weekly session.
    contentLayout->addWidget(showAllSessions_m);

    // --- FOOTER (Progress & Buttons) ---
    auto *footerLayout = new QHBoxLayout();
    dayProgress_m = new QProgressBar(this);
    dayProgress_m->setFormat(tr("Daily goal: %v / %m Min"));
    footerLayout->addWidget(dayProgress_m);

    // A container for the resource icons
    resourceLayout_m = new QHBoxLayout();
    pdfIcon_m = new QPushButton(tr("PDF"));
    videoIcon_m = new QPushButton(tr("Video"));
    audioIcon_m = new QPushButton(tr("Audio"));

    pdfIcon_m->setEnabled(false);
    videoIcon_m->setEnabled(false);
    audioIcon_m->setEnabled(false);

    resourceLayout_m->addWidget(pdfIcon_m);
    resourceLayout_m->addWidget(videoIcon_m);
    resourceLayout_m->addWidget(audioIcon_m);
    // resourceLayout_m->addWidget(gpIcon_m);
    resourceLayout_m->addStretch();

    footerLayout->insertLayout(0, resourceLayout_m);

    saveBtn_m = new QPushButton(tr("Save"));
    footerLayout->addWidget(saveBtn_m);
    saveBtn_m->setEnabled(false);

    contentLayout->addLayout(footerLayout);
    layout->addLayout(contentLayout, 3);

    sitesConnects();

    updateButtonState();
}

void SonarLessonPage::sitesConnects() {
    // Establish connection: When a song is changed in the ComboBox
    if (!isConnectionsEstablished_m) {
        isConnectionsEstablished_m = true;

        connect(gpIcon_m, &QPushButton::clicked, this, [this]() {
            if (currentSongPath_m.isEmpty()) return;

            QString fullPath = QDir::cleanPath(currentSongPath_m);
            UIHelper::openFileWithFeedback(this, fullPath);
        });

        connect(songSelector_m, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SonarLessonPage::onSongChanged);

        connect(notesEdit_m, &QTextEdit::textChanged, this, [this]() {
            if (isLoading_m) return;
            isDirtyNotes_m = true;
            updateButtonState();
        });

        // To make the grey text disappear when the user clicks again:
        connect(notesEdit_m, &QTextEdit::selectionChanged, this, [this]() {
            if (notesEdit_m->textColor() == Qt::gray) {
                notesEdit_m->clear();
                notesEdit_m->setTextColor(Qt::white);
            }
        });

        // In the setup area:
        connect(sessionTable_m, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* item) {
            if (!item || isLoading_m) return;

            int row = item->row();
            bool rowComplete = true;

            // Check all columns to see if they contain text.
            for (int col = 0; col < sessionTable_m->columnCount(); ++col) {
                QTableWidgetItem* checkItem = sessionTable_m->item(row, col);
                if (!checkItem || checkItem->text().trimmed().isEmpty()) {
                    rowComplete = false;
                    break;
                }
            }

            if (rowComplete) {
                isDirtyTable_m = true;
                addEmptyRow(QDate::currentDate());
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
        });

        // Show alle entries
        connect(showAllSessions_m, &QCheckBox::toggled, this, [this]() {
            refreshTableDisplay(calendar_m->selectedDate());
        });


        calendar_m->setContextMenuPolicy(Qt::CustomContextMenu);

        // Context menu-Slot
        connect(calendar_m, &QCalendarWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
            QDate selectedDate = calendar_m->selectedDate();
            QMap<int, QString> songs = dbManager_m->getPracticedSongsForDay(selectedDate);

            if (songs.isEmpty()) return;

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
    }

}

// The logic: Check files and activate icons.
void SonarLessonPage::onSongChanged(int index) {
    if (index < 0 || isLoading_m || !dbManager_m) {
        currentSongPath_m.clear();
        currentFileId_m = -1;
        updateButtonState();
        return;
    }

    isLoading_m = true;

    currentFileId_m = getCurrentSongId();
    currentSongPath_m = songSelector_m->itemData(index, PathRole).toString();
    qInfo() << "[SonarLessonPage] Song changed:" << currentSongPath_m << "(song_id: " << currentFileId_m << ")";

    QDate selectedDate = calendar_m->selectedDate(); // Keep current calendar day
    if (currentFileId_m > 0) {
        loadJournalForDay(currentFileId_m, selectedDate);
    }

    // Keep lists for the file types
    QList<DatabaseManager::RelatedFile> pdfFiles, videoFiles, audioFiles;

    // Get linked files
    QList<DatabaseManager::RelatedFile> related = dbManager_m->getFilesByRelation(currentFileId_m);

    for (const auto &file : std::as_const(related)) {
        QString ext = QFileInfo(file.fileName).suffix().toLower();

        DatabaseManager::RelatedFile correctedFile = file;
        correctedFile.relativePath = QDir::cleanPath(dbManager_m->getManagedPath() + "/" + file.fileName);

        if (FileUtils::getPdfFormats().contains("*." + ext)) pdfFiles.append(correctedFile);
        else if (FileUtils::getVideoFormats().contains("*." + ext)) videoFiles.append(correctedFile);
        else if (FileUtils::getAudioFormats().contains("*." + ext)) audioFiles.append(correctedFile);
    }

    // Helper function for filling in the buttons
    setupResourceButton(pdfIcon_m, pdfFiles);
    setupResourceButton(videoIcon_m, videoFiles);
    setupResourceButton(audioIcon_m, audioFiles);
    isLoading_m = false;
    updateButtonState();
}

void SonarLessonPage::setupResourceButton(QPushButton *btn, const QList<DatabaseManager::RelatedFile> &files) {
    if (btn->menu()) {
        btn->menu()->deleteLater();
        btn->setMenu(nullptr);
    }

    // Separate ONLY the signals that point to 'this' (the SonarLessonPage).
    // This leaves the internal Qt signals (such as 'destroyed') unaffected.
    btn->disconnect(this);

    if (files.isEmpty()) {
        btn->setEnabled(false);
        return;
    }

    btn->setEnabled(true);

    if (files.size() == 1) {
        // Direct click for a file
        QString fullPath = files.first().relativePath;
        connect(btn, &QPushButton::clicked, this, [this, fullPath]() {
            UIHelper::openFileWithFeedback(this, fullPath);
        });
    } else {
        // Menu for multiple files
        QMenu *menu = new QMenu(this);
        for (const auto &file : files) {
            QString cleanName = QFileInfo(file.fileName).baseName();
            QAction *action = menu->addAction(cleanName);
            QString fullPath = QDir::cleanPath(file.relativePath);

            // qDebug() << "[SonarLessonPage] setupResourceButton - file.fileName: " << file.fileName;
            // qDebug() << "[SonarLessonPage] setupResourceButton - file.relativePath: " << file.relativePath;

            // FIX for the Windows "Drive Letter" error (removes the leading / before F:/)
            // if (fullPath.startsWith("/") && fullPath.contains(":/")) {
            //     fullPath.remove(0, 1);
            // }

            connect(action, &QAction::triggered, this, [this, fullPath]() {
                qDebug() << "[SonarLessonPage] setupResourceButton - fullPath: " << fullPath;
                UIHelper::openFileWithFeedback(this, fullPath);
            });
        }
        btn->setMenu(menu);
    }
}

void SonarLessonPage::loadData() {
    if (!dbManager_m) return;

    isLoading_m = true;

    songSelector_m->clear();
    QSqlQuery query;

    QString sql = "SELECT DISTINCT "
                  "mf.song_id, "
                  "s.title, "
                  "mf.file_path, "
                  "a.name AS artist_name, "
                  "t.name AS tuning_name "
                  "FROM songs s "
                  "INNER JOIN media_files mf ON s.id = mf.song_id "
                  "LEFT JOIN artists a ON s.artist_id = a.id "
                  "LEFT JOIN tunings t ON s.tuning_id = t.id "
                  "WHERE mf.can_be_practiced = 1";

    query.prepare(sql);

    if (query.exec()) {
        while (query.next()) {
            int songId = query.value("song_id").toInt();
            QString rawPath = query.value("file_path").toString();
            QString managedPath = dbManager_m->getManagedPath();

            QString path = managedPath.isEmpty() ? rawPath : QDir(managedPath).filePath(rawPath);
            QString cleaned = QDir::cleanPath(path);

            QString fileName = QFileInfo(cleaned).fileName();
            QString title = query.value("title").toString();
            QString artist = query.value("artist_name").toString();
            QString tuning = query.value("tuning_name").toString();

            // qDebug() << "Song:" << title << "from" << artist << "Tuning:" << tuning << " path: " << path;
            // qDebug() << "SongId: " << songId;
            // qDebug() << "FileName: " << fileName;

            // The song_id is stored in the 'UserData' of the ComboBox.
             songSelector_m->addItem(fileName, songId);
             int index = songSelector_m->count() - 1;
             songSelector_m->setItemData(index, songId, FileIdRole);
             songSelector_m->setItemData(index, path, PathRole);
        }
    }

    updateCalendarHighlights();
    isLoading_m = false;
}

// Triggers when changes have been made.
void SonarLessonPage::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    loadData(); // Reload the ComboBox
    // Update icons if a song is selected
    if (songSelector_m->currentIndex() != -1) {
        onSongChanged(getCurrentSongId());
    }
}

void SonarLessonPage::onSaveClicked() {
    int songId = getCurrentSongId();
    if (songId <= 0) {
        qDebug() << "[SonarLessonPage] onSaveClicked wrong songId: " << songId;
        return;
    }

    bool notesSuccess = false;
    bool tableSuccess = false;
    QDate selectedDate = calendar_m->selectedDate();

    qDebug() << "* isDirtyNotes_m in onSaveClicked: " << isDirtyNotes_m;
    qDebug() << "* isDirtyTable_m in onSaveClicked: " << isDirtyTable_m;

    if (isDirtyNotes_m) {
        notesSuccess = dbManager_m->updateSongNotes(songId, notesEdit_m->toPlainText(), selectedDate);
        qDebug() << "* onSaveClicked - notesSuccess: " << notesSuccess;
        if (notesSuccess) isDirtyNotes_m = false;

    }

    if (isDirtyTable_m) {
        tableSuccess = saveTableRowsToDatabase();
        qDebug() << "* onSaveClicked - tableSuccess: " << tableSuccess;
        if (tableSuccess) isDirtyTable_m = false;
    }

    qDebug() << "* isDirtyNotes_m in onSaveClicked: after saved: " << isDirtyNotes_m;
    qDebug() << "* isDirtyTable_m in onSaveClicked: after saved: " << isDirtyTable_m;

    updateButtonState();
    updateCalendarHighlights();
}

bool SonarLessonPage::saveTableRowsToDatabase() {
    int songId = getCurrentSongId();
    if (songId <= 0) return false;

    auto sessions = collectTableData();

    bool allOk = dbManager_m->saveTableSessions(songId, calendar_m->selectedDate(), sessions);
    if (allOk) {
        isDirtyTable_m = false;
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

    notesEdit_m->setTextColor(Qt::gray);
    notesEdit_m->setPlainText(questionOne + "\n" + questionTwo + "\n" +
                              questionThree + "\n" + questionFour);

}

void SonarLessonPage::loadJournalForDay(int songId, QDate date) {
    if (songId <= 0) return;
    qInfo() << "[SonarLessonPage] loadJournalForDay START";

    isLoading_m = true;

    // Retrieve a note for this specific day from the database
    QString dailyNote = dbManager_m->getNoteForDay(songId, date);
    notesEdit_m->setPlainText(dailyNote);

    if (!dailyNote.isEmpty()) {
        notesEdit_m->setTextColor(Qt::white);
        notesEdit_m->setPlainText(dailyNote);
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
        // Spalte 0 ist "Day" -> Wir starten bei 1
        auto itemStart   = sessionTable_m->item(i, 1); // beat of
        auto itemEnd     = sessionTable_m->item(i, 2); // beat until
        auto itemBpm     = sessionTable_m->item(i, 3); // BPM
        auto itemReps    = sessionTable_m->item(i, 4); // Repetitions
        auto itemStreaks = sessionTable_m->item(i, 5); // Length of time

        if (itemStart && !itemStart->text().isEmpty() && itemEnd && !itemEnd->text().isEmpty()) {
            PracticeSession s;
            s.date = calendar_m->selectedDate();
            s.startBar = itemStart->text().toInt();
            s.endBar   = itemEnd->text().toInt();
            s.bpm      = itemBpm ? itemBpm->text().toInt() : 0;
            s.reps     = itemReps ? itemReps->text().toInt() : 0;
            s.streaks  = itemStreaks ? itemStreaks->text().toInt() : 0;
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
    // qDebug() << "[SonarLessonPage] loadTableDataForDay currentSessions_m.size : " << currentSessions_m.size();

    // Logic: If it's not today and "Show All" is off, only show the last 2 entries for reference.
    referenceSessions_m = dbManager_m->getLastSessions(songId, 2);
    // qDebug() << "[SonarLessonPage] loadTableDataForDay referenceSessions_m.size : " << referenceSessions_m.size();

    refreshTableDisplay(date);

    isLoading_m = false;
}

void SonarLessonPage::refreshTableDisplay(QDate date) {
    sessionTable_m->setRowCount(0);
    bool isToday = (date == QDate::currentDate());

    // First, the references (the last two, if they are not from today)
    for (const auto& s : std::as_const(referenceSessions_m)) {
        qDebug() << "referenzen s.date: " << s.date << ", date: " << date;
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

void SonarLessonPage::addEmptyRow(QDate date) {
    // Security check: If the last line is already empty, do nothing.
    int lastRow = sessionTable_m->rowCount() - 1;
    if (lastRow >= 0) {
        // Check, for example, if the BPM field in the last row is still empty.
        if (sessionTable_m->item(lastRow, 3) == nullptr ||
            sessionTable_m->item(lastRow, 3)->text().isEmpty()) {
            return;
        }
    }

    int row = sessionTable_m->rowCount();
    sessionTable_m->insertRow(row);

    // set date
    auto* dateItem = new QTableWidgetItem(date.toString("dd.MM.yyyy"));
    dateItem->setFlags(dateItem->flags() & ~Qt::ItemIsEditable); // Read only
    sessionTable_m->setItem(row, 0, dateItem);

    // Create empty items for the remaining columns (1-5) so that the user can click directly into them and itemChanged fires correctly.
    for (int col = 1; col <= 5; ++col) {
        sessionTable_m->setItem(row, col, new QTableWidgetItem(""));
    }

    sessionTable_m->scrollToBottom(); // Komfort-Feature
}

void SonarLessonPage::addSessionToTable(const PracticeSession &s, bool isReadOnly) {
    int row = sessionTable_m->rowCount();
    sessionTable_m->insertRow(row);

    auto* itemDate = new QTableWidgetItem(s.date.toString("dd.MM.yyyy"));
    itemDate->setFlags(itemDate->flags() & ~Qt::ItemIsEditable); // Date never editable

    if (isReadOnly) {
        itemDate->setForeground(Qt::gray);
    }
    sessionTable_m->setItem(row, 0, itemDate);

    QList<QString> values = {
        QString::number(s.startBar),
        QString::number(s.endBar),
        QString::number(s.bpm),
        QString::number(s.reps),
        QString::number(s.streaks)
    };

    for (int i = 0; i < values.size(); ++i) {
        auto* item = new QTableWidgetItem(values[i]);
        if (isReadOnly) {
            item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            item->setForeground(Qt::gray);
        }
        sessionTable_m->setItem(row, i + 1, item); // i+1 because of the date in column 0
    }
}
