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

    [[nodiscard]] inline QString formatBytes(qint64 bytes) {
        return QLocale().formattedDataSize(bytes, 2);
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
        return {"*.mp4", "*.mkv", "*.mov", "*.wmv", "*.webm", "*.flv", "*.m4v", "*.avchd", "*.mxf", "*.vob", "*.ifo", "*.bup"};
    }

    [[nodiscard]] static QStringList getDocFormats() {
        return {"*.pdf", "*.txt", "*.md", "*.jpg", "*.jpeg", "*.png"};
    }
}

#endif // FILEUTILS_H
