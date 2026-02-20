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
        savedBytes = other.savedBytes;
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
            savedBytes = other.savedBytes;
        }
        return *this;
    }

    int totalFiles{0};
    int selectedFiles{0};
    int defects{0};
    int duplicates{0};
    int managed{0};
    int ignoredFiles{0};
    int alreadyInDb{0};

    long long ignoredBytes{0};
    long long totalBytes{0};
    long long savedBytes{0};

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
        savedBytes += other.savedBytes;
    }

    void reset() {
        QMutexLocker locker(&mutex_m);
        totalFiles = selectedFiles = defects = duplicates = 0;
        managed = ignoredFiles = alreadyInDb = 0;
        ignoredBytes = totalBytes = savedBytes = 0;
    }

private:
    mutable QMutex mutex_m;
};

Q_DECLARE_METATYPE(ReviewStats)

#endif // REVIEWSTRUCT_H
