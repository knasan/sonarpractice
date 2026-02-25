/**
 * @file databasemanager.cpp
 * @brief Implementation of the DatabaseManager singleton class for managing SQLite database operations.
 *
 * This file contains the complete implementation of the DatabaseManager, which serves as the
 * central database access layer for a guitar practice application. It handles:
 *
 * - **Singleton Pattern & Lifecycle**: Manages database connection initialization and cleanup
 * - **Schema Management**: Creates and maintains database tables and schema versioning
 * - **File Management**: Handles media file tracking, hashing, and relationships
 * - **Song Management**: CRUD operations for songs, artists, and tunings
 * - **Practice Logging**: Records practice sessions, progress tracking, and journal notes
 * - **Statistics & Reporting**: Provides practice history and dashboard data
 * - **Settings & Configuration**: Manages application settings via key-value store
 *
 * The database uses SQLite with foreign key constraints enabled. All database operations
 * use prepared statements with parameterized queries to prevent SQL injection.
 *
 * Key Tables:
 * - users: User profiles (teachers/students)
 * - songs: Practice songs with metadata
 * - media_files: Physical files associated with songs
 * - practice_journal: Practice session history and notes
 * - artists: Artist/band information
 * - tunings: Guitar tuning options
 * - file_relations: Links between related media files
 * - settings: Application configuration key-value pairs
 *
 * @warning Foreign key constraints are enabled via PRAGMA. Ensure ON DELETE CASCADE is
 *          properly configured in foreign key definitions for cascading deletes.
 */
#include "databasemanager.h"
#include "fileutils.h"

#include <QDir>
#include <QSqlError>
#include <QSqlQuery>
#include <QUrl>

// =============================================================================
// --- Singleton & Lifecycle
// =============================================================================

DatabaseManager &DatabaseManager::instance()
{
    // The static local variable is initialized on the first call.
    static DatabaseManager _instance;
    return _instance;
}

/**
 * @brief Initializes or opens the application database.
 *
 * Attempts to establish a connection to a SQLite database at the specified path.
 * If a connection already exists with the same path, it reuses that connection.
 * If an existing connection has a different path, it removes the old connection
 * and creates a new one.
 *
 * Upon successful connection, this function:
 * - Enables foreign key constraints via PRAGMA
 * - Checks the database schema version
 * - Creates initial tables if the database is new
 * - Sets the database version if newly created
 *
 * @param dbPath The file path to the SQLite database file.
 *
 * @return true if the database initialization/opening succeeds and all setup
 *         operations complete successfully; false otherwise.
 *
 * @see getDatabaseVersion(), setDatabaseVersion(), createInitialTables()
 */

bool DatabaseManager::initDatabase(const QString &dbPath)
{
    if (QSqlDatabase::contains(QSqlDatabase::defaultConnection)) {
        QSqlDatabase existingDb = QSqlDatabase::database();

        if (existingDb.isOpen() && existingDb.databaseName() == dbPath) {
            QSqlQuery query(existingDb);
            query.exec("PRAGMA foreign_keys = ON;");
            return true;
        }

        existingDb = QSqlDatabase(); // Handle freigeben
        QSqlDatabase::removeDatabase(QSqlDatabase::defaultConnection);
    }

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(dbPath);

    if (!db.open()) {
        qCritical() << "Connection failed:" << db.lastError().text();
        return false;
    }

    QSqlQuery q(db);
    if (!q.exec("PRAGMA foreign_keys = ON;")) {
        qWarning() << "Could not activate foreign keys: " << q.lastError().text();
    }

    int currentVersion = getDatabaseVersion();
    const int targetVersion = 1;

    if (currentVersion < targetVersion) {
        if (!createInitialTables())
            return false;
        if (!setDatabaseVersion(targetVersion))
            return false;
    }

    return true;
}

/**
 * @brief Closes the database connection and removes it from the database manager.
 *
 * This function safely closes an open database connection and performs cleanup by:
 * - Storing the connection name before closure
 * - Closing the database if it is currently open
 * - Resetting the database object
 * - Removing the database connection from the QSqlDatabase registry
 *
 * @note After calling this function, the database connection will no longer be available
 *       and a new connection must be established before further database operations.
 *
 * @see QSqlDatabase::close()
 * @see QSqlDatabase::removeDatabase()
 */
void DatabaseManager::closeDatabase()
{
    // Remember names
    QSqlDatabase db = QSqlDatabase::database();

    // Close database
    if (db.isOpen()) {
        db.close();
    }

    db = QSqlDatabase();
}

// =============================================================================
// --- Setup & Metadata (Initialization & Versioning)
// =============================================================================

/**
 * @brief Creates the initial database schema for the SonarPractice application.
 *
 * Initializes all required tables and indexes for managing music practice sessions,
 * including users, songs, media files, practice tracking, and related metadata.
 *
 * Tables created:
 * - users: Stores user profiles (teachers/students) with creation timestamps
 * - songs: Contains song metadata including title, artist, tuning, BPM settings, and favorites
 * - media_files: Physical media files associated with songs, with file metadata and hashing
 * - practice_journal: Exercise progress tracking with practice dates, BPM, repetitions, and ratings
 * - settings: Key-value store for application configuration and paths
 * - artists: Artist/band names referenced by songs
 * - tunings: Guitar tuning standards (populated with E-Standard, Eb-Standard, Drop D, Drop C, D-Standard)
 * - file_relations: Relationships between media files
 *
 * Additionally creates:
 * - Index on media_files.file_path for optimized file lookups
 * - Default settings entries for managed paths, import tracking, and management flags
 * - Foreign key constraints with cascading deletes where appropriate
 *
 * @return bool True if all tables and indexes were successfully created, false if any operation fails.
 *
 * @note Enables PRAGMA foreign_keys to enforce referential integrity.
 * @note Uses "INSERT OR IGNORE" to safely populate reference data without duplicates.
 * @note All file paths are stored as UNIQUE to prevent duplicate media entries.
 */
