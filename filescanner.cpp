#include "filescanner.h"
#include "fnv1a.h"

bool FileScanner::isScanning() {
    return isScanning_m;
}

void FileScanner::doScan(const QStringList &paths, const QStringList &filters) {
    isScanning_m = true;
    ReviewStats stats;
    QList<ScanBatch> localBatch; // Saves EVERYTHING for later duplicate correction
    QMap<QString, int> countMap; // Counts hashes across all files
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

            ScanBatch data;
            data.info = info;
            data.hash = hash;
            data.status = alreadyInDb ? StatusAlreadyInDatabase : (isDefect ? StatusDefect : StatusReady);

            // Statistics for the "Live" display (without duplicates)
            stats.addFile(info.size(), false, isDefect, alreadyInDb);

            countMap[hash]++; // Important for Phase 2

            localBatch.append(data);
            if (localBatch.size() % 20 == 0) {
                emit progressStats(stats);
            }
        }
    }

    // 2.PHASE: Finding the "truth" (correcting duplicates)
    QList<ScanBatch> duplicateUpdates;
    stats.duplicates = 0;

    for (ScanBatch &file : localBatch) {
        if (countMap.value(file.hash) > 1 && file.status == StatusReady) {
            file.status = StatusDuplicate;
            stats.duplicates++;

            if (!groupMap.contains(file.hash)) {
                groupMap.insert(file.hash, nextGroupId++);
            }
            file.groupId = groupMap.value(file.hash);

            duplicateUpdates.append(file);
        }
    }

    emit batchesFound(localBatch);
    emit finished(stats);
    emit finishWithAllBatches(localBatch, stats);
    isScanning_m = false;
}
