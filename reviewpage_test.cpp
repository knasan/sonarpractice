// #include <QTest>
// #include <QTemporaryFile>
// #include <QTreeWidget>
// #include "reviewpage.h"
// #include "setupwizard.h"

// struct TestSetup {
//     SetupWizard wizard;
//     ReviewPage page;

//     TestSetup() {
//         int id = wizard.addPage(&page);
//         wizard.setStartId(id); // Optional, aber sauberer
//     }
// };

// class ReviewPageTest : public QObject {
//     Q_OBJECT

// private slots:
//     /**
//      * @brief testProcessNextChunks Prüft die automatische Erkennung und UI-Markierung defekter Dateien (0 Byte).
//      * @test Verifiziert, dass processNextChunks() Dateien mit 0 Bytes als 'Defekt' markiert,
//      * rot einfärbt und für den Benutzer deaktiviert.
//      * es wird setItemStatus aufgerufen und indirekt auch geprüft.
//      * > handleItemChanged
//      * > excludeDirectory
//      */
//     void testProcessNextChunks() {
//         // 1. Setup auf dem Stack (verhindert Leaks)
//         TestSetup ts;

//         // 2. Mock-Daten erstellen
//         QTemporaryFile normalFile;
//         QVERIFY(normalFile.open());
//         normalFile.write("Inhalt");
//         normalFile.flush();

//         QTemporaryFile defectFile;
//         QVERIFY(defectFile.open());
//         // Bleibt leer (0 Bytes)

//         ts.wizard.scannedFiles_m << QFileInfo(normalFile.fileName())
//                             << QFileInfo(defectFile.fileName());

//         // 3. Aktion: Seite initialisieren
//         // Da wir 'friend class ReviewPageTest' im Header haben, klappt das:
//         ts.page.initializePage();

//         // Da processNextChunks mit einem Timer (1ms) arbeitet, müssen wir kurz warten
//         QTest::qWait(200);

//         // 4. Prüfung der UI-Elemente
//         QTreeWidget *tree = ts.page.findChild<QTreeWidget*>("reviewPageTree_m");
//         QVERIFY(tree != nullptr);

//         // Wir suchen im Baum nach dem Item der defekten Datei
//         bool defectFound = false;
//         // Wir nutzen die Spalten-Logik aus reviewpage.cpp: ColStatus = 2
//         for (int i = 0; i < tree->topLevelItemCount(); ++i) {
//             QTreeWidgetItem *dir = tree->topLevelItem(i);
//             for (int j = 0; j < dir->childCount(); ++j) {
//                 QTreeWidgetItem *file = dir->child(j);
//                 // Prüfe ob die Größe > 0 angezeigt wird (ColSize = 1)
//                 // QVERIFY(!file->text(1).isEmpty());
//                 // QCOMPARE(file->text(2), tr("OK"));
//                 if (file->text(0) == QFileInfo(defectFile.fileName()).fileName()) {
//                     // Prüfe, ob Status "Defekt" gesetzt wurde
//                     QCOMPARE(file->text(2), tr("Defekt"));
//                     // Prüfe, ob die Schriftfarbe rot ist
//                     QCOMPARE(file->foreground(2).color(), QColor(Qt::red));
//                     // Prüfe, ob das Item deaktiviert ist
//                     QVERIFY(!(file->flags() & Qt::ItemIsEnabled));
//                     defectFound = true;
//                 }
//             }
//         }
//         QVERIFY(defectFound);
//     }

//     /**
//      * @brief testExcludeDirectoryRemovesFromUI Prüft das vollständige Entfernen eines Verzeichnisses aus der UI.
//      * @test Stellt sicher, dass excludeDirectory() Items sowohl aus dem QTreeWidget
//      * als auch aus der internen pathToItem_m Map entfernt.
//      */
//     void testExcludeDirectoryRemovesFromUI() {
//         TestSetup ts;
//         QTreeWidget *tree = ts.page.findChild<QTreeWidget*>("reviewPageTree_m");
//         // Nutze QDir::cleanPath auch im Test!
//         QString testPath = QDir::cleanPath("C:/MeinOrdner/Test");

//         QTreeWidgetItem *folderItem = new QTreeWidgetItem(tree);
//         folderItem->setText(0, "MeinOrdner");
//         folderItem->setData(0, Qt::UserRole, testPath); // Wichtig für StartsWith

//         // Manuelles Füllen der Map, da wir initializePage() hier überspringen
//         ts.page.pathToItem_m.insert(testPath, folderItem);

//         ts.page.excludeDirectory(testPath);

//         QList<QTreeWidgetItem*> results = tree->findItems("MeinOrdner", Qt::MatchExactly | Qt::MatchRecursive, 0);
//         QCOMPARE(results.size(), 0);
//         QVERIFY(!ts.page.pathToItem_m.contains(testPath));
//     }

//     /**
//     * @brief testExcludeDirectoryIsolation Prüft die Isolation beim Löschen von Quellverzeichnissen.
//     * @test Stellt sicher, dass das Exkludieren eines Verzeichnisses keine
//     * anderen Top-Level-Verzeichnisse beeinflusst.
//     */
//     void testExcludeDirectoryIsolation() {
//         // --- ARRANGE ---
//         TestSetup ts;
//         QTreeWidget *tree = ts.page.findChild<QTreeWidget*>("reviewPageTree_m");