bool DatabaseManager::createInitialTables()
{
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);
    if (!q.exec("PRAGMA foreign_keys = ON"))
        return false;

    // 1. USER (Teacher / Student)
    if (!q.exec("CREATE TABLE IF NOT EXISTS users ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                "name TEXT UNIQUE, "
                "role TEXT, " // 'teacher' oder 'student'
                "created_at DATETIME DEFAULT CURRENT_TIMESTAMP)")) {
        qCritical() << "[DatabaseManager] create table users failed, error: "
                    << q.lastError().text();
        qDebug() << "[DatabaseManager] create table users failed, fullquery: " << q.executedQuery();
        return false;
    }

    if (!q.exec("INSERT OR IGNORE INTO users (id, name, role) VALUES (1, 'Admin', 'admin')")) {
        qCritical() << "[DatabaseManager] Could not create default user:" << q.lastError().text();
        qDebug() << "[DatabaseManager] Create user fullquery:" << q.executedQuery();
    }

    // 2. SONGS (The logical unit)
    if (!q.exec("CREATE TABLE IF NOT EXISTS songs ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                "user_id INTEGER NOT NULL DEFAULT 1, "
                "title TEXT, "
                "artist_id INTEGER, "
                "tuning_id INTEGER, "
                "base_bpm INTEGER, "
                "total_bars INTEGER, "
                "current_bpm INTEGER DEFAULT 0, "
                "is_favorite INTEGER DEFAULT 0, "
                "updated_at DATETIME DEFAULT CURRENT_TIMESTAMP, "
                "FOREIGN KEY(artist_id) REFERENCES artists(id), "
                "FOREIGN KEY(tuning_id) REFERENCES tunings(id))")) {
        qCritical() << "[DatabaseManager] create table songs failed, error: "
                    << q.lastError().text();
        qDebug() << "[DatabaseManager] create table songs failed, fullquery: " << q.executedQuery();
        return false;
    }

    // 3. MEDIA FILES (Physical files on the disk)
    // Link a file to a song (N:1 - A song can have multiple files)
    if (!q.exec("CREATE TABLE IF NOT EXISTS media_files ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                "song_id INTEGER, "
                "file_path TEXT UNIQUE, "
                "is_managed INTEGER DEFAULT 0, " // 1 = Relativ zu SonarPath, 0 = Absolut
                "file_type TEXT, "
                "file_size INTEGER, "
                "file_hash TEXT UNIQUE,"
                "can_be_practiced BOOL, "
                "FOREIGN KEY(song_id) REFERENCES songs(id) ON DELETE CASCADE)")) {
        qCritical() << "[DatabaseManager] create table media_files failed, error: "
                    << q.lastError().text();
        qDebug() << "[DatabaseManager] create table media_files failed, fullquery: "
                 << q.executedQuery();
        return false;
    }

    // 4. EXERCISE JOURNAL (Progress Tracking)
    if (!q.exec("CREATE TABLE IF NOT EXISTS practice_journal ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                "user_id INTEGER NOT NULL DEFAULT 1, "
                "song_id INTEGER, "
                "practice_date DATETIME DEFAULT CURRENT_TIMESTAMP, "
                "start_bar INTEGER, "
                "end_bar INTEGER, "
                "practiced_bpm INTEGER, "
                "total_reps INTEGER, "         // Total number of times played
                "successful_streaks INTEGER, " // How many times has the 7 series been achieved?
                "rating INTEGER, "
                "note_text TEXT, "
                "FOREIGN KEY(user_id) REFERENCES users(id) ON DELETE CASCADE, "
                "FOREIGN KEY(song_id) REFERENCES songs(id) ON DELETE CASCADE)")) {
        qCritical() << "[DatabaseManager] create table practice_journal failed, error: "
                    << q.lastError().text();
        qDebug() << "[DatabaseManager] create table practice_journal failed, fullquery: "
                 << q.executedQuery();
        return false;
    }

    // 7. SETTINGS (Key-Value-Store for paths, flags)
    if (!q.exec("CREATE TABLE IF NOT EXISTS settings ("
                "key TEXT PRIMARY KEY, "
                "value TEXT)")) {
        qCritical() << "[DatabaseManager] create table settings failed, error: "
                    << q.lastError().text();
        qDebug() << "[DatabaseManager] create table settings failed, fullquery: "
                 << q.executedQuery();
        return false;
    }

    // 8. Artists/Bands
    // -- Table for artists
    if (!q.exec("CREATE TABLE IF NOT EXISTS artists ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                "name TEXT UNIQUE NOT NULL)")) {
        qCritical() << "[DatabaseManager]  create table artists failed, error: "
                    << q.lastError().text();
        qDebug() << "[DatabaseManager]  create table artists failed, fullquery: "
                 << q.executedQuery();
        return false;
    }

    // 9. Tunings
    if (!q.exec("CREATE TABLE IF NOT EXISTS tunings ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                "name TEXT UNIQUE NOT NULL)")) {
        qCritical() << "[DatabaseManager] create table tunings failed, error: "
                    << q.lastError().text();
        qDebug() << "[DatabaseManager] create table tunings failed, fullquery: "
                 << q.executedQuery();
        return false;
    }

    // 10. relation
    if (!q.exec("CREATE TABLE IF NOT EXISTS file_relations ("
                "file_id_a INTEGER, "
                "file_id_b INTEGER, "
                // "relation_type TEXT, " // -- e.g. 'backing track', 'tutorial', 'tab'
                "PRIMARY KEY (file_id_a, file_id_b))")) {
        qCritical() << "[DatabaseManager] create table file_relations failed, error: "
                    << q.lastError().text();
        qDebug() << "[DatabaseManager] create table file_relations failed, fullpath: "
                 << q.executedQuery();
        return false;
    }

    /*
     * Explanation of the tables:
     * - reminders:
     *      Stores the basic information for the reminder
     *      reminder_date: For unique memories
     *      weekday: For weekly reminders (0-6, 0=Sunday)
     *      is_daily/is_monthly: For recurring memories
     *      is_active: Allows deactivation without deletion (not used yet)
     *
     * - reminder_completion_conditions:
     *      Definiert die Bedingungen, die erfüllt sein müssen, damit die Erinnerung als erledigt gilt
     *      start_bar/end_bar: Bar range that needs to be practiced. (is currently only used as a range indicator)
     *      min_bpm: Minimum BPM
     *      min_minutes: Minimum exercise duration in minutes
     *
     ****** This table is not yet in use ****
     * - reminder_completions:
     *      Link a reminder to a practice session (practice_journal)
     *      It will be created automatically if the conditions are met.
     *      This is not currently in use and I'm wondering if it's even necessary!
     */

    // 11. REMINDERS (Reminders for exercises)
    if (!q.exec("CREATE TABLE IF NOT EXISTS reminders ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                "user_id INTEGER NOT NULL DEFAULT 1, "
                "song_id INTEGER NOT NULL, "
                "title TEXT, "                   // Reminder title (optional)
                "reminder_date DATE, "           // Date for unique memories
                "weekday INTEGER, "              // Day of the week (0-6, 0=Sunday)
                "is_daily INTEGER DEFAULT 0, "   // Daily reminder
                "is_monthly INTEGER DEFAULT 0, " // Monthly reminder
                "is_active INTEGER DEFAULT 1, "  // Active/Inactive
                "created_at DATETIME DEFAULT CURRENT_TIMESTAMP, "
                "FOREIGN KEY(user_id) REFERENCES users(id) ON DELETE CASCADE, "
                "FOREIGN KEY(song_id) REFERENCES songs(id) ON DELETE CASCADE)")) {
        qCritical() << "[DatabaseManager] create table reminders failed, error: "
                    << q.lastError().text();
        qDebug() << "[DatabaseManager] create table reminders failed, fullpath: "
                 << q.executedQuery();
        return false;
    }

    // 12. REMINDER_COMPLETION_CONDITIONS (Conditions for completion)
    if (!q.exec("CREATE TABLE IF NOT EXISTS reminder_completion_conditions ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                "reminder_id INTEGER NOT NULL, "
                "start_bar INTEGER, "   // Start-Takt
                "end_bar INTEGER, "     // End-Takt
                "min_bpm INTEGER, "     // Mindest-BPM
                "min_minutes INTEGER, " // Minimum exercise duration in minutes
                "FOREIGN KEY(reminder_id) REFERENCES reminders(id) ON DELETE CASCADE)")) {
        qCritical()
            << "[DatabaseManager] create table reminder_completion_conditions failed, error: "
            << q.lastError().text();
        qDebug()
            << "[DatabaseManager] create table reminder_completion_conditions failed, fullpath: "
            << q.executedQuery();
        return false;
    }

    // 13. REMINDER_COMPLETIONS (Completed Memories) NOT USED YET
    if (!q.exec("CREATE TABLE IF NOT EXISTS reminder_completions ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                "reminder_id INTEGER NOT NULL, "
                "completion_date DATETIME DEFAULT CURRENT_TIMESTAMP, "
                "practice_journal_id INTEGER, " // Reference to the exercise session
                "FOREIGN KEY(reminder_id) REFERENCES reminders(id) ON DELETE CASCADE, "
                "FOREIGN KEY(practice_journal_id) REFERENCES practice_journal(id) ON DELETE SET "
                "NULL)")) {
        qCritical() << "[DatabaseManager] create table reminder_completions failed, error: "
                    << q.lastError().text();
        qDebug() << "[DatabaseManager] create table reminder_completions failed, fullpath: "
                 << q.executedQuery();
        return false;
    }

    if (!q.exec("INSERT OR IGNORE INTO settings (key, value) VALUES ('managed_path', '')")) {
        qCritical() << "[DatabaseManager] insert into settings managed_path failed, error: "
                    << q.lastError().text();
        qDebug() << "[DatabaseManager] insert into settings managed_path failed, fullquery: "
                 << q.executedQuery();
        return false;
    }

    if (!q.exec("INSERT OR IGNORE INTO settings (key, value) VALUES ('is_managed', 'false')")) {
        qCritical() << "[DatabaseManager] insert into settings is_managed failed, error: "
                    << q.lastError().text();
        qDebug() << "[DatabaseManager] insert into settings is_managed failed, fullquery: "
                 << q.executedQuery();
        return false;
    }

    if (!q.exec("INSERT OR IGNORE INTO settings (key, value) VALUES ('last_import_date', '')")) {
        qCritical() << "[DatabaseManager] insert into settings last_import_date failed, error: "
                    << q.lastError().text();
        qDebug() << "[DatabaseManager] insert into settings last_import_date failed, fullquery: "
                 << q.executedQuery();
        return false;
    }

    if (!q.exec("CREATE INDEX IF NOT EXISTS idx_filepath ON media_files(file_path)")) {
        qCritical() << "[DatabaseManager] create index for media_files failed, error: "
                    << q.lastError().text();
        qDebug() << "[DatabaseManager] create index for media_files failed, fullquery: "
                 << q.executedQuery();
        return false;
    }

    return true;
}

/**
 * @brief Checks if the songs table contains any data.
 * Executes a query to retrieve the first record from the songs table.
 * @return true if the query executed successfully and at least one row exists in the songs table; false otherwise.
 */
bool DatabaseManager::hasData()
{
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);
    q.prepare("SELECT id FROM songs LIMIT 1");
    return q.exec() && q.next();
}

/**
 * @brief Retrieves the user version of the SQLite database.
 *
 * Executes a PRAGMA user_version query to get the current version number
 * stored in the database header.
 *
 * @return The user version number as an integer. Returns 0 if the query
 *         fails or no version is set.
 */
int DatabaseManager::getDatabaseVersion()
{
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);
    q.prepare("PRAGMA user_version");
    if (q.next()) {
        return q.value(0).toInt();
    }
    return 0;
}

// --- Schema Versioning ---

/**
 * @brief Sets the user version of the SQLite database.
 *
 * @details This function executes a PRAGMA user_version statement to set the
 * database version. Note that PRAGMA user_version cannot be used with bind values (?),
 * so string concatenation via .arg() is used instead.
 *
 * @param version The version number to set for the database.
 *
 * @return true if the version was successfully set, false otherwise.
 *         On failure, the error message is logged using qCritical().
 *
 * @see QSqlQuery, QSqlDatabase
 */
bool DatabaseManager::setDatabaseVersion(int version)
{
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);
    // PRAGMA user_version cannot be used with bind values ​​(?).
    // Therefore, .arg() is the correct way to go here.
    if (!q.exec(QString("PRAGMA user_version = %1").arg(version))) {
        qCritical() << "[DatabaseManager] Error setting DB version: " << q.lastError().text();
        qDebug() << "[DatabaseManager] Error setting DB version, fullquery: " << q.executedQuery();
        return false;
    }
    return true;
}

// =============================================================================
// --- File Management & Media (Files & Relations)
// =============================================================================

/**
 * @brief Adds a media file associated with a song to the database.
 *
 * Inserts a new record into the media_files table linking a file to a song.
 * Automatically determines if the file is a Guitar Pro format that can be used
 * for practice based on its file extension.
 *
 * @param songId The ID of the song to associate the file with.
 * @param filePath The file path of the media file to add.
 * @param isManaged Whether the file is managed by the application.
 * @param fileType The type/category of the media file (e.g., "audio", "guitar_pro").
 * @param fileSize The size of the file in bytes.
 * @param fileHash The hash value of the file for integrity verification.
 *
 * @return true if the file was successfully added to the database, false otherwise.
 *
 * @note If the file extension matches a Guitar Pro format (e.g., .gp5, .gp4),
 *       the can_be_practiced flag is automatically set to true in the database.
 *
 * @see FileUtils::getGuitarProFormats()
 */
bool DatabaseManager::addFileToSong(qlonglong songId,
                                    const QString &filePath,
                                    bool isManaged,
                                    const QString &fileType,
                                    qint64 fileSize,
                                    QString fileHash)
{
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);

    // Logic: Is it a Guitar Pro file?
    // Take the ending (e.g., "gp5") and add "*." before it.
    // so that it matches QStringLists in FileUtils exactly.
    QString suffix = "*." + QFileInfo(filePath).suffix().toLower();
    bool isPracticeTarget = FileUtils::getGuitarProFormats().contains(suffix);

    q.prepare("INSERT OR IGNORE INTO media_files (song_id, file_path, is_managed, file_type, "
              "file_size, file_hash, can_be_practiced) "
              "VALUES (?, ?, ?, ?, ?, ?, ?)");

    q.addBindValue(songId);
    q.addBindValue(filePath);
    q.addBindValue(isManaged ? 1 : 0);
    q.addBindValue(fileType);
    q.addBindValue(fileSize);
    q.addBindValue(fileHash);
    q.addBindValue(isPracticeTarget ? 1 : 0);

    if (!q.exec()) {
        qCritical() << "[DatabaseManager] Error addFileToSong: " << q.lastError().text();
        qDebug() << "[DatabaseManager] Error addFileToSong, fullquery: " << q.executedQuery();
        return false;
    }
    return true;
}

