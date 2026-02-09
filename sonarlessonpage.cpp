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
#include <QHeaderView>
#include <QMenu>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QSqlQuery>
#include <QTableWidget>
#include <QTextEdit>
#include <QLCDNumber>
#include <QTimer>

SonarLessonPage::SonarLessonPage(QWidget * parent, DatabaseManager *dbManager) : QWidget(parent), dbManager_m(dbManager), isLoading_m(false) {
    setupUI();
    loadData();
}

void SonarLessonPage::setupUI() {
    auto *layout = new QHBoxLayout(this);

    // --- SIDEBAR (left) ---
    auto *sidebar = new QVBoxLayout();
    calendar_m = new QCalendarWidget(this);
    calendar_m->setObjectName("lessonCalendar");

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
    auto *groupSongInformation = new QGroupBox(tr("Song information"));
    groupSongInformation->setObjectName("styledGroup");
    auto *formLayout = new QGridLayout(groupSongInformation);

    songSelector_m = new QComboBox(this);
    songSelector_m->setObjectName("songSelection");
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

    lcdNumber_m = new QLCDNumber(this);
    lcdNumber_m->display("00:00");
    uiRefreshTimer_m = new QTimer(this);

    formLayout->addWidget(lcdNumber_m, 0, 4);

    formLayout->addWidget(tuningLabel_m, 1, 0, 1, 2); // Tuning under the song selector

    timerBtn_m = new QPushButton(tr("Start Timer"), this);
    timerBtn_m->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    formLayout->addWidget(timerBtn_m, 1, 4);

    gpIcon_m = new QPushButton(tr("Open song"));
    formLayout->addWidget(gpIcon_m, 1, 3);

    contentLayout->addWidget(groupSongInformation);

    // Notes section
    contentLayout->addWidget(new QLabel(tr("Notes:")));

    notesEdit_m = new QTextEdit(this);

    auto *toolbarLayout = new QHBoxLayout();
    toolbarLayout->setSpacing(5);

    // bold button
    QPushButton *btnBold = new QPushButton(this);
    btnBold->setObjectName("boldButton");
    btnBold->setCheckable(true);
    btnBold->setIcon(QIcon(":/bold.svg"));
    btnBold->setShortcut(QKeySequence("Ctrl+B"));
    btnBold->setToolTip(tr("Bold (Ctrl+B)"));
    btnBold->setFixedWidth(35);

    // italic button
    QPushButton *btnItalic = new QPushButton(this);
    btnItalic->setObjectName("italicButton");
    btnItalic->setCheckable(true);
    btnItalic->setIcon(QIcon(":/italic.svg"));
    btnItalic->setShortcut(QKeySequence("Ctrl+I"));
    btnItalic->setToolTip(tr("Italic (Ctrl+I)"));
    btnItalic->setFixedWidth(35);

    // heading buttons
    QPushButton *btnHeader1 = new QPushButton(this);
    btnHeader1->setObjectName("heading1Button");
    btnHeader1->setCheckable(true);
    btnHeader1->setIcon(QIcon(":/heading-1.svg"));
    btnHeader1->setShortcut(QKeySequence("Ctrl+1"));
    btnHeader1->setToolTip(tr("Heading 1 (Ctrl+1)"));
    btnHeader1->setFixedWidth(35);

    QPushButton *btnHeader2 = new QPushButton(this);
    btnHeader2->setObjectName("heading2Button");
    btnHeader2->setCheckable(true);
    btnHeader2->setIcon(QIcon(":/heading-2.svg"));
    btnHeader2->setShortcut(QKeySequence("Ctrl+2"));
    btnHeader2->setToolTip(tr("heading 2 (Ctrl+2)"));
    btnHeader2->setFixedWidth(35);

    // list button
    QPushButton *btnList = new QPushButton(this);
    btnList->setObjectName("listButton");
    btnList->setCheckable(true);
    btnList->setIcon(QIcon(":/minus.svg"));
    btnList->setShortcut(QKeySequence("Ctrl+L"));
    btnList->setToolTip(tr("List(Ctrl+L)"));
    btnList->setFixedWidth(35);

    // btnCheck
    QPushButton *btnCheck = new QPushButton(this);
    btnCheck->setObjectName("checkButton");
    btnCheck->setCheckable(true);
    btnCheck->setIcon(QIcon(":/brackets.svg"));
    btnCheck->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return)); // Key_Return ist die Enter-Taste
    btnCheck->setToolTip(tr("Task List (Ctrl+Enter)"));
    btnCheck->setFixedWidth(35);

    toolbarLayout->addWidget(btnBold);
    toolbarLayout->addWidget(btnItalic);
    toolbarLayout->addWidget(btnHeader1);
    toolbarLayout->addWidget(btnHeader2);
    toolbarLayout->addWidget(btnList);
    toolbarLayout->addWidget(btnCheck);
    toolbarLayout->addStretch();

    contentLayout->addLayout(toolbarLayout);
    contentLayout->addWidget(notesEdit_m);

    // Table: Practice session (time from/to, tempo, duration, repetitions)
    sessionTable_m = new QTableWidget(5, 6, this);
    sessionTable_m->setObjectName("journalTable");
    sessionTable_m->setAlternatingRowColors(true);
    sessionTable_m->setHorizontalHeaderLabels({tr("Day"), tr("Takt from"), tr("Takt to"), tr("Tempo (BPM)"), tr("Repetitions"), tr("Duration (Min)")});

    sessionTable_m->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    contentLayout->addWidget(sessionTable_m);

    // FIXIT: show empty lines
    // showAllSessions_m = new QCheckBox(tr("Show all entries"), this);
    // showAllSessions_m->setCheckState(Qt::Unchecked);
    // // showAllSessions_m->hide(); // TODO: Hide it until properly integrated, or check if this makes sense. It's rather optional anyway – you'd have to test it once in a weekly session.
    // contentLayout->addWidget(showAllSessions_m);

    // --- FOOTER (Progress & Buttons) ---
    auto *footerLayout = new QHBoxLayout();
    //dayProgress_m = new QProgressBar(this);
    //dayProgress_m->setFormat(tr("Daily goal: %v / %m Min"));
    //footerLayout->addWidget(dayProgress_m);

    // A container for the resource icons
    resourceLayout_m = new QHBoxLayout();
    pdfIcon_m = new QPushButton(this);

    pdfIcon_m->setObjectName("docsButton");
    pdfIcon_m->setIcon(QIcon(":/file-stack.svg"));
    pdfIcon_m->setCheckable(true);
    pdfIcon_m->setAutoExclusive(true);
    pdfIcon_m->setToolTip(tr("Open linked documents"));

    videoIcon_m = new QPushButton(this);
    videoIcon_m->setObjectName("videosButton");
    videoIcon_m->setIcon(QIcon(":/list-video.svg"));
    videoIcon_m->setCheckable(true);
    videoIcon_m->setAutoExclusive(true);
    videoIcon_m->setToolTip(tr("Open linked video files"));

    audioIcon_m = new QPushButton(this);
    audioIcon_m->setObjectName("audioButton");
    audioIcon_m->setIcon(QIcon(":/audio-lines.svg"));
    audioIcon_m->setCheckable(true);
    audioIcon_m->setAutoExclusive(true);
    audioIcon_m->setToolTip(tr("Open linked audio tracks"));

    pdfIcon_m->setEnabled(false);
    videoIcon_m->setEnabled(false);
    audioIcon_m->setEnabled(false);

    resourceLayout_m->addWidget(pdfIcon_m);
    resourceLayout_m->addWidget(videoIcon_m);
    resourceLayout_m->addWidget(audioIcon_m);
    // resourceLayout_m->addWidget(gpIcon_m);
    resourceLayout_m->addStretch();

    footerLayout->insertLayout(0, resourceLayout_m);

    statusLabel_m = new QLabel(this);
    statusLabel_m->setObjectName("statusMessageLabel");
    footerLayout->addWidget(statusLabel_m);

    saveBtn_m = new QPushButton(tr("Save"));
    footerLayout->addWidget(saveBtn_m);
    saveBtn_m->setEnabled(false);

    contentLayout->addLayout(footerLayout);

    layout->addLayout(contentLayout, 3);

    // connect with local objects
    // Bold Button
    connect(btnBold, &QPushButton::clicked, this, [this]() {
        QTextCharFormat format;
        format.setFontWeight(notesEdit_m->fontWeight() == QFont::Bold ? QFont::Normal : QFont::Bold);
        notesEdit_m->mergeCurrentCharFormat(format);
    });

    // Italic Button
    connect(btnItalic, &QPushButton::clicked, this, [this]() {
        QTextCharFormat format;
        format.setFontItalic(!notesEdit_m->fontItalic());
        notesEdit_m->mergeCurrentCharFormat(format);
    });

    // Heading Buttons
    // --- HEADER 1 (# ) ---
    connect(btnHeader1, &QPushButton::clicked, this, [this]() {
        QTextCursor cursor = notesEdit_m->textCursor();
        cursor.movePosition(QTextCursor::StartOfLine);
        cursor.insertText("# ");
        notesEdit_m->setFocus();
    });

    // --- HEADER 2 (## ) ---
    connect(btnHeader2, &QPushButton::clicked, this, [this]() {
        QTextCursor cursor = notesEdit_m->textCursor();
        cursor.movePosition(QTextCursor::StartOfLine);
        cursor.insertText("## ");
        notesEdit_m->setFocus();
    });

    // --- AUFLISTUNG (- ) ---
    connect(btnList, &QPushButton::clicked, this, [this]() {
        QTextCursor cursor = notesEdit_m->textCursor();
        cursor.movePosition(QTextCursor::StartOfLine);
        cursor.insertText("- ");
        notesEdit_m->setFocus();
    });

    // --- CHECKLISTE / ERLEDIGT ([ ] ) ---
    connect(btnCheck, &QPushButton::clicked, this, [this]() {
        QTextCursor cursor = notesEdit_m->textCursor();
        cursor.movePosition(QTextCursor::StartOfLine);
        cursor.insertText("- [ ] ");
        notesEdit_m->setFocus();
    });

    // Cursor-Update: Buttons aktivieren/deaktivieren, wenn man durch Text klickt
    connect(notesEdit_m, &QTextEdit::currentCharFormatChanged, this, [=](const QTextCharFormat &format) {
        btnBold->setChecked(format.fontWeight() == QFont::Bold);
        btnItalic->setChecked(format.fontItalic());
    });

    sitesConnects();

    updateButtonState();
}

