#ifndef REVIEWSTRUCT_H
#define REVIEWSTRUCT_H

#include <QMetaType>
#include <QMutex>
#include <QMutexLocker>

struct ReviewStats {
public:
    ReviewStats() = default;

    ReviewStats(const ReviewStats& other) {
        QMutexLocker locker(&other.mutex_m);
        totalFiles = other.totalFiles;
        selectedFiles = other.selectedFiles;
        defects = other.defects;
        duplicates = other.duplicates;
        managed = other.managed;
        ignoredFiles = other.ignoredFiles;
        alreadyInDb = other.alreadyInDb;
        ignoredBytes = other.ignoredBytes;
        totalBytes = other.totalBytes;
        selectedBytes = other.selectedBytes;
    }

    ReviewStats& operator=(const ReviewStats& other) {
        if (this != &other) {
            QMutexLocker locker1(&mutex_m);
            QMutexLocker locker2(&other.mutex_m);
            totalFiles = other.totalFiles;
            selectedFiles = other.selectedFiles;
            defects = other.defects;
            duplicates = other.duplicates;
            managed = other.managed;
            ignoredFiles = other.ignoredFiles;
            alreadyInDb = other.alreadyInDb;
            ignoredBytes = other.ignoredBytes;
            totalBytes = other.totalBytes;
            selectedBytes = other.selectedBytes;
        }
        return *this;
    }

    long long totalFiles{0};
    long long selectedFiles{0};
    long long defects{0};
    long long duplicates{0};
    long long managed{0};
    long long ignoredFiles{0};
    long long alreadyInDb{0};
    long long ignoredBytes{0};
    long long totalBytes{0};
    long long selectedBytes{0};

    void addFile(long long size, bool isDuplicate, bool isDefect, bool alreadyInDbParam = false) {
        QMutexLocker locker(&mutex_m);
        if (alreadyInDbParam) {
            alreadyInDb++;
            return;
        }
        totalFiles++;
        totalBytes += size;
        if (isDuplicate) duplicates++;
        if (isDefect) defects++;
    }

    void addIgnored(long long size) {
        QMutexLocker locker(&mutex_m);
        ignoredFiles++;
        ignoredBytes += size;
    }

    void addSelected(int count = 1) {
        QMutexLocker locker(&mutex_m);
        selectedFiles += count;
    }

    void merge(const ReviewStats& other) {
        QMutexLocker locker1(&mutex_m);
        QMutexLocker locker2(&other.mutex_m);
        totalFiles += other.totalFiles;
        selectedFiles += other.selectedFiles;
        defects += other.defects;
        duplicates += other.duplicates;
        managed += other.managed;
        ignoredFiles += other.ignoredFiles;
        alreadyInDb += other.alreadyInDb;
        ignoredBytes += other.ignoredBytes;
        totalBytes += other.totalBytes;
        selectedBytes += other.selectedBytes;
    }

    void reset() {
        QMutexLocker locker(&mutex_m);
        totalFiles = selectedFiles = defects = duplicates = 0;
        managed = ignoredFiles = alreadyInDb = 0;
        ignoredBytes = totalBytes = selectedBytes = 0;
    }

private:
    mutable QMutex mutex_m;
};

Q_DECLARE_METATYPE(ReviewStats)

#endif // REVIEWSTRUCT_H
