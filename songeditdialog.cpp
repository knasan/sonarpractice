#include "songeditdialog.h"

#include <QFormLayout>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>

SongEditDialog::SongEditDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle(tr("Edit song metadata"));
    setMinimumWidth(400);

    setupLayout();
}

void SongEditDialog::setupLayout() {
    auto *mainLayout = new QVBoxLayout(this);
    auto *formLayout = new QFormLayout();

    titleEdit_m = new QLineEdit(this);

    artistCombo_m = new QComboBox(this);
    artistCombo_m->setEditable(true);
    artistCombo_m->setInsertPolicy(QComboBox::NoInsert);

    tuningCombo_m = new QComboBox(this);
    tuningCombo_m->setEditable(true);
    tuningCombo_m->setInsertPolicy(QComboBox::NoInsert);

    bpmSpin_m = new QSpinBox(this);
    bpmSpin_m->setRange(0, 300);

    formLayout->addRow(tr("Titel:"), titleEdit_m);
    formLayout->addRow(tr("Artist:"), artistCombo_m);
    formLayout->addRow(tr("Tuning:"), tuningCombo_m);
    formLayout->addRow(tr("BPM:"), bpmSpin_m);

    mainLayout->addLayout(formLayout);

    // Buttons
    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);
}

void SongEditDialog::setSongData(const QString &title,
                                 const QString &artist,
                                 const QString &tuning,
                                 int bpm,
                                 const QStringList &allArtists,
                                 const QStringList &allTunings)
{
    artistCombo_m->clear();
    artistCombo_m->addItems(allArtists);

    tuningCombo_m->clear();
    tuningCombo_m->addItems(allTunings);

    titleEdit_m->setText(title);
    artistCombo_m->setEditText(artist);
    tuningCombo_m->setEditText(tuning);
    bpmSpin_m->setValue(bpm);
}
