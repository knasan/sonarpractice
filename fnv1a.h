#ifndef FNV1A_HPP
#define FNV1A_HPP

#include <QFile>
#include <QString>

class FNV1a {
public:
    [[nodiscard]] static QString calculate(const QString &filePath) {
        const uint64_t FNV_prime = 1099511628211u;
        uint64_t hash = 14695981039346656037u;

        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) [[unlikely]] {
            return QString();
        }

        qint64 fileSize = file.size();
        if (fileSize == 0) return QString("0");

        // Read 3 blocks of 1024 bytes each (start, middle, end)
        // That's only 3KB per file -> extremely fast!
        QList<qint64> offsets;
        offsets << 0;                               // start
        if (fileSize > 2048) {
            offsets << (fileSize / 2);              // middle
            offsets << (fileSize - 1024);           // end
        }

        for (qint64 offset : std::as_const(offsets)) {
            if (file.seek(offset)) {
                QByteArray chunk = file.read(1024);
                for (int i = 0; i < chunk.size(); ++i) {
                    hash ^= static_cast<uint8_t>(chunk.at(i));
                    hash *= FNV_prime;
                }
            }
        }
        file.close();

        // Combine the hash with the file size for maximum security
        hash ^= static_cast<uint64_t>(fileSize);
        hash *= FNV_prime;

        return QString("%1").arg(hash, 16, 16, QChar('0')).toUpper();
    }
};

#endif
