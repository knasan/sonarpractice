#include <QTest>
#include <QObject>
#include <QStandardItemModel>

#include <QSortFilterProxyModel>
#include <qtmetamacros.h>

#include "../../filefilterproxymodel.h"
#include "sonarstructs.h"

class FileFilterProxyModelWrapper : public FileFilterProxyModel {
public:
    explicit FileFilterProxyModelWrapper(QObject* parent = nullptr)
            : FileFilterProxyModel(parent) {}
};

class TestFileFilterProxyModel : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    void testSetIgnoredPaths();

private:
    QStandardItemModel* sourceModel;
    FileFilterProxyModelWrapper* proxyModel;

    void addFileItem(QString name, QString path, FileStatus status, QStandardItem* parent = nullptr);
};

void TestFileFilterProxyModel::init() {
    sourceModel = new QStandardItemModel();
    // initialisieren den Proxy mit einer leeren Liste
    proxyModel = new FileFilterProxyModelWrapper();
    proxyModel->setSourceModel(sourceModel);
}

void TestFileFilterProxyModel::cleanup() {
    delete proxyModel;   // Löscht auch die Proxy-Logik
    delete sourceModel;  // Löscht alle Items im Model
}

// Hilfmethode für funktionen innerhalb vom Test aufgerufen werden kann.
void TestFileFilterProxyModel::addFileItem(QString name, QString path, FileStatus status, QStandardItem* parent) {
    // Erstelle ein Item für die erste Spalte (Name)
    QStandardItem* item = new QStandardItem(name);

    // Daten in die entsprechenden Rollen schreiben
    item->setData(path, RoleFilePath);
    item->setData(status, RoleFileStatus);

    if (parent) {
        parent->appendRow(item);
    } else {
        sourceModel->appendRow(item);
    }
}

void TestFileFilterProxyModel::testSetIgnoredPaths() {
    // 1. Pfade definieren
    QString root = "C:/test_root";
    QString ignoredDir = root + "/ignored_folder";
    QString fileInIgnored = ignoredDir + "/file.txt";
    QString fileInAllowed = root + "/documents/report.pdf";

    // 2. Daten über die Member-Variable sourceModel hinzufügen
    addFileItem("Ignored File", fileInIgnored, StatusReady);
    addFileItem("Allowed File", fileInAllowed, StatusReady);

    // 3. Ignorier-Liste setzen
    QList<QFileInfo> ignoredList;
    ignoredList << QFileInfo(ignoredDir);

    // Nutze die Methode deines ProxyModels, um die Liste zu aktualisieren

    // 4. Prüfung
    // Da ein Item im ignorierten Pfad liegt, darf nur 1 übrig bleiben
    // QCOMPARE(proxyModel->rowCount(), 1);

    // Prüfen, ob das richtige Item noch da ist
    //QString pathInProxy = proxyModel->index(0, 0).data(RoleFilePath).toString();
    //QCOMPARE(pathInProxy, fileInAllowed);
}

// -- ENDE --
QTEST_MAIN(TestFileFilterProxyModel) // wird nur Benötigt mit einem Konstruktor
#include "tst_filefilterproxymodel.moc"
