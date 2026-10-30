#include "sharppy/speed_widget.hpp"
#include "sharppy/skewt_widget.hpp"
#include <QPainter>
#include <cmath>
#include <algorithm>

namespace sharppy {

SpeedWidget::SpeedWidget(QWidget* parent) : QWidget(parent) { setMinimumWidth(54); }

void SpeedWidget::setProfile(const Profile& prof) { m_prof = prof; m_has = !prof.empty(); update(); }

void SpeedWidget::setPreferences(const VizPrefs& p) {
    m_bg = prefColor(p, "bg_color", m_bg);
    m_fg = prefColor(p, "fg_color", m_fg);
    if (p.contains("wind_units")) m_units = p.value("wind_units");
    m_c01 = prefColor(p, "0_1_color", m_c01);
    m_c13 = prefColor(p, "1_3_color", m_c13);
    m_c36 = prefColor(p, "3_6_color", m_c36);
    m_c69 = prefColor(p, "6_9_color", m_c69);
    m_c912 = prefColor(p, "9_12_color", m_c912);
    m_isotach = prefColor(p, "spd_itach_color", m_isotach);
    update();
}

double SpeedWidget::pres_to_pix(double p) const {
    double scl1 = std::log(1050.) - std::log(100.), scl2 = std::log(1050.) - std::log(p);
    return bry() - (scl2 / scl1) * bry();
}
double SpeedWidget::speed_to_pix(double s) const {
    double brx = width();
    return brx - ((smax() - s) / smax()) * brx;
}

void SpeedWidget::paintEvent(QPaintEvent*) {
    QPainter qp(this);
    qp.fillRect(rect(), m_bg);
    qp.setRenderHint(QPainter::Antialiasing);
    qp.setRenderHint(QPainter::TextAntialiasing);
    const double brx = width(), by = bry();

    QFont label("Helvetica"); label.setPointSize(std::max(1, int(std::lround(width() * 0.12)) + 2));
    QFont title("Helvetica"); title.setPointSize(std::max(1, int(std::lround(width() * 0.12)) + 1));

    // frame
    qp.setPen(QPen(m_fg, 2));
    qp.drawLine(QPointF(0, 0), QPointF(brx, 0));
    qp.drawLine(QPointF(brx, 0), QPointF(brx, by));
    qp.drawLine(QPointF(brx, by), QPointF(0, by));
    qp.drawLine(QPointF(0, by), QPointF(0, 0));

    // isotachs
    const int d = int(delta());
    for (int s = 0; s < int(smax()); s += d) {
        const bool lab = (s % (d * 2) == 0 && s != 0);
        double x1 = speed_to_pix(s);
        double lx = speed_to_pix(s - d), lw = speed_to_pix(s + d) - speed_to_pix(s - d);
        qp.setPen(QPen(m_isotach, 1, Qt::DashLine));
        qp.setFont(label);
        qp.drawLine(QPointF(x1, by), QPointF(x1, 0));
        if (lab && s > 0) {
            qp.setPen(QPen(m_fg, 1, Qt::DashLine));
            qp.drawText(QRectF(lx, by + 5, lw, 10), Qt::AlignTop | Qt::AlignCenter, QString::number(s));
        }
    }
    qp.setPen(QPen(m_fg, 1, Qt::DashLine));
    qp.setFont(title);
    qp.drawText(QRectF(2, 2, brx, 30), Qt::AlignTop | Qt::AlignLeft | Qt::TextWordWrap,
                "Wind Speed\n(" + m_units + ")");

    if (!m_has) return;

    // y offset between this widget's origin and the Skew-T's (speed.py _skew_vertical_offset)
    double skewOffset = 0;
    if (m_skewt) {
        QWidget* top = window();
        skewOffset = mapTo(top, QPoint(0, 0)).y() - m_skewt->mapTo(top, QPoint(0, 0)).y();
    }
    // A raw sounding has far more levels than pixel rows. Bars less than a pixel apart overlap (same colour), so only the
    // longest of each such run matters: merge them instead of drawing every one.
    bool pend = false; double py = 0, px = 0; const QColor* pc = nullptr;
    auto flush = [&]() { if (pend) { qp.setPen(QPen(*pc, 2)); qp.drawLine(QPointF(0, py), QPointF(px, py)); pend = false; } };
    for (size_t i = 0; i < m_prof.size(); ++i) {
        double u = m_prof.u[i], v = m_prof.v[i], p = m_prof.pres[i], h = m_prof.hght[i];
        if (u == MISSING || v == MISSING || h == MISSING || p == MISSING) continue;
        double spd = std::hypot(u, v);
        if (m_units == "m/s") spd *= 0.514444;
        double hg = m_prof.to_agl(h);
        double x1 = speed_to_pix(spd);
        double y1 = m_skewt ? m_skewt->presToY(p) - skewOffset : pres_to_pix(p);
        const QColor& c = hg < 1000 ? m_c01 : hg < 3000 ? m_c13 : hg < 6000 ? m_c36 : hg < 9000 ? m_c69 : m_c912;
        if (pend && pc == &c && std::fabs(y1 - py) < 1.0) { if (x1 > px) px = x1; continue; }   // same row, same colour: keep the longer bar
        flush();
        if (y1 < -2 || y1 > by + 2) continue;                                                      // off the panel (zoomed Skew-T)
        pend = true; py = y1; px = x1; pc = &c;
    }
    flush();
}

} // namespace sharppy
