#ifndef SONARSTRUCTS_H
#define SONARSTRUCTS_H

#include <QFileInfo>
#include <QStandardItem>
#include <QString>

// Wizard
struct DuplicateGroup {
    QString hash;
    QStringList filePaths;
    qint64 size;
};

struct ScanBatch {
    QFileInfo info;
    QString hash;
    int groupId;
    int status;
    bool layoutChange;
};

// TODO: mal überprüfen was hiervon noch benötigt wird. Denke vieles waren nur tests und gibt es nicht mehr!
enum Column {
    ColName = 0,
    ColSize,
    ColStatus,
    ColGroup,
    ColItemType,
    ColFileType,
    ColFolderType,
    ColRootType
};

enum ItemRole {
    RoleFileInfo = Qt::UserRole + 1,    // Das komplette QFileInfo Objekt
    RoleFileSizeRaw,                    // qint64 (Rohwert für Berechnungen)
    RoleFileHash,                       // QString (Der berechnete Hash)
    RoleItemType,                       // int (0 = Datei, 1 = Ordner)
    RoleFileStatus,                     // int (Enum: OK, Defekt, Dublette)
    RoleFilePath,                       // speichert den den pfad
    RoleDuplicateId,                    // Gruppen im Model markieren.
    RoleIsFolder
};

// Zusätzliches Enum für den Status (für RoleFileStatus)
enum FileStatus {
    StatusReady = 0,    // Datei ist in Ordnung (Grün)
    StatusDefect,       // 0-Byte Datei (Rot/Deaktiviert)
    StatusDuplicate,    // Echte Dublette (Orange)
    StatusManaged,      // File is managed
    StatusFiles,
    StatusReject,
};

// Database
struct SongImportData {
    QString title;
    QString artist = "Unbekannt";
    QString tuning = "E-Standard";
    QString relativePath;
    QString fileSuffix;
    qint64 fileSize;
    bool isManaged;
};

// Pages wie songeditdialog
struct SongData {
    int id = -1;
    int artistId = -1;
    QString title;
    QString artistName;
    QString tuning;
    int bpm = 120;
    int bars = 0;
};

#endif // SONARSTRUCTS_H