/**
 * @brief Updates the practice flag for a media file in the database.
 *
 * Sets whether a media file can be practiced or not by updating the
 * can_be_practiced column in the media_files table.
 *
 * @param fileId The ID of the media file to update.
 * @param canPractice True if the file should be available for practice,
 *                    false otherwise.
 *
 * @return True if the update was successful, false if a database error occurred.
 *         On failure, an error message is logged to qCritical().
 *
 * @see media_files table schema
 */
bool DatabaseManager::updatePracticeFlag(int fileId, bool canPractice)
{
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);
    q.prepare("UPDATE media_files SET can_be_practiced = ? WHERE id = ?");
    q.addBindValue(canPractice ? 1 : 0);
    q.addBindValue(fileId);
    if (!q.exec()) {
        qCritical() << "[DatabaseManager] Error updatePracticeFlag: " << q.lastError().text();
        qDebug() << "[DatabaseManager] Error updatePracticeFlag, fullquery: " << q.executedQuery();
        return false;
    }
    return true;
}

/**
 * @brief Retrieves the managed path setting from the database.
 *
 * Fetches the "managed_path" configuration value from the settings storage.
 * If the setting does not exist, returns an empty string.
 *
 * @return QString The managed path as a string. Returns an empty string if the setting is not found.
 */
QString DatabaseManager::getManagedPath()
{
    QVariant val = getSetting("managed_path", QString(""));
    return val.toString();
}

/**
 * @brief Creates a new song in the database.
 *
 * Inserts a new song record with the specified title, artist, and tuning.
 * If the artist or tuning do not exist in the database, they will be created automatically.
 *
 * @param title The title of the song.
 * @param artist The name of the artist performing the song.
 * @param tuning The tuning configuration for the song.
 *
 * @return The ID of the newly created song as a qlonglong, or -1 if the operation failed.
 *
 * @note On failure, an error message is logged using qCritical().
 */
qlonglong DatabaseManager::createSong(const QString &title,
                                      const QString &artist,
                                      const QString &tuning,
                                      int bpm)
{
    int artistId = getOrCreateArtist(artist);
    int tuningId = getOrCreateTuning(tuning);

    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);
    q.prepare("INSERT INTO songs (title, artist_id, tuning_id, base_bpm) VALUES (?, ?, ?, ?)");
    q.addBindValue(title);
    q.addBindValue(artistId);
    q.addBindValue(tuningId);
    // q.addBindValue(bpm > 0 ? bpm : 120); // Default 120 falls 0
    q.addBindValue(bpm);

    if (q.exec()) {
        return q.lastInsertId().toLongLong();
    } else {
        qCritical() << "[DatabaseManager] Error createSong:" << q.lastError().text();
        qDebug() << "[DatabaseManager] Error createSong, fullquery: " << q.executedQuery();
        return -1;
    }
}

/**
 * @brief Retrieves the ID of an artist by name, creating a new artist record if it doesn't exist.
 *
 * This method searches the artists table for an existing artist with the specified name.
 * If found, it returns the artist's ID. If not found, a new artist record is created with
 * the provided name and its ID is returned.
 *
 * @param name The name of the artist to retrieve or create. Leading and trailing whitespace
 *             will be automatically trimmed.
 *
 * @return The ID of the artist (either existing or newly created).
 *
 * @note The artist name is trimmed of whitespace before both querying and inserting.
 *
 * @see QSqlQuery, QSqlDatabase
 */
int DatabaseManager::getOrCreateArtist(const QString &name)
{
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);
    q.prepare("SELECT id FROM artists WHERE name = ?");
    q.addBindValue(name.trimmed());

    if (q.exec() && q.next()) {
        return q.value(0).toInt();
    } else {
        q.prepare("INSERT INTO artists (name) VALUES (?)");
        q.addBindValue(name.trimmed());
        if (!q.exec()) {
            qCritical() << "[DatabaseManager] getOrCreateArtist, error: " << q.lastError().text();
            qDebug() << "[DatabaseManager] getOrCreateArtist, fullquery: " << q.executedQuery();
        }
        return q.lastInsertId().toInt();
    }
}

/**
 * @brief Retrieves or creates a tuning record in the database.
 *
 * Searches the tunings table for a record with the given name. If found,
 * returns its ID. If not found, creates a new tuning record with the provided
 * name and returns the ID of the newly inserted record.
 *
 * @param name The name of the tuning to search for or create. Whitespace is
 *             trimmed before the operation.
 *
 * @return The ID of the existing or newly created tuning record.
 *
 * @note The name parameter is trimmed of leading and trailing whitespace
 *       before being used in database operations.
 */
int DatabaseManager::getOrCreateTuning(const QString &name)
{
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);
    q.prepare("SELECT id FROM tunings WHERE name = ?");
    q.addBindValue(name.trimmed());

    if (q.exec() && q.next()) {
        return q.value(0).toInt();
    } else {
        q.prepare("INSERT INTO tunings (name) VALUES (?)");
        q.addBindValue(name.trimmed());
        if (!q.exec()) {
            qCritical() << "[DatabaseManager] getOrCreateTuning, error: " << q.lastError().text();
            qDebug() << "[DatabaseManager] getOrCreateTuning, fullquery: " << q.executedQuery();
        }
        return q.lastInsertId().toInt();
    }
}

/**
 * @brief Retrieves all file hashes from the media_files table.
 *
 * Executes a SQL query to fetch all file_hash values from the database.
 * Each hash is trimmed of whitespace, converted to uppercase, and added
 * to the result set if non-empty.
 *
 * @return QSet<QString> A set containing all unique file hashes from the database.
 *                       Returns an empty set if the query fails or no hashes exist.
 *
 * @note If the database query fails, a warning message is logged via qWarning().
 * @see QSqlQuery, QSet
 */
QSet<QString> DatabaseManager::getAllFileHashes()
{
    QSet<QString> hashSet;
    QSqlDatabase db = QSqlDatabase::database();

    if(!db.isOpen()) return hashSet;

    QSqlQuery query(db);
    query.prepare("SELECT file_hash FROM media_files");

    if (!query.exec()) {
        qCritical() << "[DatabaseManager] getAllFileHashes - Error fetching hashes:"
                    << query.lastError().text();
        qDebug() << "[DatabaseManager] getAllFileHashes - Error fetching hashes, fullquery: "
                 << query.executedQuery();
        return hashSet;
    }

    while (query.next()) {
        QString h = query.value(0).toString().trimmed().toUpper();
        if (!h.isEmpty()) {
            hashSet.insert(h);
        }
    }

    return hashSet;
}

// =============================================================================
// --- Linking
// =============================================================================

/**
 * @brief Links multiple media files to a single song record.
 *
 * This function associates multiple media files with a common song by setting their
 * song_id to the same value. If any of the files already has a song_id, that ID is
 * reused for all files. Otherwise, a new song record is created with a title derived
 * from the first file's name.
 *
 * @param fileIds A QList of integer file IDs to be linked together.
 *
 * @return The song_id that all files are now linked to on success, or -1 on failure.
 *         Possible failure reasons:
 *         - Less than 2 files provided (fileIds.size() < 2)
 *         - Database transaction could not be started
 *         - Failed to insert new song record
 *         - Failed to update media_files records
 *         - Failed to commit transaction
 *
 * @note The function uses database transactions to ensure atomicity. If any operation
 *       fails, all changes are rolled back and -1 is returned.
 * @note If multiple files already have different song_ids, the first found song_id
 *       is used (others are overwritten).
 */
int DatabaseManager::linkFiles(const QList<int> &fileIds)
{
    if (fileIds.size() < 2)
        return -1;
    QSqlDatabase db = QSqlDatabase::database();

    if (!db.transaction())
        return -1;

    int finalSongId = -1;

    // Find existing song_id
    for (int id : fileIds) {
        QSqlQuery songIdQuery(db);
        songIdQuery.prepare("SELECT song_id FROM media_files WHERE id = ?");
        songIdQuery.addBindValue(id);
        if (songIdQuery.exec() && songIdQuery.next()) {
            int sid = songIdQuery.value(0).toInt();
            if (sid > 0) {
                finalSongId = sid;
                break;
            }
        } else {
            qCritical() << "[DatabaseManager] linkFiles failed:" << songIdQuery.lastError().text();
            qDebug() << "[DatabaseManager] linkFiles fullquery: " << songIdQuery.executedQuery();
        }
    }

    // If no ID is found, create a new song.
    if (finalSongId <= 0) {
        // Get the name of the first file for the default title.
        QSqlQuery q(db);
        q.prepare("SELECT file_path FROM media_files WHERE id = ?");
        q.addBindValue(fileIds.first());
        QString title = "New song";
        if (q.exec() && q.next()) {
            title = QFileInfo(q.value(0).toString()).baseName();
        }

        QSqlQuery insertQuery(db);
        insertQuery.prepare("INSERT INTO songs (title) VALUES (?)");
        insertQuery.addBindValue(title);
        if (!insertQuery.exec()) {
            db.rollback();
            qCritical() << "[DatabaseManager] insert linkFiles failed:"
                        << insertQuery.lastError().text();
            qDebug() << "[DatabaseManager] insert linkFiles fullquery: "
                     << insertQuery.executedQuery();
            return -1;
        }
        finalSongId = insertQuery.lastInsertId().toInt();
    }

    // link them all
    for (int id : fileIds) {
        QSqlQuery updateQuery(db);
        updateQuery.prepare("UPDATE media_files SET song_id = ? WHERE id = ?");
        updateQuery.addBindValue(finalSongId);
        updateQuery.addBindValue(id);
        if (!updateQuery.exec()) {
            db.rollback();
            qCritical() << "[DatabaseManager] update linkFiles failed:"
                        << updateQuery.lastError().text();
            qDebug() << "[DatabaseManager] update linkFiles fullquery: "
                     << updateQuery.executedQuery();
            return -1;
        }
    }

    if (db.commit())
        return finalSongId;
    return -1;
}

