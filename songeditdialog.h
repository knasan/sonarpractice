#ifndef SONGEDITDIALOG_H
#define SONGEDITDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QStringList>

class SongEditDialog : public QDialog {
    Q_OBJECT

public:
    explicit SongEditDialog(QWidget *parent = nullptr);

    void setSongData(const QString &title,
                     const QString &artist,
                     const QString &tuning,
                     int bpm,
                     const QStringList &allArtists,
                     const QStringList &allTunings);

    [[nodiscard]] QString title() const { return titleEdit_m->text(); }
    [[nodiscard]] QString artist() const { return artistCombo_m->currentText(); }
    [[nodiscard]] QString tuning() const { return tuningCombo_m->currentText(); }
    [[nodiscard]] int bpm() const { return bpmSpin_m->value(); }

private:
    // UI Elements
    QLineEdit* titleEdit_m;
    QComboBox* artistCombo_m;
    QComboBox* tuningCombo_m;
    QSpinBox* bpmSpin_m;
    void setupLayout();
};

#endif // SONGEDITDIALOG_H
