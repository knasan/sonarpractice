#ifndef BRANDLABEL_H
#define BRANDLABEL_H

#include <QLabel>

class BrandLabel : public QLabel {
    Q_OBJECT
    Q_PROPERTY(QColor baseColor  READ baseColor  WRITE setBaseColor)
    Q_PROPERTY(QColor pulseColor READ pulseColor WRITE setPulseColor)
    Q_PROPERTY(qreal  pulse      READ pulse      WRITE setPulse)

public:
    explicit BrandLabel(QWidget *parent = nullptr);

    QColor baseColor()  const { return base_m; }
    QColor pulseColor() const { return pulseColor_m; }
    qreal  pulse()      const { return pulse_m; }

    void setBaseColor(const QColor &c)  { base_m = c; update(); }
    void setPulseColor(const QColor &c) { pulseColor_m = c; update(); }
    void setPulse(qreal v)              { pulse_m = v; update(); }

protected:
    void paintEvent(QPaintEvent *e) override;

private:
    QColor base_m       = QColor("#2FA8FF");
    QColor pulseColor_m = QColor("#7CFFEC");
    qreal  pulse_m      = 0.0; // 0..1
};


#endif // BRANDLABEL_H
