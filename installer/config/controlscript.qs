function Controller() {
    gui.contentWidth = 800;
    gui.contentHeight = 700;
}

Controller.prototype.IntroductionPageCallback = function() {
    // Wir prüfen nur beim Installieren oder Updaten, ob die App läuft
    if (installer.isInstaller() || installer.isUpdater()) {
        var isRunning = true;

        while (isRunning) {
            // Wir prüfen, ob der Prozess "SonarPractice.exe" aktiv ist
            // 'tasklist' gibt 0 zurück, wenn der Prozess gefunden wurde
            var result = installer.execute("cmd", ["/C", "tasklist /FI \"IMAGENAME eq SonarPractice.exe\" 2>NUL | find /I /N \"SonarPractice.exe\">NUL"]);
            
            if (result[1] == 0) {
                // Die App läuft noch -> Dialog anzeigen
                gui.critical(
                    "SonarPractice läuft noch", 
                    "SonarPractice ist derzeit geöffnet. Bitte speichern Sie Ihre Arbeit und schließen Sie die Anwendung, um mit der Installation/dem Update fortzufahren."
                );
            } else {
                // App wurde geschlossen, wir können die Schleife verlassen
                isRunning = false;
            }
        }
    }
}