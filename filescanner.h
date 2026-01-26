// filescanner.h
#ifndef FILESCANNER_H
#define FILESCANNER_H


#include "reviewstruct.h"
#include "setupwizard.h"

#include <QFileInfo>
#include <QObject>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QThread>

class FileScanner : public QObject {
    Q_OBJECT
public slots:
    void doScan(const QStringList &paths, const QStringList &filters, SetupWizard* wizard);

signals:
    void batchesFound(const QList<ScanBatch> &batches);
    void progressStats(const ReviewStats &stats);
    void finished(const ReviewStats &finalStats);
    void finishWithAllBatches(const QList<ScanBatch> &allBatches, const ReviewStats &stats);

private:
    std::atomic<bool> abort_m{false};
};

#endif
