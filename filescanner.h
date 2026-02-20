// filescanner.h
#ifndef FILESCANNER_H
#define FILESCANNER_H

#include "reviewstruct.h"
#include "sonarstructs.h"

#include <QFileInfo>
#include <QObject>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QThread>

class FileScanner : public QObject {
    Q_OBJECT
public slots:
    void doScan(const QStringList &paths, const QStringList &filters);
    void abort() { abort_m = true; }

signals:
    void batchesFound(const QList<ScanBatch> &batches);
    void progressStats(const ReviewStats &stats);
    void finished(const ReviewStats &finalStats);
    void finishWithAllBatches(const QList<ScanBatch> &allBatches, const ReviewStats &stats); // call in MainWindow

public:
    void setExistingHashes(const QSet<QString> &hashes) { existingHashes_m = hashes; };
    bool isScanning();

private:
    std::atomic<bool> abort_m{false};
    QSet<QString> existingHashes_m;
    bool isScanning_m{false};
};

#endif
