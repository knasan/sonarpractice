#ifndef SONARLESSONPAGE_H
#define SONARLESSONPAGE_H

#include "databasemanager.h"

#include <QCheckBox>
#include <QElapsedTimer>
#include <QHBoxLayout>
#include <QLCDNumber>
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
    explicit SonarLessonPage(QWidget *parent = nullptr, DatabaseManager *dbManager = nullptr);

    enum SelectorRole {
        FileIdRole = Qt::UserRole + 1,
        PathRole = Qt::UserRole
    };

private:
    void setupUI();
    void sitesConnects();
    void onTimerButtonClicked();
    void setupResourceButton(QPushButton *btn, const QList<DatabaseManager::RelatedFile> &files);
    void updateButtonState();
    void loadJournalForDay(int songId, QDate date);
    void loadTableDataForDay(int songId, QDate date);
    void refreshTableDisplay(QDate date);
    void showSaveMessage();
    void addEmptyRow(QDate date);
    void addSessionToTable(const PracticeSession &s, bool isReadOnly);
    void addTimeToTable(int minutes);

    void dailyNotePlaceholder();
    void updateCalendarHighlights();

    [[nodiscard]] bool saveTableRowsToDatabase();
    [[nodiscard]] QList<PracticeSession> collectTableData();

    [[nodiscard]] int getCurrentSongId();

    DatabaseManager* dbManager_m;
    QSqlTableModel* model_m;

    QCalendarWidget* calendar_m;
    QComboBox* songSelector_m;
    QLabel* tuningLabel_m;
    QSpinBox* tempoSpin_m;
    QTextEdit* notesEdit_m;
    QTableWidget* sessionTable_m;
    // QProgressBar* dayProgress_m; // unused yet

    QPushButton* pdfIcon_m;
    QPushButton* videoIcon_m;
    QPushButton* audioIcon_m;
    QPushButton* gpIcon_m;
    QPushButton* saveBtn_m;
    QPushButton* timerBtn_m;

    QCheckBox* showAllSessions_m;

    QHBoxLayout* resourceLayout_m;

    QLabel* statusLabel_m;

    QString currentSongPath_m;
    int currentFileId_m{0};

    bool isConnectionsEstablished_m{false};
    bool isLoading_m{true};

    bool isDirtyNotes_m{false};
    bool isDirtyTable_m{false};
    bool isPlaceholderActive_m{true};

    QTimer *uiRefreshTimer_m;                   // Timer for UI update (1-second intervals)
    QElapsedTimer elapsedTimer_m;
    bool isTimerRunning_m{false};

    QLCDNumber* lcdNumber_m;

    QList<PracticeSession> currentSessions_m;   // Buffering the loaded data
    QList<PracticeSession> referenceSessions_m; // Buffering the last sessions data

protected:
    void showEvent(QShowEvent *event) override; // trigger tab switch

private slots:
    void onSongChanged(int index);
    void onSaveClicked();
    void updateTimerDisplay();

public slots:
    void loadData();
};

#endif // SONARLESSONPAGE_H
