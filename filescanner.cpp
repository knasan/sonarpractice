#include "filescanner.h"
#include "fnv1a.h"

void FileScanner::doScan(const QStringList &paths, const QStringList &filters) {
    isScanning_m = true;
    ReviewStats stats;
    QList<ScanBatch> allScannedFiles;
    QMap<QString, int> countMap;

    QMap<QString, int> groupMap;
    int nextGroupId = 1;

    // 1. PHASE: Find and hash everything
    for (const QString &path : paths) {
        QDirIterator it(QDir(path).absolutePath(), QDir::Files | QDir::NoSymLinks | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            if (abort_m) { emit finished(stats); return; }
            it.next();
            QFileInfo info = it.fileInfo();

            // Filter Check
            bool match = false;
            for (const QString &f : filters) { if (QDir::match(f, info.fileName())) { match = true; break; } }
            if (!match) continue;

            bool isDefect = (info.size() == 0);
            QString hash = isDefect ? "0" : FNV1a::calculate(info.absoluteFilePath());
            bool alreadyInDb = existingHashes_m.contains(hash);

            stats.addFile(info.size(), false, isDefect, alreadyInDb);

            ScanBatch data;
            data.info = info;
            data.hash = hash;

            if (!groupMap.contains(data.hash)) {
                groupMap.insert(data.hash, nextGroupId++);
            }
            data.groupId = groupMap.value(data.hash);
            countMap[data.hash]++; // IMPORTANT: This counts

            data.status = alreadyInDb ? StatusAlreadyInDatabase : (isDefect ? StatusDefect : StatusReady);

            allScannedFiles.append(data);
            countMap[hash]++;
        }
    }

    // 2. PHASE: Mark duplicates
    int finalDupCount = 0;
    for (ScanBatch &batch : allScannedFiles) {
        if (countMap.value(batch.hash) > 1 && batch.status == StatusReady) {
            batch.status = StatusDuplicate;
            finalDupCount++;
        }
    }
    stats.duplicates = finalDupCount;

    // 3. PHASE: Send data to the UI in batches (for smooth loading)
    const int batchSize = 50;
    for (int i = 0; i < allScannedFiles.size(); i += batchSize) {
        emit batchesFound(allScannedFiles.mid(i, batchSize));
    }

    emit progressStats(stats);
    emit finished(stats);
    isScanning_m = false;
}

bool FileScanner::isScanning() {
    return isScanning_m;
}
