#include "sharppy/thetae_widget.hpp"
#include "sharppy/thermo.hpp"
#include "sharppy/constants.hpp"
#include <QPainter>
#include <cmath>
#include <algorithm>

namespace sharppy {

ThetaEWidget::ThetaEWidget(QWidget* parent) : QWidget(parent) { setMinimumHeight(100); }

void ThetaEWidget::setPreferences(const VizPrefs& p) {
    m_bg = prefColor(p, "bg_color", m_bg);
    m_fg = prefColor(p, "fg_color", m_fg);
    m_thte = prefColor(p, "temp_color", m_thte);
    update();
}

void ThetaEWidget::setProfile(const Profile& prof) {
    m_pres.clear(); m_thetae.clear();
    for (size_t i = 0; i < prof.size(); ++i) {
        double t = prof.temp[i], d = prof.dwpt[i], p = prof.pres[i];
        if (t == MISSING || d == MISSING || p == MISSING) continue;   // masked in Python
        double te = theta_e(p, t, d);          // exact tab::thetae in K (ma.masked_invalid)
        if (!std::isfinite(te)) continue;
        m_pres.push_back(p);
        m_thetae.push_back(te);
    }
    m_has = !m_pres.empty();
    updateRange();
    update();
}

// setProf/resizeEvent: tmin/tmax from theta-e below 400 hPa +/- 10
void ThetaEWidget::updateRange() {
    if (!m_has) return;
    double lo = 1e9, hi = -1e9;
    for (size_t i = 0; i < m_pres.size(); ++i)
        if (m_pres[i] > 400.) { lo = std::min(lo, m_thetae[i]); hi = std::max(hi, m_thetae[i]); }
    if (lo <= hi) { m_tmin = lo - 10.; m_tmax = hi + 10.; }
}
void ThetaEWidget::resizeEvent(QResizeEvent* e) { QWidget::resizeEvent(e); updateRange(); }

double ThetaEWidget::pres_to_pix(double p) const {
    return bry() - ((m_pmax - p) / (m_pmax - m_pmin)) * bry();        // tpad = 0
}
double ThetaEWidget::theta_to_pix(double t) const {
    return bry() - ((m_tmax - t) / (m_tmax - m_tmin)) * bry();        // thetae.py uses bry here too
}

void ThetaEWidget::paintEvent(QPaintEvent*) {
    QPainter qp(this);
    qp.fillRect(rect(), m_bg);
    qp.setRenderHint(QPainter::Antialiasing);
    qp.setRenderHint(QPainter::TextAntialiasing);

    const double brx = width(), by = bry();
    QFont label("Helvetica"); label.setPointSize(std::max(1, int(std::lround(height() * 0.0512))));
    qp.setFont(label);

    // frame
    qp.setPen(QPen(m_fg, 2));
    qp.drawLine(QPointF(0, 0), QPointF(brx, 0));
    qp.drawLine(QPointF(brx, 0), QPointF(brx, by));
    qp.drawLine(QPointF(brx, by), QPointF(0, by));
    qp.drawLine(QPointF(0, by), QPointF(0, 0));
    qp.drawText(QRect(35, 15, 50, 50), Qt::AlignVCenter | Qt::AlignHCenter, "Theta-E\nv.\nPres");

    // isobar ticks
    qp.setPen(QPen(m_fg, 1));
    for (int p : {1000, 900, 800, 700, 600, 500}) {
        double y = pres_to_pix(p);
        qp.drawLine(QPointF(0, y), QPointF(5, y));
        qp.drawLine(QPointF(brx - 5, y), QPointF(brx, y));
        qp.drawText(QRectF(0, y - 20, 20, 40), Qt::AlignVCenter | Qt::AlignRight, QString::number(p));
    }
    // theta-e ticks
    for (int t = 200; t < 400; t += 10) {
        double x = theta_to_pix(t);
        if (x < -15 || x > brx + 15) continue;
        qp.drawLine(QPointF(x, 0), QPointF(x, 5));
        qp.drawLine(QPointF(x, by - 5), QPointF(x, by));
        qp.drawText(QRectF(x, by - 20, 15, 20), Qt::AlignTop | Qt::AlignCenter, QString::number(t));
    }

    if (!m_has) return;
    qp.setPen(QPen(m_thte, 2));
    for (size_t i = 0; i + 1 < m_pres.size(); ++i) {
        if (m_pres[i] > 400.)
            qp.drawLine(QPointF(theta_to_pix(m_thetae[i]), pres_to_pix(m_pres[i])),
                        QPointF(theta_to_pix(m_thetae[i + 1]), pres_to_pix(m_pres[i + 1])));
    }
}

} // namespace sharppy