/**
 * @brief Unlinks a media file from its associated song.
 *
 * Sets the song_id to NULL for the specified media file, effectively removing
 * the association between the file and any song it was linked to.
 *
 * @param fileId The ID of the media file to unlink.
 * @return true if the database update was successful, false otherwise.
 *         On failure, an error message is logged to the critical output.
 *
 * @note This function uses prepared statements to prevent SQL injection.
 * @see linkFile()
 */
bool DatabaseManager::unlinkFile(int fileId)
{
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);
    q.prepare("UPDATE media_files SET song_id = NULL WHERE id = ?");
    q.addBindValue(fileId);
    if (!q.exec()) {
        qCritical() << "[DatabaseManager] update unlinkFile failed:" << q.lastError().text();
        qDebug() << "[DatabaseManager] update unlinkFile fullquery: " << q.executedQuery();
        return false;
    }
    return true;
}

/**
 * @brief Adds a file relation between two files in the database.
 *
 * Inserts a relation between two files identified by their IDs into the file_relations table.
 * The method automatically orders the IDs to prevent duplicate pairs (e.g., (1,2) and (2,1) are treated as the same relation).
 * If a relation already exists, it is ignored due to the INSERT OR IGNORE clause.
 *
 * @param idA The ID of the first file
 * @param idB The ID of the second file
 *
 * @return true if the relation was successfully inserted or already existed, false otherwise
 * @return false if idA equals idB or if the database query fails
 *
 * @note IDs are sorted internally using qMin/qMax to ensure consistent relation storage
 * @note If the insertion fails, an error message is logged to the critical output stream
 */
bool DatabaseManager::addFileRelation(int idA, int idB)
{
    if (idA == idB)
        return false;

    QSqlDatabase db = QSqlDatabase::database();

    QSqlQuery q(db);
    q.prepare("INSERT OR IGNORE INTO file_relations (file_id_a, file_id_b) VALUES (?, ?)");
    q.addBindValue(qMin(idA, idB)); // Sorting prevents duplicate pairs (1,2 and 2,1)
    q.addBindValue(qMax(idA, idB));
    if (!q.exec()) {
        qCritical() << "[DatabaseManager] update addFileRelation failed:" << q.lastError().text();
        qDebug() << "[DatabaseManager] update addFileRelation fullquery: " << q.executedQuery();
        return false;
    }
    return true;
}

/**
 * @brief Removes a relation between two files from the database.
 *
 * Deletes the relation record between two files identified by their IDs from the
 * file_relations table. The method handles bidirectional relations by searching for
 * entries where the file IDs match in either order (A, B) or (B, A).
 *
 * @param fileIdA The ID of the first file.
 * @param fileIdB The ID of the second file.
 *
 * @return true if the relation was successfully deleted (or no matching relation existed),
 *         false if a database error occurred during the deletion.
 *
 * @note If an error occurs, it is logged to the critical output stream with details
 *       of the database error.
 */
bool DatabaseManager::removeRelation(int fileIdA, int fileIdB)
{
    QSqlDatabase db = QSqlDatabase::database();

    QSqlQuery query(db);

    // Delete the line, regardless of whether the IDs were stored as (A, B) or (B, A).
    query.prepare("DELETE FROM file_relations WHERE "
                  "(file_id_a = :idA AND file_id_b = :idB) OR "
                  "(file_id_a = :idB AND file_id_b = :idA)");

    query.bindValue(":idA", fileIdA);
    query.bindValue(":idB", fileIdB);

    if (!query.exec()) {
        qCritical() << "[DatabaseManager] update deleting failed:" << query.lastError().text();
        qDebug() << "[DatabaseManager] update deleting fullquery: " << query.executedQuery();
        return false;
    }
    return true;
}

/**
 * @brief Deletes a song record and its associated data from the database.
 *
 * Removes a song entry from the 'songs' table based on the provided song ID.
 * This operation will also delete all data associated with the song record due to
 * foreign key constraints or cascade delete rules configured in the database.
 *
 * @param songId The unique identifier of the song record to delete.
 * @return true if the deletion was successful, false otherwise.
 *
 * @note If the deletion fails, an error message is logged to qCritical().
 *
 * @see QSqlQuery, QSqlError
 */
bool DatabaseManager::deleteFileRecord(int songId)
{
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);

    q.prepare("DELETE FROM songs WHERE id = ?");
    q.addBindValue(songId);

    if (!q.exec()) {
        qCritical() << "[DatabaseManager] deleteFileRecord failed:" << q.lastError().text();
        qDebug() << "[DatabaseManager] deleteFileRecord fullquery: " << q.executedQuery();
        return false;
    }
    return true;
}

// =============================================================================
// --- File queries
// =============================================================================

/**
 * @brief Retrieves all media files related to a given song.
 *
 * Queries the database to find all media files that are connected to the specified
 * song through file relations. A file is considered related if it appears in either
 * the file_id_a or file_id_b column of the file_relations table with the given songId.
 *
 * @param songId The ID of the song for which related files should be retrieved.
 *
 * @return A QList of RelatedFile objects containing the id, file path, and file type
 *         of all files related to the specified song. Returns an empty list if no
 *         related files are found or if the query fails.
 *
 * @note If the database query fails, an error message is logged using qCritical().
 *
 * @see RelatedFile
 */
QList<DatabaseManager::RelatedFile> DatabaseManager::getFilesByRelation(int songId)
{
    QList<RelatedFile> list;

    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);

    q.prepare("SELECT mf.id, mf.file_path, mf.file_type "
              "FROM media_files mf "
              "JOIN file_relations fr ON (mf.id = fr.file_id_b AND fr.file_id_a = ?) "
              "OR (mf.id = fr.file_id_a AND fr.file_id_b = ?)");
    q.addBindValue(songId);
    q.addBindValue(songId);

    bool isManaged = getSetting("is_managed", QString("false")) == "true";

    if (q.exec()) {
        while (q.next()) {
            RelatedFile rf;
            rf.id = q.value("id").toInt();
            rf.fileName = QFileInfo(q.value("file_path").toString()).fileName();
            rf.type = q.value("file_type").toString();
            QString relpath = QFileInfo(q.value("file_path").toString()).filePath();
            // qDebug() << "isManaged: " << isManaged << " with managed Path: " << getManagedPath();
            if(isManaged) {
                rf.absolutePath = QDir::cleanPath(getManagedPath() + "/" + relpath);
            } else {
                rf.absolutePath = QDir::cleanPath(relpath);
            }
            // qDebug() << "path: " << rf.relativePath;
            list.append(rf);
        }
    } else {
        qCritical() << "[DatabaseManager] getFilesBySongId Error: " << q.lastError().text();
        qDebug() << "[DatabaseManager] getFilesBySongId fullquery: " << q.executedQuery();
    }
    return list;
}

/**
 * Retrieves all partner files for a given song_id, except the current file.
 */
/**
 * @brief Retrieves a list of related media files for a given song.
 *
 * Queries the database to fetch all media files associated with a specific song,
 * optionally excluding a file by its ID. For each file found, constructs a RelatedFile
 * structure containing file metadata extracted from the database and file system.
 *
 * @param songId The ID of the song to retrieve related files for. Must be greater than 0.
 * @param excludeFileId The ID of a file to exclude from the results (default: 0).
 *
 * @return QList<DatabaseManager::RelatedFile> A list of RelatedFile structures containing:
 *         - id: The file's unique identifier
 *         - relativePath: The file path from the database
 *         - songId: The associated song ID
 *         - fileName: The filename extracted from the relative path
 *         - title: Defaults to the filename (no title field in database query)
 *         - type: The file extension in lowercase
 *         Returns an empty list if songId is invalid (≤ 0) or if the database query fails.
 *
 * @note The title field is set to the filename as a fallback since the database
 *       query does not retrieve a title column.
 * @note File type is determined solely from the file extension.
 */
QList<DatabaseManager::RelatedFile> DatabaseManager::getRelatedFiles(const int songId,
                                                                     const int excludeFileId)
{
    QList<DatabaseManager::RelatedFile> list;
    if (songId <= 0)
        return list;

    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);

    q.prepare("SELECT id, file_path FROM media_files WHERE song_id = ? AND id != ?");
    q.addBindValue(songId);
    q.addBindValue(excludeFileId);

    if (q.exec()) {
        while (q.next()) {
            RelatedFile res;
            res.id = q.value(0).toInt();              // Correctly (id)
            res.absolutePath = q.value(1).toString(); // Correctly (file_path)
            res.songId = songId;

            // Use the filename as the title if there is no title in the database.
            QFileInfo fi(res.absolutePath);
            res.fileName = fi.fileName();
            res.title = res.fileName; // Fallback, since 'title' is not in the SELECT statement

            // We determine the type simply from the ending.
            res.type = fi.suffix().toLower();

            list.append(res);
        }
    } else {
        qCritical() << "[DatabaseManager] getRelatedFiles Error: " << q.lastError().text();
        qDebug() << "[DatabaseManager] getRelatedFiles fullquery: " << q.executedQuery();
    }
    return list;
}

// =============================================================================
// --- Practice Sessions & Journal (Logging)
// =============================================================================

/**
 * @brief Records a practice session for a song with performance metrics and optional notes.
 *
 * This function atomically inserts a practice session record into the database, updating
 * both the practice history and song master data. If a note is provided, it is saved
 * separately to the practice journal.
 *
 * @param songId The unique identifier of the song being practiced.
 * @param bpm The tempo (beats per minute) at which the song was practiced.
 * @param totalReps The total number of repetitions performed during the session.
 * @param cleanReps The number of successful/clean repetitions (consecutive streak count).
 * @param note An optional practice note/comment. If empty or whitespace, it is not recorded.
 *
 * @return true if the practice session was successfully recorded and committed to the database;
 *         false if any operation failed (transaction is rolled back on failure).
 *
 * @details
 * - Begins a database transaction to ensure data consistency.
 * - Inserts a record into practice_history with session metrics.
 * - Updates the song's last_practiced timestamp and current_bpm in the songs table.
 * - Optionally inserts a note into practice_journal if note is not empty.
 * - Commits the transaction on success or rolls back on any error.
 *
 * @note All operations are performed within a single transaction. If any query fails,
 *       the entire transaction is rolled back to maintain database integrity.
 */
