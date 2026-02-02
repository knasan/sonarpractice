#ifndef FILTERPAGE_H
#define FILTERPAGE_H

#include "basepage.h"

class QListWidget;
class QCheckBox;
class QListWidgetItem;
class FilterPage : public BasePage {
    Q_OBJECT
public:
    explicit FilterPage(QWidget *parent = nullptr);
    virtual void initializePage() override;
    int nextId() const override;

    [[nodiscard]] bool isComplete() const override;

    QStringList sourcePaths_m;
    QStringList activeFilters_m;

private slots:
    void addTargetPath();
    void addSourcePath();

    [[nodiscard]] QStringList getActiveFilters();
    [[nodiscard]] bool validatePage() override;

private:
    bool isConnectionsEstablished_m = false;
    void updateTargetPathStyle(bool checked);
    void flashItem(QListWidgetItem *item);
    void removeSourcePath();
    void updateRemoveSourceButtonState();

    QCheckBox *cbManageData_m;
    QCheckBox *cbPdf_m;
    QCheckBox *cbAudio_m;
    QCheckBox *cbVideo_m;
    QCheckBox *cbGuitarPro_m;
    QCheckBox *skipImport_m;

    QLabel *lblTargetPath_m;
    QPushButton *btnSelectTargetPath_m;
    QPushButton *btnRemSource_m;
    QPushButton *btnAddSource_m;

    QListWidget *listWidgetSource_m;
};

#endif // FILTERPAGE_H
