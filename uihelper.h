#ifndef UIHELPER_H
#define UIHELPER_H

class QString;
class QWidget;
class QLayout;

class UIHelper
{
public:
    /**
    * @brief Öffnet eine Datei und zeigt bei Fehlern eine Messagebox an.
    * Da die Funktion primär Seiteneffekte hat (UI öffnen), ist hier kein nodiscard nötig.
    */
    static void openFileWithFeedback(QWidget *parent, const QString &fullPath);
    static void clearLayout(QLayout *layout);
};

#endif // UIHELPER_H
