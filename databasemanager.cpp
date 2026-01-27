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
    // Die statische lokale Variable wird beim ersten Aufruf initialisiert
    static DatabaseManager _instance;
    return _instance;
}

bool DatabaseManager::initDatabase(const QString &dbPath) {
    const QString connectionName = QSqlDatabase::defaultConnection;

    // 1. Wenn die Verbindung schon existiert
    if (QSqlDatabase::contains(connectionName)) {
        {
            // Wir nutzen einen Scope {}, damit das 'db' Objekt zerstört wird,
            // bevor wir removeDatabase aufrufen!
            QSqlDatabase db = QSqlDatabase::database(connectionName);
            if (db.isOpen() && db.databaseName() == dbPath) {
                db_m = db; // Bestehende Verbindung übernehmen
                db_m = QSqlDatabase::database(db_m.connectionName());
                return true;
            }
        }
        // Wenn Name falsch oder zu: Alte Verbindung sauber entfernen
        QSqlDatabase::removeDatabase(connectionName);
    }

    // 2. Jetzt frisch anlegen und der Member-Variable zuweisen
    db_m = QSqlDatabase::addDatabase("QSQLITE");
    db_m.setDatabaseName(dbPath);

    if (!db_m.open()) {
        qCritical() << "Verbindung fehlgeschlagen:" << db_m.lastError().text();
        return false;
    }

    // Fremdschlüssel für diese Verbindung aktivieren (wichtig für ON DELETE CASCADE)
    QSqlQuery q(db_m);
    q.exec("PRAGMA foreign_keys = ON");

    // Versionierung prüfen
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
                    return false; // Abbruch, wenn Version nicht gespeichert werden konnte
                }
            } else {
                return false;
            }
        }
    */

    return true;
}

