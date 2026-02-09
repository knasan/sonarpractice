#include "collapsiblesection.h"
#include <QLabel>
#include <QStyle>

CollapsibleSection::CollapsibleSection(const QString &title,
                                       bool collapsible,
                                       bool expandedByDefault,
                                       QWidget *parent)
    : QWidget(parent)
    , isCollapsed_m(!expandedByDefault)
    , isCollapsible_m(collapsible)
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Header (Titel + Toggle-Button)
    auto *headerWidget = new QWidget(this);
    auto *headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(0, 0, 0, 0);

    toggleButton_m = new QToolButton(headerWidget);
    toggleButton_m->setArrowType(isCollapsed_m ? Qt::RightArrow : Qt::DownArrow);
    toggleButton_m->setCheckable(true);
    toggleButton_m->setChecked(!isCollapsed_m);

    connect(toggleButton_m, &QToolButton::clicked, this, &CollapsibleSection::onToggleButtonClicked);

    auto *titleLabel = new QLabel(title, headerWidget);
    headerLayout->addWidget(toggleButton_m);
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();

    // GroupBox (Content)
    groupBox_m = new QGroupBox(this);
    groupBox_m->setFlat(true);
    groupBox_m->setVisible(!isCollapsed_m);

    contentLayout_m = new QVBoxLayout(groupBox_m);
    contentLayout_m->setContentsMargins(5, 5, 5, 5);

    // Main layout
    mainLayout->addWidget(headerWidget);
    mainLayout->addWidget(groupBox_m);

    // Non-folding sections hide the toggle button.
    if (!isCollapsible_m) {
        toggleButton_m->setVisible(false);
    }
}

void CollapsibleSection::addContentWidget(QWidget *widget)
{
    contentLayout_m->addWidget(widget);
}

void CollapsibleSection::setTitle(const QString &title)
{
    groupBox_m->setTitle(title);
}

void CollapsibleSection::setCollapsed(bool collapsed)
{
    if (isCollapsed_m == collapsed)
        return;
    isCollapsed_m = collapsed;
    groupBox_m->setVisible(!collapsed);
    toggleButton_m->setArrowType(collapsed ? Qt::ArrowType::RightArrow : Qt::ArrowType::DownArrow);
    emit toggled(collapsed);
}

bool CollapsibleSection::isCollapsed() const
{
    return isCollapsed_m;
}

void CollapsibleSection::onToggleButtonClicked()
{
    setCollapsed(!isCollapsed_m);
}
