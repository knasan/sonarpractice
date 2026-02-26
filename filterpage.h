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
    int nextId() const override;

    [[nodiscard]] bool isComplete() const override;

    QStringList sourcePaths_m;
    QStringList activeFilters_m;

private slots:
    void onSettingsChanged();
    void addTargetPath();
    void addSourcePath();

private:
    void setupLayout();
    void setupConnections();
    [[nodiscard]] bool handleSkipImport();

    [[nodiscard]] QStringList getActiveFilters();
    [[nodiscard]] bool validatePage() override;

    void updateTargetPathStyle(bool checked);
    void flashItem(QListWidgetItem *item);
    void removeSourcePath();
    void updateRemoveSourceButtonState();

    QCheckBox* cbManageData_m{nullptr};
    QCheckBox* cbDoc_m{nullptr};
    QCheckBox* cbAudio_m{nullptr};
    QCheckBox* cbVideo_m{nullptr};
    QCheckBox* cbGuitarPro_m{nullptr};
    QCheckBox* cbSkipImport_m{nullptr};
    QCheckBox* cbMoveFiles_m{nullptr};

    QLabel* lblTargetPath_m{nullptr};
    QPushButton* btnSelectTargetPath_m{nullptr};
    QPushButton* btnRemSource_m{nullptr};
    QPushButton* btnAddSource_m{nullptr};

    QListWidget* listWidgetSource_m{nullptr};
};

#endif // FILTERPAGE_H
