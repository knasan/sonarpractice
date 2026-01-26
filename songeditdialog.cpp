#include "songeditdialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QVBoxLayout>

SongEditDialog::SongEditDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle(tr("Song-Metadaten bearbeiten"));
    setMinimumWidth(350);

    auto *mainLayout = new QVBoxLayout(this);
    auto *formLayout = new QFormLayout();

    // Widgets initialisieren
    m_titleEdit = new QLineEdit(this);

    m_artistCombo = new QComboBox(this);
    m_artistCombo->setEditable(true); // Erlaubt neue Namen einzutippen

    m_tuningCombo = new QComboBox(this);
    // TODO sollte auch eine eigene Tabelle in SQL sein.
    m_tuningCombo->addItems({"E-Standard", "Eb-Standard", "Drop D", "Drop C"});

    m_bpmSpin = new QSpinBox(this);
    m_bpmSpin->setRange(20, 300);
    m_bpmSpin->setValue(120);

    m_barsSpin = new QSpinBox(this);
    m_barsSpin->setRange(0, 9999);

    // Zum Formular hinzufügen
    formLayout->addRow(tr("Titel:"), m_titleEdit);
    formLayout->addRow(tr("Künstler:"), m_artistCombo);
    formLayout->addRow(tr("Stimmung:"), m_tuningCombo);
    formLayout->addRow(tr("Tempo (BPM):"), m_bpmSpin);
    formLayout->addRow(tr("Takte gesamt:"), m_barsSpin);

    mainLayout->addLayout(formLayout);

    // Buttons (OK / Abbrechen)
    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);
}

void SongEditDialog::setArtist(const QString &artist, const QStringList &allArtists) {
    m_artistCombo->clear();
    m_artistCombo->addItems(allArtists);
    m_artistCombo->setCurrentText(artist);
}

// Im Konstruktor oder einer init-Methode
void SongEditDialog::setTunings(const QStringList &tunings) {
    m_tuningCombo->clear();
    m_tuningCombo->addItems(tunings);
    m_tuningCombo->setEditable(true); // Erlaubt Custom Tunings direkt einzutippen!
}
