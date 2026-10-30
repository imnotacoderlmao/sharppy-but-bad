#include "sharppy/vrot_widget.hpp"
#include "sharppy/inset_data.hpp"
#include <QPainter>
#include <QFontMetrics>
#include <QInputDialog>
#include <QMouseEvent>
#include <cmath>

namespace sharppy {

VROTWidget::VROTWidget(QWidget* parent) : QWidget(parent) { setMinimumHeight(140); }

void VROTWidget::setPreferences(const VizPrefs& p) {
    m_bg = prefColor(p, "bg_color", m_bg);
    m_fg = prefColor(p, "fg_color", m_fg);
    m_line = prefColor(p, "stp_line_color", m_line);
    update();
}
void VROTWidget::setVrot(double v) { m_vrot = v; m_has = true; update(); }

void VROTWidget::mouseDoubleClickEvent(QMouseEvent*) {
    bool ok = false;
    double v = QInputDialog::getDouble(this, "VROT", "Enter VROT (kts):", m_has ? m_vrot : 0, 0, 500, 0, &ok);
    if (ok) setVrot(v);
}

VROTWidget::Geo VROTWidget::geo() const {
    Geo g;
    g.fs = int(std::lround(height() * 0.0512)) + 2;
    QFont f("Helvetica"); f.setPointSize(std::max(1, g.fs));
    g.plot_h = QFontMetrics(f).xHeight();
    g.tpad = g.plot_h + 15;
    g.bpad = g.plot_h + 2;
    g.wid = width();
    g.hgt = height() - g.bpad;
    g.brx = g.wid;
    g.bry = g.hgt - g.bpad;
    return g;
}
double VROTWidget::prob_to_pix(const Geo& g, double p) const {
    return g.bry - (p / 70.0) * (g.bry - g.tpad);
}
double VROTWidget::vrot_to_pix(const Geo& g, double v) const {
    return g.brx * (v / 110.0);
}
// nearest-xpt lookup like vrot.py interp_vrot
double VROTWidget::interp(const std::array<double, 11>& d, double v) const {
    const auto& x = inset::vrot().xpts;
    int best = 0;
    for (int i = 1; i < 11; ++i)
        if (std::fabs(x[i] - v) < std::fabs(x[best] - v)) best = i;
    return d[best];
}

void VROTWidget::paintEvent(QPaintEvent*) {
    QPainter qp(this);
    qp.fillRect(rect(), m_bg);
    qp.setPen(QPen(QColor("#3399CC"), 1));
    qp.drawRect(0, 0, width() - 1, height() - 1);
    qp.translate(1, 1);
    qp.setRenderHint(QPainter::Antialiasing);
    const Geo g = geo();
    QFont pf("Helvetica"); pf.setPointSize(std::max(1, g.fs));
    while (pf.pointSize() > 5 && QFontMetrics(pf).horizontalAdvance("Conditional Tornado Probs based on VROT") > g.brx - 4)
        pf.setPointSize(pf.pointSize() - 1);
    qp.setFont(pf);
    qp.setPen(QPen(m_fg, 2));
    qp.drawText(QRectF(0, 5, g.brx, g.plot_h), Qt::TextDontClip | Qt::AlignCenter,
                "Conditional Tornado Probs based on VROT");

    const auto& d = inset::vrot();
    int tfs = int(std::lround(0.0512 * g.hgt)) + 1;
    QFont yf("Helvetica"); yf.setPointSize(std::max(1, tfs));
    qp.setFont(yf);
    for (const char* t : d.ytexts) {
        QString s = t;
        if (s.trimmed().isEmpty()) continue;
        double y = prob_to_pix(g, s.toInt());
        qp.setPen(QPen(m_line, 1, Qt::DashLine));
        qp.drawLine(QPointF(0, y), QPointF(g.brx, y));
        qp.setPen(QPen(m_fg, 1));
        qp.drawText(QRect(0, int(y - tfs / 2), 20, tfs), Qt::TextDontClip | Qt::AlignCenter, s);
    }
    QFont sf("Helvetica"); sf.setPointSize(std::max(4, int(std::lround(0.0512 * g.hgt)) - 2));
    qp.setFont(sf);
    for (int v = 10; v <= 100; v += 10)
        qp.drawText(QRectF(vrot_to_pix(g, v) - 15, g.bry + 2, 30, g.bpad),
                    Qt::TextDontClip | Qt::AlignCenter, QString::number(v));

    struct L { const std::array<double, 11>* v; const char* n; QColor c; };
    const L lines[] = {{&d.ef01, "EF0-EF1", QColor("#ffff00")}, {&d.ef23, "EF2-EF3", QColor("#ff9900")},
                       {&d.ef45, "EF4-EF5", QColor("#e700df")}};
    for (const auto& l : lines) {
        qp.setPen(QPen(l.c, 2));
        QPointF prev;
        for (int i = 0; i < 11; ++i) {
            QPointF p(vrot_to_pix(g, d.xpts[i]), prob_to_pix(g, (*l.v)[i]));
            if (i) qp.drawLine(prev, p);
            prev = p;
        }
        qp.drawText(QPointF(vrot_to_pix(g, 45) - 20, prob_to_pix(g, (*l.v)[4]) - 4), l.n);
    }

    if (!m_has) return;
    bool clamped = m_vrot > 70.0;
    double x = vrot_to_pix(g, m_vrot);
    qp.setPen(QPen(QColor("#00ff00"), 1.5, clamped ? Qt::DotLine : Qt::SolidLine));
    qp.drawLine(QPointF(x, g.tpad), QPointF(x, g.bry));
    // probabilities at the marker
    qp.setPen(QPen(m_fg, 1));
    qp.setFont(sf);
    double lx = std::min(x + 4, g.brx - QFontMetrics(sf).horizontalAdvance("EF4-5: 100%") - 2);
    qp.drawText(QPointF(lx, g.tpad + 12), QString("EF0-1: %1%").arg(interp(d.ef01, m_vrot), 0, 'f', 0));
    qp.drawText(QPointF(lx, g.tpad + 12 + QFontMetrics(sf).height()), QString("EF2-3: %1%").arg(interp(d.ef23, m_vrot), 0, 'f', 0));
    qp.drawText(QPointF(lx, g.tpad + 12 + 2 * (QFontMetrics(sf).height())), QString("EF4-5: %1%").arg(interp(d.ef45, m_vrot), 0, 'f', 0));
}

} // namespace sharppy
