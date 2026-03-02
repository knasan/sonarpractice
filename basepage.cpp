// basepage.cpp
#include "basepage.h"
#include "brandlabel.h"
#include "setupwizard.h"

#include <QWizardPage>
#include <QTreeWidget>
#include <QLabel>
#include <QPushButton>
#include <qlogging.h>
#include <qradiobutton.h>
#include <QTextBrowser>
#include <QLayout>
#include <QPropertyAnimation>

BasePage::BasePage(QWidget *parent)
    : QWizardPage(parent)
{
}

/**
 * @brief Applies styling to a QLabel widget for info display.
 * 
 * Configures the provided label with text wrapping and a consistent visual style
 * suitable for displaying information. The label is styled with a soft blue-grey
 * color, increased line spacing for readability, and bottom margin for spacing.
 * 
 * @param label Pointer to the QLabel widget to style. If nullptr, the function
 *              returns without performing any operations.
 * 
 * @note The label's properties are modified in-place. The function applies the
 *       following styling: font-size (16px), color (soft blue-grey #aaccff),
 *       line-height (140%), and margin-bottom (15px).
 * 
 * @see QLabel::setWordWrap()
 * @see QLabel::setStyleSheet()
 */
void BasePage::styleInfoLabel(QLabel* label) const
{
    if (!label)
        return;

    label->setWordWrap(true);
    label->setStyleSheet(
        "font-size: 16px; "
        "color: #aaccff; "       // A soft blue-grey
        "line-height: 140%; "   // Increased line spacing for better readability "line-height: 1.4;"
        "margin-bottom: 15px;"
    );
}

/**
 * @brief Recursively filters tree widget items based on text and custom criteria.
 *
 * This function traverses the tree structure recursively, filtering items based on a text search
 * and optional custom filter criteria. Items are made visible if they match the search text and
 * criteria, or if any of their descendants match. Parent items of matching children are
 * automatically shown and expanded to reveal the matches.
 *
 * @param item The current tree widget item to process and its subtree.
 * @param text The search text to match against item text. If empty, text filtering is disabled.
 * @param extraCriteria Optional custom filter function that returns true if the item meets
 *                      additional criteria. If nullptr or evaluates to false, the item is hidden
 *                      (unless it has visible children).
 *
 * @return true if the item or any of its descendants should be visible after filtering;
 *         false if the item and all its descendants should be hidden.
 *
 * @note The function modifies the hidden state and expansion state of items in the tree.
 *       Non-empty text matches automatically expand parent items to show matching children.
 */
bool BasePage::filterItemRecursive(QTreeWidgetItem* item, const QString& text, CustomFilterCriteria extraCriteria) {
    bool hasVisibleChild = false;

    // Go through all children recursively
    for (int i = 0; i < item->childCount(); ++i) {
        if (filterItemRecursive(item->child(i), text, extraCriteria)) {
            hasVisibleChild = true;
        }
    }

    // Check if this item itself fits the text.
    bool matchesText = text.isEmpty() || item->text(0).contains(text, Qt::CaseInsensitive);

    // Check extra criteria (if any)
    bool matchesExtra = !extraCriteria || extraCriteria(item);

    // An item is visible if: (It matches the text AND meets the extra criterion) OR it has a visible child.
    bool shouldBeVisible = (matchesText && matchesExtra) || hasVisibleChild;

    item->setHidden(!shouldBeVisible);

    // When an item is visible, we automatically expand the tree so that the user can see the match.
    if (shouldBeVisible && !text.isEmpty() && item->childCount() > 0) {
        item->setExpanded(true);
    }

    return shouldBeVisible;
}

/**
 * @brief Adds a header logo section to the specified layout with animated branding elements.
 *
 * Creates a horizontal layout containing two branded labels ("Sonar" and "Practice"),
 * a slogan/title label, and applies a continuous pulsing animation effect to the
 * branded labels. The animation cycles with a 10-second duration, creating a
 * breathing effect using a sine easing curve.
 *
 * @param layout Pointer to the QVBoxLayout where the header will be added.
 *               If nullptr, the function returns without performing any action.
 * @param title The text to display as the slogan/subtitle in the header.
 *
 * @note The animation runs indefinitely (loop count of -1) and is owned by this object.
 * @note The layout margins and spacing are set to 0 for a compact appearance.
 *
 * @see BrandLabel
 * @see QVariantAnimation
 */
void BasePage::addHeaderLogo(QVBoxLayout* layout, const QString& title)
{
    if (!layout) return;

    auto *row = new QHBoxLayout;
    row->setContentsMargins(0,0,0,0);
    row->setSpacing(0);

    auto *sonar = new BrandLabel(this);
    sonar->setObjectName("brandSonar");
    sonar->setText("Sonar");
    sonar->setMargin(0);

    auto *practice = new BrandLabel(this);
    practice->setObjectName("brandPractice");
    practice->setText("Practice");
    practice->setMargin(0);

    row->addWidget(sonar);
    row->addWidget(practice);
    row->addStretch(1);

    auto *slogan = new QLabel(title, this);
    slogan->setObjectName("brandSlogan");
    row->addWidget(slogan);

    auto *a = new QVariantAnimation(this);
    a->setDuration(10000);
    a->setLoopCount(-1);
    a->setStartValue(0.0);
    a->setEndValue(1.0);
    a->setKeyValueAt(0.0, 0.0);
    a->setKeyValueAt(0.5, 1.0);
    a->setKeyValueAt(1.0, 0.0);
    a->setEasingCurve(QEasingCurve::InOutSine);

    connect(a, &QVariantAnimation::valueChanged, this, [=](const QVariant &v){
        const qreal t = v.toReal();
        sonar->setPulse(t);
        practice->setPulse(t);
    });

    a->start();

    layout->addLayout(row);
}

SetupWizard* BasePage::wiz() {
    if (!wiz_m) {
        wiz_m = qobject_cast<SetupWizard*>(wizard());
    }
    return wiz_m;
}
