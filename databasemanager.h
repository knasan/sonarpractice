#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QSqlDatabase>
#include <QString>
#include <QObject>
#include <QVariant>
#include <QDate>

struct PracticeSession {
    QDate date;
    int startBar;
    int endBar;
    int bpm;
    int reps;
    int streaks;
};

class DatabaseManager : public QObject {
    Q_OBJECT
public:
    struct RelatedFile {
        int id;
        QString fileName;
        QString relativePath;

        int songId;
        QString type;
        QString title;
        QString pathOrUrl;
    };

    // Singleton & Lifecycle
    static DatabaseManager& instance();
    DatabaseManager(const DatabaseManager&) = delete;
    void operator=(const DatabaseManager&) = delete;
    explicit DatabaseManager(QObject *parent = nullptr) : QObject(parent) {}
    [[nodiscard]] bool initDatabase(const QString &dbPath);
    void closeDatabase();
    QSqlDatabase database() const;

    // Setup & Metadata (Initialization & Versioning)
    [[nodiscard]] bool createInitialTables();
    [[nodiscard]] bool hasData();
    [[nodiscard]] int getDatabaseVersion();
    [[nodiscard]] bool setDatabaseVersion(int version);

    // File Management & Media (Files & Relations)
    [[nodiscard]] bool addFileToSong(qlonglong songId, const QString &filePath, bool isManaged, const QString &fileType, qint64 fileSize, QString fileHash);
    [[nodiscard]] bool updatePracticeFlag(int fileId, bool canPractice);
    [[nodiscard]] QString getManagedPath();
    [[nodiscard]] qlonglong createSong(const QString &title,
                                       const QString &artist = "Unknown",
                                       const QString &tuning = "Unknown");

    [[nodiscard]] int getOrCreateArtist(const QString &name);
    [[nodiscard]] int getOrCreateTuning(const QString &name);

    // Linking
    [[nodiscard]] int linkFiles(const QList<int> &fileIds);
    [[nodiscard]] bool unlinkFile(const int fileId);
    [[nodiscard]] bool addFileRelation(int idA, int idB);
    [[nodiscard]] bool removeRelation(int fileIdA, int fileIdB);
    [[nodiscard]] bool deleteFileRecord(int fileId);

    // File queries
    [[nodiscard]] QList<RelatedFile> getResourcesForSong(int songId);
    [[nodiscard]] QList<RelatedFile> getFilesByRelation(int fileId);
    [[nodiscard]] QList<RelatedFile> getRelatedFiles(const int songId, const int excludeFileId = 0);

    // Practice Sessions & Journal (Logging)
    [[nodiscard]] bool addPracticeSession(int songId, int bpm, int totalReps, int cleanReps, const QString &note);
    [[nodiscard]] bool saveTableSessions(int songId, QDate date, const QList<PracticeSession> &sessions);
    [[nodiscard]] QList<PracticeSession> getSessionsForDay(int songId, QDate date);
    [[nodiscard]] QList<PracticeSession> getLastSessions(int songId, int limit);

    // Journal & Notice
    [[nodiscard]] bool addJournalNote(int songId, const QString &note);
    [[nodiscard]] bool saveOrUpdateNote(int songId, QDate date, const QString &note);
    [[nodiscard]] bool updateSongNotes(int songId, const QString &notes, QDate date);
    [[nodiscard]] QString getNoteForDay(int songId, QDate date);

    // Statistics & Dashboard
    [[nodiscard]] QMap<int, QString> getPracticedSongsForDay(QDate date);
    [[nodiscard]] QString getPracticeSummaryForDay(QDate date);
    [[nodiscard]] QList<QDate> getAllPracticeDates();

    // Settings (configuration)
    [[nodiscard]] bool setSetting(const QString &key, const QVariant &value);
    [[nodiscard]] QString getSetting(const QString &key, const QString &defaultValue = QString());
    [[nodiscard]] QVariant getSetting(const QString &key, const QVariant &defaultValue = QVariant());

    // Transaction Management
    [[nodiscard]] bool beginTransaction() { return QSqlDatabase::database().transaction(); }
    [[nodiscard]] bool commit() { return QSqlDatabase::database().commit(); }
    void rollback() { QSqlDatabase::database().rollback(); }

private:
    [[nodiscard]] int getOrCreateUserId(const QString &name, const QString &role = "student");

    QSqlDatabase db_m;
};

#endif // DATABASEMANAGER_H
