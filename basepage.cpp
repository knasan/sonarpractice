// basepage.cpp
#include "basepage.h"
#include "setupwizard.h"

#include <QWizardPage>
#include <QTreeWidget>
#include <QLabel>
#include <QPushButton>
#include <qlogging.h>
#include <qradiobutton.h>
#include <QTextBrowser>
#include <QLayout>

BasePage::BasePage(QWidget *parent)
    : QWizardPage(parent)
{
}

void BasePage::styleInfoLabel(QLabel* label) const
{
    if (!label)
        return;

    label->setWordWrap(true);
    label->setStyleSheet(
        "font-size: 16px; "
        "color: #aaccff; "       // Ein sanftes Blau-Grau
        "line-height: 140%; "   // Mehr Zeilenabstand für bessere Lesbarkeit "line-height: 1.4;"
        "margin-bottom: 15px;"
    );
}

void BasePage::styleRadioButton(QRadioButton* radio) const {

    if (!radio) return;
    QString radioStyle = R"(
        QRadioButton {
            color: white;
            spacing: 8px;
        }
        /* Der Kreis des RadioButtons */
        QRadioButton::indicator {
            width: 18px;
            height: 18px;
            border-radius: 9px;
            border: 2px solid #555555; /* Grauer Rand im Standby */
            background-color: #2D2D2D;
        }
        /* Hover-Effekt */
        QRadioButton::indicator:hover {
            border-color: #0078D7; /* Blaues Highlight */
        }
        /* Aktivierter Zustand (Der Punkt in der Mitte) */
        QRadioButton::indicator:checked {
            background-color: #0078D7;
            border: 2px solid white;
        }
    )";
    radio->setStyleSheet(radioStyle);
}

void BasePage::stylePushButton(QPushButton* button) const {
    if (!button)
        return;

    button->setStyleSheet(
        "QPushButton {"
        "  background-color: #444; color: white; border-radius: 4px; padding: 5px;"
        "}"
        "QPushButton:disabled {"
        "  background-color: #222;" // Dunklerer Hintergrund
        "  color: #555;"            // Graue, "ausgebleichte" Schrift
        "  border: 1px solid #333;" // Dezenterer Rand
        "}"
    );
}

QString BasePage::createHeader(const QString &title) {
    return QString(
               "<div>"
               "  <table width='100%' cellpadding='0' cellspacing='0'>"
               "    <tr>"
               "      <td>"
               "        <h1 style='color: #3498db; margin: 0; font-family: sans-serif; font-size: 20px;'>Sonar<span style='color: #ffffff;'>Practice</span></h1>"
               "      </td>"
               "      <td align='right'>"
               "        <span style='color: #aaaaaa; font-size: 14px; font-weight: bold; text-transform: uppercase;'>%1</span>"
               "      </td>"
               "    </tr>"
               "  </table>"
               "</div>"
               ).arg(title);
}

void BasePage::applyFilterToTree(QTreeWidget* tree, const QString& text, CustomFilterCriteria extraCriteria) {
    if (!tree) return;
    tree->setUpdatesEnabled(false);

    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        // Wir rufen eine rekursive Hilfsfunktion für jedes Top-Level-Element auf
        if (!filterItemRecursive(tree->topLevelItem(i), text, extraCriteria)) {
            qWarning() << "FilterItemRecursive error";
        }
    }

    tree->setUpdatesEnabled(true);
}

bool BasePage::filterItemRecursive(QTreeWidgetItem* item, const QString& text, CustomFilterCriteria extraCriteria) {
    bool hasVisibleChild = false;

    // 1. Gehe rekursiv durch alle Kinder
    for (int i = 0; i < item->childCount(); ++i) {
        if (filterItemRecursive(item->child(i), text, extraCriteria)) {
            hasVisibleChild = true;
        }
    }

    // 2. Prüfe, ob dieses Item selbst zum Text passt
    bool matchesText = text.isEmpty() || item->text(0).contains(text, Qt::CaseInsensitive);

    // 3. Prüfe Extra-Kriterien (falls vorhanden)
    bool matchesExtra = !extraCriteria || extraCriteria(item);

    // Ein Item ist sichtbar, wenn:
    // (Es den Text matcht UND das Extra-Kriterium erfüllt) ODER es ein sichtbares Kind hat
    bool shouldBeVisible = (matchesText && matchesExtra) || hasVisibleChild;

    item->setHidden(!shouldBeVisible);

    // Wenn ein Item sichtbar ist, klappen wir den Baum automatisch auf,
    // damit der User den Treffer auch sieht
    if (shouldBeVisible && !text.isEmpty() && item->childCount() > 0) {
        item->setExpanded(true);
    }

    return shouldBeVisible;
}

void BasePage::addHeaderLogo(QLayout* layout, const QString& title) {
    if (!layout) return;

    // Der QTextBrowser für den Erklärtext
    QTextBrowser *browser = new QTextBrowser(this);

    // Optisches Tuning (dein Original-Code)
    browser->setFrameStyle(QFrame::NoFrame);
    QPalette p = browser->palette();
    p.setColor(QPalette::Base, Qt::transparent);
    browser->setPalette(p);

    browser->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    browser->setFixedHeight(50);

    // Nutzt die bereits existierende createHeader Funktion der BasePage
    QString headerLogo = createHeader(title);
    browser->setHtml(headerLogo);

    // Ins Layout einfügen
    layout->addWidget(browser);
}

SetupWizard* BasePage::wiz() {
    if (!wiz_m) {
        wiz_m = qobject_cast<SetupWizard*>(wizard());
    }
    return wiz_m;
}
