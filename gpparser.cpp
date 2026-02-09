#include "gpparser.h"

#include <QDebug>
#include <QDomElement>
#include <QFile>
#include <QtEndian>

#include "miniz.h"

GpParser::GpParser() {}

QString GpParser::readVersionString(QDataStream &in)
{
    quint8 length;
    in >> length;

    if (length == 0)
        return QString();

    QByteArray data;
    data.resize(length);
    in.readRawData(data.data(), length);

    return QString::fromLatin1(data);
}

qint32 GpParser::scanBpm(QDataStream &in)
{
    qint64 savePos = in.device()->pos();
    in.device()->seek(0);
    QByteArray header = in.device()->read(4000);

    for (int i = 300; i < header.size() - 4; ++i) {
        unsigned char b1 = (unsigned char) header[i];
        unsigned char b2 = (unsigned char) header[i + 1];
        unsigned char b3 = (unsigned char) header[i + 2];
        unsigned char b4 = (unsigned char) header[i + 3];

        uint32_t val = b1 | (b2 << 8) | (b3 << 16) | (b4 << 24);

        if (val >= 30000 && val <= 300000 && val % 1000 == 0) {
            return val / 1000;
        }

        if (val >= 40 && val <= 250) {
            if ((unsigned char) header[i - 1] == 0 && (unsigned char) header[i + 4] <= 1) {
                return val;
            }
        }
    }

    in.device()->seek(savePos);
    return 0;
}

int GpParser::scanBpm2(QDataStream &in, qint64 startPos)
{
    qint64 originalPos = in.device()->pos();
    in.device()->seek(startPos);

    while (!in.atEnd()) {
        quint32 value;
        in >> value;

        if (value >= 30 && value <= 300) {
            return (int) value;
        }
    }

    in.device()->seek(originalPos);
    return 0;
}

GpParser::GPMetadata GpParser::parseGP2(QDataStream &in, GPMetadata &meta)
{
    const int padding = 50;

    meta.title = readGTPString(in, padding);
    meta.subtitle = readGTPString(in, padding);
    meta.artist = readGTPString(in, padding);
    qint64 currentPos = in.device()->pos();
    in.device()->seek(currentPos + 10);
    meta.bpm = scanBpm2(in, in.device()->pos());
    meta.tuning = scanTuning(in);

    meta.isValid = true;

    return meta;
}

QString GpParser::readGTPString(QDataStream &in, int fixedLength)
{
    quint8 actualLength;
    in >> actualLength;
    QByteArray data;
    data.resize(fixedLength);
    in.readRawData(data.data(), fixedLength);
    return QString::fromLatin1(data.left(actualLength)).trimmed();
}

QString GpParser::readGPString(QDataStream &in)
{
    if (in.atEnd())
        return QString();

    quint32 bufferSize;
    in >> bufferSize;

    if (bufferSize == 0)
        return QString();

    quint8 actualLength;
    in >> actualLength;

    int bytesToRead = bufferSize - 1;
    QByteArray data;
    if (bytesToRead > 0) {
        data.resize(bytesToRead);
        in.readRawData(data.data(), bytesToRead);
    }

    return QString::fromLatin1(data.left(actualLength));
}

