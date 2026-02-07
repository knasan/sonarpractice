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

/**
 * @brief Paints the brand label with a pulsing color effect and glow.
 * 
 * This function is called whenever the widget needs to be repainted. It renders
 * the label text with a dynamic color that pulses between a base color and a
 * pulse color. A glow effect is achieved by drawing the text twice: first with
 * a semi-transparent glow layer slightly offset, then with the final color on top.
 * 
 * The glow intensity increases proportionally with the pulse animation value,
 * creating a breathing or pulsing visual effect. Text rendering uses antialiasing
 * for smooth appearance.
 * 
 * @param e The paint event (unused, but required by Qt's paintEvent signature).
 * 
 * @note Relies on member variables:
 *       - base_m: The base color of the text
 *       - pulseColor_m: The target color for the pulse animation
 *       - pulse_m: Pulse animation factor (0.0 to 1.0)
 *       - alignment(): Text alignment setting
 *       - text(): The text content to render
 */
void BrandLabel::paintEvent(QPaintEvent *e) {
    Q_UNUSED(e);
    QPainter p(this);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    // font color pulse
    const QColor c = mix(base_m, pulseColor_m, pulse_m);

    // Optional: "Glow" fake (2x draw, once transparent + minimal offset)
    QColor glow = c;
    glow.setAlphaF(0.22 + 0.25 * pulse_m); // more glow when pulsing
    p.setPen(glow);
    p.drawText(rect().adjusted(0, 1, 0, 0), alignment(), text());

    p.setPen(c);
    p.drawText(rect(), alignment(), text());
}