bool DatabaseManager::addPracticeSession(
    int songId, int bpm, int totalReps, int cleanReps, const QString &note)
{
    QSqlDatabase db = QSqlDatabase::database();

    if (!db.transaction())
        return false;

    QSqlQuery q(db);

    // Journal entry / History
    q.prepare("INSERT INTO practice_history (song_id, bpm_start, total_reps, successful_streaks) "
              "VALUES (:sid, :bpm, :total, :clean)");
    q.bindValue(":sid", songId);
    q.bindValue(":bpm", bpm);
    q.bindValue(":total", totalReps);
    q.bindValue(":clean", cleanReps); // How many times a repetition was mastered without making a mistake.

    if (!q.exec()) {
        db.rollback();
        qCritical() << "[DatabaseManager] addPracticeSession Error: " << q.lastError().text();
        qDebug() << "[DatabaseManager] addPracticeSession fullquery: " << q.executedQuery();
        return false;
    }

    // Update song master data (Important for dashboard!)
    q.prepare(
        "UPDATE songs SET last_practiced = CURRENT_TIMESTAMP, current_bpm = :bpm WHERE id = :sid");
    q.bindValue(":bpm", bpm);
    q.bindValue(":sid", songId);

    if (!q.exec()) {
        db.rollback();
        qCritical() << "[DatabaseManager] addPracticeSession Error: " << q.lastError().text();
        qDebug() << "[DatabaseManager] addPracticeSession fullquery: " << q.executedQuery();
        return false;
    }

    // Save note (if available)
    if (!note.trimmed().isEmpty()) {
        q.prepare(
            "INSERT INTO practice_journal (song_id, practiced_bpm, note_text) VALUES (?, ?, ?)");
        q.addBindValue(songId);
        q.addBindValue(bpm);
        q.addBindValue(note);
        if (!q.exec()) {
            qCritical() << "[DatabaseManager] addPracticeSession Error: " << q.lastError().text();
            qDebug() << "[DatabaseManager] addPracticeSession fullquery: " << q.executedQuery();
            return false;
        }
    }

    return db.commit();
}

/**
 * @brief Saves a list of practice sessions for a song on a specific date to the database.
 *
 * This function persists practice session data by first deleting any existing sessions
 * for the given song and date, then inserting the new session records. The operation
 * is wrapped in a database transaction to ensure data consistency.
 *
 * @param songId The ID of the song associated with the practice sessions.
 * @param date The practice date for these sessions.
 * @param sessions A list of PracticeSession objects containing practice metrics
 *                 (start bar, end bar, BPM, repetitions, and successful streaks).
 *
 * @return true if all database operations completed successfully and were committed;
 *         false if any operation failed and the transaction was rolled back.
 *
 * @note The function uses a database transaction. If any INSERT or DELETE operation fails,
 *       the entire transaction is rolled back to maintain data integrity.
 *
 * @todo Retrieve and preserve the note_text from existing records before deletion,
 *       then insert or update the practice_journal with preserved notes.
 *
 * @see PracticeSession
 */
bool DatabaseManager::saveTableSessions(int songId,
                                        QDate date,
                                        const QList<PracticeSession> &sessions)
{
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.transaction()) {
        qCritical() << "[DatabaseManager] saveTableSessions Could not start transaction:"
                    << db.lastError().text();
        return false;
    }

    QSqlQuery q(db);
    QString dateStr = date.toString("yyyy-MM-dd");

    // TODO: get first note_text bevor delete.
    // INSERT now the note_text or make insert or update the practice_journal

    q.prepare("DELETE FROM practice_journal WHERE song_id = :sId AND practice_date = :pDate");
    q.bindValue(":sId", songId);
    q.bindValue(":pDate", dateStr);

    if (!q.exec()) {
        qCritical() << "[DatabaseManager] saveTableSessions Delete Error saveTableSessions:"
                    << q.lastError().text();
        db.rollback();
        return false;
    }

    q.prepare("INSERT INTO practice_journal (song_id, practice_date, start_bar, end_bar, "
              "practiced_bpm, total_reps, successful_streaks) "
              "VALUES (:songId, :date, :start, :end, :bpm, :reps, :streaks)");

    for (const auto &s : sessions) {
        q.bindValue(":songId", songId);
        q.bindValue(":date", dateStr);
        q.bindValue(":start", s.startBar);
        q.bindValue(":end", s.endBar);
        q.bindValue(":bpm", s.bpm);
        q.bindValue(":reps", s.reps);
        q.bindValue(":streaks", s.streaks);

        if (!q.exec()) {
            qCritical() << "[DatabaseManager] saveTableSessions insert Error: "
                        << q.lastError().text();
            qDebug() << "[DatabaseManager] saveTableSessions insert fullquery: "
                     << q.executedQuery();
            db.rollback();
            return false;
        }
    }

    return db.commit();
}

/**
 * @brief Retrieves all practice sessions for a specific song on a given date.
 *
 * Queries the practice_journal table to fetch practice session records that match
 * the specified song ID and date. Only returns entries that have complete table data
 * (i.e., start_bar is not NULL).
 *
 * @param songId The ID of the song to retrieve sessions for.
 * @param date The date to filter practice sessions by. The comparison is done on
 *             the date portion only, regardless of time component.
 *
 * @return A QList<PracticeSession> containing all matching practice sessions.
 *         Returns an empty list if no sessions are found or if the database query fails.
 *         On query failure, an error message is logged via qCritical().
 *
 * @note Each PracticeSession in the returned list contains:
 *       - practice_date (QDate)
 *       - start_bar (int)
 *       - end_bar (int)
 *       - practiced_bpm (int)
 *       - total_reps (int)
 *       - successful_streaks (int)
 */
QList<PracticeSession> DatabaseManager::getSessionsForDay(int songId, QDate date)
{
    QList<PracticeSession> sessions;

    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);

    q.prepare(
        "SELECT practice_date, start_bar, end_bar, practiced_bpm, total_reps, successful_streaks "
        "FROM practice_journal "
        "WHERE song_id = ? AND DATE(practice_date) = ? "
        "AND start_bar IS NOT NULL"); // Only entries with table data
    q.addBindValue(songId);
    q.addBindValue(date.toString("yyyy-MM-dd"));

    if (q.exec()) {
        while (q.next()) {
            sessions.append({q.value(0).toDate(),
                             q.value(1).toInt(),
                             q.value(2).toInt(),
                             q.value(3).toInt(),
                             q.value(4).toInt(),
                             q.value(5).toInt()});
        }
    } else {
        qCritical() << "[DatabaseManager] getSessionsForDay error: " << q.lastError().text();
        qDebug() << "[DatabaseManager] getSessionsForDay fullquery: " << q.executedQuery();
    }
    return sessions;
}

/**
 * @brief Retrieves the last N practice sessions for a specific song.
 *
 * Fetches the most recent practice sessions from the database for the given song ID,
 * ordered by practice date and session ID in descending order (newest first).
 * The results are then reversed to return them in chronological order (oldest to newest).
 *
 * @param songId The ID of the song to retrieve practice sessions for.
 * @param limit The maximum number of sessions to retrieve.
 *
 * @return A QList containing the practice sessions ordered chronologically (oldest to newest).
 *         Returns an empty list if the query fails or no sessions are found.
 *
 * @note On query failure, error details are logged to qCritical() and qDebug().
 *
 * @see PracticeSession
 */
QList<PracticeSession> DatabaseManager::getLastSessions(int songId, int limit)
{
    QList<PracticeSession> sessions;

    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);

    // Sort by date and ID in descending order to get the very latest entries.

    QString sql = "SELECT practice_date, start_bar, end_bar, practiced_bpm, total_reps, successful_streaks "
                  "FROM practice_journal "
                  "WHERE song_id = :songId AND start_bar IS NOT NULL "
                  "ORDER BY practice_date DESC, id DESC";

    // We only append LIMIT to the string if limit > 0.
    if (limit > 0) {
        sql += " LIMIT :limit";
    }

    q.prepare(sql);

    q.bindValue(":songId", songId);

    // We only append LIMIT to the string if limit > 0.
    if (limit > 0) {
        q.bindValue(":limit", limit);
    }

    if (q.exec()) {
        while (q.next()) {
            PracticeSession s;
            s.date = q.value("practice_date").toDate();
            s.startBar = q.value("start_bar").toInt();
            s.endBar = q.value("end_bar").toInt();
            s.bpm = q.value("practiced_bpm").toInt();
            s.reps = q.value("total_reps").toInt();
            s.streaks = q.value("successful_streaks").toInt();

            sessions.prepend(s);
        }
    } else {
        qCritical() << "[DatabaseManager] getLastSessions error:" << q.lastError().text();
        qDebug() << "[DatabaseManager] getLastSessions fullquery:" << q.executedQuery();
    }
    return sessions;
}

// =============================================================================
// --- Journal & Notice
// =============================================================================

/**
 * @brief Adds a new journal note entry for a practice session
 *
 * Inserts a new record into the practice_journal table with the provided song ID,
 * note text, and current timestamp.
 *
 * @param songId The ID of the song associated with this journal entry
 * @param note The practice journal note text to be recorded
 *
 * @return true if the journal entry was successfully inserted into the database,
 *         false if the insertion failed. On failure, an error message is logged
 *         with the specific database error details.
 *
 * @note The practice_date is automatically set to the current timestamp (CURRENT_TIMESTAMP)
 *       at the time of insertion.
 *
 * @see practice_journal table schema
 */
bool DatabaseManager::addJournalNote(int songId, const QString &note)
{
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);

    // Create a new historical entry
    q.prepare("INSERT INTO practice_journal (song_id, note_text, practice_date) "
              "VALUES (?, ?, CURRENT_TIMESTAMP)");
    q.addBindValue(songId);
    q.addBindValue(note);

    if (!q.exec()) {
        qCritical() << "[DatabaseManager] addJournalNote error: " << q.lastError().text();
        qCritical() << "[DatabaseManager] addJournalNote fullquery: " << q.executedQuery();
        return false;
    }
    return true;
}

