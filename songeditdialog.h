#ifndef SONGEDITDIALOG_H
#define SONGEDITDIALOG_H

#include "sonarstructs.h"

#include <QComboBox>
#include <QDialog>
#include <QSpinBox>
#include <QLineEdit>

class SongEditDialog : public QDialog {
    Q_OBJECT
public:
    explicit SongEditDialog(QWidget *parent = nullptr);

    // Methoden zum Datenaustausch
    void setTitle(const QString &title) { m_titleEdit->setText(title); }
    QString getTitle() const { return m_titleEdit->text(); }

    void setArtist(const QString &artist, const QStringList &allArtists);
    QString getArtist() const { return m_artistCombo->currentText(); }

    void setTunings(const QStringList &tunings);

    void setData(const SongData &data, const QStringList &allArtists);
    SongData getData() const;

    // ... weitere Getter/Setter für BPM, Tuning etc.

private:
    QLineEdit *m_titleEdit;
    QComboBox *m_artistCombo;
    QComboBox *m_tuningCombo;
    QSpinBox *m_bpmSpin;
    QSpinBox *m_barsSpin;
};

#endif
