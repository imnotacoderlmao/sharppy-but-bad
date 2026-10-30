#include "sharppy/slinky_widget.hpp"
#include "sharppy/utils_fmt.hpp"
#include "sharppy/tab.hpp"
#include <QPainter>
#include <QFontMetrics>
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace sharppy {

namespace {
constexpr double KTS2MS = 0.514444;
bool bad(double v) { return !std::isfinite(v) || v == MISSING; }
QString i2s(double v) { return int2str(v); }
}

SlinkyWidget::SlinkyWidget(QWidget* parent) : QWidget(parent) {
    setMinimumSize(120, 130);
    initUI();
}

void SlinkyWidget::initUI() {
    m_wid = width() - m_rpad;
    m_hgt = height() - m_bpad;
    m_centerx = m_wid / 2; m_centery = m_hgt / 2;
    m_scale = (m_wid - m_rpad) / m_mag;
    const int ps = std::max(1, (int)std::lround(height() * 0.0512));
    m_titleFont = QFont("Helvetica"); m_titleFont.setPointSize(ps + 2);
    m_plotFont = QFont("Helvetica"); m_plotFont.setPointSize(ps);
    m_titleHeight = QFontMetrics(m_titleFont).xHeight() + m_fpad;
}

void SlinkyWidget::resizeEvent(QResizeEvent*) { initUI(); }

void SlinkyWidget::setPreferences(const VizPrefs& p) {
    m_bg = prefColor(p, "bg_color", m_bg);
    m_fg = prefColor(p, "fg_color", m_fg);
    m_c01 = prefColor(p, "0_1_color", m_c01);
    m_c13 = prefColor(p, "1_3_color", m_c13);
    m_c36 = prefColor(p, "3_6_color", m_c36);
    m_c69 = prefColor(p, "6_9_color", m_c69);
    m_c912 = prefColor(p, "9_12_color", m_c912);
    update();
}

void SlinkyWidget::setProfile(const Profile& prof, const ParcelResult& pcl, const BunkersMotion& srwind) {
    m_prof = prof; m_pcl = pcl; m_srwind = srwind;
    m_hasData = !prof.empty();
    computeTrajectory();
    update();
}

void SlinkyWidget::setDeviant(bool use_left) {
    m_useLeft = use_left;
    if (m_hasData) computeTrajectory();
    update();
}

QPointF SlinkyWidget::xyToPix(double x, double y) const {
    return QPointF(m_centerx + x * m_scale, m_centery - y * m_scale);
}

// params.parcelTraj
void SlinkyWidget::computeTrajectory() {
    m_traj.clear(); m_hasTraj = false; m_hasEl = false; m_tilt = std::nan("");
    if (!m_hasData || bad(m_pcl.start_p) || bad(m_pcl.start_t) || bad(m_pcl.start_dp) || m_prof.size() < 2) return;

    const double smu = m_useLeft ? m_srwind.left_mover.u : m_srwind.right_mover.u;
    const double smv = m_useLeft ? m_srwind.left_mover.v : m_srwind.right_mover.v;

    const auto env_hold_ = tab::env_for(m_prof); const tab::Env& env = *env_hold_;
    tab::Parcel pcl = tab::parcelx(env, m_pcl.start_p, m_pcl.start_t, m_pcl.start_dp);
    if (!(pcl.bplus >= 1e-3)) return;                       // no positively buoyant area -> masked / NaN tilt
    m_hasEl = std::isfinite(pcl.elhght);
    double elhght = pcl.elhght;
    if (!std::isfinite(elhght)) elhght = m_prof.hght.back();

    // log10(p) ascending parcel trace (ptrace is stored descending)
    std::vector<double> lp, tt;
    for (size_t i = pcl.ptrace.size(); i-- > 0;) {
        if (!std::isfinite(pcl.ptrace[i]) || !std::isfinite(pcl.ttrace[i]) || pcl.ptrace[i] <= 0) continue;
        lp.push_back(std::log10(pcl.ptrace[i])); tt.push_back(pcl.ttrace[i]);
    }
    if (lp.size() < 2) return;
    auto pclT = [&](double p) {
        double x = std::log10(p);
        if (x < lp.front() || x > lp.back()) return std::nan("");
        size_t j = std::upper_bound(lp.begin(), lp.end(), x) - lp.begin();
        if (j == 0) return tt.front();
        if (j >= lp.size()) return tt.back();
        double dx = lp[j] - lp[j - 1];
        return dx == 0 ? tt[j] : tt[j - 1] + (tt[j] - tt[j - 1]) * (x - lp[j - 1]) / dx;
    };

    const double g = 9.8, dt = 25.0;
    double x0 = 0, y0 = 0, z0 = pcl.lfchght, p0 = pcl.lfcpres, t0 = 0, w0 = 5.0;
    m_traj.push_back({x0, y0, z0});

    for (int guard = 0; z0 < elhght && guard < 4000; ++guard) {
        double t1 = dt + t0;
        double envT = env.vtmp_at(p0) + 273.15;
        double pt = pclT(p0) + 273.15;
        if (!std::isfinite(envT) || !std::isfinite(pt)) break;
        double accel = g * ((pt - envT) / envT);
        double z1 = 0.5 * accel * (t1 - t0) * (t1 - t0) + w0 * (t1 - t0) + z0;
        double w1 = accel * (t1 - t0) + w0;
        double u, v; m_prof.wind_at_pres(p0, u, v);
        if (bad(u) || bad(v)) break;
        double u0 = KTS2MS * (u - smu), v0 = KTS2MS * (v - smv);
        double x1 = u0 * (t1 - t0) + x0, y1 = v0 * (t1 - t0) + y0;
        m_traj.push_back({x1, y1, z1});
        z0 = z1; y0 = y1; x0 = x1; t0 = t1; w0 = w1;
        p0 = m_prof.pres_at_hght(m_prof.to_msl(z1));
        if (bad(p0)) break;
    }
    const SlinkyPoint& last = m_traj.back();
    m_tilt = std::atan2(last.z, std::hypot(last.x, last.y)) * 180.0 / M_PI;
    m_hasTraj = true;
}

void SlinkyWidget::paintEvent(QPaintEvent*) {
    QPainter qp(this);
    qp.fillRect(rect(), m_bg);
    qp.setRenderHint(QPainter::Antialiasing);
    qp.setRenderHint(QPainter::TextAntialiasing);

    // axes, then frame (backgroundSlinky.plotBackground)
    const int tlx = m_rpad, tly = m_tpad, brx = (int)m_wid, bry = (int)m_hgt;
    qp.setPen(QPen(QColor("#003366"), 2, Qt::SolidLine));
    qp.drawLine((int)m_centerx, tly, (int)m_centerx, bry);
    qp.drawLine(tlx, (int)m_centery, brx, (int)m_centery);

    qp.setPen(QPen(m_fg, 2, Qt::SolidLine));
    qp.setFont(m_titleFont);
    qp.drawLine(tlx, tly, brx, tly);
    qp.drawLine(brx, tly, brx, bry);
    qp.drawLine(brx, bry, tlx, bry);
    qp.drawLine(tlx, bry, tlx, tly);
    int yval = (int)(bry - m_titleHeight - 2);
    qp.drawText(QRect(m_lpad, yval, 20, (int)m_titleHeight), Qt::TextDontClip | Qt::AlignLeft, "Storm Slinky");

    if (!m_hasData) return;

    // plotSlinky
    if (m_hasTraj) {
        const double zlast = m_traj.back().z;
        for (const SlinkyPoint& t : m_traj) {
            if (bad(t.x) || bad(t.y)) continue;
            QColor c;
            if (m_hasEl && t.z == zlast) c = QColor("#0000FF");
            else if (t.z < 1000) c = m_c01;
            else if (t.z < 3000) c = m_c13;
            else if (t.z < 6000) c = m_c36;
            else if (t.z < 9000) c = m_c69;
            else if (t.z < 12000) c = m_c912;
            else continue;
            qp.setPen(QPen(c, 1, Qt::SolidLine));
            qp.drawEllipse(xyToPix(t.x, t.y), 5, 5);
        }
    }

    // plotSMV
    const double smu = m_useLeft ? m_srwind.left_mover.u : m_srwind.right_mover.u;
    const double smv = m_useLeft ? m_srwind.left_mover.v : m_srwind.right_mover.v;
    qp.setPen(QPen(m_fg, 2, Qt::SolidLine));
    if (!bad(smu) && !bad(smv)) {
        double wdir, wspd; comp2vec(smu, smv, wdir, wspd);
        double r = wdir * M_PI / 180.0;
        double u = -3000.0 * std::sin(r), v = -3000.0 * std::cos(r);     // vec2comp(wdir, 3000)
        qp.drawLine(xyToPix(u, v), xyToPix(0, 0));
    }

    // plotTilt
    qp.setPen(QPen(m_fg, 1, Qt::SolidLine));
    qp.setFont(m_titleFont);
    qp.drawText(QRect(brx - 30, tly + 2, 30, (int)m_titleHeight), Qt::TextDontClip | Qt::AlignRight,
                i2s(m_tilt) + " deg  ");
}

} // namespace sharppy
