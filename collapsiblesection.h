#ifndef COLLAPSIBLESECTION_H
#define COLLAPSIBLESECTION_H

#include <QGroupBox>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

class CollapsibleSection : public QWidget
{
    Q_OBJECT
public:
    explicit CollapsibleSection(const QString &title,
                                bool collapsible = true,
                                bool expandedByDefault = true,
                                QWidget *parent = nullptr);

    void addContentWidget(QWidget *widget);

    void setTitle(const QString &title);

    void setCollapsed(bool collapsed);
    bool isCollapsed() const;

    bool expandedByDefault{true};

signals:
    void toggled(bool collapsed);

private slots:
    void onToggleButtonClicked();

private:
    QGroupBox *groupBox_m;
    QToolButton *toggleButton_m;
    QVBoxLayout *contentLayout_m;
    bool isCollapsed_m;
    bool isCollapsible_m;
};

#endif // COLLAPSIBLESECTION_H