/**
 * @brief Saves or updates a practice journal note for a song on a specific date.
 *
 * This function checks if a practice journal entry already exists for the given song
 * on the specified date. If an entry exists, it updates the note text. If no entry
 * exists, it creates a new practice journal record.
 *
 * @param songId The ID of the song to which the note belongs.
 * @param date The date of the practice session (QDate object).
 * @param note The note text to save or update.
 *
 * @return bool True if the operation (insert or update) was successful, false otherwise.
 *              On failure, an error message is logged to the debug output.
 *
 * @note The date is converted to "yyyy-MM-dd" format for database storage and comparison.
 * @note If the query execution fails, the error details are logged via qCritical().
 *
 * @see QSqlQuery, QDate
 */
bool DatabaseManager::saveOrUpdateNote(int songId, QDate date, const QString &note)
{
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);

    QString dateStr = date.toString("yyyy-MM-dd");

    // Check if an entry already exists for this day.
    q.prepare("SELECT id FROM practice_journal WHERE song_id = ? AND DATE(practice_date) = ?");
    q.addBindValue(songId);
    q.addBindValue(dateStr);

    if (q.exec() && q.next()) {
        // Update existing entry
        int entryId = q.value(0).toInt();
        q.prepare("UPDATE practice_journal SET note_text = ? WHERE id = ?");
        q.addBindValue(note);
        q.addBindValue(entryId);
    } else {
        // Create new entry
        q.prepare(
            "INSERT INTO practice_journal (song_id, note_text, practice_date) VALUES (?, ?, ?)");
        q.addBindValue(songId);
        q.addBindValue(note);
        q.addBindValue(dateStr); // Use the date from the calendar here!
    }
    if (!q.exec()) {
        qCritical() << "[DatabaseManager] saveOrUpdateNote error: " << q.lastError().text();
        qDebug() << "[DatabaseManager] saveOrUpdateNote fullquery: " << q.executedQuery();
        return false;
    }
    return true;
}

// Saves the general note for the song
/**
 * @brief Updates or creates a practice journal entry with notes for a specific song on a given date.
 *
 * This function checks if a practice journal entry already exists for the specified song on the given date.
 * If an entry exists, it updates the note_text field. If no entry exists, it creates a new one.
 *
 * @param songId The unique identifier of the song to update notes for.
 * @param notes The note text to be stored or updated in the practice journal.
 * @param date The practice date for the journal entry (formatted as "yyyy-MM-dd").
 *
 * @return true if the update or insert operation was successful; false otherwise.
 *         On failure, an error message is logged via qDebug().
 *
 * @note The function uses prepared statements to prevent SQL injection.
 * @note If an existing entry is found, only the note_text field is updated.
 * @note If no existing entry is found, a new entry is created with song_id, note_text, and practice_date.
 */
bool DatabaseManager::updateSongNotes(int songId, const QString &notes, QDate date)
{
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);

    QString dateStr = date.toString("yyyy-MM-dd");

    // Check if an entry already exists for this song on this day.
    q.prepare("SELECT id FROM practice_journal WHERE song_id = ? AND DATE(practice_date) = ?");
    q.addBindValue(songId);
    q.addBindValue(dateStr);

    if (q.exec() && q.next()) {
        // Update existing entry
        int entryId = q.value(0).toInt();
        q.prepare("UPDATE practice_journal SET note_text = ? WHERE id = ?");
        q.addBindValue(notes);
        q.addBindValue(entryId);
    } else {
        // Create a new entry for this day
        q.prepare(
            "INSERT INTO practice_journal (song_id, note_text, practice_date) VALUES (?, ?, ?)");
        q.addBindValue(songId);
        q.addBindValue(notes);
        q.addBindValue(dateStr);
    }

    if (!q.exec()) {
        qCritical() << "[DatabaseManager] updateSongNotes error: " << q.lastError().text();
        qDebug() << "[DatabaseManager] updateSongNotes fullquery: " << q.executedQuery();
        return false;
    }

    return true;
}

/**
 * @brief Retrieves the practice note for a specific song on a given date.
 *
 * @param songId The unique identifier of the song.
 * @param date The date for which to retrieve the practice note.
 *
 * @return A QString containing the note text for the specified song and date.
 *         Returns an empty string if no entry exists for the given song and date combination.
 *
 * @note The function queries the practice_journal table and returns only the first matching entry.
 *       The date comparison is performed using the DATE() SQL function to ensure time components are ignored.
 */
QString DatabaseManager::getNoteForDay(int songId, QDate date)
{
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);

    // Convert date to ISO format yyyy-MM-dd
    QString dateStr = date.toString("yyyy-MM-dd");

    q.prepare("SELECT note_text FROM practice_journal "
              "WHERE song_id = ? AND DATE(practice_date) = ? "
              "LIMIT 1");
    q.addBindValue(songId);
    q.addBindValue(dateStr);

    if (q.exec() && q.next()) {
        return q.value(0).toString();
    } else {
        if (!q.lastError().text().isEmpty()) {
            qCritical() << "[DatabaseManager] getNoteForDay error: " << q.lastError().text();
            qDebug() << "[DatabaseManager] getNoteForDay fullquery: " << q.executedQuery();
        }
    }

    return ""; // If no entry exists for this day
}

DatabaseManager::SongDetails DatabaseManager::getSongDetails(qlonglong songId)
{
    SongDetails details;
    if (songId == 0)
        return details;
    details.id = -1; // Fehler-Indikator

    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);

    q.prepare("SELECT s.id, s.title, a.name AS artist_name, t.name AS tuning_name, s.base_bpm, "
              "       pj.practiced_bpm AS last_practice_bpm "
              "FROM songs s "
              "LEFT JOIN artists a ON s.artist_id = a.id "
              "LEFT JOIN tunings t ON s.tuning_id = t.id "
              "LEFT JOIN practice_journal pj ON pj.id = ("
              "    SELECT id FROM practice_journal "
              "    WHERE song_id = s.id "
              "    ORDER BY practice_date DESC, id DESC LIMIT 1"
              ") "
              "WHERE s.id = ?");

    q.addBindValue(songId);

    if (q.exec() && q.next()) {
        details.id = q.value("id").toLongLong();
        details.title = q.value("title").toString();
        details.artist = q.value("artist_name").toString();
        details.tuning = q.value("tuning_name").toString();
        details.bpm = q.value("base_bpm").toInt();
        details.practice_bpm = q.value("last_practice_bpm").toInt();
    } else {
        qCritical() << "[DatabaseManager] getSongDetails songId" << songId
                    << "not found: " << q.lastError().text();
        qCritical() << "[DatabaseManager] getSongDetails fullqueryL: " << q.executedQuery();
    }

    return details;
}

// for combobox
// Artist, Title, songId
QList<DatabaseManager::SongDetails> DatabaseManager::getFilteredFiles(bool gp, bool audio, bool video, bool pdf, bool unlinkedOnly) {

    QList<SongDetails> songs;
    QStringList extensions;

    if (gp)    extensions << FileUtils::getGuitarProFormats();
    if (audio) extensions << FileUtils::getAudioFormats();
    if (pdf)   extensions << FileUtils::getPdfFormats();
    if (video) extensions << FileUtils::getVideoFormats();

    if (extensions.isEmpty()) return songs;

    QSqlDatabase db = QSqlDatabase::database();

    QString sql = "SELECT "
                  "mf.id AS file_id, "
                  "mf.song_id, "
                  "mf.file_path, "
                  "s.title, "
                  "s.base_bpm, "
                  "a.name AS artist_name, "
                  "t.name AS tuning_name "
                  "FROM media_files mf "
                  "LEFT JOIN songs s ON mf.song_id = s.id "
                  "LEFT JOIN artists a ON s.artist_id = a.id "
                  "LEFT JOIN tunings t ON s.tuning_id = t.id "
                  "WHERE (";

    QStringList conditions;
    for (QString ext : std::as_const(extensions)) {
        ext.remove('*');
        conditions << "mf.file_path LIKE '%" + ext + "'";
    }
    sql += conditions.join(" OR ") + ")";

    if (unlinkedOnly) {
        sql += " AND mf.song_id IS NULL";
    }

    sql += " ORDER BY s.title ASC, mf.file_path ASC";

    QSqlQuery q(db);

    if(q.exec(sql)) {
        while (q.next()) {
            SongDetails details;
            details.songId = q.value("song_id").toInt();
            details.id = q.value("file_id").toInt();

            QString rawPath = q.value("file_path").toString();
            QString managedPath = getManagedPath();

            QString path = managedPath.isEmpty() ? rawPath : QDir(managedPath).filePath(rawPath);
            QString cleaned = QDir::cleanPath(path);

            details.filePath = QFileInfo(cleaned).fileName();
            details.fullPath = cleaned;

            details.artist = q.value("artist_name").toString();
            details.title = q.value("title").toString();
            details.bpm = q.value("base_bpm").toInt();
            details.tuning = q.value("tuning_name").toString();

            songs.append(details);
        }
    } else {
        QString error = q.lastError().text();
        QString executedSql = q.executedQuery();
        qCritical() << "[DatabaseManager] getFilteredFiles error: " << q.lastError().text();
        qDebug() << "[DatabaseManager] getFilteredFiles fullquery: " << q.executedQuery();
    }

    return songs;
}

// =============================================================================
// --- Statistics & Dashboard
// =============================================================================

/**
 * @brief Retrieves all songs practiced on a specific date.
 *
 * Queries the database to find all songs that have practice entries for the given date.
 * Songs are grouped by ID to avoid duplicates if practiced multiple times on the same day.
 *
 * @param date The date for which to retrieve practiced songs (QDate format).
 * @return QMap<int, QString> A map where keys are song IDs and values are song titles.
 *         Returns an empty map if no songs were practiced on the given date or if the query fails.
 *
 * @note On query failure, an error message is logged to qDebug() with details from lastError().
 * @see getPracticedSongsForDay
 */
