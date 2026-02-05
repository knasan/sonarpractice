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
    q.exec("PRAGMA foreign_keys = ON");

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

// Helper function for the Wizard/Main Check
bool DatabaseManager::hasData() {
    QSqlQuery q(database());
    q.prepare("SELECT id FROM songs LIMIT 1");
    return q.exec() && q.next();
}

// for Database Upgrades
int DatabaseManager::getDatabaseVersion() {
    QSqlQuery q(database());
    q.prepare("PRAGMA user_version");
    if (q.next()) {
        return q.value(0).toInt();
    }
    return 0;
}

// --- Schema Versioning ---
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

bool DatabaseManager::addFileToSong(qlonglong songId, const QString &filePath, bool isManaged, const QString &fileType, qint64 fileSize, QString fileHash) {
    QSqlQuery q(database());

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

QString DatabaseManager::getManagedPath() {
    QVariant val = getSetting("managed_path", QString(""));
    return val.toString();
}

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

bool DatabaseManager::deleteFileRecord(int fileId) {
    QSqlQuery q(database());
    q.prepare("DELETE FROM media_files WHERE id = ?");
    q.addBindValue(fileId);

    if (!q.exec()) {
        qCritical() << "[DatabaseManager] Delete record failed:" << q.lastError().text();
        return false;
    }
    return true;
}

// =============================================================================
// --- File queries
// =============================================================================

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

/* Use Transaction here to save either everything or nothing.
 * Delete the day's old sessions and write the new ones:
 * */
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
