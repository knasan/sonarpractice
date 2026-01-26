#include <QTest>
#include <QObject>

#include <QSortFilterProxyModel>
#include <qtmetamacros.h>

// add necessary includes here

#include "../../filefilterproxymodel.h"
#include "../../filemanager.h"

class FileFilterProxyModelWrapper : public FileFilterProxyModel {
public:
    explicit FileFilterProxyModelWrapper(QObject* parent = nullptr)
            : FileFilterProxyModel(parent) {}

    // Protected function now as public for testing.
    using FileFilterProxyModel::collectPathsRecursive;
};

class TestFileFilterProxyModel : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    void testSetIgnoredPaths();
    void testCollectPathsRecursive();

private:
    QStandardItemModel* sourceModel;
    FileFilterProxyModelWrapper* proxyModel;

    void addFileItem(QString name, QString path, FileStatus status, QStandardItem* parent = nullptr);
};

void TestFileFilterProxyModel::init() {
    sourceModel = new QStandardItemModel();
    // Wir initialisieren den Proxy mit einer leeren Liste
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
    QCOMPARE(proxyModel->rowCount(), 1);

    // Prüfen, ob das richtige Item noch da ist
    QString pathInProxy = proxyModel->index(0, 0).data(RoleFilePath).toString();
    QCOMPARE(pathInProxy, fileInAllowed);
}

void TestFileFilterProxyModel::testCollectPathsRecursive() {
    // 1. Struktur aufbauen
    // Root-Ebene
    addFileItem("Root_Checked.txt", "/root/1.txt", StatusReady);
    sourceModel->setData(sourceModel->index(0, 0), Qt::Checked, Qt::CheckStateRole);

    addFileItem("Root_Unchecked.txt", "/root/2.txt", StatusReady);
    sourceModel->setData(sourceModel->index(1, 0), Qt::Unchecked, Qt::CheckStateRole);

    // Ordner erstellen
    QStandardItem* folder = new QStandardItem("Folder");
    sourceModel->appendRow(folder);

    // Dateien im Ordner (über Hilfsmethode mit parent)
    addFileItem("Sub_Checked.txt", "/root/folder/sub1.txt", StatusReady, folder);
    sourceModel->setData(folder->child(0, 0)->index(), Qt::Checked, Qt::CheckStateRole);

    // 2. Test: Alle Pfade sammeln (onlyChecked = false)
    QStringList allPaths;
    proxyModel->collectPathsRecursive(QModelIndex(), allPaths, false);

    // Erwartet: 3 Dateien (Root_Checked, Root_Unchecked, Sub_Checked)
    // Hinweis: Der Ordner selbst wird in deiner Logik übersprungen, nur Dateien zählen.
    QCOMPARE(allPaths.size(), 3);
    QVERIFY(allPaths.contains("/root/1.txt"));
    QVERIFY(allPaths.contains("/root/folder/sub1.txt"));

    // 3. Test: Nur markierte Pfade sammeln (onlyChecked = true)
    QStringList checkedPaths;
    proxyModel->collectPathsRecursive(QModelIndex(), checkedPaths, true);

    // Erwartet: 2 Dateien
    QCOMPARE(checkedPaths.size(), 2);
    QVERIFY(checkedPaths.contains("/root/1.txt"));
    QVERIFY(checkedPaths.contains("/root/folder/sub1.txt"));
    QVERIFY(!checkedPaths.contains("/root/2.txt")); // Die unmarkierte darf nicht dabei sein
}

// -- ENDE --
QTEST_MAIN(TestFileFilterProxyModel) // wird nur Benötigt mit einem Konstruktor
#include "tst_filefilterproxymodel.moc"