QMap<int, QString> DatabaseManager::getPracticedSongsForDay(QDate date)
{
    QMap<int, QString> songMap;

    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);

    // Get song ID and title for all entries this day
    q.prepare("SELECT s.id, s.title FROM songs s "
              "JOIN practice_journal pj ON s.id = pj.song_id "
              "WHERE DATE(pj.practice_date) = ? "
              "GROUP BY s.id");
    q.addBindValue(date.toString("yyyy-MM-dd"));

    if (q.exec()) {
        while (q.next()) {
            songMap.insert(q.value(0).toInt(), q.value(1).toString());
        }
    } else {
        qCritical() << "[DatabaseManager] getPracticedSongsForDay error: " << q.lastError().text();
        qDebug() << "[DatabaseManager] getPracticedSongsForDay fullquery: " << q.executedQuery();
    }
    return songMap;
}

// Creates the text for mouseover
/**
 * @brief Retrieves a formatted summary of songs practiced on a specific date.
 *
 * Queries the database for all songs that were practiced on the given date,
 * grouped by song ID to avoid duplicates within the same day.
 *
 * @param date The date for which to retrieve the practice summary.
 *
 * @return A QString containing a newline-separated list of song titles
 *         prefixed with bullet points (•). Returns an empty string if no
 *         songs were practiced on the given date or if the query fails.
 *
 * @note If the database query fails, an error message is logged to qDebug().
 *
 * @see database(), QSqlQuery, QDate
 */
QString DatabaseManager::getPracticeSummaryForDay(QDate date)
{
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);

    q.prepare("SELECT s.title FROM songs s "
              "JOIN practice_journal pj ON s.id = pj.song_id "
              "WHERE DATE(pj.practice_date) = ? "
              "GROUP BY s.id");
    q.addBindValue(date.toString("yyyy-MM-dd"));

    QStringList songs;
    if (q.exec()) {
        while (q.next()) {
            songs << "• " + q.value(0).toString();
        }
    } else {
        qCritical() << "[DatabaseManager] getPracticeSummaryForDay error: " << q.lastError().text();
        qDebug() << "[DatabaseManager] getPracticeSummaryForDay fullquery: " << q.executedQuery();
    }
    return songs.isEmpty() ? "" : songs.join("\n");
}

/**
 * @brief Retrieves all unique practice dates from the database.
 *
 * Queries the practice_journal table and extracts distinct practice dates.
 * Each date is returned only once, even if multiple practice sessions occurred
 * on the same date.
 *
 * @return QList<QDate> A list of all unique practice dates sorted in the order
 *                       they appear in the database. Returns an empty list if
 *                       no practice dates are found or if the query fails.
 *
 * @note If the database query fails, an error message is logged to qDebug()
 *       but the function will still return an empty list without throwing.
 *
 * @see practice_journal table
 */
QList<QDate> DatabaseManager::getAllPracticeDates()
{
    QList<QDate> dates;

    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);

    // DISTINCT ensures that we only receive each date once.
    if (!q.exec("SELECT DISTINCT DATE(practice_date) FROM practice_journal")) {
        qCritical() << "[DatabaseManager] getAllPracticeDates error: " << q.lastError().text();
        qDebug() << "[DatabaseManager] getAllPracticeDates fullquery: " << q.executedQuery();
    }
    while (q.next()) {
        dates << QDate::fromString(q.value(0).toString(), "yyyy-MM-dd");
    }
    return dates;
}

// =============================================================================
// --- Reminder
// =============================================================================

/**
 * Checks if a reminder for the current period has already been completed.
 * Consider the intervals: One-time, Dail and Monthly.
 * Idea - add Weekly
 */
bool DatabaseManager::isReminderCompleted(int reminderId)
{
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);

    q.prepare("SELECT reminder_date, weekday, is_daily, is_monthly FROM reminders WHERE id = ?");
    q.addBindValue(reminderId);

    if (!q.exec() || !q.next()) {
        qCritical() << "[DatabaseManager] isReminderCompleted error: " << q.lastError().text();
        qDebug() << "[DatabaseManager] isReminderCompleted fullquery: " << q.executedQuery();
        return false;
    }

    QVariant weekdayVar = q.value("weekday");
    bool isDaily = q.value("is_daily").toBool();
    bool isMonthly = q.value("is_monthly").toBool();
    QString reminderDate = q.value("reminder_date").toString();

    if (!weekdayVar.isNull()) {
        int targetWeekday = weekdayVar.toInt(); // 0=So, 1=Mo...
        QSqlQuery dayCheck(db);
        dayCheck.exec("SELECT strftime('%w', 'now')");
        if (dayCheck.next()) {
            int today = dayCheck.value(0).toInt();
            if (today != targetWeekday) {
                return true;
            }
        }
    }

    QString checkSql = "SELECT COUNT(*) FROM reminder_completions WHERE reminder_id = :rid AND ";

    if (!reminderDate.isEmpty()) {
        checkSql += "date(completion_date) = date(:rdate)";
    } else if (isDaily) {
        checkSql += "date(completion_date) = date('now')";
    } else if (!weekdayVar.isNull()) {
        checkSql += "strftime('%W', completion_date) = strftime('%W', 'now') "
                    "AND strftime('%Y', completion_date) = strftime('%Y', 'now')";
    } else if (isMonthly) {
        checkSql += "strftime('%m-%Y', completion_date) = strftime('%m-%Y', 'now')";
    } else {
        return false;
    }

    QSqlQuery checkQuery(db);

    checkQuery.prepare(checkSql);
    checkQuery.bindValue(":rid", reminderId);

    if (!reminderDate.isEmpty())
        checkQuery.bindValue(":rdate", reminderDate);

    if (checkQuery.exec() && checkQuery.next()) {
        return checkQuery.value(0).toInt() > 0;
    }

    return false;
}

bool DatabaseManager::addReminder(int songId,
                                  int startBar,
                                  int endBar,
                                  int bpm,
                                  bool isDaily,
                                  bool isMonthly,
                                  int weekday,
                                  const QString &reminderDate)
{
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.transaction())
        return false;

    QSqlQuery q(db);

    // If reminderDate is empty, it is a recurring reminder.
    q.prepare(
        "INSERT INTO reminders (song_id, reminder_date, is_daily, is_monthly, weekday, is_active) "
        "VALUES (:sid, :rdate, :daily, :monthly, :wd, 1)");

    q.bindValue(":sid", songId);
    q.bindValue(":rdate",
                reminderDate.isEmpty() ? QVariant(QMetaType::fromType<QString>()) : reminderDate);
    q.bindValue(":daily", isDaily ? 1 : 0);
    q.bindValue(":monthly", isMonthly ? 1 : 0);
    q.bindValue(":wd", weekday != -1 ? weekday : QVariant(QMetaType::fromType<int>()));

    if (!q.exec()) {
        qCritical() << "[DatabaseManager] Error inserting reminder:" << q.lastError().text();
        qDebug() << "[DatabaseManager] reminder fullquery: " << q.executedQuery();
        db.rollback();
        return false;
    }

    int reminderId = q.lastInsertId().toInt();

    // 2. Conditions in 'reminder_completion_conditions'
    q.prepare(
        "INSERT INTO reminder_completion_conditions (reminder_id, min_bpm, start_bar, end_bar) "
        "VALUES (:rid, :bpm, :sbar, :ebar)");
    q.bindValue(":rid", reminderId);
    q.bindValue(":bpm", bpm);
    q.bindValue(":sbar", startBar);
    q.bindValue(":ebar", endBar);

    if (!q.exec()) {
        qCritical() << "[DatabaseManager] Error inserting conditions:" << q.lastError().text();
        qDebug() << "[DatabaseManager] reminder fullquery: " << q.executedQuery();
        db.rollback();
        return false;
    }

    db.transaction();
    db.commit();

    return true;
}

QVariantList DatabaseManager::getRemindersForDate(const QDate &date)
{
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);

    QString sql = "SELECT r.id AS reminder_id, s.id AS song_id, s.title, c.start_bar, c.end_bar, "
                  "c.min_bpm, "
                  "(EXISTS ( "
                  "  SELECT 1 FROM practice_journal p "
                  "  WHERE p.song_id = r.song_id "
                  "  AND p.start_bar <= c.start_bar "
                  "  AND p.end_bar >= c.end_bar "
                  "  AND p.practiced_bpm >= c.min_bpm "
                  "  AND p.practice_date = :date "
                  ")) AS is_done "
                  "FROM reminders r "
                  "JOIN songs s ON r.song_id = s.id "
                  "JOIN reminder_completion_conditions c ON r.id = c.reminder_id "
                  "WHERE r.user_id = 1 AND r.is_active = 1 AND ("
                  "  r.reminder_date = :date OR r.is_daily = 1 OR r.weekday = :wd OR "
                  "  (r.is_monthly = 1 AND strftime('%d', r.reminder_date) = :dom)"
                  ")";

    q.prepare(sql);

    q.bindValue(":date", date.toString("yyyy-MM-dd"));
    q.bindValue(":wd", date.dayOfWeek());     // Montag = 1, Sonntag = 7
    q.bindValue(":dom", date.toString("dd")); // Tag des Monats (01-31)

    QVariantList results;
    if (q.exec()) {
        while (q.next()) {
            QVariantMap item;
            item["id"] = q.value("reminder_id");
            item["songId"] = q.value("song_id");
            item["title"] = q.value("title");
            item["start_bar"] = q.value("start_bar");
            item["end_bar"] = q.value("end_bar");
            item["is_done"] = q.value("is_done").toBool();
            item["range"] = QString("Bar %1 - %2")
                                .arg(q.value("start_bar").toInt())
                                .arg(q.value("end_bar").toInt());
            item["bpm"] = q.value("min_bpm");
            results.append(item);
        }
    } else {
        qCritical() << "[DatabaseManager] getRemindersForDate, error:" << q.lastError().text();
        qDebug() << "[DatabaseManager] getRemindersForDate, fullquery: " << q.executedQuery();
    }
    return results;
}

