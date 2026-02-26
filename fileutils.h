#ifndef FILEUTILS_H
#define FILEUTILS_H

#include <QDir>
#include <QFile>
#include <QStringList>
#include <QUrl>
#include <QFileDialog>
#include <QDirIterator>
#include <QDesktopServices>
#include <QStorageInfo>
#include <QFileInfo>
#include <QLocale>

class QWidget;

namespace FileUtils {

    struct FolderCleanupReport {
        bool isNowEmpty{false};
        QStringList filesLeft;
        int zeroByteFiles;
    };

    inline QString getCleanDirectory(QWidget *parent, const QString &title) {
        QString dir = QFileDialog::getExistingDirectory(parent, title, QDir::homePath());
        if (dir.isEmpty()) return QString();
        return QDir::toNativeSeparators(QDir::cleanPath(dir));
    }

    inline void scanDirectories(const QStringList &paths, const QStringList &filters, std::function<void(const QFileInfo&)> callback) {
        for (const QString &path : paths) {
            QDirIterator it(path, filters, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                it.next();
                callback(it.fileInfo());
            }
        }
    }

    [[nodiscard]] inline QString formatBytes(qint64 bytes) {
        return QLocale().formattedDataSize(bytes, 2);
    }

    [[nodiscard]] inline FolderCleanupReport analyzeAndCleanup(const QString &path) {
        QDir dir(path);
        FolderCleanupReport report;
        // Only genuine files count, . and .. are ignored.
        QStringList allFiles = dir.entryList(QDir::Files | QDir::NoDotAndDotDot);

        report.filesLeft = allFiles;
        report.zeroByteFiles = 0;

        for(const QString &f : std::as_const(allFiles)) {
            if(QFileInfo(dir.filePath(f)).size() == 0) {
                report.zeroByteFiles++;
            }
        }

        if(allFiles.isEmpty()) {
            // rmdir only deletes if the folder is empty (not even subfolders).
            report.isNowEmpty = dir.rmdir(path);
        } else {
            report.isNowEmpty = false;
        }
        return report;
    }

    // Helpful for your point 3 (delete original after Copy & DB Save)
    [[nodiscard]] inline bool safeDeleteSource(const QString &sourcePath) {
        if (QFile::exists(sourcePath)) {
            return QFile::remove(sourcePath);
        }
        return false;
    }


    [[nodiscard]] static bool openLocalFile(const QString &fullPath) {
        if (fullPath.isEmpty() || !QFile::exists(fullPath)) {
            return false;
        }
        return QDesktopServices::openUrl(QUrl::fromLocalFile(fullPath));
    }

    [[nodiscard]] static QStringList getGuitarProFormats() {
        return {"*.gp3", "*.gp4", "*.gp5", "*.gpx", "*.gp", "*.gtp"};
    }

    [[nodiscard]] static QStringList getAudioFormats() {
        return {"*.wav", "*.mp3", "*.m4a", "*.aac", "*.ogg", "*.wma", "*.opus", "*.flac", "*.aiff", "*.mid"};
    }

    [[nodiscard]] static QStringList getVideoFormats() {
        return {"*.mp4", "*.mkv", "*.mov", "*.wmv", "*.webm", "*.flv", "*.m4v", "*.avchd", "*.mxf"};
    }

    [[nodiscard]] static QStringList getDocFormats() {
        return {"*.pdf", "*.txt", "*.md"};
    }

    [[nodiscard]] inline bool hasEnoughSpace(const QStringList &sourceFiles, const QString &targetPath) {
        qint64 totalRequiredSize = 0;
        for (const QString &file : sourceFiles) {
            totalRequiredSize += QFileInfo(file).size();
        }

        QStorageInfo storage(targetPath);
        if (storage.isValid() && storage.isReady()) {
            // 50 MB safety buffer
            qint64 buffer = 50 * 1024 * 1024;
            return storage.bytesAvailable() > (totalRequiredSize + buffer);
        }
        return false;
    }
}

#endif // FILEUTILS_H
