#include "sharppy/advbar_widget.hpp"
#include "sharppy/constants.hpp"
#include "sharppy/interp.hpp"
#include "sharppy/winds.hpp"
#include <QPainter>
#include <QFontMetrics>
#include <cmath>
#include <algorithm>

namespace sharppy {

namespace {
// winds.mean_wind: pressure-weighted mean of 1 hPa-spaced interpolated winds over [pbot, ptop]
bool mean_wind_p(const Profile& prof, double pbot, double ptop, double& mu, double& mv) {
    double su = 0, sv = 0, sw = 0;
    for (double p = pbot; p >= ptop - 1e-9; p -= 1.0) {
        double u, v; prof.wind_at_pres(p, u, v);
        if (u == MISSING || v == MISSING) continue;
        su += u * p; sv += v * p; sw += p;
    }
    if (sw <= 0) return false;
    mu = su / sw; mv = sv / sw;
    return true;
}
} // namespace

AdvBarWidget::AdvBarWidget(QWidget* parent) : QWidget(parent) { setMinimumWidth(50); }

void AdvBarWidget::setPreferences(const VizPrefs& p) {
    m_bg = prefColor(p, "bg_color", m_bg);
    m_fg = prefColor(p, "fg_color", m_fg);
    update();
}

// params.inferred_temp_adv(prof, dp=-100, lat=prof.latitude)
void AdvBarWidget::setProfile(const Profile& prof) {
    m_layers.clear();
    m_has = false;
    if (prof.size() < 2 || prof.latitude == MISSING || !std::isfinite(prof.latitude)) { update(); return; }
    if (*std::max_element(prof.pres.begin(), prof.pres.end()) <= 100) { update(); return; }

    const double omega = (2. * M_PI) / 86164.;
    const double f = 2. * omega * std::sin(prof.latitude * M_PI / 180.);
    const double multiplier = (f / 9.81) * (M_PI / 180.);

    double ptopLimit = 0;   // pres[pres >= 100][-1]
    for (double p : prof.pres) if (p >= 100.) ptopLimit = p;
    std::vector<double> ps;
    for (double p = prof.sfc_pres; p > ptopLimit; p -= 100.) ps.push_back(p);
    if (ps.size() < 2) { update(); return; }

    auto dirAt = [&](double p) { double u, v, d, s; prof.wind_at_pres(p, u, v); if (u == MISSING) return (double)MISSING; comp2vec(u, v, d, s); return d; };
    for (size_t i = 1; i < ps.size(); ++i) {
        double pb = ps[i - 1], pt = ps[i];
        double tb = prof.temp_at_pres(pb), tt = prof.temp_at_pres(pt);
        double hb = prof.hght_at_pres(pb), ht = prof.hght_at_pres(pt);
        double db = dirAt(pb), dt = dirAt(pt);
        double mu, mv;
        if (tb == MISSING || tt == MISSING || hb == MISSING || ht == MISSING || db == MISSING || dt == MISSING
            || !mean_wind_p(prof, pb, pt, mu, mv)) {
            m_layers.push_back({pt, pb, std::nan("")}); continue;
        }
        double avg_temp = ((tt + 273.15) + (tb + 273.15)) * 2.;   // advection code multiplies by 2 (as written)
        double d, spd; comp2vec(mu, mv, d, spd);
        double spd_ms = spd * 0.514444;
        double top_wdir = dt + (180. - db);
        if (top_wdir < 0) top_wdir += 360.; else if (top_wdir >= 360.) top_wdir -= 360.;
        double d_theta = top_wdir - 180.;
        double t_adv = multiplier * spd_ms * spd_ms * avg_temp * (d_theta / (ht - hb));
        m_layers.push_back({pt, pb, t_adv * 3600.});
    }
    m_has = !m_layers.empty();
    update();
}

double AdvBarWidget::pres_to_pix(double p) const {
    double scl1 = std::log(m_pmax) - std::log(m_pmin);
    double scl2 = std::log(m_pmax) - std::log(p);
    return bry() - (scl2 / scl1) * bry();
}
double AdvBarWidget::adv_to_pix(double a) const {
    double brx = width();
    double half = brx / 2.;
    double scl1 = m_adv_max - m_adv_min, scl2 = m_adv_max - a;
    double offset = brx - ((scl2 / scl1) * brx);
    return a == 0 ? half : offset / 2 + half;
}

void AdvBarWidget::paintEvent(QPaintEvent*) {
    QPainter qp(this);
    qp.fillRect(rect(), m_bg);
    qp.setRenderHint(QPainter::Antialiasing);
    qp.setRenderHint(QPainter::TextAntialiasing);
    const double brx = width(), by = bry();
    QFont lf("Helvetica"); lf.setPointSize(std::max(1, int(std::lround(width() * 0.12)) + 3));

    qp.setPen(QPen(m_fg, 2));
    qp.drawLine(QPointF(0, 0), QPointF(brx, 0));
    qp.drawLine(QPointF(brx, 0), QPointF(brx, by));
    qp.drawLine(QPointF(brx, by), QPointF(0, by));
    qp.drawLine(QPointF(0, by), QPointF(0, 0));

    qp.setPen(QPen(m_fg, 1, Qt::DashLine));
    qp.drawLine(QPointF(adv_to_pix(0), pres_to_pix(m_pmax)), QPointF(adv_to_pix(0), pres_to_pix(m_pmin)));
    qp.setFont(lf);
    qp.setPen(QPen(m_fg, 1));
    qp.drawText(QRect(2, 2, int(brx) - 2, 40), Qt::AlignLeft | Qt::TextWordWrap, "Inf. Temp. Adv. (C/hr)");

    if (!m_has) return;
    for (const Layer& L : m_layers) {
        if (!std::isfinite(L.val)) continue;
        double pt = pres_to_pix(L.ptop), pb = pres_to_pix(L.pbot), pa = adv_to_pix(L.val);
        const double label_w = 5, box_h = 8;
        double label_loc;
        if (L.val > 0) { qp.setPen(QPen(Qt::red, 1)); label_loc = adv_to_pix(8) - label_w; }
        else if (L.val < 0) { qp.setPen(QPen(QColor("#3399CC"), 1)); label_loc = adv_to_pix(-8); }
        else { qp.setPen(QPen(m_fg, 1)); label_loc = adv_to_pix(8) - label_w; }
        qp.drawText(QRectF(label_loc, (pt + pb) / 2, label_w, box_h), Qt::TextDontClip | Qt::AlignCenter,
                    QString::number(std::round(L.val * 10) / 10, 'f', 1));
        qp.drawLine(QPointF(pa, pt), QPointF(pa, pb));
        qp.drawLine(QPointF(adv_to_pix(0), pb), QPointF(pa, pb));
        qp.drawLine(QPointF(pa, pt), QPointF(adv_to_pix(0), pt));
        qp.drawLine(QPointF(adv_to_pix(0), pt), QPointF(adv_to_pix(0), pb));
    }
}

} // namespace sharppy
