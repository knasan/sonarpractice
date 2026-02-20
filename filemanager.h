#ifndef FILEMANAGER_H
#define FILEMANAGER_H

#include "sonarstructs.h"

#include <QObject>
#include <QStandardItemModel>
#include <QFileInfo>
#include <QDir>

class FileManager : public QObject {
    Q_OBJECT
signals:
    void progressChanged(int value);
    void statusTextChanged(const QString &text);
    void rangeChanged(int min, int max);
public:
    explicit FileManager(QObject *parent = nullptr) : QObject(parent) {}

    [[nodiscard]] static QColor getStatusColor(int status);
    [[nodiscard]] static QString getStatusText(int status);

    void addBatchesToModel(const QList<ScanBatch> &batches);

    void setModel(QStandardItemModel *model) {
        model_m = model;
    }

    void clearCaches();

    [[nodiscard]] QStandardItem* getFolderItem(const QString &path) const;

    void updateStatuses(const QList<ScanBatch> &allBatches);

    void setExistingHashes(const QSet<QString> &hashes) { existingHashes_m = hashes; };

private:
    [[nodiscard]] QStandardItem* findItemByPath(const QString& filePath);
    QStandardItemModel* model_m;

    QMap<int, QStringList> duplicateGroups_m;
    QHash<QString, QStandardItem*> pathCache_m;
    QHash<int, QStandardItem*> groupHeaderCache_m;

    QSet<QString> existingHashes_m;

    void printModelStructure(QStandardItem *item, int level = 0) const;
};

#endif // FILEMANAGER_H