GpParser::GPMetadata GpParser::parseGP345(QDataStream &in, GPMetadata &meta)
{
    meta.title = readGPString(in);
    meta.subtitle = readGPString(in);
    meta.artist = readGPString(in);
    meta.album = readGPString(in);
    meta.author = readGPString(in);
    meta.copyright = readGPString(in);
    meta.tab = readGPString(in);
    meta.instruction = readGPString(in);

    quint32 noticeCount;
    in >> noticeCount;
    if (noticeCount > 50)
        noticeCount = 0;
    for (uint i = 0; i < noticeCount; ++i) {
        meta.notice += readGPString(in) + (i < noticeCount - 1 ? "\n" : "");
    }

    in.skipRawData(1);

    if (meta.version.contains("v4")) {
        in.skipRawData(4);
        for (int i = 0; i < 5; ++i) {
            in.skipRawData(4);
            (void) readGPString(in);
        }
    }

    if (meta.version.contains("v5")) {
        qint64 startPos = in.device()->pos();
        QByteArray buffer = in.device()->read(1500);

        bool bpmFound = false;
        meta.bpm = scanBpm(in);
        if (meta.bpm > 0) {
            bpmFound = true;
        }

        for (int i = 0; i < buffer.size() - 4; ++i) {
            uint32_t val = qFromLittleEndian<uint32_t>(
                reinterpret_cast<const uchar *>(buffer.data() + i));

            if (val >= 30000 && val <= 300000 && (val % 1000 == 0)) {
                meta.bpm = val / 1000;
                in.device()->seek(startPos + i + 4);
                bpmFound = true;
                break;
            }
        }

        if (!bpmFound) {
            for (int i = 0; i < buffer.size() - 20; ++i) {
                uint8_t len = (uint8_t) buffer[i];
                if (len > 0 && len <= 12) {
                    uint32_t possibleBpm = qFromLittleEndian<uint32_t>(
                        reinterpret_cast<const uchar *>(buffer.data() + i + 1 + len));
                    if (possibleBpm >= 40 && possibleBpm <= 300) {
                        meta.bpm = possibleBpm;
                        in.device()->seek(startPos + i + 1 + len + 4);
                        bpmFound = true;
                        break;
                    }
                }
            }
        }

        if (!bpmFound) {
            for (int i = 0; i <= buffer.size() - 20; ++i) {
                uint32_t blockSize = qFromLittleEndian<uint32_t>(
                    reinterpret_cast<const uchar *>(buffer.data() + i));
                uint8_t stringLen = static_cast<uint8_t>(buffer[i + 4]);

                if (stringLen > 0 && stringLen < 20 && blockSize == (uint32_t) stringLen + 1) {
                    int bpmOffset = i + 5 + stringLen;
                    if (bpmOffset + 4 <= buffer.size()) {
                        uint32_t possibleBpm = qFromLittleEndian<uint32_t>(
                            reinterpret_cast<const uchar *>(buffer.data() + bpmOffset));

                        if (possibleBpm >= 30 && possibleBpm <= 500) {
                            meta.bpm = possibleBpm;
                            in.device()->seek(startPos + bpmOffset + 4);
                            bpmFound = true;
                            break;
                        }
                    }
                }
            }
        }

        if (bpmFound) {
            meta.tuning = scanTuning(in);
        }

    } else {
        quint16 bpmValue;
        in >> bpmValue;
        meta.bpm = bpmValue;
        meta.tuning = scanTuning(in);
    }

    meta.isValid = true;

    return meta;
}

QString GpParser::scanTuning(QDataStream &in)
{
    QString tuning;
    in.device()->seek(0);
    QByteArray header = in.device()->read(4000);

    for (int i = 400; i < header.size() - 32; ++i) {
        uint32_t strings = qFromLittleEndian<uint32_t>(
            reinterpret_cast<const uchar *>(header.data() + i));

        if (strings >= 4 && strings <= 8) {
            QList<int> notes;
            bool plausible = true;

            for (int j = 0; j < 7; ++j) {
                uint32_t n = qFromLittleEndian<uint32_t>(
                    reinterpret_cast<const uchar *>(header.data() + i + 4 + (j * 4)));

                if (j < (int) strings) {
                    if ((n < 10) || (n > 100)) {
                        plausible = false;
                        break;
                    }
                    notes.append(n);
                }
            }

            if (plausible) {
                tuning = identifyTuning(notes);
                if (tuning.isEmpty()) {
                    tuning = formatTuning(notes);
                }
            }
        }
    }
    return tuning;
}

