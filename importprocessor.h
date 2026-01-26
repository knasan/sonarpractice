#ifndef IMPORTPROCESSOR_H
#define IMPORTPROCESSOR_H

#include "databasemanager.h"

#include <QCoreApplication>
#include <QDir>
#include <qvariant.h>

struct ImportTask {
    QString sourcePath;     // Full source path (C:/...)
    QString relativePath;   // Target path relative to the base directory (Ordner/Datei.gp5)
    QString itemName;       // Display name/File name
    qint64 fileSize;
    QString fileSuffix;
    QString categoryPath; // So that the processor knows where to go (z.B. "Übungen/Technik")
    QString fileHash;     // For your duplicate check
};

class ImportProcessor : public QObject {
    Q_OBJECT
public:
    explicit ImportProcessor(QObject *parent = nullptr) : QObject(parent) {}

    [[nodiscard]] bool executeImport(const QList<ImportTask> &tasks, const QString &basePath, bool isManaged) {
        qDebug() << "executeImport basePath: " << basePath;
        qDebug() << "executeImport isManaged: " << isManaged;
        qDebug() << "executeImport tasks: " << tasks.size();

        if (!DatabaseManager::instance().beginTransaction()) {
            qDebug() << "executeImport: instance false";
            return false;
        }

        int count = 0;

        QMap<QString, qlonglong> createdSongs;

        for (const auto &task : tasks) {
            try {
                qDebug() << "executeImport task: categoryPath: " << task.categoryPath;
                qDebug() << "executeImport task: relativePath: " << task.relativePath;
                qDebug() << "executeImport task: sourcePath: " << task.sourcePath;
                qDebug() << "executeImport task: fileHash: " << task.fileHash;
                qDebug() << "executeImport task: fileSize: " << task.fileSize;
                qDebug() << "executeImport task: itemName: " << task.itemName;
                qDebug() << "executeImport task: fileSuffix: " << task.fileSuffix;
                qDebug() << "-------------------------------------------------------------";

                // --- File system operations ---
                QString finalDest;
                if (isManaged) {
                    finalDest = QDir::cleanPath(QDir(basePath).absoluteFilePath(task.relativePath));
                    QDir().mkpath(QFileInfo(finalDest).absolutePath());
                    if (!QFile::exists(finalDest)) {
                        if (!QFile::copy(task.sourcePath, finalDest)) [[unlikely]] {
                            throw std::runtime_error("Kopierfehler: " + task.sourcePath.toStdString());
                        }
                    }
                } else {
                    finalDest = task.sourcePath;
                }

                // --- Database logic (DRY & grouping) ---
                QString baseName = QFileInfo(task.sourcePath).baseName();
                qlonglong songId;

                if (!createdSongs.contains(baseName)) {
                    // First, resolve (or create) the category ID from the path
                    // task.categoryPath contains, for example, "Rock/Classic"
                    qlonglong catId = DatabaseManager::instance().getOrCreateCategoryRecursive(task.categoryPath);

                    // Create a new song - ENTER THE CAT ID HERE
                    songId = DatabaseManager::instance().createSong(baseName, catId);

                    if (songId == -1) throw std::runtime_error("DB Song Fehler");
                    createdSongs.insert(baseName, songId);
                } else {
                    songId = createdSongs[baseName];
                }

                // Attach the file to the (new or existing) song
                bool ok = DatabaseManager::instance().addFileToSong(
                    songId,
                    isManaged ? task.relativePath : finalDest,
                    isManaged,
                    task.fileSuffix,
                    task.fileSize
                    );

                if (!ok) [[unlikely]] throw std::runtime_error("DB Media Fehler bei: " + task.itemName.toStdString());

                // --- Progress ---
                if (count % 20 == 0) {
                    QCoreApplication::processEvents();
                }

            } catch (const std::exception &e) {
                qDebug() << "Import fehlgeschlagen:" << e.what();
                DatabaseManager::instance().rollback();
                return false;
            }
        }

        // --- Settings ---
        auto &db = DatabaseManager::instance();

        if (isManaged) {
            if (!db.setSetting("managed_path", QVariant(basePath))) {
                qCritical() << "CRITICAL: managed_path konnte nicht in DB gespeichert werden:" << basePath;
            }
        }

        if (!db.setSetting("is_managed", QVariant(isManaged))) {
            qCritical() << "CRITICAL: isManaged konnte nicht in DB gespeichert werden:" << isManaged;
        }

        if (!db.setSetting("last_import_date", QVariant(QDateTime::currentDateTime().toString(Qt::ISODate)))) {
            qCritical() << "CRITICAL: last_import_date konnte nicht in DB gespeichert werden:" << QDateTime::currentDateTime().toString(Qt::ISODate);
        }

        return DatabaseManager::instance().commit();
    }

signals:
    void progressUpdated(int value);
    void error(const QString &message);
};

#endif // IMPORTPROCESSOR_H
