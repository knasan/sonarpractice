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
    int groupId{0};
    int status{0};
    bool layoutChange{false};
};

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
    RoleFileInfo = Qt::UserRole + 1, // The complete QFileInfo object
    RoleFileSizeRaw,                 // qint64 (raw value for calculations)
    RoleFileHash,                    // QString (The calculated hash)
    RoleItemType,                    // int (0 = file, 1 = directroy)
    RoleFileStatus,                  // int (Enum: OK, defect, duplicate)
    RoleFilePath,                    // saves the path
    RoleDuplicateId,                 // Mark groups in the model.
    RoleIsFolder                     // Stores information about whether this is a folder.
};

// Additional enum for status (for RoleFileStatus)
enum FileStatus {
    StatusReady = 0, // File is OK (green)
    StatusDefect,    // 0-byte file (Red/Disabled)
    StatusDuplicate, // True doublet (orange)
    StatusManaged,   // File is managed
    StatusFiles,
    StatusReject,
    StatusAlreadyInDatabase
};

// Database
struct SongImportData {
    QString title;
    QString artist = "Unknown";
    QString tuning = "E-Standard";
    QString relativePath;
    QString fileSuffix;
    qint64 fileSize;
    bool isManaged;
};

// Pages like songeditdialog
/* SongDialog hasn't been used for a long time and isn't included in this version.
 * It needs to be completely rewritten, and it also needs to be reconsidered whether 
 * a SongDialog Editor is even necessary.
*/
struct SongData {
    int id{-1};
    int artistId{-1};
    QString title;
    QString artistName;
    QString tuning;
    int bpm{120};
    int bars{0};
};

#endif // SONARSTRUCTS_H