QString GpParser::identifyTuning(const QList<int> &pitches)
{
    if (pitches.isEmpty())
        return "Unknown";

    QList<int> sortedPitches = pitches;
    std::sort(sortedPitches.begin(), sortedPitches.end());

    int size = sortedPitches.size();

    // qDebug() << "Size: " << size;
    // qDebug() << "SortedPitches: " << sortedPitches;

    // 6-Saiter Standard & Transponiert
    if (size == 6) {
        if (sortedPitches == QList<int>{40, 45, 50, 55, 59, 64})
            return "E-Standard";
        if (sortedPitches == QList<int>{39, 44, 49, 54, 58, 63})
            return "Eb-Standard";
        if (sortedPitches == QList<int>{38, 43, 48, 53, 57, 62})
            return "D-Standard";
        if (sortedPitches == QList<int>{37, 42, 47, 52, 56, 61})
            return "C#-Standard";
        if (sortedPitches == QList<int>{36, 41, 46, 51, 55, 60})
            return "C-Standard";
        if (sortedPitches == QList<int>{35, 40, 45, 50, 54, 59})
            return "B-Standard";

        // 6-Saiter Drop
        if (sortedPitches == QList<int>{38, 45, 50, 55, 59, 64})
            return "Drop D";
        if (sortedPitches == QList<int>{37, 44, 49, 54, 58, 63})
            return "Drop C#";
        if (sortedPitches == QList<int>{36, 43, 48, 53, 57, 62})
            return "Drop C";
        if (sortedPitches == QList<int>{35, 42, 47, 52, 56, 61})
            return "Drop B";
        if (sortedPitches == QList<int>{34, 41, 46, 51, 55, 60})
            return "Drop Bb";
        if (sortedPitches == QList<int>{33, 40, 45, 50, 54, 59})
            return "Drop A";
    }

    // 7-Saiter
    if (size == 7) {
        if (sortedPitches == QList<int>{35, 40, 45, 50, 55, 59, 64})
            return "7-String Standard (B)";
        if (sortedPitches == QList<int>{64, 59, 54, 50, 45, 40, 35})
            return "7-String Standard (B)";
        if (sortedPitches == QList<int>{33, 40, 45, 50, 55, 59, 64})
            return "7-String Drop A";
    }

    // Bass (4-Saiter)
    if (size == 4) {
        if (sortedPitches == QList<int>{28, 33, 38, 43})
            return "Bass E-Standard";
        if (sortedPitches == QList<int>{43, 38, 33, 28, 23})
            return "Bass B-Standard";
        if (sortedPitches == QList<int>{26, 33, 38, 43})
            return "Bass Drop D";
    }

    // Fallback: Benennung nach der TATSÄCHLICH tiefsten Note
    const QStringList noteNames = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    int lowPitch = sortedPitches.first(); // .first() ist jetzt garantiert die tiefste Saite
    return QString("Custom (%1)").arg(noteNames.at(lowPitch % 12));
}

QString GpParser::formatTuning(const QList<int> &notes)
{
    QString tuning = "";
    if (notes.isEmpty())
        return tuning;

    static const QStringList noteNames
        = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

    QList<int> sortedNotes = notes;
    std::sort(sortedNotes.begin(), sortedNotes.end());

    QStringList result;
    for (int n : sortedNotes) {
        if (n > 0 && n < 128) {
            result.append(noteNames[n % 12]);
        }
    }
    return result.join("");
}

QByteArray GpParser::unzipGPFile(const QString &filePath) {
    mz_zip_archive zip_archive;
    memset(&zip_archive, 0, sizeof(zip_archive));

    if (!mz_zip_reader_init_file(&zip_archive, filePath.toUtf8().constData(), 0)) {
        qCritical() << "Konnte GP-Datei nicht als ZIP öffnen:" << filePath;
        return QByteArray();
    }

    size_t uncompressed_size;
    void* p = mz_zip_reader_extract_file_to_heap(&zip_archive, "Content/Score.gpif", &uncompressed_size, 0);

    if (!p) {
        qCritical() << "Score.gpif nicht im Archiv gefunden oder Extraktion fehlgeschlagen!";
        mz_zip_reader_end(&zip_archive);
        return QByteArray();
    }

    QByteArray data(static_cast<const char*>(p), static_cast<int>(uncompressed_size));

    mz_free(p);
    mz_zip_reader_end(&zip_archive);

    return data;
}

