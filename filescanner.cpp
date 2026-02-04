#include "filescanner.h"
#include "fnv1a.h"

void FileScanner::doScan(const QStringList &paths, const QStringList &filters) {
    QElapsedTimer timer;
    timer.start();

    ReviewStats stats;
    QList<ScanBatch> allBatches;
    QMap<QString, int> groupMap;
    QMap<QString, int> countMap;
    int nextGroupId = 1;

    QList<ScanBatch> pendingBatches;
    int batchCounter = 0;

    for (const QString &path : paths) {
        QDirIterator it(path, filters, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);

        while (it.hasNext()) {
            if (abort_m) {
                emit finished(stats);
                return;
            }
            it.next();
            QFileInfo info = it.fileInfo();

            if (info.isDir()) {
                qDebug() << "[FileScanner] doScan - Skipping directory:" << info.absoluteFilePath();
                continue;
            }

            ScanBatch data;
            data.info = info;
            stats.addFile(info.size());

            if (info.size() == 0) {
                data.status = StatusDefect;
                data.hash = "0"; // Unique marker for empty files
                stats.addDefect();
            } else {
                data.hash = FNV1a::calculate(info.absoluteFilePath());
                if (existingHashes_m.contains(data.hash)) {
                    data.status = StatusAlreadyInDatabase;
                    stats.addAlreadyExists();
                } else if (data.hash.isEmpty()) {
                    data.status = StatusDefect;
                    stats.addDefect();
                } else {
                    // Normal case: Hash is calculated
                    if (!groupMap.contains(data.hash)) {
                        groupMap.insert(data.hash, nextGroupId++);
                    }
                    data.groupId = groupMap.value(data.hash);
                    countMap[data.hash]++; // IMPORTANT: This counts

                    data.status = StatusReady; // By default, it's ready.
                }
            }

            pendingBatches.append(data);
            batchCounter++;

            // Notify the UI only every 20 files.
            if (batchCounter >= 20) {
                emit batchesFound(pendingBatches);
                pendingBatches.clear();
                batchCounter = 0;
                QThread::msleep(1);
            }
            allBatches.append(data);
        }
    }

    for (ScanBatch &batch : allBatches) {
        // If the file has already been marked as defective, we will no longer touch it.
        if (batch.status == StatusDefect) {
            // stats.addDefect();
            continue;
        }

        if (batch.status == StatusAlreadyInDatabase) {
            batch.status = StatusAlreadyInDatabase;
            continue; // Diese Dateien dürfen NICHT als StatusDuplicate markiert werden!
        }

        // Check if the hash occurs multiple times.
        if (countMap.value(batch.hash) > 1) {
            if (batch.status == StatusAlreadyInDatabase) {
                batch.status = StatusAlreadyInDatabase;
            } else {
                batch.status = StatusDuplicate;
            }
            stats.addDuplicate();
            continue;
        }
    }

    if (!pendingBatches.isEmpty()) emit batchesFound(pendingBatches);

    qDebug() << "Scan completed after:" << timer.elapsed() / 1000.0 << "s";
    // Finales Signal senden
    // emit batchesFound(allBatches);
    emit finishWithAllBatches(allBatches, stats);
    emit finished(stats);

}
