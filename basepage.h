#ifndef BASEPAGE_H
#define BASEPAGE_H

#include "setupwizard.h"

#include <QObject>

class QTreeWidgetItem;
class QTreeWidget;
class QLabel;
class QRadioButton;;
class SetupWizard;
class QVBoxLayout;

class BasePage : public QWizardPage {
    Q_OBJECT
public:
    explicit BasePage(QWidget *parent = nullptr);
protected:
    void addHeaderLogo(QVBoxLayout* layout, const QString& title);
    void styleInfoLabel(QLabel* label) const;

    using CustomFilterCriteria = std::function<bool(QTreeWidgetItem*)>;

    [[nodiscard]] bool filterItemRecursive(QTreeWidgetItem* item,
                                               const QString& text,
                                               CustomFilterCriteria extraCriteria);

    [[nodiscard]] SetupWizard* wiz();
private:
    SetupWizard* wiz_m = nullptr;
};

#endif // BASEPAGE_H
