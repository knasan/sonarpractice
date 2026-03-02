function Component() {
    // constructor [cite: 183]
    component.loaded.connect(this, Component.prototype.loaded);
    if (!installer.addWizardPage(component, "Page", QInstaller.TargetDirectory))
        console.log("Could not add the dynamic page.");
}

Component.prototype.isDefault = function() {
    return true;
}

Component.prototype.loaded = function () {
    var page = gui.pageByObjectName("DynamicPage");
    if (page != null) {
        page.entered.connect(Component.prototype.dynamicPageEntered);
    }
}

Component.prototype.dynamicPageEntered = function () {
    var pageWidget = gui.pageWidgetByObjectName("DynamicPage");
    if (pageWidget != null) {
        var currentVersion = component.value("Version");
        
       var content = "<div style='font-family: sans-serif; line-height: 1.4;'>" +
                "<h2 style='color: #2c3e50; margin-bottom: 5px;'>SonarPractice " + currentVersion + "</h2>" +
                "<p><b>Vielen Dank</b>, dass du SonarPractice nutzt!</p>" +

                "<div style='background-color: #e3f2fd; border-left: 5px solid #2196f3; padding: 10px; margin: 15px 0;'>" +
                    "<b>Hilfe & Dokumentation:</b><br>" +
                    "Du suchst Anleitungen? Besuche unsere Doku unter:<br>" +
                    "<a href='https://knasan.github.io/sonarpractice/docs/' style='color: #1976d2;'>knasan.github.io/sonarpractice/docs/</a>" +
                "</div>" +

                "<div style='background-color: #e8f5e9; border-left: 5px solid #4caf50; padding: 10px; margin: 15px 0;'>" +
                    "<b>Update-System aktiv:</b> Zukünftige Aktualisierungen können direkt über das " +
                    "MaintenanceTool im Installationsverzeichnis geladen werden." +
                "</div>" +

                "<p><b>Projekt auf GitHub:</b> <a href='https://github.com/knasan/sonarpractice' style='color: #2980b9;'>github.com/knasan/sonarpractice</a></p>" +

                "<div style='margin-top: 10px; padding: 5px; border-top: 1px solid #eee;'>" +
                    "<p>Dir gefällt das Projekt?</p>" +
                    "<a href='https://buymeacoffee.com/sonarpractice' " +
                       "style='background-color: #FFDD00; color: #000000; padding: 5px 10px; " +
                             "text-decoration: none; border-radius: 5px; font-weight: bold;'>" +
                        "☕ Buy Me A Coffee" +
                    "</a>" +
                "</div>" +
            "</div>";

        pageWidget.m_pageLabel.textFormat = 1;
        pageWidget.m_pageLabel.text = content;
        gui.pageByObjectName("DynamicPage").title = "SonarPractice: Informationen";
    }
}

Component.prototype.createOperations = function() {
    // Ruft die Standard-Operationen auf [cite: 203]
    component.createOperations();

    if (systemInfo.productType === "windows") {
        // Erstellt die Verknüpfungen [cite: 204, 205]
        component.addOperation("CreateShortcut",
                               "@TargetDir@/SonarPractice.exe",
                               "@DesktopDir@/SonarPractice.lnk");
    }
}