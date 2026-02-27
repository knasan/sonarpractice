#ifndef SONARLESSONPAGE_H
#define SONARLESSONPAGE_H

#include "databasemanager.h"

#include <QCheckBox>
#include <QElapsedTimer>
#include <QHBoxLayout>
#include <QLCDNumber>
#include <QLineEdit>
#include <QSortFilterProxyModel>
#include <QStandardItem>
#include <QWidget>

class QPushButton;
class QProgressBar;
class QTableWidget;
class QTextEdit;
class QSpinBox;
class QLabel;
class QComboBox;
class QSqlTableModel;
class QCalendarWidget;

class SonarLessonPage : public QWidget
{
    Q_OBJECT

public:
    explicit SonarLessonPage(DatabaseManager *dbManager = nullptr, QWidget *parent = nullptr);

    enum SelectorRole {
        FileIdRole = Qt::UserRole + 1,
        PathRole = Qt::UserRole + 2,
        ArtistRole = Qt::UserRole + 3,
        TitleRole = Qt::UserRole + 4,
        TempoRole = Qt::UserRole + 5,
        TuningRole = Qt::UserRole + 6,
        TypeRole = Qt::UserRole + 7,
        SongIdRole = Qt::UserRole + 8,
    };

    enum ReminderRole {
        ReminderIdRole  = Qt::UserRole + 1,
        ReminderFileIdRole = Qt::UserRole + 2,
        ReminderSongTitle = Qt::UserRole + 3,
    };
protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void setupUI();
    void setupSidebar(QHBoxLayout *mainLayout);
    void setupSongInformationSection(QVBoxLayout *contentLayout);
    void setupTrainingSection(QVBoxLayout *contentLayout);
    void setupNotesSection(QVBoxLayout *contentLayout);
    void setupPracticeTable(QVBoxLayout *contentLayout);

    QPushButton *createFormattingButton(const QString &objectName,
                                        const QString &iconPath,
                                        const QKeySequence &shortcut,
                                        const QString &tooltip);
    void setupFooterSection(QVBoxLayout *contentLayout);
    void setupResourceButtons();
    QPushButton *createResourceButton(const QString &objectName,
                                      const QString &iconPath,
                                      const QString &tooltip);
    void setupTimer();

    void selectSongByExternalId(int songId);

    void sitesConnects();
    void onTimerButtonClicked();

    void setupResourceButton(QPushButton *btn, const QList<DatabaseManager::RelatedFile> &files);
    void setupSingleFileButton(QPushButton *btn, const DatabaseManager::RelatedFile &file);
    void setupMultiFileButton(QPushButton *btn, const QList<DatabaseManager::RelatedFile> &files);

    void updateButtonState();

    void loadJournalForDay(int songId, QDate date);
    void loadTableDataForDay(int songId, QDate date);
    void refreshTableDisplay(QDate date);
    void showSaveMessage(QString message);

    // TODO: rename to addPracticeToTable
    void addSessionToTable(const PracticeSession &s, bool isReadOnly);

    void syncCurrentSessionToTable(int startBar, int endBar, int currentBpm, int minutes);
    void updatePracticeHistory(int fileId);
    void updatePracticeTable(const QList<PracticeSession>& sessions);

    void updateFilterButtonsForFile(const QString& filePath);

    void updateReminderTable(const QDate &date);

    [[nodiscard]] int findOrCreateEmptyTableRow();
    [[nodiscard]] bool isRowEmpty(int row);
    void updateTableRow(int targetRow, int startBar, int endBar, int bpm, int minutes);

    void dailyNotePlaceholder();
    void updateCalendarHighlights();

    [[nodiscard]] bool saveTableRowsToDatabase();
    [[nodiscard]] QList<PracticeSession> collectTableData();

    [[nodiscard]] int getCurrentSongId();

    void initialLoadFromDb();
    void updateEmptyTableMessage();

    QString savedMessage_m = tr("Successfully saved");
    QString savedMessageFailed_m = tr("Saved failed");

    // UI Elements
    QComboBox* songSelector_m;
    QLabel* artist_m;
    QLabel* title_m;
    QLabel* tempo_m;
    QLabel* tuningLabel_m;
    QPushButton* btnGpIcon_m;
    // TODO': Tuning Notes

    // Training Controls
    QSpinBox* beatOf_m;
    QSpinBox* beatTo_m;
    QSpinBox* practiceBpm_m;
    QTableWidget* practiceTable_m;

    // Notes Section
    QTextEdit* notesEdit_m;
    QPushButton* btnBold_m;
    QPushButton* btnItalic_m;
    QPushButton* btnHeader1_m;
    QPushButton* btnHeader2_m;
    QPushButton* btnList_m;
    QPushButton* btnCheck_m;
    QPushButton* btnAddReminder_m;

    // Resource Buttons
    QPushButton* btnPdfIcon_m;
    QPushButton* btnVideoIcon_m;
    QPushButton* btnAudioIcon_m;

    // Timer Controls
    QPushButton* timerBtn_m;
    QLCDNumber* lcdNumber_m;
    QTimer* refreshTimer_m;
    QElapsedTimer elapsedTimer_m;

    // Status and State
    QLabel *statusLabel_m;
    QPushButton *saveBtn_m;
    QCalendarWidget *calendar_m;
    QTableWidget *reminderTable_m;
    DatabaseManager *dbManager_m;
    QSqlTableModel *model_m;

    // ToolBox
    QToolButton *btnFilterGp_m;
    QToolButton *btnFilterAudio_m;
    QToolButton *btnFilterVideo_m;
    QToolButton *btnFilterDocument_m;

    // State Flags
    bool isLoading_m{true};
    bool isDirtyNotes_m{false};
    bool isDirtyTable_m{false};
    bool isPlaceholderActive_m{true};
    bool isTimerRunning_m{false};
    bool isConnectionsEstablished_m{false};

    // Data
    QString currentSongPath_m;
    int currentFileId_m{-1};
    QList<PracticeSession> currentSessions_m;
    QList<PracticeSession> referenceSessions_m;

    QCheckBox *showAllSessions_m;
    QHBoxLayout *resourceLayout_m;

    QStandardItemModel *sourceModel_m;
    QSortFilterProxyModel *proxyModel_m;

// protected:
//     void showEvent(QShowEvent *event) override; // trigger tab switch

private slots:
    void onSongChanged(int songId);
    void onSaveClicked();
    void updateTimerDisplay();
    void onEditSongClicked();
    void onFilterToggled();
    void selectSongById(int targetId);
    void onEditReminder(int reminderId, const QString title);

public slots:
    void addTableRow();
    void removeTableRow();
    void onAddReminderClicked();

};

#endif // SONARLESSONPAGE_H