void DatabaseManager::closeDatabase() {
    // 1. Namen merken
    QString connectionName = db_m.connectionName();

    // 2. Datenbank schließen
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

    // 1. BENUTZER (Lehrer / Studenten)
    if (!q.exec("CREATE TABLE IF NOT EXISTS users ("
                    "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                    "name TEXT UNIQUE, "
                    "role TEXT, " // 'teacher' oder 'student'
                    "created_at DATETIME DEFAULT CURRENT_TIMESTAMP)")) return false;

    // 2. SONGS (Die logische Einheit)
    if (!q.exec("CREATE TABLE IF NOT EXISTS songs ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                "user_id INTEGER, "
                "category_id INTEGER, " // NEU: Verknüpfung zur Kategorie
                "title TEXT, "
                "artist_id INTEGER, "
                "tuning_id INTEGER, "
                "base_bpm INTEGER, "
                "total_bars INTEGER, "
                "last_practiced DATETIME, "
                "current_bpm INTEGER DEFAULT 0, "
                "target_bpm INTEGER DEFAULT 0, " // Für die %-Berechnung
                "is_favorite INTEGER DEFAULT 0, "
                "updated_at DATETIME DEFAULT CURRENT_TIMESTAMP, "
                "FOREIGN KEY(category_id) REFERENCES categories(id) ON DELETE SET NULL, " // NEU
                "FOREIGN KEY(artist_id) REFERENCES artists(id), "
                "FOREIGN KEY(tuning_id) REFERENCES tunings(id))")) return false;

    // 3. MEDIA FILES (Physische Dateien auf der Platte)
    // Verknüpft eine Datei mit einem Song (N:1 - Ein Song kann mehrere Dateien haben)
    if (!q.exec("CREATE TABLE IF NOT EXISTS media_files ("
                    "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                    "song_id INTEGER, "
                    "file_path TEXT UNIQUE, "
                    "is_managed INTEGER DEFAULT 0, " // ANPASSUNG: 1 = Relativ zu SonarPath, 0 = Absolut
                    "file_type TEXT, "
                    "file_size INTEGER, "
                    "can_be_practiced BOOL, "
                    "FOREIGN KEY(song_id) REFERENCES songs(id) ON DELETE CASCADE)")) return false;

    // 4. ÜBUNGS-JOURNAL (Fortschrittstracking)
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

    // 5. PLAYLISTEN / SETLISTS
    if (!q.exec("CREATE TABLE IF NOT EXISTS playlists ("
                    "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                    "user_id INTEGER, "
                    "name TEXT, "
                    "description TEXT, "
                    "FOREIGN KEY(user_id) REFERENCES users(id) ON DELETE CASCADE)")) return false;

    // 6. PLAYLIST-SONGS (N:M Beziehung)
    if (!q.exec("CREATE TABLE IF NOT EXISTS playlist_items ("
                    "playlist_id INTEGER, "
                    "song_id INTEGER, "
                    "position INTEGER, " // Reihenfolge in der Playlist
                    "PRIMARY KEY (playlist_id, song_id), "
                    "FOREIGN KEY(playlist_id) REFERENCES playlists(id) ON DELETE CASCADE, "
                    "FOREIGN KEY(song_id) REFERENCES songs(id) ON DELETE CASCADE)")) return false;

    // 7. EINSTELLUNGEN (Key-Value-Store für Pfade, Flags etc.)
    if (!q.exec("CREATE TABLE IF NOT EXISTS settings ("
                    "key TEXT PRIMARY KEY, "
                    "value TEXT)")) return false;

    // 8. Artists/Bands
    // -- Tabelle für Künstler
    if (!q.exec("CREATE TABLE IF NOT EXISTS artists ("
                    "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                    "name TEXT UNIQUE NOT NULL)")) return false;

    // 9. Tunings
    if (!q.exec("CREATE TABLE IF NOT EXISTS tunings ("
                    "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                    "name TEXT UNIQUE NOT NULL)")) return false;

    // Tunings:
    // TODO: später mit allen Tunings befüllen oder besser, mit einen GP Parser
    // über alle GP Files gehen und dann befüllen.
    // Standard-Tunings initial befüllen, falls Tabelle leer ist
    if (!q.exec("INSERT OR IGNORE INTO tunings (name) VALUES "
               "('E-Standard'), ('Eb-Standard'), ('Drop D'), ('Drop C'), ('D-Standard')")) return false;

    // 10. CATEGORIES (Ordnerstruktur aus dem Import)
    if (!q.exec("CREATE TABLE IF NOT EXISTS categories ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                "parent_id INTEGER, "
                "name TEXT NOT NULL, "
                "UNIQUE(parent_id, name), " // Verhindert doppelte Ordner im selben Parent
                "FOREIGN KEY(parent_id) REFERENCES categories(id) ON DELETE CASCADE)")) return false;

    // URL
    if (!q.exec("CREATE TABLE IF NOT EXISTS resources ("
                "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                "song_id INTEGER, "
                "type TEXT, "          // 'gp', 'pdf', 'video', 'audio', 'url'
                "title TEXT, "         // "Lektion 1 Video"
                "path_or_url TEXT, "   // Pfad oder URL
                "is_managed INTEGER, " // 1 = lokal, 0 = extern
                "FOREIGN KEY(song_id) REFERENCES songs(id) ON DELETE CASCADE)")) return false;

    // relation
    if (!q.exec("CREATE TABLE IF NOT EXISTS file_relations (" // IF NOT EXISTS ist sicherer
                "file_id_a INTEGER, "
                "file_id_b INTEGER, "
                "relation_type TEXT, " // -- z.B. 'backingtrack', 'tutorial', 'tab'
                "PRIMARY KEY (file_id_a, file_id_b))")) return false;

    // N:N
    if (!q.exec("CREATE TABLE IF NOT EXISTS file_category_link ("
        "file_id INTEGER, "
        "category_id INTEGER, "
        "PRIMARY KEY(file_id, category_id), "
        "FOREIGN KEY(file_id) REFERENCES media_files(id) ON DELETE CASCADE, "
        "FOREIGN KEY(category_id) REFERENCES categories(id) ON DELETE CASCADE)")) return false;

    // Standardwerte setzen, falls sie noch nicht existieren
    if (!q.exec("INSERT OR IGNORE INTO settings (key, value) VALUES ('managed_path', '')")) return false;
    if (!q.exec("INSERT OR IGNORE INTO settings (key, value) VALUES ('is_managed', 'false')")) return false;
    if (!q.exec("INSERT OR IGNORE INTO settings (key, value) VALUES ('last_import_date', '')")) return false;
    if (!q.exec("CREATE INDEX IF NOT EXISTS idx_filepath ON media_files(file_path)")) return false;

    return true;
}

// Hilfsfunktion für den Wizard/Main-Check
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
    // PRAGMA user_version kann nicht mit Bind-Werten (?) genutzt werden.
    // Daher ist .arg() hier der korrekte Weg.
    if (!q.exec(QString("PRAGMA user_version = %1").arg(version))) {
        qDebug() << "Fehler beim Setzen der DB-Version:" << q.lastError().text();
        return false;
    }
    return true;
}

// =============================================================================
// --- File Management & Media (Files & Relations)
// =============================================================================

bool DatabaseManager::addFileToSong(qlonglong songId, const QString &filePath, bool isManaged, const QString &fileType, qint64 fileSize) {
    QSqlQuery q(database());

    // 1. Logik: Ist es eine Guitar Pro Datei?
    // Wir nehmen die Endung (z.B. "gp5") und hängen "*." davor,
    // damit es exakt zu deinen QStringLists in FileUtils passt.
    QString suffix = "*." + QFileInfo(filePath).suffix().toLower();
    bool isPracticeTarget = FileUtils::getGuitarProFormats().contains(suffix);

    q.prepare("INSERT INTO media_files (song_id, file_path, is_managed, file_type, file_size, can_be_practiced) "
              "VALUES (?, ?, ?, ?, ?, ?)");

    q.addBindValue(songId);
    q.addBindValue(filePath);
    q.addBindValue(isManaged ? 1 : 0);
    q.addBindValue(fileType);
    q.addBindValue(fileSize);
    q.addBindValue(isPracticeTarget ? 1 : 0); // Das neue Flag

    if (!q.exec()) {
        qDebug() << "Fehler beim Anhängen der Datei:" << q.lastError().text();
        return false;
    }
    return true;
}

bool DatabaseManager::updatePracticeFlag(int fileId, bool canPractice) {
    QSqlQuery q(database());
    q.prepare("UPDATE media_files SET can_be_practiced = ? WHERE id = ?");
    q.addBindValue(canPractice ? 1 : 0);
    q.addBindValue(fileId);
    return q.exec();
}

QString DatabaseManager::getManagedPath() {
    QVariant val = getSetting("managed_path", QString(""));
    return val.toString();
}

qlonglong DatabaseManager::createSong(const QString &title, const qlonglong &categoryId, const QString &artist, const QString &tuning) {
    qDebug() << "[DatabaseManager] createSong";

    qDebug() << "title: " << title;
    qDebug() << "categoryId: " << categoryId;
    qDebug() << "artist: " << artist;
    qDebug() << "tuning: " << tuning;

    int artistId = getOrCreateArtist(artist);
    int tuningId = getOrCreateTuning(tuning);

    qDebug() << "artistId: " << artistId;
    qDebug() << "tuningId: " << tuningId;
    qDebug() << "-----------------------------";

    QSqlQuery q(database());
    // q.prepare("INSERT INTO songs (title, artist_id, tuning_id) VALUES (?, ?, ?)");
    q.prepare("INSERT INTO songs (title, artist_id, tuning_id, category_id) VALUES (?, ?, ?, ?)");
    q.addBindValue(title);
    q.addBindValue(artistId);
    q.addBindValue(tuningId);

    // NULL-Handling für SQLite
    if (categoryId != -1) {
        q.addBindValue(categoryId);
    } else {
        q.addBindValue(QVariant(QMetaType::fromType<qlonglong>())); // Schreibt NULL
    }

    if (q.exec()) {
        return q.lastInsertId().toLongLong();
    } else {
        qDebug() << "Fehler beim Erstellen des Songs:" << q.lastError().text();
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

qlonglong DatabaseManager::getOrCreateCategoryRecursive(const QString &categoryPath) {
    if (categoryPath.isEmpty()) return -1; // Root-Ebene

    QStringList parts = categoryPath.split('/', Qt::SkipEmptyParts);
    qlonglong currentParentId = -1;

    for (const QString &name : std::as_const(parts)) {
        // 1. Prüfen, ob Kategorie auf dieser Ebene existiert
        QSqlQuery query(database());
        query.prepare("SELECT id FROM Categories WHERE name = :name AND parent_id = :pId");
        query.bindValue(":name", name);
        query.bindValue(":pId", currentParentId == -1 ? QVariant(QMetaType::fromType<qlonglong>()) : currentParentId);

        if (query.exec() && query.next()) {
            currentParentId = query.value(0).toLongLong();
        } else {
            // 2. Wenn nicht, neu anlegen
            QSqlQuery insert;
            insert.prepare("INSERT INTO Categories (name, parent_id) VALUES (:name, :pId)");
            insert.bindValue(":name", name);
            insert.bindValue(":pId", currentParentId == -1 ? QVariant(QMetaType::fromType<qlonglong>()) : currentParentId);

            if (insert.exec()) {
                currentParentId = insert.lastInsertId().toLongLong();
            } else {
                qDebug() << "Fehler beim Erstellen der Kategorie:" << name;
                return -1;
            }
        }
    }
    return currentParentId;
}

// =============================================================================
// --- Linking
// =============================================================================

int DatabaseManager::linkFiles(const QList<int> &fileIds) {
    if (fileIds.size() < 2) return -1;
    if (!db_m.transaction()) return -1;

    int finalSongId = -1;

    // SCHRITT 1: Existierende song_id finden
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

    // SCHRITT 2: Falls keine ID gefunden, neuen Song anlegen
    if (finalSongId <= 0) {
        // Wir holen uns den Namen der ersten Datei für den Default-Titel
        QSqlQuery nameQuery(database());
        nameQuery.prepare("SELECT file_path FROM media_files WHERE id = ?");
        nameQuery.addBindValue(fileIds.first());
        QString title = "Neuer Song";
        if (nameQuery.exec() && nameQuery.next()) {
            title = QFileInfo(nameQuery.value(0).toString()).baseName();
        }

        QSqlQuery insertQuery(database());
        insertQuery.prepare("INSERT INTO songs (title) VALUES (?)");
        insertQuery.addBindValue(title);
        if (!insertQuery.exec()) {
            db_m.rollback();
            return -1;
        }
        finalSongId = insertQuery.lastInsertId().toInt();
    }

    // SCHRITT 3: Alle verknüpfen
    for (int id : fileIds) {
        QSqlQuery updateQuery(database());
        updateQuery.prepare("UPDATE media_files SET song_id = ? WHERE id = ?");
        updateQuery.addBindValue(finalSongId);
        updateQuery.addBindValue(id);
        if (!updateQuery.exec()) {
            db_m.rollback();
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
    return q.exec();
}

bool DatabaseManager::addFileRelation(int idA, int idB) {
    if (idA == idB) return false;
    QSqlQuery q(database());
    q.prepare("INSERT OR IGNORE INTO file_relations (file_id_a, file_id_b) VALUES (?, ?)");
    q.addBindValue(qMin(idA, idB)); // Sortierung verhindert doppelte Paare (1,2 und 2,1)
    q.addBindValue(qMax(idA, idB));
    return q.exec();
}

bool DatabaseManager::removeRelation(int fileIdA, int fileIdB) {
    QSqlQuery query;
    // Wir löschen die Zeile, egal ob die IDs als (A, B) oder (B, A) gespeichert wurden
    query.prepare("DELETE FROM file_relations WHERE "
                  "(file_id_a = :idA AND file_id_b = :idB) OR "
                  "(file_id_a = :idB AND file_id_b = :idA)");

    query.bindValue(":idA", fileIdA);
    query.bindValue(":idB", fileIdB);

    if (!query.exec()) {
        qWarning() << "[DatabaseManager] Fehler beim Löschen der Relation:"
                   << query.lastError().text();
        return false;
    }

    return true;
}

// =============================================================================
// --- File queries
// =============================================================================

QList<DatabaseManager::RelatedFile> DatabaseManager::getResourcesForSong(int songId) {
    QList<RelatedFile> list;

    // Wir nutzen die interne db_m - das ist der sichere Weg!
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
        qDebug() << "Error loading resources:" << q.lastError().text();
    }

    return list;
}

QList<DatabaseManager::RelatedFile> DatabaseManager::getFilesByRelation(int songId) {
    QList<RelatedFile> list;
    QSqlQuery q(database());

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
        qDebug() << "getFilesBySongId Error:" << q.lastError().text();
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
            res.id           = q.value(0).toInt();    // Korrekt (id)
            res.relativePath = q.value(1).toString(); // Korrekt (file_path)
            res.songId       = songId;

            // FEHLER-QUELLE: Du hast kein 'title' oder 'type' im SELECT!
            // Wir nehmen den Dateinamen als Titel, wenn kein Titel in der DB ist.
            QFileInfo fi(res.relativePath);
            res.fileName = fi.fileName();
            res.title    = res.fileName; // Fallback, da 'title' nicht im SELECT steht

            // Den Typ bestimmen wir einfach aus der Endung
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

    // 1. Eintrag ins Journal / History
    q.prepare("INSERT INTO practice_history (song_id, bpm_start, total_reps, successful_streaks) "
              "VALUES (:sid, :bpm, :total, :clean)");
    q.bindValue(":sid", songId);
    q.bindValue(":bpm", bpm);
    q.bindValue(":total", totalReps);
    q.bindValue(":clean", cleanReps); // Deine "7-er Serie" Logik

    if (!q.exec()) {
        db_m.rollback();
        return false;
    }

    // 2. Song-Stammdaten aktualisieren (Wichtig für Dashboard!)
    q.prepare("UPDATE songs SET last_practiced = CURRENT_TIMESTAMP, current_bpm = :bpm WHERE id = :sid");
    q.bindValue(":bpm", bpm);
    q.bindValue(":sid", songId);

    if (!q.exec()) {
        db_m.rollback();
        return false;
    }

    // 3. Notiz speichern (falls vorhanden)
    if (!note.trimmed().isEmpty()) {
        q.prepare("INSERT INTO practice_journal (song_id, practiced_bpm, note_text) VALUES (?, ?, ?)");
        q.addBindValue(songId);
        q.addBindValue(bpm);
        q.addBindValue(note);
        q.exec();
    }

    return db_m.commit();
}

/* Use Transaction here to save either everything or nothing.
 * Delete the day's old sessions and write the new ones:
 * */
bool DatabaseManager::saveTableSessions(int songId, QDate date, const QList<PracticeSession> &sessions) {
    QSqlDatabase db = QSqlDatabase::database();

    if (!db.isOpen()) {
        qDebug() << "Database was closed! Attempts to open...";
        if (!db.open()) {
            qDebug() << "Error opening:" << db.lastError().text();
            return false;
        }
    }

    if (!db.transaction()) {
        qDebug() << "Transaction Error:" << db.lastError().text();
        return false;
    }

    QSqlQuery q(database());
    QString dateStr = date.toString("yyyy-MM-dd");

    // Delete existing exercise values ​​for this day/song
    q.prepare("DELETE FROM practice_journal WHERE song_id = ? AND DATE(practice_date) = ? AND start_bar IS NOT NULL");
    q.addBindValue(songId);
    q.addBindValue(dateStr);
    q.exec();

    // Insert new lines
    q.prepare("INSERT INTO practice_journal (song_id, practice_date, start_bar, end_bar, practiced_bpm, total_reps, successful_streaks) "
              "VALUES (?, ?, ?, ?, ?, ?, ?)");

    for (const auto &s : sessions) {
        q.addBindValue(songId);
        q.addBindValue(dateStr);
        q.addBindValue(s.startBar);
        q.addBindValue(s.endBar);
        q.addBindValue(s.bpm);
        q.addBindValue(s.reps);
        q.addBindValue(s.streaks);
        q.exec();
    }
    if (!db.commit()) {
        db.rollback();
        return false;
    }
    return true;
}

QList<PracticeSession> DatabaseManager::getSessionsForDay(int songId, QDate date) {
    QList<PracticeSession> sessions;
    QSqlQuery q(database());

    q.prepare("SELECT start_bar, end_bar, practiced_bpm, total_reps, successful_streaks "
              "FROM practice_journal "
              "WHERE song_id = ? AND DATE(practice_date) = ? "
              "AND start_bar IS NOT NULL"); // Nur Einträge mit Tabellendaten
    q.addBindValue(songId);
    q.addBindValue(date.toString("yyyy-MM-dd"));

    if (q.exec()) {
        while (q.next()) {
            sessions.append({
                q.value(0).toInt(),
                q.value(1).toInt(),
                q.value(2).toInt(),
                q.value(3).toInt(),
                q.value(4).toInt()
            });
        }
    }
    return sessions;
}

// =============================================================================
// --- Journal & Notice
// =============================================================================

bool DatabaseManager::addJournalNote(int songId, const QString &note) {
    QSqlQuery q(database());
    // Wir erstellen einen neuen historischen Eintrag
    q.prepare("INSERT INTO practice_journal (song_id, note_text, practice_date) "
              "VALUES (?, ?, CURRENT_TIMESTAMP)");
    q.addBindValue(songId);
    q.addBindValue(note);

    if (!q.exec()) {
        qDebug() << "Journal Error:" << q.lastError().text();
        return false;
    }
    return true;
}

bool DatabaseManager::saveOrUpdateNote(int songId, QDate date, const QString &note) {
    QSqlQuery q(database());
    QString dateStr = date.toString("yyyy-MM-dd");

    // 1. Prüfen, ob für diesen Tag bereits ein Eintrag existiert
    q.prepare("SELECT id FROM practice_journal WHERE song_id = ? AND DATE(practice_date) = ?");
    q.addBindValue(songId);
    q.addBindValue(dateStr);

    if (q.exec() && q.next()) {
        // 2. Vorhandenen Eintrag aktualisieren
        int entryId = q.value(0).toInt();
        q.prepare("UPDATE practice_journal SET note_text = ? WHERE id = ?");
        q.addBindValue(note);
        q.addBindValue(entryId);
    } else {
        // 3. Neuen Eintrag anlegen
        q.prepare("INSERT INTO practice_journal (song_id, note_text, practice_date) VALUES (?, ?, ?)");
        q.addBindValue(songId);
        q.addBindValue(note);
        q.addBindValue(dateStr); // Hier das Datum vom Kalender nutzen!
    }
    return q.exec();
}

// Saves the general note for the song
bool DatabaseManager::updateSongNotes(int songId, const QString &notes, QDate date) {
    QSqlQuery q(db_m);
    QString dateStr = date.toString("yyyy-MM-dd");

    // 1. Prüfen, ob für diesen Song an diesem Tag bereits ein Eintrag existiert
    q.prepare("SELECT id FROM practice_journal WHERE song_id = ? AND DATE(practice_date) = ?");
    q.addBindValue(songId);
    q.addBindValue(dateStr);

    if (q.exec() && q.next()) {
        // 2. Vorhandenen Eintrag aktualisieren
        int entryId = q.value(0).toInt();
        q.prepare("UPDATE practice_journal SET note_text = ? WHERE id = ?");
        q.addBindValue(notes);
        q.addBindValue(entryId);
    } else {
        // 3. Neuen Eintrag für diesen Tag anlegen
        q.prepare("INSERT INTO practice_journal (song_id, note_text, practice_date) VALUES (?, ?, ?)");
        q.addBindValue(songId);
        q.addBindValue(notes);
        q.addBindValue(dateStr);
    }

    bool success = q.exec();
    if (!success) {
        qDebug() << "[DatabaseManager] SQL Error:" << q.lastError().text();
    }
    return success;
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

    return ""; // Falls für diesen Tag noch kein Eintrag existiert
}

// =============================================================================
// --- Statistics & Dashboard
// =============================================================================

QMap<int, QString> DatabaseManager::getPracticedSongsForDay(QDate date) {
    QMap<int, QString> songMap;
    QSqlQuery q(database());
    // Wir holen Song-ID und Titel für alle Einträge dieses Tages
    q.prepare("SELECT s.id, s.title FROM songs s "
              "JOIN practice_journal pj ON s.id = pj.song_id "
              "WHERE DATE(pj.practice_date) = ? "
              "GROUP BY s.id");
    q.addBindValue(date.toString("yyyy-MM-dd"));

    if (q.exec()) {
        while (q.next()) {
            songMap.insert(q.value(0).toInt(), q.value(1).toString());
        }
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
    }
    return songs.isEmpty() ? "" : songs.join("\n");
}

// It returns all the days on which ANY song was practiced.
QList<QDate> DatabaseManager::getAllPracticeDates() {
    QList<QDate> dates;
    QSqlQuery q(database());
    // DISTINCT sorgt dafür, dass wir jedes Datum nur einmal erhalten
    q.exec("SELECT DISTINCT DATE(practice_date) FROM practice_journal");
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
    q.addBindValue(value.toString()); // QVariant wandelt bool automatisch in "true"/"false" um
    return q.exec();
}

QString DatabaseManager::getSetting(const QString &key, const QString &defaultValue) {
    QSqlQuery q(database());
    q.prepare("SELECT value FROM settings WHERE key = :key");
    q.bindValue(":key", key);
    if (q.exec() && q.next()) {
        return q.value(0).toString();
    }
    return defaultValue;
}

QVariant DatabaseManager::getSetting(const QString &key, const QVariant &defaultValue) {
    QSqlQuery q(database());
    q.prepare("SELECT value FROM settings WHERE key = ?");
    q.addBindValue(key);

    if (q.exec() && q.next()) {
        return q.value(0);
    }
    return defaultValue;
}


// Privat
int DatabaseManager::getOrCreateUserId(const QString &name, const QString &role) {
    QSqlQuery q(database());
    // Tabelle muss 'users' sein, nicht 'groups'
    q.prepare("SELECT id FROM users WHERE name = :name");
    q.bindValue(":name", name);

    if (q.exec() && q.next()) {
        return q.value(0).toInt();
    }

    q.prepare("INSERT INTO users (name, role) VALUES (:name, :role)");
    q.bindValue(":name", name);
    q.bindValue(":role", role);

    if (q.exec()) {
        return q.lastInsertId().toInt();
    }

    return -1;
}
