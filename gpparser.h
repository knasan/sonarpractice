#ifndef GPPARSER_H
#define GPPARSER_H

#include <QString>

class GpParser
{
public:
    GpParser();

    // For SonarPractice using title, artist, bpm and tuning
    struct GPMetadata
    {
        QString version;
        QString title;
        QString subtitle;
        QString artist;
        QString album;
        QString author;
        QString copyright;
        QString tab;
        QString instruction;
        QString notice;
        quint32 bpm = 0;
        quint32 measures;
        quint32 tracks;
        QString tuning;
        QString instrument;

        bool isValid = false;
    };

    [[nodiscard]] static GPMetadata parseMetadata(const QString &filePath);

private:
    [[nodiscard]] static QString readVersionString(QDataStream &in);
    [[nodiscard]] qint32 static scanBpm(QDataStream &in);
    [[nodiscard]] qint32 static scanBpm2(QDataStream &in, qint64 startPos);
    [[nodiscard]] static QString readGPString(QDataStream &in);

    [[nodiscard]] static GPMetadata parseGP2(QDataStream &in, GPMetadata &meta);
    [[nodiscard]] static QString readGTPString(QDataStream &in, int fixedLength);

    [[nodiscard]] static GPMetadata parseGP345(QDataStream &in, GPMetadata &meta);

    // void static parseBCF(const QString &zipPath);
    [[nodiscard]] static QByteArray unzipGPFile(const QString &filePath);

    [[nodiscard]] GPMetadata static parseXmlMetadata(const QByteArray &xmlData);

    [[nodiscard]] static QString scanTuning(QDataStream &in);
    [[nodiscard]] static QString identifyTuning(
        const QList<int> &
            pitches); // XML Tuning Helper QList("39", "44", "49", "54", "58", "63") -> Eb-Standard Stimmung
    [[nodiscard]] static QString formatTuning(const QList<int> &notes);
};

#endif // GPPARSER_H
