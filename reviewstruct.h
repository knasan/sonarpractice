#ifndef REVIEWSTRUCT_H
#define REVIEWSTRUCT_H

#include <QObject>
#include <QMutex>

class ReviewStats : public QObject {
    Q_OBJECT
public:
    explicit ReviewStats(QObject* parent = nullptr) : QObject(parent) {}

    // Default copy constructor
    ReviewStats(const ReviewStats& other) : QObject(other.parent()) {
        *this = other;
    }

    // Copy assignment operator
    ReviewStats& operator=(const ReviewStats& other) {
        if (this != &other) {
            QMutexLocker locker(&mutex_m);
            totalFiles_m = other.totalFiles_m;
            selectedFiles_m = other.selectedFiles_m;
            defects_m = other.defects_m;
            duplicates_m = other.duplicates_m;
            managed_m = other.managed_m;
            ignoredFiles_m = other.ignoredFiles_m;
            ignoredBytes_m = other.ignoredBytes_m;
            totalBytes_m = other.totalBytes_m;
            savedBytes_m = other.savedBytes_m;
            alreadyInDb_m = other.alreadyInDb_m;
        }
        return *this;
    }

    // Move constructor
    ReviewStats(ReviewStats&& other) noexcept
        : QObject(other.parent()),
        totalFiles_m(other.totalFiles_m),
        selectedFiles_m(other.selectedFiles_m),
        defects_m(other.defects_m),
        duplicates_m(other.duplicates_m),
        managed_m(other.managed_m),
        ignoredFiles_m(other.ignoredFiles_m),
        ignoredBytes_m(other.ignoredBytes_m),
        totalBytes_m(other.totalBytes_m),
        savedBytes_m(other.savedBytes_m),
        alreadyInDb_m(other.alreadyInDb_m)
    {
        // Reset the moved-from object
        other.reset();
    }

    // Move assignment operator
    ReviewStats& operator=(ReviewStats&& other) noexcept {
        if (this != &other) {
            QMutexLocker locker(&mutex_m);
            totalFiles_m = other.totalFiles_m;
            selectedFiles_m = other.selectedFiles_m;
            defects_m = other.defects_m;
            duplicates_m = other.duplicates_m;
            managed_m = other.managed_m;
            ignoredFiles_m = other.ignoredFiles_m;
            ignoredBytes_m = other.ignoredBytes_m;
            totalBytes_m = other.totalBytes_m;
            savedBytes_m = other.savedBytes_m;
            alreadyInDb_m  += other.alreadyInDb_m;
            other.reset();
        }
        return *this;
    }

    void add(const ReviewStats& other) {
        totalFiles_m    += other.totalFiles_m;
        totalBytes_m    += other.totalBytes_m;
        defects_m       += other.defects_m;
        duplicates_m    += other.duplicates_m;
        selectedFiles_m += other.selectedFiles_m;
        savedBytes_m    += other.savedBytes_m;
        alreadyInDb_m  += other.alreadyInDb_m;
    }

    // ---- Getter ----
    [[nodiscard]] int totalFiles() const noexcept { return totalFiles_m; }
    [[nodiscard]] int selectedFiles() const noexcept { return selectedFiles_m; }
    [[nodiscard]] int defects() const noexcept { return defects_m; }
    [[nodiscard]] int duplicates() const noexcept { return duplicates_m; }
    [[nodiscard]] int managed() const noexcept { return managed_m; }
    [[nodiscard]] int ignoredFiles() const noexcept { return ignoredFiles_m; }
    [[nodiscard]] int alreadyFilesInDb() const noexcept { return alreadyInDb_m; }
    [[nodiscard]] qlonglong ignoredBytes() const noexcept { return ignoredBytes_m; }
    [[nodiscard]] qlonglong totalBytes() const noexcept { return totalBytes_m; }
    [[nodiscard]] qlonglong savedBytes() const noexcept { return savedBytes_m; }

    // ---- State changes (business logic) ----
    void addFile(qint64 size) {
        QMutexLocker locker(&mutex_m);
        totalFiles_m++;
        totalBytes_m += size;
    }

    void addSelectedFile(qint64 size) {
        QMutexLocker locker(&mutex_m);
        selectedFiles_m++;
        totalBytes_m += size;
    }

    void addDefect() {
        QMutexLocker locker(&mutex_m);
        defects_m++;
    }

    void addAlreadyExists() {
        QMutexLocker locker(&mutex_m);
        alreadyInDb_m++;
    }

    void addDuplicate() {
        QMutexLocker locker(&mutex_m);
        duplicates_m++;
    }

    void addManaged(qint64 savedBytes) {
        QMutexLocker locker(&mutex_m);
        managed_m++;
        savedBytes_m += savedBytes;
    }

    void addIgnoredFile(qint64 size) {
        QMutexLocker locker(&mutex_m);
        ignoredFiles_m++;
        ignoredBytes_m += size;
    }

    void merge(const ReviewStats& other) {
        QMutexLocker locker(&mutex_m);
        totalFiles_m += other.totalFiles_m;
        selectedFiles_m += other.selectedFiles_m;
        defects_m += other.defects_m;
        duplicates_m += other.duplicates_m;
        managed_m += other.managed_m;
        ignoredFiles_m += other.ignoredFiles_m;
        ignoredBytes_m += other.ignoredBytes_m;
        totalBytes_m += other.totalBytes_m;
        savedBytes_m += other.savedBytes_m;
        alreadyInDb_m  += other.alreadyInDb_m;
    }

    void reset() {
        QMutexLocker locker(&mutex_m);
        totalFiles_m = 0;
        selectedFiles_m = 0;
        defects_m = 0;
        duplicates_m = 0;
        managed_m = 0;
        ignoredFiles_m = 0;
        ignoredBytes_m = 0;
        totalBytes_m = 0;
        savedBytes_m = 0;
        alreadyInDb_m  = 0;
    }

    // Delete move assignment operator (since it's not implemented)
    // ReviewStats& operator=(ReviewStats&&) = delete;
    int totalFiles_m = 0;
    int selectedFiles_m = 0;
    int defects_m = 0;
    int duplicates_m = 0;
    int managed_m = 0;
    int ignoredFiles_m = 0;
    int alreadyInDb_m = 0;
    qlonglong ignoredBytes_m = 0;
    qlonglong totalBytes_m = 0;
    qlonglong savedBytes_m = 0;
private:
    mutable QMutex mutex_m;
};

#endif // REVIEWSTRUCT_H
