#ifndef BASEPAGE_H
#define BASEPAGE_H

#include "setupwizard.h"

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
    [[nodiscard]] QString createHeader(const QString &title);
protected:
    void addHeaderLogo(QVBoxLayout* layout, const QString& title);
    void styleInfoLabel(QLabel* label) const;
    void stylePushButton(QPushButton* button) const;
    void styleRadioButton(QRadioButton* radio) const;

    using CustomFilterCriteria = std::function<bool(QTreeWidgetItem*)>;

    void applyFilterToTree(QTreeWidget* tree,
                           const QString& text,
                           CustomFilterCriteria extraCriteria = nullptr);

    [[nodiscard]] bool filterItemRecursive(QTreeWidgetItem* item,
                                               const QString& text,
                                               CustomFilterCriteria extraCriteria);

    [[nodiscard]] SetupWizard* wiz();
private:
    SetupWizard* wiz_m = nullptr;
};

#endif // BASEPAGE_H
