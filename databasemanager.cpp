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
 * @note All database operations use the active QSqlDatabase connection managed by db_m.
 *       Transactions are used for multi-step operations to ensure data consistency.
 *
 * @warning Foreign key constraints are enabled via PRAGMA. Ensure ON DELETE CASCADE is
 *          properly configured in foreign key definitions for cascading deletes.
 */
#include "databasemanager.h"
#include "fileutils.h"

#include <QDir>
#include <QSqlQuery>
#include <QSqlError>
#include <QUrl>

// =============================================================================
// --- Singleton & Lifecycle
// =============================================================================

DatabaseManager& DatabaseManager::instance() {
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
 * @note The database member variable (db_m) is populated with the active
 *       database connection on success.
 * 
 * @see getDatabaseVersion(), setDatabaseVersion(), createInitialTables()
 */
bool DatabaseManager::initDatabase(const QString &dbPath) {
    const QString connectionName = QSqlDatabase::defaultConnection;

    // If the connection already exists
    if (QSqlDatabase::contains(connectionName)) {
        {
            // Use Scope {} so that the 'db' object is destroyed before calling removeDatabase!
            QSqlDatabase db = QSqlDatabase::database(connectionName);
            if (db.isOpen() && db.databaseName() == dbPath) {
                db_m = db;
                db_m = QSqlDatabase::database(db_m.connectionName());
                QSqlQuery query(db_m);
                if (!query.exec("PRAGMA foreign_keys = ON;")) {
                    qWarning() << "Konnte Foreign Keys nicht aktivieren:" << query.lastError().text();
                }
                return true;
            }
        }
        // If name is incorrect or too long: Remove old connection cleanly
        QSqlDatabase::removeDatabase(connectionName);
    }

    // Create a new one now and assign it to the member variable.
    db_m = QSqlDatabase::addDatabase("QSQLITE");
    db_m.setDatabaseName(dbPath);

    if (!db_m.open()) {
        qCritical() << "Connection failed:" << db_m.lastError().text();
        return false;
    }

    // Enable foreign keys for this connection (important for ON DELETE CASCADE)
    QSqlQuery q(db_m);
    if (!q.exec("PRAGMA foreign_keys = ON;")) {
        qWarning() << "Konnte Foreign Keys nicht aktivieren:" << q.lastError().text();
    }

    // Check versioning
    int currentVersion = getDatabaseVersion();
    int targetVersion = 1;

    if (currentVersion < 1) {
        if (!createInitialTables()) return false;
        if (!setDatabaseVersion(targetVersion)) return false;
    }

    /*
        if (currentVersion < 2) {
            if (upgradeToVersion2()) {
                if (!setDatabaseVersion(2)) {
                    return false; // Abort if version could not be saved
                }
            } else {
                return false;
            }
        }
    */

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
void DatabaseManager::closeDatabase() {
    // Remember names
    QString connectionName = db_m.connectionName();

    // Close database
    if (db_m.isOpen()) {
        db_m.close();
    }

    db_m = QSqlDatabase();
    QSqlDatabase::removeDatabase(connectionName);
}

QSqlDatabase DatabaseManager::database() const {
    if (QSqlDatabase::contains(QSqlDatabase::defaultConnection)) {
        return QSqlDatabase::database(QSqlDatabase::defaultConnection);
    }
    return QSqlDatabase();
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
bool DatabaseManager::createInitialTables() {
    QSqlQuery q(database());
    if (!q.exec("PRAGMA foreign_keys = ON")) return false;

    // 1. USER (Teacher / Student)
    // TODO: Preparations are underway to manage more users.
    if (!q.exec("CREATE TABLE IF NOT EXISTS users ("
                    "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                    "name TEXT UNIQUE, "
                    "role TEXT, " // 'teacher' oder 'student'
                    "created_at DATETIME DEFAULT CURRENT_TIMESTAMP)")) return false;

    // 2. SONGS (The logical unit)
    if (!q.exec("CREATE TABLE IF NOT EXISTS songs ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                "user_id INTEGER, "
                "title TEXT, "
                "artist_id INTEGER, "
                "tuning_id INTEGER, "
                "base_bpm INTEGER, "
                "total_bars INTEGER, "
                "current_bpm INTEGER DEFAULT 0, "
                "target_bpm INTEGER DEFAULT 0, " // for the % calculation, for later by gp parser.
                "is_favorite INTEGER DEFAULT 0, "
                "updated_at DATETIME DEFAULT CURRENT_TIMESTAMP, "
                "FOREIGN KEY(artist_id) REFERENCES artists(id), "
                "FOREIGN KEY(tuning_id) REFERENCES tunings(id))")) return false;

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
                    "FOREIGN KEY(song_id) REFERENCES songs(id) ON DELETE CASCADE)")) return false;

    // 4. EXERCISE JOURNAL (Progress Tracking)
    if (!q.exec("CREATE TABLE IF NOT EXISTS practice_journal ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                "user_id INTEGER, "
                "song_id INTEGER, "
                "practice_date DATETIME DEFAULT CURRENT_TIMESTAMP, "
                "start_bar INTEGER, "
                "end_bar INTEGER, "
                "practiced_bpm INTEGER, "
                "total_reps INTEGER, "        // NEU: Wie oft insgesamt gespielt
                "successful_streaks INTEGER, "// NEU: Wie oft die 7er-Serie geschafft
                "rating INTEGER, "            // Kannst du als "Gefühl" behalten (0-100)
                "note_text TEXT, "
                "FOREIGN KEY(user_id) REFERENCES users(id) ON DELETE CASCADE, "
                "FOREIGN KEY(song_id) REFERENCES songs(id) ON DELETE CASCADE)")) return false;

    // 7. SETTINGS (Key-Value-Store für paths, flags)
    if (!q.exec("CREATE TABLE IF NOT EXISTS settings ("
                    "key TEXT PRIMARY KEY, "
                    "value TEXT)")) return false;

    // 8. Artists/Bands
    // -- Table for artists
    if (!q.exec("CREATE TABLE IF NOT EXISTS artists ("
                    "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                    "name TEXT UNIQUE NOT NULL)")) return false;

    // 9. Tunings
    if (!q.exec("CREATE TABLE IF NOT EXISTS tunings ("
                    "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                    "name TEXT UNIQUE NOT NULL)")) return false;

    // Tunings:
    // TODO: Later, fill it with all the tuning options, or better yet, use a GP Parser.
    // Go through all the GP files and then fill them.
    // Initially populate standard tunings if the table is empty.
    if (!q.exec("INSERT OR IGNORE INTO tunings (name) VALUES "
               "('E-Standard'), ('Eb-Standard'), ('Drop D'), ('Drop C'), ('D-Standard')")) return false;

    // relation
    if (!q.exec("CREATE TABLE IF NOT EXISTS file_relations ("
                "file_id_a INTEGER, "
                "file_id_b INTEGER, "
                // "relation_type TEXT, " // -- e.g. 'backing track', 'tutorial', 'tab'
                "PRIMARY KEY (file_id_a, file_id_b))")) return false;

    if (!q.exec("INSERT OR IGNORE INTO settings (key, value) VALUES ('managed_path', '')")) return false;
    if (!q.exec("INSERT OR IGNORE INTO settings (key, value) VALUES ('is_managed', 'false')")) return false;
    if (!q.exec("INSERT OR IGNORE INTO settings (key, value) VALUES ('last_import_date', '')")) return false;
    if (!q.exec("CREATE INDEX IF NOT EXISTS idx_filepath ON media_files(file_path)")) return false;

    return true;
}

/**
 * @brief Checks if the songs table contains any data.
 * 
 * Executes a query to retrieve the first record from the songs table.
 * 
 * @return true if the query executed successfully and at least one row exists in the songs table; false otherwise.
 */
bool DatabaseManager::hasData() {
    QSqlQuery q(database());
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
int DatabaseManager::getDatabaseVersion() {
    QSqlQuery q(database());
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
bool DatabaseManager::setDatabaseVersion(int version) {
    QSqlQuery q(database());
    // PRAGMA user_version cannot be used with bind values ​​(?).
    // Therefore, .arg() is the correct way to go here.
    if (!q.exec(QString("PRAGMA user_version = %1").arg(version))) {
        qCritical() << "[DatabaseManager] Error setting DB version: " << q.lastError().text();
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
bool DatabaseManager::addFileToSong(qlonglong songId, const QString &filePath, bool isManaged, const QString &fileType, qint64 fileSize, QString fileHash) {
    QSqlQuery q(database());

    qDebug() << "[DatabaseManager] addFileToSong - filePath: " << filePath;

    // Logic: Is it a Guitar Pro file?
    // Take the ending (e.g., "gp5") and add "*." before it.
    // so that it matches QStringLists in FileUtils exactly.
    QString suffix = "*." + QFileInfo(filePath).suffix().toLower();
    bool isPracticeTarget = FileUtils::getGuitarProFormats().contains(suffix);

    q.prepare("INSERT INTO media_files (song_id, file_path, is_managed, file_type, file_size, file_hash, can_be_practiced) "
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
bool DatabaseManager::updatePracticeFlag(int fileId, bool canPractice) {
    QSqlQuery q(database());
    q.prepare("UPDATE media_files SET can_be_practiced = ? WHERE id = ?");
    q.addBindValue(canPractice ? 1 : 0);
    q.addBindValue(fileId);
    if (!q.exec()) {
        qCritical() << "[DatabaseManager] Error updatePracticeFlag: " << q.lastError().text();
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
QString DatabaseManager::getManagedPath() {
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
qlonglong DatabaseManager::createSong(const QString &title, const QString &artist, const QString &tuning) {
    int artistId = getOrCreateArtist(artist);
    int tuningId = getOrCreateTuning(tuning);

    QSqlQuery q(database());
    q.prepare("INSERT INTO songs (title, artist_id, tuning_id) VALUES (?, ?, ?)");
    q.addBindValue(title);
    q.addBindValue(artistId);
    q.addBindValue(tuningId);

    if (q.exec()) {
        return q.lastInsertId().toLongLong();
    } else {
        qCritical() << "[DatabaseManager] Error createSong:" << q.lastError().text();
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
int DatabaseManager::getOrCreateArtist(const QString &name) {
    QSqlQuery q(database());
    q.prepare("SELECT id FROM artists WHERE name = ?");
    q.addBindValue(name.trimmed());

    if (q.exec() && q.next()) {
        return q.value(0).toInt();
    } else {
        q.prepare("INSERT INTO artists (name) VALUES (?)");
        q.addBindValue(name.trimmed());
        q.exec();
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
int DatabaseManager::getOrCreateTuning(const QString &name) {
    QSqlQuery q(database());
    q.prepare("SELECT id FROM tunings WHERE name = ?");
    q.addBindValue(name.trimmed());

    if (q.exec() && q.next()) {
        return q.value(0).toInt();
    } else {
        q.prepare("INSERT INTO tunings (name) VALUES (?)");
        q.addBindValue(name.trimmed());
        q.exec();
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
QSet<QString> DatabaseManager::getAllFileHashes() {
    QSet<QString> hashSet;
    QSqlQuery query(database());
    query.prepare("SELECT file_hash FROM media_files");

    if (!query.exec()) {
        qWarning() << "[DatabaseManager] getAllFileHashes - Error fetching hashes:" << query.lastError().text();
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
int DatabaseManager::linkFiles(const QList<int> &fileIds) {
    if (fileIds.size() < 2) return -1;
    if (!db_m.transaction()) return -1;

    int finalSongId = -1;

    // Find existing song_id
    for (int id : fileIds) {
        QSqlQuery songIdQuery(database());
        songIdQuery.prepare("SELECT song_id FROM media_files WHERE id = ?");
        songIdQuery.addBindValue(id);
        if (songIdQuery.exec() && songIdQuery.next()) {
            int sid = songIdQuery.value(0).toInt();
            if (sid > 0) {
                finalSongId = sid;
                break;
            }
        }
    }

    // If no ID is found, create a new song.
    if (finalSongId <= 0) {
        // Get the name of the first file for the default title.
        QSqlQuery q(database());
        q.prepare("SELECT file_path FROM media_files WHERE id = ?");
        q.addBindValue(fileIds.first());
        QString title = "New song";
        if (q.exec() && q.next()) {
            title = QFileInfo(q.value(0).toString()).baseName();
        }

        QSqlQuery insertQuery(database());
        insertQuery.prepare("INSERT INTO songs (title) VALUES (?)");
        insertQuery.addBindValue(title);
        if (!insertQuery.exec()) {
            db_m.rollback();
            qCritical() << "[DatabaseManager] Error insertQuery: " << insertQuery.lastError().text();
            return -1;
        }
        finalSongId = insertQuery.lastInsertId().toInt();
    }

    // link them all
    for (int id : fileIds) {
        QSqlQuery updateQuery(database());
        updateQuery.prepare("UPDATE media_files SET song_id = ? WHERE id = ?");
        updateQuery.addBindValue(finalSongId);
        updateQuery.addBindValue(id);
        if (!updateQuery.exec()) {
            db_m.rollback();
            qCritical() << "[DatabaseManager] Error linkFiles: " << updateQuery.lastError().text();
            return -1;
        }
    }

    if (db_m.commit()) return finalSongId;
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
bool DatabaseManager::unlinkFile(int fileId) {
    QSqlQuery q(database());
    q.prepare("UPDATE media_files SET song_id = NULL WHERE id = ?");
    q.addBindValue(fileId);
    if (!q.exec()) {
        qCritical() << "[DatabaseManager] Error unlinkFile: " << q.lastError().text();
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
bool DatabaseManager::addFileRelation(int idA, int idB) {
    if (idA == idB) return false;
    QSqlQuery q(database());
    q.prepare("INSERT OR IGNORE INTO file_relations (file_id_a, file_id_b) VALUES (?, ?)");
    q.addBindValue(qMin(idA, idB)); // Sorting prevents duplicate pairs (1,2 and 2,1)
    q.addBindValue(qMax(idA, idB));
    if (!q.exec()) {
        qCritical() << "[DatabaseManager] Error addFileRelation: " << q.lastError().text();
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
bool DatabaseManager::removeRelation(int fileIdA, int fileIdB) {
    QSqlQuery query;
    // Delete the line, regardless of whether the IDs were stored as (A, B) or (B, A).
    query.prepare("DELETE FROM file_relations WHERE "
                  "(file_id_a = :idA AND file_id_b = :idB) OR "
                  "(file_id_a = :idB AND file_id_b = :idA)");

    query.bindValue(":idA", fileIdA);
    query.bindValue(":idB", fileIdB);

    if (!query.exec()) {
        qCritical() << "[DatabaseManager] Error deleting the relation: " << query.lastError().text();
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
 *       On successful deletion, a debug message is logged to qDebug().
 * 
 * @see QSqlQuery, QSqlError
 */
bool DatabaseManager::deleteFileRecord(int songId) {
    QSqlQuery q(database());
    q.prepare("DELETE FROM songs WHERE id = ?");
    q.addBindValue(songId);

    if (!q.exec()) {
        qCritical() << "[DatabaseManager] Delete record failed:" << q.lastError().text();
        return false;
    }
    qDebug() << "[DatabaseManager] Song and all associated data deleted via fileId:" << songId;
    return true;
}

// =============================================================================
// --- File queries
// =============================================================================

/**
 * @brief Retrieves all related resources (files) associated with a specific song.
 * 
 * Queries the database for all resources linked to the given song ID and returns
 * them as a list of RelatedFile objects. Each resource contains metadata including
 * its ID, type, title, and path or URL.
 * 
 * @param songId The unique identifier of the song whose resources should be retrieved.
 * 
 * @return A QList<RelatedFile> containing all resources associated with the song.
 *         Returns an empty list if no resources are found or if a database error occurs.
 * 
 * @note On database query failure, an error message is logged using qCritical().
 *       The internal database connection is used for safe query execution.
 * 
 * @see RelatedFile
 */
QList<DatabaseManager::RelatedFile> DatabaseManager::getResourcesForSong(int songId) {
    QList<RelatedFile> list;

    // Use the internal db_m - that's the safe way!
    QSqlQuery q(database());
    q.prepare("SELECT id, type, title, path_or_url FROM resources WHERE song_id = ?");
    q.addBindValue(songId);

    if (q.exec()) {
        while (q.next()) {
            RelatedFile res;
            res.id        = q.value(0).toInt();
            res.songId    = songId;
            res.type      = q.value(1).toString();
            res.title     = q.value(2).toString();
            res.pathOrUrl = q.value(3).toString();

            list.append(res);
        }
    } else {
        qCritical() << "[DatabaseManager] Error loading resources: " << q.lastError().text();
    }

    return list;
}

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
QList<DatabaseManager::RelatedFile> DatabaseManager::getFilesByRelation(int songId) {
    QList<RelatedFile> list;
    QSqlQuery q(database());

    // qDebug() << "search files for Song: " << songId;

    q.prepare("SELECT mf.id, mf.file_path, mf.file_type "
              "FROM media_files mf "
              "JOIN file_relations fr ON (mf.id = fr.file_id_b AND fr.file_id_a = ?) "
              "OR (mf.id = fr.file_id_a AND fr.file_id_b = ?)");
    q.addBindValue(songId);
    q.addBindValue(songId);

    if (q.exec()) {
        while (q.next()) {
            RelatedFile rf;
            rf.id = q.value("id").toInt();
            rf.fileName = q.value("file_path").toString();
            rf.type = q.value("file_type").toString();
            list.append(rf);
        }
    } else {
        qCritical() << "[DatabaseManager] getFilesBySongId Error: " << q.lastError().text();
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
QList<DatabaseManager::RelatedFile> DatabaseManager::getRelatedFiles(const int songId, const int excludeFileId) {
    QList<DatabaseManager::RelatedFile> list;
    if (songId <= 0) return list;

    QSqlQuery q(database());
    q.prepare("SELECT id, file_path FROM media_files WHERE song_id = ? AND id != ?");
    q.addBindValue(songId);
    q.addBindValue(excludeFileId);

    if (q.exec()) {
        while (q.next()) {
            RelatedFile res;
            res.id           = q.value(0).toInt();    // Correctly (id)
            res.relativePath = q.value(1).toString(); // Correctly (file_path)
            res.songId       = songId;

            // Use the filename as the title if there is no title in the database.
            QFileInfo fi(res.relativePath);
            res.fileName = fi.fileName();
            res.title    = res.fileName; //Fallback, since 'title' is not in the SELECT statement

            // We determine the type simply from the ending.
            res.type     = fi.suffix().toLower();

            list.append(res);
        }
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
bool DatabaseManager::addPracticeSession(int songId, int bpm, int totalReps, int cleanReps, const QString &note) {
    if (!db_m.transaction()) return false;

    QSqlQuery q(database());

    // Journal entry / History
    q.prepare("INSERT INTO practice_history (song_id, bpm_start, total_reps, successful_streaks) "
              "VALUES (:sid, :bpm, :total, :clean)");
    q.bindValue(":sid", songId);
    q.bindValue(":bpm", bpm);
    q.bindValue(":total", totalReps);
    q.bindValue(":clean", cleanReps); // Deine "7-er Serie" Logik

    if (!q.exec()) {
        db_m.rollback();
        qDebug() << "[DatabaseManager] addPracticeSession: rollback - return false: " << q.lastError().text();
        return false;
    }

    // Update song master data (Important for dashboard!)
    q.prepare("UPDATE songs SET last_practiced = CURRENT_TIMESTAMP, current_bpm = :bpm WHERE id = :sid");
    q.bindValue(":bpm", bpm);
    q.bindValue(":sid", songId);

    if (!q.exec()) {
        db_m.rollback();
        qDebug() << "[DatabaseManager] addPracticeSession: rollback - return false: " << q.lastError().text();
        return false;
    }

    // Save note (if available)
    if (!note.trimmed().isEmpty()) {
        q.prepare("INSERT INTO practice_journal (song_id, practiced_bpm, note_text) VALUES (?, ?, ?)");
        q.addBindValue(songId);
        q.addBindValue(bpm);
        q.addBindValue(note);
        if (!q.exec()) {
            qDebug() << "[DatabaseManager] addPracticeSession: save note: " << q.lastError().text();
            return false;
        }
    }

    return db_m.commit();
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
bool DatabaseManager::saveTableSessions(int songId, QDate date, const QList<PracticeSession> &sessions) {
    QSqlDatabase db = database(); // Get the active connection

    if (!db.transaction()) {
        qCritical() << "[DatabaseManager] Could not start transaction:" << db.lastError().text();
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
        qCritical() << "[DatabaseManager] Delete Error saveTableSessions:" << q.lastError().text();
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
            qCritical() << "[DatabaseManager] Insert Error:" << q.lastError().text();
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
QList<PracticeSession> DatabaseManager::getSessionsForDay(int songId, QDate date) {
    QList<PracticeSession> sessions;
    QSqlQuery q(database());

    q.prepare("SELECT practice_date, start_bar, end_bar, practiced_bpm, total_reps, successful_streaks "
              "FROM practice_journal "
              "WHERE song_id = ? AND DATE(practice_date) = ? "
              "AND start_bar IS NOT NULL"); // Only entries with table data
    q.addBindValue(songId);
    q.addBindValue(date.toString("yyyy-MM-dd"));

    if (q.exec()) {
        while (q.next()) {
            sessions.append({
                q.value(0).toDate(),
                q.value(1).toInt(),
                q.value(2).toInt(),
                q.value(3).toInt(),
                q.value(4).toInt(),
                q.value(5).toInt()
            });
        }
    } else {
        qCritical() << "[DatabaseManager] getSessionsForDay Error:" << q.lastError().text();
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
QList<PracticeSession> DatabaseManager::getLastSessions(int songId, int limit) {
    QList<PracticeSession> sessions;
    QSqlQuery query;

    // Sort by date and ID in descending order to get the very latest entries.
    query.prepare("SELECT practice_date, start_bar, end_bar, practiced_bpm, total_reps, successful_streaks "
                  "FROM practice_journal "
                  "WHERE song_id = :songId "
                  "ORDER BY practice_date DESC, id DESC "
                  "LIMIT :limit");

    // 2. BEIDE Parameter binden (Wichtig!)
    query.bindValue(":songId", songId);
    query.bindValue(":limit", limit);

    if (query.exec()) {
        while (query.next()) {
            PracticeSession s;
            s.date = query.value("practice_date").toDate();
            s.startBar = query.value("start_bar").toInt();
            s.endBar = query.value("end_bar").toInt();
            s.bpm = query.value("practiced_bpm").toInt();
            s.reps = query.value("total_reps").toInt();
            s.streaks = query.value("successful_streaks").toInt();

            sessions.prepend(s);
        }
    } else {
        qCritical() << "[DatabaseManager] getLastSessions SQL Error:" << query.lastError().text();
        qDebug() << "[DatabaseManager] getLastSessions Full Query:" << query.executedQuery();
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
bool DatabaseManager::addJournalNote(int songId, const QString &note) {
    QSqlQuery q(database());
    // Create a new historical entry
    q.prepare("INSERT INTO practice_journal (song_id, note_text, practice_date) "
              "VALUES (?, ?, CURRENT_TIMESTAMP)");
    q.addBindValue(songId);
    q.addBindValue(note);

    if (!q.exec()) {
        qCritical() << "[DatabaseManager] Journal Error:" << q.lastError().text();
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
bool DatabaseManager::saveOrUpdateNote(int songId, QDate date, const QString &note) {
    QSqlQuery q(database());
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
        q.prepare("INSERT INTO practice_journal (song_id, note_text, practice_date) VALUES (?, ?, ?)");
        q.addBindValue(songId);
        q.addBindValue(note);
        q.addBindValue(dateStr); // Use the date from the calendar here!
    }
    if (!q.exec()) {
        qCritical() << "[DatabaseManager] saveOrUpdateNote Error:" << q.lastError().text();
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
bool DatabaseManager::updateSongNotes(int songId, const QString &notes, QDate date) {
    QSqlQuery q(db_m);
    QString dateStr = date.toString("yyyy-MM-dd");
    qDebug() << "Date: " << date;

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
        q.prepare("INSERT INTO practice_journal (song_id, note_text, practice_date) VALUES (?, ?, ?)");
        q.addBindValue(songId);
        q.addBindValue(notes);
        q.addBindValue(dateStr);
    }

    if(!q.exec()) {
        qDebug() << "[DatabaseManager] Error updateSongNotes:" << q.lastError().text();
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
QString DatabaseManager::getNoteForDay(int songId, QDate date) {
    QSqlQuery q(database());
    // Convert date to ISO format yyyy-MM-dd
    QString dateStr = date.toString("yyyy-MM-dd");

    q.prepare("SELECT note_text FROM practice_journal "
              "WHERE song_id = ? AND DATE(practice_date) = ? "
              "LIMIT 1");
    q.addBindValue(songId);
    q.addBindValue(dateStr);

    if (q.exec() && q.next()) {
        return q.value(0).toString();
    }

    return ""; // If no entry exists for this day
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
QMap<int, QString> DatabaseManager::getPracticedSongsForDay(QDate date) {
    QMap<int, QString> songMap;
    QSqlQuery q(database());
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
        qDebug() << "[DatabaseManager] Error getPracticedSongsForDay: " << q.lastError().text();
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
QString DatabaseManager::getPracticeSummaryForDay(QDate date) {
    QSqlQuery q(database());
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
        qDebug() << "[DatabaseManager] Error getPracticeSummaryForDay: " << q.lastError().text();
    }
    return songs.isEmpty() ? "" : songs.join("\n");
}

// It returns all the days on which ANY song was practiced.
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
QList<QDate> DatabaseManager::getAllPracticeDates() {
    QList<QDate> dates;
    QSqlQuery q(database());
    // DISTINCT ensures that we only receive each date once.
    if (!q.exec("SELECT DISTINCT DATE(practice_date) FROM practice_journal")) {
        qDebug() << "[DatabaseManager] Error getAllPracticeDates: " << q.lastError().text();
    }
    while (q.next()) {
        dates << QDate::fromString(q.value(0).toString(), "yyyy-MM-dd");
    }
    return dates;
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
bool DatabaseManager::setSetting(const QString &key, const QVariant &value) {
    QSqlQuery q(database());
    q.prepare("INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?)");
    q.addBindValue(key);
    q.addBindValue(value.toString()); // QVariant automatically converts boolean values ​​to "true"/"false".
    if (!q.exec()) {
        qDebug() << "[DatabaseManager] Error setSetting: " << q.lastError().text();
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
QString DatabaseManager::getSetting(const QString &key, const QString &defaultValue) {
    QSqlQuery q(database());
    q.prepare("SELECT value FROM settings WHERE key = :key");
    q.bindValue(":key", key);
    if (q.exec() && q.next()) {
        return q.value(0).toString();
    } else {
        qDebug() << "[DatabaseManager] Error getSetting (String): " << q.lastError().text();
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
QVariant DatabaseManager::getSetting(const QString &key, const QVariant &defaultValue) {
    QSqlQuery q(database());
    q.prepare("SELECT value FROM settings WHERE key = ?");
    q.addBindValue(key);

    if (q.exec() && q.next()) {
        return q.value(0);
    } else {
        qDebug() << "[DatabaseManager] Error getSetting (QVariant): " << q.lastError().text();
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
int DatabaseManager::getOrCreateUserId(const QString &name, const QString &role) {
    QSqlQuery q(database());
    // The table must be 'users', not 'groups'.
    q.prepare("SELECT id FROM users WHERE name = :name");
    q.bindValue(":name", name);

    if (q.exec() && q.next()) {
        return q.value(0).toInt();
    } else {
        qDebug() << "[DatabaseManager] Error getOrCreateUserId: " << q.lastError().text();
    }

    q.prepare("INSERT INTO users (name, role) VALUES (:name, :role)");
    q.bindValue(":name", name);
    q.bindValue(":role", role);

    if (q.exec()) {
        return q.lastInsertId().toInt();
    } else {
        qDebug() << "[DatabaseManager] Error getOrCreateUserId: " << q.lastError().text();
    }

    return -1;
}
