#ifndef REMINDERDIALOG_H
#define REMINDERDIALOG_H

#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QSpinBox>

class ReminderDialog : public QDialog {
    Q_OBJECT
public:
    explicit ReminderDialog(QWidget *parent = nullptr, int startBar = 1, int endBar = 1, int practiceBpm = 50);

    struct ReminderData {
        int songId;
        int startBar;
        int endBar;
        int targetBpm;
        bool isDaily;
        bool isWeekly;
        bool isMonthly;
        int weekday; // 0=None, 1=Mon...7=Sun
        QString reminderDate;
    };

    [[nodiscard]] ReminderData getValues() const;
    [[nodiscard]] ReminderData getResults() const;
    void setReminderData(const ReminderData &data);
    void setTargetSong(int id, const QString &name);

private:
    void updateOkButtonState();
    QSpinBox* startBarSpin_m, *endBarSpin_m, *bpmSpin_m;
    QCheckBox* dailyCheck_m;
    QCheckBox* weekleyCheck_m;
    QCheckBox* monthlyCheck_m;
    QComboBox* weekdayCombo_m;
    QDateEdit* dateEdit_m;

    QDialogButtonBox* dialogButtons_m;
    QPushButton* btnOk_m;

    int currentSongId_m;

    QLabel* songDisplayLabel_m;
};

#endif // REMINDERDIALOG_H
