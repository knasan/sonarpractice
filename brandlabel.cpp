#include "brandlabel.h"

#include <QPainter>
#include <algorithm>

static QColor mix(const QColor &a, const QColor &b, qreal t) {
    t = std::clamp(t, 0.0, 1.0);
    return QColor::fromRgbF(
        a.redF()   * (1.0 - t) + b.redF()   * t,
        a.greenF() * (1.0 - t) + b.greenF() * t,
        a.blueF()  * (1.0 - t) + b.blueF()  * t,
        a.alphaF() * (1.0 - t) + b.alphaF() * t
        );
}

BrandLabel::BrandLabel(QWidget *parent) : QLabel(parent) {
    setAttribute(Qt::WA_TranslucentBackground);
}

void BrandLabel::paintEvent(QPaintEvent *e) {
    Q_UNUSED(e);
    QPainter p(this);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    // Textfarbe pulst
    const QColor c = mix(base_m, pulseColor_m, pulse_m);

    // Optional: "Glow" fake (2x zeichnen, einmal transparent + minimal versetzt)
    QColor glow = c;
    glow.setAlphaF(0.22 + 0.25 * pulse_m);   // mehr Glow bei mehr Pulse
    p.setPen(glow);
    p.drawText(rect().adjusted(0, 1, 0, 0), alignment(), text());

    p.setPen(c);
    p.drawText(rect(), alignment(), text());
}