//         QString pathA = QDir::cleanPath("C:/Fotos");
//         QString pathB = QDir::cleanPath("C:/Dokumente");

//         // Zwei separate Top-Level-Ordner erstellen
//         QTreeWidgetItem *itemA = new QTreeWidgetItem(tree);
//         itemA->setData(0, Qt::UserRole, pathA);
//         itemA->setText(0, "Fotos");

//         QTreeWidgetItem *itemB = new QTreeWidgetItem(tree);
//         itemB->setData(0, Qt::UserRole, pathB);
//         itemB->setText(0, "Dokumente");

//         ts.page.pathToItem_m.insert(pathA, itemA);
//         ts.page.pathToItem_m.insert(pathB, itemB);

//         // --- ACT ---
//         ts.page.excludeDirectory(pathA);

//         // --- ASSERT ---
//         // 1. Fotos müssen weg sein
//         QCOMPARE(tree->topLevelItemCount(), 1);
//         QVERIFY(!ts.page.pathToItem_m.contains(pathA));

//         // 2. Dokumente müssen noch da sein
//         QCOMPARE(tree->topLevelItem(0)->text(0), QString("Dokumente"));
//         QVERIFY(ts.page.pathToItem_m.contains(pathB));
//     }


//     /**
//  * @brief testExcludeDirectoryCleansMapRecursively
//  * Prüft, ob beim Exkludieren eines Ordners auch alle darin enthaltenen
//  * Dateien aus der pathToItem_m Map entfernt werden.
//  */
//     void testExcludeDirectoryCleansMapRecursively() {
//         // --- ARRANGE ---
//         TestSetup ts;
//         QTreeWidget *tree = ts.page.findChild<QTreeWidget*>("reviewPageTree_m");

//         QString parentPath = QDir::cleanPath("C:/Bilder");
//         QString file1Path = QDir::cleanPath("C:/Bilder/Urlaub.jpg");
//         QString file2Path = QDir::cleanPath("C:/Bilder/Hund.png");

//         // Parent erstellen
//         QTreeWidgetItem *parentItem = new QTreeWidgetItem(tree);
//         parentItem->setData(0, Qt::UserRole, parentPath);
//         ts.page.pathToItem_m.insert(parentPath, parentItem);

//         // Zwei Kinder erstellen
//         QTreeWidgetItem *child1 = new QTreeWidgetItem(parentItem);
//         child1->setData(0, Qt::UserRole, file1Path);
//         ts.page.pathToItem_m.insert(file1Path, child1);

//         QTreeWidgetItem *child2 = new QTreeWidgetItem(parentItem);
//         child2->setData(0, Qt::UserRole, file2Path);
//         ts.page.pathToItem_m.insert(file2Path, child2);

//         // --- ACT ---
//         ts.page.excludeDirectory(parentPath);

//         // --- ASSERT ---
//         // 1. Visueller Check
//         QCOMPARE(tree->topLevelItemCount(), 0);

//         // 2. Map-Check: Nichts darf mehr übrig sein!
//         QVERIFY2(!ts.page.pathToItem_m.contains(parentPath), "Parent-Pfad noch in Map!");
//         QVERIFY2(!ts.page.pathToItem_m.contains(file1Path), "Erstes Kind noch in Map!");
//         QVERIFY2(!ts.page.pathToItem_m.contains(file2Path), "Zweites Kind noch in Map!");

//         // 3. Bonus-Check: Ist die Map jetzt komplett leer?
//         QCOMPARE(ts.page.pathToItem_m.size(), 0);
//     }


//     // Test für später wenn ein Reset-Button inplementiert wird.
//     /**
//      * @brief testResetRestoresOriginalTree Prüft die Wiederherstellungsfunktion der Seite.
//      * @test Verifiziert, dass initializePage() nach manuellen Löschungen den
//      * ursprünglichen Zustand basierend auf scannedFiles_m wiederherstellen kann.
//      */
//     /*void testResetRestoresOriginalTree() {
//         TestSetup ts;
//         // 1. Setup: Zwei Dateien im Wizard hinterlegen
//         ts.wizard.scannedFiles_m << QFileInfo("C:/A.txt") << QFileInfo("C:/B.txt");

//         // Initialer Aufbau
//         ts.page.initializePage();
//         QTest::qWait(200);
//         QTreeWidget *tree = ts.page.findChild<QTreeWidget*>("reviewPageTree_m");
//         int initialCount = tree->topLevelItemCount();

//         // 2. Aktion: Etwas löschen
//         ts.page.excludeDirectory("C:/");
//         QCOMPARE(tree->topLevelItemCount(), 0); // Baum jetzt leer

//         // 3. Reset: Die Seite neu initialisieren
//         // Hier nutzt du die Tatsache, dass foundFiles_m noch alle Daten hat
//         ts.page.initializePage();
//         QTest::qWait(200);

//         // 4. Verifikation
//         QCOMPARE(tree->topLevelItemCount(), initialCount);
//         QVERIFY(ts.page.pathToItem_m.contains("C:/A.txt"));
//     }*/

// };

// QTEST_MAIN(ReviewPageTest)
// #include "reviewpage_test.moc"
