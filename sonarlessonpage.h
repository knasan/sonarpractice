#ifndef SONARLESSONPAGE_H
#define SONARLESSONPAGE_H

#include "databasemanager.h"

#include <QHBoxLayout>
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
    void loadData();
    void setupResourceButton(QPushButton *btn, const QList<DatabaseManager::RelatedFile> &files);
    void updateButtonState();
    void loadJournalForDay(int songId, QDate date);
    void loadTableDataForDay(int songId, QDate date);
    void dailyNotePlaceholder();
    void updateCalendarHighlights();

    [[nodiscard]] bool saveTableRowsToDatabase();
    [[nodiscard]] QList<PracticeSession> collectTableData();

    DatabaseManager* dbManager_m;
    QSqlTableModel* model_m;

    QCalendarWidget* calendar_m;
    QComboBox* songSelector_m;
    QLabel* tuningLabel_m;
    QSpinBox* tempoSpin_m;
    QTextEdit* notesEdit_m;
    QTableWidget* sessionTable_m;
    QProgressBar* dayProgress_m;

    QPushButton* pdfIcon_m;
    QPushButton* videoIcon_m;
    QPushButton* audioIcon_m;
    QPushButton* gpIcon_m;
    QPushButton* saveBtn_m;

    QHBoxLayout* resourceLayout_m;

    QString currentSongPath_m;
    int currentFileId_m;

    bool isConnectionsEstablished_m = false;
    bool isLoading_m = true;

    bool isDirtyNotes_m = false;
    bool isDirtyTable_m = false;

protected:
    void showEvent(QShowEvent *event) override; // tab wechsel triggern

private slots:
    void onSongChanged(int index);
    void onSaveClicked();
};

#endif // SONARLESSONPAGE_H
