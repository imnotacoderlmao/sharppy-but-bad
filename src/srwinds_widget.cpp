#include "sharppy/srwinds_widget.hpp"
#include <QPainter>
#include <cmath>

namespace sharppy {

namespace {
bool bad(double v) { return !std::isfinite(v) || v == MISSING; }
}

SRWindsWidget::SRWindsWidget(QWidget* parent) : QWidget(parent) {
    setMinimumSize(80, 60);
    initUI();
}

void SRWindsWidget::initUI() {
    m_brx = width() - m_rpad;
    m_bry = height() - m_bpad;
    m_labelFont = QFont("Helvetica");
    m_labelFont.setPointSize(std::max(1, (int)std::lround(height() * 0.0512)));
}

void SRWindsWidget::resizeEvent(QResizeEvent*) { initUI(); }

void SRWindsWidget::setPreferences(const VizPrefs& p) {
    m_bg = prefColor(p, "bg_color", m_bg);
    m_fg = prefColor(p, "fg_color", m_fg);
    m_clsc = prefColor(p, "srw_clsc_color", m_clsc);
    m_trace = prefColor(p, "srw_trace_color", m_trace);
    m_c02 = prefColor(p, "srw_0_2_color", m_c02);
    m_c46 = prefColor(p, "srw_4_6_color", m_c46);
    m_c911 = prefColor(p, "srw_9_11_color", m_c911);
    update();
}

void SRWindsWidget::setProfile(const Profile& prof, const BunkersMotion& srwind) {
    m_prof = prof; m_srwind = srwind; m_hasData = !prof.empty();
    compute();
    update();
}

void SRWindsWidget::setDeviant(bool use_left) {
    m_useLeft = use_left;
    if (m_hasData) compute();
    update();
}

void SRWindsWidget::compute() {
    const double su = m_useLeft ? m_srwind.left_mover.u : m_srwind.right_mover.u;
    const double sv = m_useLeft ? m_srwind.left_mover.v : m_srwind.right_mover.v;
    auto presAgl = [&](double h) { return m_prof.pres_at_hght(m_prof.to_msl(h)); };
    auto srSpd = [&](double hb, double ht) {
        double pb = (hb == 0.0 && !m_prof.pres.empty()) ? m_prof.pres[0] : presAgl(hb), pt = presAgl(ht);
        if (bad(pb) || bad(pt)) return (double)MISSING;
        WindVector w = mean_wind_pres(m_prof, pb, pt, true);
        return std::hypot(w.u - su, w.v - sv);
    };
    m_srw02 = srSpd(0, 2000);
    m_srw46 = srSpd(4000, 6000);
    m_srw911 = srSpd(9000, 11000);
}

double SRWindsWidget::hgtToPix(double h) const {
    double scl1 = m_hmax - m_hmin, scl2 = m_hmin + h;
    return (m_bry - 2) - (scl2 / scl1) * (m_bry - m_tpad);
}
double SRWindsWidget::speedToPix(double s) const {
    double scl1 = m_smax - m_smin, scl2 = m_smax - s;
    return m_bry - (scl2 / scl1) * (m_bry - m_rpad);     // sic: Python scales x by bry
}

void SRWindsWidget::drawFrame(QPainter& qp) {
    qp.setPen(QPen(m_fg, 2, Qt::SolidLine));
    qp.setFont(m_labelFont);
    qp.drawText(QRect(15, 5, 45, 35), Qt::TextDontClip | Qt::AlignVCenter | Qt::AlignHCenter, "SR Wind\nv.\nHeight");
    const int brx = (int)m_brx, bry = (int)m_bry, tlx = m_rpad, tly = m_tpad;
    qp.drawLine(tlx, tly, brx, tly);
    qp.drawLine(brx, tly, brx, bry);
    qp.drawLine(brx, bry, tlx, bry);
    qp.drawLine(tlx, bry, tlx, tly);
    qp.setPen(QPen(m_fg, 1, Qt::DashLine));
    int zero = (int)speedToPix(15.);
    qp.drawLine(zero, bry, zero, tly);
    int lower = (int)hgtToPix(8.), upper = (int)hgtToPix(16.);
    int c1 = (int)speedToPix(40.), c2 = (int)speedToPix(70.);
    qp.setPen(QPen(m_clsc, 1, Qt::DashLine));
    qp.drawLine(c1, lower, c1, upper);
    qp.drawLine(c2, lower, c2, upper);
    qp.drawText(QRect(c1 - 5, 2, 50, 50), Qt::TextDontClip | Qt::AlignVCenter | Qt::AlignHCenter, "Classic\nSupercell");
}

void SRWindsWidget::drawHeight(int h, QPainter& qp) {
    qp.setPen(QPen(m_fg, 1, Qt::SolidLine));
    qp.setFont(m_labelFont);
    int y1 = (int)hgtToPix(h);
    const int off = 5;
    qp.drawLine(m_lpad, y1, m_lpad + off, y1);
    qp.drawLine((int)m_brx + m_rpad - off, y1, (int)m_brx + m_rpad, y1);
    qp.drawText(QRect(0, y1 - 20, 20, 40), Qt::AlignVCenter | Qt::AlignRight, QString::number(h));
}

void SRWindsWidget::drawSpeed(int s, QPainter& qp) {
    qp.setPen(QPen(m_fg, 1, Qt::SolidLine));
    int x1 = (int)speedToPix(s);
    const int off = 5;
    qp.drawLine(x1, 0, x1, off);
    qp.drawLine(x1, (int)m_bry + m_tpad - off, x1, (int)m_bry + m_rpad);
}

void SRWindsWidget::drawProfile(QPainter& qp) {
    const Profile& P = m_prof;
    if (P.empty() || P.u.size() != P.pres.size()) return;
    const double su = m_useLeft ? m_srwind.left_mover.u : m_srwind.right_mover.u;
    const double sv = m_useLeft ? m_srwind.left_mover.v : m_srwind.right_mover.v;

    std::vector<double> hgt, sru, srv;
    for (size_t i = 0; i < P.size(); ++i) {
        if (bad(P.u[i]) || bad(P.v[i]) || bad(P.hght[i])) continue;
        hgt.push_back(P.hght[i]); sru.push_back(P.u[i] - su); srv.push_back(P.v[i] - sv);
    }
    if (hgt.empty()) return;

    // np.arange(hght[sfc], min(hgt top, hmax*1000), 10)
    const double start = P.hght[0], stop = std::min(hgt.back(), m_hmax * 1000.);
    long n = (long)std::ceil((stop - start) / 10.0);
    if (n < 2) n = 0;
    auto interp = [&](double x, const std::vector<double>& y) {      // np.interp (clamped)
        if (x <= hgt.front()) return y.front();
        if (x >= hgt.back()) return y.back();
        size_t j = std::upper_bound(hgt.begin(), hgt.end(), x) - hgt.begin();
        double dx = hgt[j] - hgt[j - 1];
        return dx == 0 ? y[j] : y[j - 1] + (y[j] - y[j - 1]) * (x - hgt[j - 1]) / dx;
    };
    std::vector<double> ih(n), spd(n);
    for (long i = 0; i < n; ++i) {
        ih[i] = start + 10.0 * i;
        spd[i] = std::hypot(interp(ih[i], sru), interp(ih[i], srv));
    }
    qp.setPen(QPen(m_trace, 1, Qt::SolidLine));
    for (long i = 0; i + 1 < n; ++i) {
        if (bad(spd[i]) || bad(spd[i + 1])) continue;
        double h1 = (ih[i] - ih[0]) / 1000, h2 = (ih[i + 1] - ih[0]) / 1000;
        qp.drawLine((int)speedToPix(spd[i]), (int)hgtToPix(h1), (int)speedToPix(spd[i + 1]), (int)hgtToPix(h2));
    }

    auto bar = [&](double s, const QColor& c, double h0, double h1) {
        if (bad(s)) return;
        qp.setPen(QPen(c, 2, Qt::SolidLine));
        int x = (int)speedToPix(s);
        qp.drawLine(x, (int)hgtToPix(h0), x, (int)hgtToPix(h1));
    };
    bar(m_srw02, m_c02, 0.0, 2.0);
    bar(m_srw46, m_c46, 4.0, 6.0);
    bar(m_srw911, m_c911, 9.0, 11.0);
}

void SRWindsWidget::paintEvent(QPaintEvent*) {
    QPainter qp(this);
    qp.fillRect(rect(), m_bg);
    qp.setRenderHint(QPainter::Antialiasing);
    qp.setRenderHint(QPainter::TextAntialiasing);
    drawFrame(qp);
    for (int h : {2, 4, 6, 8, 10, 12, 14}) drawHeight(h, qp);
    for (int s = 0; s < 100; s += 10) drawSpeed(s, qp);
    if (m_hasData) drawProfile(qp);
}

} // namespace sharppy
