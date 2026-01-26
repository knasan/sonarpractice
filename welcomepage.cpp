#include "welcomepage.h"

#include <QTextBrowser>
#include <QVBoxLayout>
#include <qtextbrowser.h>

WelcomePage::WelcomePage(QWidget *parent) : BasePage(parent) {
    setTitle(tr("Willkommen im SonarPractice"));
    setSubTitle(tr("von einem Musiker für Musiker"));

    auto *browser = new QTextBrowser(this);

    auto *layout = new QVBoxLayout(this);
    addHeaderLogo(layout, tr("Einleitung"));

    // Dein Text mit tr() und HTML-Formatierung
    QString welcomeText = tr(
        "<style>p, li { line-height: 140%; }</style><h1>Willkommen bei SonarPractice</h1>"
        "<style>"
        "  p { margin-bottom: 15px; line-height: 140%; }"
        "  ul { margin-bottom: 15px; }"
        "  li { margin-bottom: 8px; }"
        "</style>"

        "<p><b>Die Vision hinter dem Projekt</b><br>"
        "SonarPractice entstand aus einem ganz persönlichen Bedürfnis: Als Gitarrist suchte ich nach einem Weg, "
        "meinen Lernfortschritt präzise festzuhalten. Zettelwirtschaft ist unübersichtlich, und Tabellenkalkulationen "
        "stießen schnell an ihre Grenzen. Die Lösung war eine lokale Datenbank – sicher, privat und ohne Registrierungszwang.</p>"

        "<p><b>Mehr als nur ein Journal</b><br>"
        "Während der Entwicklung wurde mir klar, dass Musiker oft vor einem weiteren Problem stehen: "
        "Über Jahrzehnte sammeln sich Tausende von Dateien an – Guitar Pro-Tabs, PDFs, MP3s und Kurs-Videos. "
        "Oft sind diese doppelt vorhanden oder durch technische Fehler beschädigt (0-Byte-Dateien).</p>"

        "<h3>Was SonarPractice für dich tut:</h3>"
        "<ul>"
        "<li><b>Tracking & Journaling:</b> Halte deine Übungserfolge und täglichen Fortschritte am Instrument fest.</li>"
        "<li><b>Kurs-Management:</b> Organisiere unterschiedliche Dateiformate (GP, PDF, Audio, Video) an einem zentralen Ort.</li>"
        "<li><b>System-Hygiene:</b> Finde defekte Dateien und bereinige Dubletten mit intelligenter Hash-Analyse, "
        "damit deine Sammlung sauber bleibt.</li>"
        "</ul>"

        "<p><i>SonarPractice ist von einem Musiker für Musiker geschrieben – für ein strukturiertes Üben und ein aufgeräumtes System.</i></p>"
        );

    QString nextSteps = tr(
        "<h3>So geht es weiter:</h3>"
        "<p>Auf der nächsten Seite konfigurieren wir gemeinsam dein System:</p>"
        "<ul>"
        "  <li><b>Datenverwaltung:</b> Du entscheidest, ob SonarPractice deine Dateien aktiv verwalten soll, "
        "      um Ordnung und Konsistenz in deiner Sammlung zu garantieren.</li>"
        "  <li><b>Einmaliger Suchlauf:</b> Du legst fest, in welchen Verzeichnissen wir einmalig nach deinen "
        "      vorhandenen Kursen, Tabs und Medien suchen dürfen, um deine Datenbank initial zu füllen.</li>"
        "</ul>"
        "<p>Keine Sorge: Du behältst jederzeit die volle Kontrolle darüber, welche Dateien importiert werden.</p>"
        );

    browser->setHtml(welcomeText + nextSteps);

    layout->addWidget(browser);
}