void SonarLessonPage::sitesConnects() {
    // Establish connection: When a song is changed in the ComboBox
    if (!isConnectionsEstablished_m) {
        isConnectionsEstablished_m = true;

        connect(gpIcon_m, &QPushButton::clicked, this, [this]() {
            currentSongPath_m = QDir::cleanPath(songSelector_m->itemData(songSelector_m->currentIndex(), PathRole).toString());
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
            static bool blockSignal = false;
            if (blockSignal) return;

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
        // TODO: show all entries dont work, enable this for testings and bug fixing
        // connect(showAllSessions_m, &QCheckBox::toggled, this, [this]() {
        //     refreshTableDisplay(calendar_m->selectedDate());
        // });

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

        connect(timerBtn_m, &QPushButton::clicked, this, &SonarLessonPage::onTimerButtonClicked);

        connect(uiRefreshTimer_m, &QTimer::timeout, this, &SonarLessonPage::updateTimerDisplay);
    }
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

        addTimeToTable(minutes);
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
void SonarLessonPage::onSongChanged(int index) {
    if (index < 0 << isLoading_m || !dbManager_m) {
        currentSongPath_m.clear();
        currentFileId_m = -1;
        updateButtonState();
        return;
    }

    isLoading_m = true;
    isPlaceholderActive_m = true;

    currentSongPath_m = songSelector_m->itemData(index, PathRole).toString();
    // qDebug() << "[SonarLessonPage] onSongChanged index: " << index << " currentSonPath_m : " << currentSongPath_m;

    QDate selectedDate = calendar_m->selectedDate(); // Keep current calendar day

    loadJournalForDay(getCurrentSongId(), selectedDate);

    // Keep lists for the file types
    QList<DatabaseManager::RelatedFile> pdfFiles, videoFiles, audioFiles;

    // Get linked files
    QList<DatabaseManager::RelatedFile> related = dbManager_m->getFilesByRelation(getCurrentSongId());

    for (const auto &file : std::as_const(related)) {
        QString ext = QFileInfo(file.fileName).suffix().toLower();

        DatabaseManager::RelatedFile correctedFile = file;
        // file.fileName is the relative or absolut path to the file
        if (dbManager_m->getManagedPath().isEmpty()) {
            correctedFile.relativePath = QDir::cleanPath(file.fileName);
        } else {
            correctedFile.relativePath = QDir::cleanPath(dbManager_m->getManagedPath() + "/" + file.fileName);
        }


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
        QString fullPath = QDir::cleanPath(files.first().relativePath);
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

            connect(action, &QAction::triggered, this, [this, fullPath]() {
                qInfo() << "[SonarLessonPage] setupResourceButton - fullPath: " << fullPath;
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
}

void SonarLessonPage::showSaveMessage() {
    static QString msgSucess = tr("Successfully saved");
    statusLabel_m->setText(msgSucess);
    // show for 10 secons a saved message
    QTimer::singleShot(10000, [this]() {
        statusLabel_m->clear();
    });
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

    // Logic: If it's not today and "Show All" is off, only show the last 2 entries for reference.
    referenceSessions_m = dbManager_m->getLastSessions(songId, 2);

    refreshTableDisplay(date);

    addEmptyRow(QDate::currentDate());

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

void SonarLessonPage::addTimeToTable(int minutes) {
    // Find or select an empty line from the currently selected line.
    int row = sessionTable_m->currentRow();
    if (row < 0) {
        // Find the first row where "Duration" (column 5) is still empty.
        for (int i = 0; i < sessionTable_m->rowCount(); ++i) {
            if (!sessionTable_m->item(i, 5) || sessionTable_m->item(i, 5)->text().isEmpty()) {
                row = i;
                break;
            }
        }
    }

    // If the table is full, insert a new row.
    if (row < 0) {
        row = sessionTable_m->rowCount();
        sessionTable_m->insertRow(row);
    }

    // Enter time (column 5: Duration (min))
    QTableWidgetItem *item = new QTableWidgetItem(QString::number(minutes));
    item->setTextAlignment(Qt::AlignCenter);
    sessionTable_m->setItem(row, 5, item);

    // Focus on the line so the user only has to type BPM/beats.
    sessionTable_m->setCurrentCell(row, 1);
}
