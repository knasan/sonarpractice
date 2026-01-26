# SonarPractice (Alpha)

Organisieren Sie Ihr Repertoire und verfolgen Sie Ihre Übungsfortschritte mit SonarPractice – entwickelt von einem Musiker für Musiker.

## Hintergrund & Motivation

SonarPractice entstand aus einer ganz persönlichen Notwendigkeit heraus. Als Musiker suchte ich nach einer effizienten Möglichkeit, mein Repertoire zu organisieren und meinen Übungsfortschritt systematisch zu dokumentieren.

Papier ist geduldig, aber unflexibel. Tabellenkalkulationen erwiesen sich als zu aufwändig und im Übungsalltag schlicht unkomfortabel. Da keine passende Software existierte, die meine Anforderungen an Performance und Struktur erfüllte, habe ich SonarPractice entwickelt.

## Key Features

- High-Performance Medien-Katalog: Blitzschnelle Verwaltung von über 20.000 Dateien durch SQLite-Backend mit dedizierter Indexierung auf Dateipfade.

- Intelligentes Repertoire-Management: Verknüpfung von Master-Songs (z.B. GuitarPro) mit Begleitmaterial wie Audio-Backings oder PDF-Sheets.

- Smart-Search: Echtzeit-Filterung im Katalog mit sofortigem Feedback.

- Übungs-Journal: Kalenderbasierte Erfassung von BPM-Werten, Notizen und Übungsdauer zur Erfolgskontrolle.

- Responsive UI: Unterbrechungsfreies Arbeiten dank asynchroner Ladevorgänge und Fortschrittsanzeigen.

### Technischer Stack

- Framework: Qt 6.10 (Modernste Signal/Slot-Syntax und UI-Optimierungen).

- Sprachstandard: C++23 (Einsatz moderner Features wie std::as_const für sauberen Code).

- Datenbank: SQLite 3 mit optimierten Indizes für schnellen Datenzugriff auch bei großen Beständen.

- Architektur: Durchgängiges Model/View-Pattern zur Trennung von Daten und Darstellung.

### Build-Anleitung

Das Projekt erfordert einen C++23-fähigen Compiler (MSVC 2022, GCC 12+ oder Clang 15+) und Qt 6.10.

Die Datenbankstruktur inklusive aller Performance-Indizes wird beim ersten Programmstart automatisch

im Hintergrund initialisiert.

### Lizenz & Open Source

Ich habe mich dazu entschieden, SonarPractice als Open-Source-Projekt unter der GNU GPLv3 Lizenz zu veröffentlichen.

Ich bin fest davon überzeugt, dass dieses Tool auch anderen Musikern dabei helfen wird, ihre tägliche Übungsroutine zu strukturieren und ihr Repertoire besser zu bändigen. Durch die Offenlegung des Quellcodes möchte ich der Musiker- und Entwickler-Community etwas zurückgeben und eine Plattform für gemeinsames Wachstum schaffen.

- Gemeinschaft: Jeder ist eingeladen, das Tool zu nutzen, zu verbessern und eigene Ideen einzubringen.

- Transparenz: Dank der GPLv3 bleibt SonarPractice dauerhaft frei und offen für alle.

### Geplante Features (Roadmap)

SonarPractice befindet sich stetig in der Weiterentwicklung. Folgende Funktionen sind für kommende Versionen geplant:

[ ] Intelligentes Guitar Pro Parsing: Automatisches Auslesen von Metadaten (Künstler, Tuning, BPM) aus .gp-Dateien zur automatischen Katalogisierung.

[ ] Setlist-Management: Erstellung gezielter Songlisten für Auftritte oder spezifische Übungssessions.

[ ] Erweiterte Dateiverwaltung: Option zum Verschieben von Dateien (statt nur Kopieren) für eine sauberere Ordnerstruktur.

[ ] Detaillierte Statistiken: Grafische Auswertung der Fortschritte (z. B. BPM-Steigerung über Zeit).

[ ] PDF-Export: Exportfunktion für Journal-Einträge und Setlists.

[ ] Performance-Boost: Weitere SQL-Optimierungen für Bestände weit jenseits der 20.000 Einträge.

### Support / Buy Me a Coffee

SonarPractice ist ein Herzensprojekt, das in unzähligen Stunden nach Feierabend entstanden ist.

Wenn dir das Programm hilft, deine Ziele am Instrument schneller zu erreichen,

freue ich mich über eine kleine Unterstützung:

[Schnell und unkompliziert eine Tasse Kaffee spendieren.](https://buymeacoffee.com/sonarpractice)

[Direkte Unterstützung für die Weiterentwicklung.](paypal.me/SandyMarkoKnauer739)

Vielen Dank für deine Unterstützung!

[README.md (english Version)](README.md)