ReminderDialog::ReminderData DatabaseManager::getReminder(int reminderId)
{
    ReminderDialog::ReminderData data;
    data.songId = -1; // Fallback, falls nichts gefunden wird

    QSqlQuery q;
    q.prepare("SELECT r.song_id, r.is_daily, r.is_monthly, r.weekday, r.reminder_date, "
              "c.start_bar, c.end_bar, c.min_bpm "
              "FROM reminders r "
              "LEFT JOIN reminder_completion_conditions c ON r.id = c.reminder_id "
              "WHERE r.id = :id");

    q.bindValue(":id", reminderId);

    if (q.exec() && q.next()) {
        data.songId       = q.value(0).toInt();    // song_id

        // First row-table (scheduling)
        data.isDaily      = q.value(1).toBool();   // is_daily
        data.isMonthly    = q.value(2).toBool();   // is_monthly
        data.weekday      = q.value(3).toInt();    // weekday
        data.reminderDate = q.value(4).toString(); // reminder_date

        // Then the column table (conditions)
        data.startBar     = q.value(5).toInt();    // start_bar
        data.endBar       = q.value(6).toInt();    // end_bar
        data.targetBpm    = q.value(7).toInt();    // min_bpm

        // Debugging for control
        // qDebug() << "Loaded ReminderId: " << reminderId << " - SongId:" << data.songId << "BPM:" << data.targetBpm
        //          << "Bars:" << data.startBar << "-" << data.endBar;
    } else {
        qCritical() << "[DatabaseManager] getReminder failed: " << q.lastError().text();
        qDebug() << "[DatabaseManager] getReminder fullquery: " << q.executedQuery();
    }

    return data;
}

bool DatabaseManager::updateReminder(int reminderId, const ReminderDialog::ReminderData &data) {
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.transaction()) {
        return false;
    }

    QSqlQuery q1(db);
    // 1. Update the schedule in 'reminders'
    q1.prepare("UPDATE reminders SET "
               "is_daily = :d, is_monthly = :m, weekday = :w, reminder_date = :rd "
               "WHERE id = :id");
    q1.bindValue(":d", data.isDaily ? 1 : 0);
    q1.bindValue(":m", data.isMonthly ? 1 : 0);
    q1.bindValue(":w", data.weekday);
    q1.bindValue(":rd", data.reminderDate);
    q1.bindValue(":id", reminderId);

    if (!q1.exec()) {
        qCritical() << "[DatabaseManager] updateReminder (Table reminders) failed:" << q1.lastError().text();
        db.rollback();
        return false;
    }

    QSqlQuery q2(db);
    // 2. Update of targets in 'reminder_completion_conditions'
    // Use 'INSERT OR REPLACE' if the condition line does not already exist.
    q2.prepare("UPDATE reminder_completion_conditions SET "
               "start_bar = :s, "
               "end_bar = :e, "
               "min_bpm = :bpm "
               "WHERE reminder_id = :id");

    q2.bindValue(":s", data.startBar);
    q2.bindValue(":e", data.endBar);
    q2.bindValue(":bpm", data.targetBpm);
    q2.bindValue(":id", reminderId);

    if (!q2.exec()) {
        qCritical() << "[DatabaseManager] updateReminder (Table conditions) failed:" << q2.lastError().text();
        db.rollback();
        return false;
    }

    return db.commit();
}

bool DatabaseManager::deleteReminder(int reminderId)
{
    QSqlDatabase db = QSqlDatabase::database();

    QSqlQuery q(db);
    q.prepare("DELETE FROM reminder_completion_conditions WHERE reminder_id = :id");
    q.bindValue(":id", reminderId);

    if (!q.exec()) {
        qCritical() << "[DatabaseManager] deleteReminder, error:" << q.lastError().text();
        qDebug() << "[DatabaseManager] deleteReminder, fullquery: " << q.executedQuery();
        return false;
    }

    QSqlQuery q2(db);
    q2.prepare("DELETE FROM reminders WHERE id = :id");
    q2.bindValue(":id", reminderId);

    if (!q2.exec()) {
        qCritical() << "[DatabaseManager] deleteReminder, error:" << q.lastError().text();
        qDebug() << "[DatabaseManager] deleteReminder, fullquery: " << q.executedQuery();
        return false;
    }

    return true;
}

// =============================================================================
// --- Settings (configuration)
// =============================================================================

/**
 * @brief Sets or updates a setting in the database.
 *
 * Inserts a new setting into the database or replaces an existing one with the same key.
 * QVariant values are automatically converted to strings, including boolean values which
 * are converted to "true" or "false".
 *
 * @param key The setting key identifier.
 * @param value The setting value as a QVariant. Will be converted to string format.
 *
 * @return true if the setting was successfully inserted or updated, false otherwise.
 *         On failure, error details are logged to debug output.
 *
 * @note Uses INSERT OR REPLACE SQL statement to handle both insertion and updating.
 * @note Boolean QVariant values are automatically converted to "true"/"false" strings.
 */
bool DatabaseManager::setSetting(const QString &key, const QVariant &value)
{
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);

    q.prepare("INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?)");
    q.addBindValue(key);
    q.addBindValue(
        value.toString()); // QVariant automatically converts boolean values ​​to "true"/"false".

    if (!q.exec()) {
        qCritical() << "[DatabaseManager] setSetting error: " << q.lastError().text();
        qDebug() << "[DatabaseManager] setSetting fullquery: " << q.executedQuery();
        return false;
    }

    return true;
}

/**
 * @brief Retrieves a string setting value from the database.
 *
 * Queries the settings table for a value associated with the given key.
 * If the key exists, returns its corresponding value. If the key does not exist
 * or if the query fails, returns the provided default value.
 *
 * @param key The setting key to look up in the database.
 * @param defaultValue The value to return if the key is not found or query fails.
 *
 * @return The setting value as a QString if found, otherwise the defaultValue.
 *         On database error, logs the error and returns defaultValue.
 *
 * @note Logs database errors to qDebug() if the query fails.
 */
QString DatabaseManager::getSetting(const QString &key, const QString &defaultValue)
{
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);

    q.prepare("SELECT value FROM settings WHERE key = :key");
    q.bindValue(":key", key);

    if (q.exec() && q.next()) {
        return q.value(0).toString();
    } else {
        qCritical() << "[DatabaseManager] getSetting error: " << q.lastError().text();
        qDebug() << "[DatabaseManager] getSetting fullquery: " << q.executedQuery();
    }
    return defaultValue;
}

/**
 * @brief Retrieves a setting value from the database.
 *
 * Queries the settings table for a value associated with the given key.
 * If the key exists in the database, returns its value. If the query fails
 * or the key is not found, returns the provided default value.
 *
 * @param key The settings key to look up in the database.
 * @param defaultValue The value to return if the key is not found or query fails.
 *
 * @return The setting value if found, otherwise the defaultValue.
 *         The return type is QVariant to support multiple data types.
 *
 * @note On query failure, an error message is logged to qDebug().
 */
QVariant DatabaseManager::getSetting(const QString &key, const QVariant &defaultValue)
{
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);

    q.prepare("SELECT value FROM settings WHERE key = ?");
    q.addBindValue(key);

    if (q.exec() && q.next()) {
        return q.value(0);
    } else {
        qCritical() << "[DatabaseManager] getSetting (QVariant) error: " << q.lastError().text();
        qDebug() << "[DatabaseManager] getSetting (QVariant): fullquery: " << q.executedQuery();
    }
    return defaultValue;
}

// Privat

/**
 * @brief Gets the user ID for a given name, or creates a new user if not found.
 *
 * Attempts to retrieve the ID of a user with the specified name from the database.
 * If the user does not exist, a new user record is created with the provided name and role.
 *
 * @param name The name of the user to look up or create.
 * @param role The role to assign to the user if a new record is created.
 *
 * @return The ID of the existing or newly created user on success, or -1 if an error occurs.
 *
 * @note If a query execution fails, an error message is logged via qDebug().
 *
 * @see QSqlQuery, QSqlDatabase
 */
int DatabaseManager::getOrCreateUserId(const QString &name, const QString &role)
{
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);

    // The table must be 'users', not 'groups'.
    q.prepare("SELECT id FROM users WHERE name = :name");
    q.bindValue(":name", name);

    if (q.exec() && q.next()) {
        return q.value(0).toInt();
    } else {
        qCritical() << "[DatabaseManager] getOrCreateUserId, error: " << q.lastError().text();
        qDebug() << "[DatabaseManager] getOrCreateUserId, fullquery: " << q.executedQuery();
    }

    q.prepare("INSERT INTO users (name, role) VALUES (:name, :role)");
    q.bindValue(":name", name);
    q.bindValue(":role", role);

    if (q.exec()) {
        return q.lastInsertId().toInt();
    } else {
        qCritical() << "[DatabaseManager] getOrCreateUserId, error:  " << q.lastError().text();
        qDebug() << "[DatabaseManager] getOrCreateUserId, fullquery: " << q.executedQuery();
    }

    return -1;
}

QStringList DatabaseManager::getAllArtists() {
    QStringList artists;
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);
    q.prepare("SELECT name FROM artists ORDER BY name ASC");

    if (q.exec()) {
        while (q.next()) {
            artists << q.value(0).toString();
        }
    } else {
        qCritical() << "[DatabaseManager] getAllArtists, error: " << q.lastError().text();
        qDebug() << "[DatabaseManager] getAllArtists, fullquery: " << q.executedQuery();
    }

    return artists;
}

QStringList DatabaseManager::getAllTunings() {
    QStringList tunings;
    QSqlDatabase db = QSqlDatabase::database();
    QSqlQuery q(db);
    q.prepare("SELECT name FROM tunings ORDER BY name ASC");

    if (q.exec()) {
        while (q.next()) {
            tunings << q.value(0).toString();
        }
    } else {
        qCritical() << "[DatabaseManager] getAllTunings, error: " << q.lastError().text();
        qDebug() << "[DatabaseManager] getAllTunings, fullquery: " << q.executedQuery();
    }

    return tunings;
}

bool DatabaseManager::updateSong(int songId, const QString &title, int artistId, int tuningId, int bpm) {
    QSqlQuery q;
    q.prepare("UPDATE songs SET "
              "title = :title, "
              "artist_id = :artistId, "
              "tuning_id = :tuningId, "
              "base_bpm = :bpm "
              "WHERE id = :id");

    q.bindValue(":title", title);
    q.bindValue(":artistId", artistId);
    q.bindValue(":tuningId", tuningId);
    q.bindValue(":bpm", bpm);
    q.bindValue(":id", songId);

    if (!q.exec()) {
        qCritical() << "[DatabaseManager] updateSong, error: " << q.lastError().text();
        qDebug() << "[DatabaseManager] updateSong, fullquery: " << q.executedQuery();
        return false;
    }
    return true;
}