GpParser::GPMetadata GpParser::parseXmlMetadata(const QByteArray &xmlData) {
    GPMetadata meta;
    QDomDocument doc;

    auto result = doc.setContent(xmlData);

    if (!result) {
        qDebug() << "XML Error:" << result.errorMessage
                 << "in Line:" << result.errorLine
                 << "in Column:" << result.errorColumn;
        return meta;
    }

    QDomElement root = doc.documentElement();

    auto titles = doc.elementsByTagName("Title");
    meta.title =  titles.at(0).toElement().text();

    auto subtitle = doc.elementsByTagName("Subtitle");
    meta.subtitle = subtitle.at(0).toElement().text();

    auto artists = doc.elementsByTagName("Artist");
    meta.artist = artists.at(0).toElement().text();

    auto album = doc.elementsByTagName("Album");
    meta.album = album.at(0).toElement().text();

    auto copyright = doc.elementsByTagName("Copyright");
    meta.copyright = copyright.at(0).toElement().text();

    // author
    auto tab = doc.elementsByTagName("Tabber");
    meta.author = tab.at(0).toElement().text();

    auto instruction = doc.elementsByTagName("instruction");
    meta.instruction = instruction.at(0).toElement().text();

    auto notice = doc.elementsByTagName("Notices");
    meta.notice = notice.at(0).toElement().text();

    auto automations = doc.elementsByTagName("Automation");

    for (int i = 0; i < automations.count(); ++i) {
        QDomElement autoElem = automations.at(i).toElement();

        if (autoElem.firstChildElement("Type").text() == "Tempo") {
            QDomElement valueElem = autoElem.firstChildElement("Value");

            if (!valueElem.isNull()) {
                QString rawValue = valueElem.text().trimmed();

                if (!rawValue.isEmpty()) {
                    QStringList parts = rawValue.split(' ', Qt::SkipEmptyParts);

                    if (!parts.isEmpty()) {
                        QString bpmValue = parts.at(0);

                        bool ok;
                        double bpm = bpmValue.toDouble(&ok);
                        if(ok) {
                            meta.bpm = bpm;
                            break;
                        }
                    }
                }
            }
        }
    }

    // Tuning
    QStringList tuningStrings = doc.elementsByTagName("Pitches").at(0).toElement().text().split(' ', Qt::SkipEmptyParts);

    QList<int> pitchList;

    for (const QString &s : std::as_const(tuningStrings)) {
        pitchList.append(s.toInt());
    }

    // qDebug() << "Tuning: " << identifyTuning(pitchList);
    meta.tuning = identifyTuning(pitchList);
    return meta;
}

// ---- PUBLIC ----
GpParser::GPMetadata GpParser::parseMetadata(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return {};

    QDataStream in(&file);
    in.setByteOrder(QDataStream::LittleEndian);

    GPMetadata meta;

    char magic[4];
    in.device()->peek(magic, 4);

    meta.version = readVersionString(in);

    if (meta.version.contains(("v2"))) {
        in.device()->seek(32);
        meta = parseGP2(in, meta);
    }

    if (meta.version.contains("v3") || meta.version.contains("v4") || meta.version.contains("v5")) {
        in.device()->seek(31);
        meta = parseGP345(in, meta);
    }

    // V6/7
    // BCFS = uncompressed
    // LZ77-like algorithm
    if (meta.version.startsWith("CFZ")) {
        meta.isValid = false;
    }

    // V6
    // GP File
    // 'P' 'K' '\003' '\004' ist die Signatur für ZIP (GPX/GP7)
    if (magic[0] == 'P' && magic[1] == 'K') {
        auto xmlData = unzipGPFile(filePath);
        meta = parseXmlMetadata(xmlData);
        meta.version = "Guitar Pro 6/7 (Compressed)";
    }

    file.close();

    if (!meta.isValid || meta.version.isEmpty())
        return meta;

    return meta;
}
