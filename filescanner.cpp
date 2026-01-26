#include "filescanner.h"
#include "fnv1a.h"

void FileScanner::doScan(const QStringList &paths, const QStringList &filters, SetupWizard* wizard) {
    qDebug() << "[FileScanner] doScan";
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
                qDebug() << "Skipping directory:" << info.absoluteFilePath();
                continue;
            }

            ScanBatch data;
            data.info = info;
            stats.addFile(info.size());

            if (info.size() == 0) {
                data.status = StatusDefect;
                data.hash = "0"; // Eindeutiger Marker für leere Dateien
                stats.addDefect();
            } else {
                data.hash = FNV1a::calculate(info.absoluteFilePath());
                if (data.hash.isEmpty()) {
                    data.status = StatusDefect;
                    stats.addDefect();
                } else {
                    // Normaler Fall: Hash berechnet
                    if (!groupMap.contains(data.hash)) {
                        groupMap.insert(data.hash, nextGroupId++);
                    }
                    data.groupId = groupMap.value(data.hash);
                    countMap[data.hash]++; // WICHTIG: Hier zählen

                    data.status = StatusReady; // Standardmäßig erst mal Ready
                }
            }

            pendingBatches.append(data);
            batchCounter++;

            // Nur alle 10 Dateien die UI benachrichtigen
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
        // Wenn die Datei bereits als Defekt markiert wurde, fassen wir sie nicht mehr an
        if (batch.status == StatusDefect) {
            // stats.addDefect();
            continue;
        }

        // Prüfen, ob der Hash mehrfach vorkommt
        if (countMap.value(batch.hash) > 1) {
            batch.status = StatusDuplicate;
            stats.addDuplicate();
            continue;
        }
    }

    if (!pendingBatches.isEmpty()) emit batchesFound(pendingBatches);

    qDebug() << "Scan abgeschlossen nach:" << timer.elapsed() / 1000.0 << "s";
    // Finales Signal senden
    // emit batchesFound(allBatches);
    emit finishWithAllBatches(allBatches, stats);
    emit finished(stats);

}
