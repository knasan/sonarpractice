#include "reminderdialog.h"

#include <QComboBox>
#include <QDate>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

ReminderDialog::ReminderDialog(QWidget *parent, int startBar, int endBar, int practiceBpm)
    : QDialog(parent)
{
    setWindowTitle(tr("Add Practice Reminder"));
    setMinimumWidth(300);

    auto *mainLayout = new QVBoxLayout(this);
    auto *formLayout = new QFormLayout();

    songDisplayLabel_m = new QLabel(this);

    dialogButtons_m = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    btnOk_m = dialogButtons_m->button(QDialogButtonBox::Ok);
    btnOk_m->setEnabled(false);

    // Felder initialisieren
    startBarSpin_m = new QSpinBox(this);
    startBarSpin_m->setMinimum(1);
    startBarSpin_m->setValue(startBar);

    endBarSpin_m = new QSpinBox(this);
    endBarSpin_m->setMinimum(1);
    endBarSpin_m->setValue(endBar);

    bpmSpin_m = new QSpinBox(this);
    bpmSpin_m->setMinimum(20);
    bpmSpin_m->setMaximum(300);
    bpmSpin_m->setValue(practiceBpm);

    dailyCheck_m = new QCheckBox(tr("Repeat Daily"), this);
    monthlyCheck_m = new QCheckBox(tr("Repeat Monthly"), this);

    weekdayCombo_m = new QComboBox(this);
    weekdayCombo_m->addItems({tr("None"),
                              tr("Monday"),
                              tr("Tuesday"),
                              tr("Wednesday"),
                              tr("Thursday"),
                              tr("Friday"),
                              tr("Saturday"),
                              tr("Sunday")});

    dateEdit_m = new QDateEdit(QDate::currentDate());
    dateEdit_m->setCalendarPopup(true);
    dateEdit_m->setEnabled(true); // Standardmäßig an

    // Formular füllen
    formLayout->addRow(tr("Song:"), songDisplayLabel_m);
    formLayout->addRow(tr("Start Bar:"), startBarSpin_m);
    formLayout->addRow(tr("End Bar:"), endBarSpin_m);
    formLayout->addRow(tr("Target BPM:"), bpmSpin_m);
    formLayout->addRow(dailyCheck_m);
    formLayout->addRow(monthlyCheck_m);
    formLayout->addRow(tr("Or on Weekday:"), weekdayCombo_m);

    // Buttons (OK / Cancel)
    connect(dialogButtons_m, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(dialogButtons_m, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // 2. Connect all relevant widgets to the validation
    connect(dailyCheck_m, &QCheckBox::toggled, this, &ReminderDialog::updateOkButtonState);
    connect(monthlyCheck_m, &QCheckBox::toggled, this, &ReminderDialog::updateOkButtonState);
    connect(weekdayCombo_m,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &ReminderDialog::updateOkButtonState);

    // If the date also counts as a "unique day":
    connect(dateEdit_m, &QDateEdit::dateChanged, this, &ReminderDialog::updateOkButtonState);

    mainLayout->addLayout(formLayout);
    mainLayout->addWidget(dialogButtons_m);

    // Minor validation: Automatically pull the end clock along.
    connect(startBarSpin_m, QOverload<int>::of(&QSpinBox::valueChanged), [this](int val) {
        if (endBarSpin_m->value() < val)
            endBarSpin_m->setValue(val);
    });
}

void ReminderDialog::setTargetSong(int id, const QString &name)
{
    currentSongId_m = id;
    songDisplayLabel_m->setText(name);
}

ReminderDialog::ReminderData ReminderDialog::getResults() const
{
    ReminderDialog::ReminderData res;
    res.songId = currentSongId_m;
    res.startBar = startBarSpin_m->value();
    res.endBar = endBarSpin_m->value();
    res.targetBpm = bpmSpin_m->value();

    res.isDaily = dailyCheck_m->isChecked();
    res.isMonthly = monthlyCheck_m->isChecked();
    res.weekday = (weekdayCombo_m->currentIndex() > 0) ? weekdayCombo_m->currentIndex() : -1;

    if (!res.isDaily && !res.isMonthly && res.weekday == -1) {
        res.reminderDate = dateEdit_m->date().toString("yyyy-MM-dd");
    } else {
        res.reminderDate = "";
    }

    return res;
}

void ReminderDialog::updateOkButtonState()
{
    bool hasDaily = dailyCheck_m->isChecked();
    bool hasMonthly = (monthlyCheck_m && monthlyCheck_m->isChecked());
    bool hasWeekday = (weekdayCombo_m->currentIndex() > 0);

    bool hasInterval = hasDaily || hasMonthly || hasWeekday;

    bool finalValid = false;

    if (hasInterval) {
        finalValid = true;
        dateEdit_m->setEnabled(false);
    } else {
        finalValid = false;
    }

    btnOk_m->setEnabled(finalValid);
}
