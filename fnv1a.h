#ifndef FNV1A_HPP
#define FNV1A_HPP

#include <QFile>
#include <QString>
#include <QByteArray>
#include <QtGlobal>

class FNV1a {
public:
    [[nodiscard]] static QString calculate(const QString &filePath) {
        const uint64_t FNV_prime = 1099511628211u;
        uint64_t hash = 14695981039346656037u;

        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) [[unlikely]] {
            return QString();
        }

        const qint64 fileSize = file.size();
        if (fileSize == 0) return QString("0000000000000000");

        hash ^= static_cast<uint64_t>(fileSize);
        hash *= FNV_prime;

        const int numSamples = 10;
        const qint64 sampleSize = 320;

        for (int i = 0; i < numSamples; ++i) {
            qint64 offset = 0;
            if (fileSize > sampleSize) {
                offset = (fileSize - sampleSize) * i / (numSamples - 1);
            }

            if (file.seek(offset)) {
                QByteArray chunk = file.read(sampleSize);
                for (char byte : std::as_const(chunk)) {
                    hash ^= static_cast<uint8_t>(byte);
                    hash *= FNV_prime;
                }
            }
        }

        file.close();

        return QString("%1").arg(hash, 16, 16, QChar('0')).toUpper();
    }
};

#endif
