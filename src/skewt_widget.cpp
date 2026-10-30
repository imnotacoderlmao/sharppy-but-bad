#include "sharppy/skewt_widget.hpp"
#include "sharppy/utils_fmt.hpp"
#include "sharppy/modify.hpp"
#include "sharppy/barbs.hpp"
#include "sharppy/constants.hpp"
#include <QFontMetrics>
#include <QPainterPath>
#include <QInputDialog>
#include <QActionGroup>
#include <algorithm>
#include <cmath>

namespace sharppy {

static const double NaNv = std::nan("");
static inline bool isn(double v) { return v != v; }
static QString i2s(double v) { return int2str(v); }                  // utils.INT2STR
static QString f2s(double v, int r) { return float2str(v, r); }      // utils.FLOAT2STR
static inline double m2ft(double m) { return m * 3.2808399; }

SkewTWidget::SkewTWidget(QWidget* parent) : CanvasBase(parent) {
    setMinimumSize(500, 500);
    setMouseTracking(true);
    initUI();
}

// ===================================================================== backgroundSkewT
void SkewTWidget::initUI() {
    m_lpad = 30; m_rpad = 65; m_tpad = 25; m_bpad = 20;
    m_tlx = m_rpad; m_tly = m_tpad;
    m_wid = width() - m_rpad; m_hgt = height() - m_bpad;
    m_brx = m_wid; m_bry = m_hgt;
    m_pmax = 1050.; m_pmin = 100.;
    m_barbx = m_brx + m_rpad / 2;
    m_logPmax = std::log(m_pmax); m_logPmin = std::log(m_pmin);
    m_bltmpc = -50; m_brtmpc = 50; m_dt = 10;
    m_xskew = 100. / 3.;
    m_xrange = m_brtmpc - m_bltmpc;
    m_yrange = std::tan(m_xskew * M_PI / 180.0) * m_xrange;
    m_clip = QRectF(QPointF(m_lpad, m_tly), QPointF(m_brx + m_rpad, m_bry));
    double fsize, fsizet;
    if (physicalDpiX() > 75) { fsize = 6; fsizet = 10; } else { fsize = 7; fsizet = 14; }
    auto mk = [](double pt) { QFont f("Helvetica"); f.setPointSizeF(pt); return f; };
    m_titleFont = mk(fsizet + m_hgt * 0.006);
    m_titleHeight = QFontMetrics(m_titleFont).xHeight() + 15 + m_hgt * 0.003;
    m_labelFont = mk(fsize + 2 + m_hgt * 0.0045);
    m_envTraceFont = mk(11 + m_hgt * 0.0045);
    m_inPlotFont = mk(fsize + m_hgt * 0.0045);
    m_esrhFont = mk(fsize + 2 + m_hgt * 0.0045);
    m_hghtFont = mk(9 + m_hgt * 0.0045);
    m_esrhHeight = QFontMetrics(m_esrhFont).xHeight() + 9 + m_hgt * 0.0045;
    m_bgDirty = true;
}

void SkewTWidget::resizeEvent(QResizeEvent* e) { QWidget::resizeEvent(e); initUI(); if (m_readout) updateReadout(); }

double SkewTWidget::presToPix(double p) const {
    double scl1 = m_logPmax - m_logPmin, scl2 = m_logPmax - std::log(p);
    return m_bry - (scl2 / scl1) * (m_bry - m_tpad);
}
double SkewTWidget::pixToPres(double y) const {
    double scl1 = std::log(m_pmax) - std::log(m_pmin), scl2 = m_bry - y, scl3 = m_bry - m_tly + 1;
    return m_pmax / std::exp((scl2 / scl3) * scl1);
}
double SkewTWidget::tmpcToPix(double t, double p) const {
    double scl1 = m_brtmpc - (((m_bry - presToPix(p)) / (m_bry - m_tpad)) * m_yrange);
    return m_brx - (((scl1 - t) / m_xrange) * (m_brx - m_lpad));
}
double SkewTWidget::pixToTmpc(double x, double y) const {
    double scl1 = m_brtmpc - (((m_bry - y) / (m_bry - m_tpad)) * m_yrange);
    return scl1 - (((m_brx - x) / (m_brx - m_lpad)) * m_xrange);
}

void SkewTWidget::drawDryAdiabat(QPainter& qp, double theta) {
    qp.setClipping(true);
    qp.setPen(QPen(m_adiab, 1, Qt::SolidLine));
    QPainterPath path; bool first = true;
    for (int p = (int)m_pmax; p >= (int)m_pmin; p -= 10) {
        double th = ((theta + ZEROCNK) / std::pow(1000. / p, ROCP)) - ZEROCNK;
        double x = m_originx + tmpcToPix(th, p) / m_scale, y = m_originy + presToPix(p) / m_scale;
        if (first) { path.moveTo(x, y); first = false; } else path.lineTo(x, y);
    }
    qp.setBrush(Qt::NoBrush);
    qp.drawPath(path);
}

void SkewTWidget::drawMixingRatio(QPainter& qp, double w, double pmin) {
    qp.setClipping(true);
    double t = tab::temp_at_mixrat(w, m_pmax);
    double x1 = m_originx + tmpcToPix(t, m_pmax) / m_scale, y1 = m_originy + presToPix(m_pmax) / m_scale;
    t = tab::temp_at_mixrat(w, pmin);
    double x2 = m_originx + tmpcToPix(t, pmin) / m_scale, y2 = m_originy + presToPix(pmin) / m_scale;
    QRectF rectF(x2 - 5, y2 - 10, 10, 10);
    qp.setPen(QPen(m_bg, 1)); qp.setBrush(m_bg);
    qp.drawRect(rectF);
    qp.setPen(QPen(m_mixr, 1)); qp.setFont(m_inPlotFont);
    qp.drawLine(QPointF(x1, y1), QPointF(x2, y2));
    qp.drawText(rectF, Qt::AlignBottom | Qt::AlignCenter, i2s(w));
}

void SkewTWidget::drawFrame(QPainter& qp) {
    qp.setClipping(false);
    qp.setPen(QPen(m_bg, 0)); qp.setBrush(m_bg);
    qp.drawRect(QRectF(0, 0, m_lpad, m_bry));
    qp.drawRect(QRectF(0, presToPix(m_pmax), m_brx, m_bry));
    qp.drawRect(QRectF(m_brx, 0, m_wid + m_rpad, presToPix(m_pmax)));
    qp.setPen(QPen(m_fg, 2));
    qp.drawLine(QPointF(m_lpad, m_tpad), QPointF(m_brx + m_rpad, m_tpad));
    qp.drawLine(QPointF(m_brx + m_rpad, m_tpad), QPointF(m_brx + m_rpad, m_bry));
    qp.drawLine(QPointF(m_brx + m_rpad, m_bry), QPointF(m_lpad, m_bry));
    qp.drawLine(QPointF(m_lpad, m_bry), QPointF(m_lpad, m_tpad));
}

void SkewTWidget::drawIsothermLabel(QPainter& qp, double t) {
    qp.setPen(QPen(m_fg));
    QFont f = m_labelFont; f.setBold(true); qp.setFont(f);
    double x1 = m_originx + tmpcToPix(t, m_pmax) / m_scale;
    if (x1 >= m_lpad && x1 <= m_wid) {
        qp.setClipping(false);
        qp.drawText(QRectF(x1 - 10, m_bry + 2, 20, 20), Qt::AlignTop | Qt::AlignCenter, i2s(t));
    }
}

void SkewTWidget::drawIsotherm(QPainter& qp, double t) {
    qp.setClipping(true);
    double x1 = m_originx + tmpcToPix(t, m_pmax) / m_scale, x2 = m_originx + tmpcToPix(t, m_pmin) / m_scale;
    double y1 = m_originy + m_bry / m_scale, y2 = m_originy + m_tpad / m_scale;
    QPen pen((int)t == 0 || (int)t == -20 ? m_isothermHgz : m_isotherm, 1);
    pen.setStyle(Qt::CustomDashLine); pen.setDashPattern({4, 2});
    qp.setPen(pen);
    qp.drawLine(QPointF(x1, y1), QPointF(x2, y2));
}

void SkewTWidget::drawIsobar(QPainter& qp, double p, bool flag) {
    qp.setPen(QPen(m_fg, 1));
    QFont f = m_labelFont; f.setBold(true); qp.setFont(f);
    double y1 = m_originy + presToPix(p) / m_scale;
    if (y1 >= m_tpad && y1 <= m_hgt) {
        const double offset = 5;
        if (flag) {
            qp.drawLine(QPointF(m_lpad, y1), QPointF(m_brx, y1));
            qp.drawText(QRectF(1, y1 - 20, m_lpad - 4, 40), Qt::AlignVCenter | Qt::AlignRight, i2s(p));
        } else {
            qp.drawLine(QPointF(m_lpad, y1), QPointF(m_lpad + offset, y1));
            qp.drawLine(QPointF(m_brx + m_rpad - offset, y1), QPointF(m_brx + m_rpad, y1));
        }
    }
}

void SkewTWidget::drawBackground(QPainter& qp) {
    qp.fillRect(rect(), m_bg);
    qp.setClipRect(m_clip);
    qp.setRenderHint(QPainter::Antialiasing);
    qp.setRenderHint(QPainter::TextAntialiasing);
    for (double t = m_bltmpc - 100; t < m_brtmpc + m_dt; t += m_dt) drawIsotherm(qp, t);
    for (double th = m_bltmpc; th < 80; th += 20) drawDryAdiabat(qp, th);
    drawMixingRatio(qp, 2, 600);
    for (double w = 4; w < 33; w += 4) drawMixingRatio(qp, w, 600);
    drawFrame(qp);
    for (double p : {1000., 850., 700., 500., 300., 200., 100.}) drawIsobar(qp, p, true);
    for (double t = m_bltmpc; t < m_brtmpc + m_dt; t += m_dt) drawIsothermLabel(qp, t);
    for (int p = (int)m_pmax; p > (int)(m_pmin - 50); p -= 50) drawIsobar(qp, p, false);
}

void SkewTWidget::wheelEvent(QWheelEvent* e) {
    const double cx = e->pos().x(), cy = e->pos().y();
    const double max_speed = 100.;
    double delta = std::max(std::min(-(double)e->angleDelta().y(), max_speed), -max_speed);
    double scale_fac = std::pow(10.0, delta / 1000.);
    if (m_scale * scale_fac > 1.0) scale_fac = 1. / m_scale;
    m_scale *= scale_fac;
    m_originx = cx - (cx - m_originx) / scale_fac;
    m_originy = cy - (cy - m_originy) / scale_fac;
    double ll_x = m_originx + width() / m_scale, ll_y = m_originy + height() / m_scale;
    if (ll_x < width()) m_originx += (width() - ll_x); else if (m_originx > 0) m_originx = 0;
    if (ll_y < height()) m_originy += (height() - ll_y); else if (m_originy > 0) m_originy = 0;
    m_bgDirty = true;
    if (m_readout) updateReadout();
    update();
    emit viewChanged(m_scale, m_originy);
    emitZoom();
}

void SkewTWidget::emitZoom() {
    emit zoomChanged(pixToPres((m_bry - m_originy) * m_scale), pixToPres((m_tpad - m_originy) * m_scale));
}

void SkewTWidget::resetZoom() {
    m_scale = 1.0; m_originx = m_originy = 0; m_bgDirty = true;
    update(); emit viewChanged(m_scale, m_originy); emitZoom();
}

void SkewTWidget::setPreferences(const VizPrefs& p) {
    m_bg = prefColor(p, "bg_color", m_bg); m_fg = prefColor(p, "fg_color", m_fg);
    m_isotherm = prefColor(p, "skew_itherm_color", m_isotherm); m_isothermHgz = prefColor(p, "skew_itherm_hgz_color", m_isothermHgz);
    m_adiab = prefColor(p, "skew_adiab_color", m_adiab); m_mixr = prefColor(p, "skew_mixr_color", m_mixr);
    m_temp = prefColor(p, "temp_color", m_temp); m_dewp = prefColor(p, "dewp_color", m_dewp);
    m_wetbulbC = prefColor(p, "wetb_color", m_wetbulbC); m_effLayer = prefColor(p, "eff_inflow_color", m_effLayer);
    m_hgtC = prefColor(p, "skew_hgt_color", m_hgtC); m_dgzC = prefColor(p, "skew_dgz_color", m_dgzC);
    m_lclMkr = prefColor(p, "skew_lcl_mkr_color", m_lclMkr); m_lfcMkr = prefColor(p, "skew_lfc_mkr_color", m_lfcMkr);
    m_elMkr = prefColor(p, "skew_el_mkr_color", m_elMkr);
    auto s = [&](const char* k, QString& out) { auto it = p.find(k); if (it != p.end()) out = it.value(); };
    s("temp_units", m_sfcUnits); s("wind_units", m_windUnits);
    s("readout_tr", m_readoutVars[0]); s("readout_br", m_readoutVars[1]);
    m_bgDirty = true; update();
}

// ===================================================================== data
double SkewTWidget::interpLevel(const std::vector<double>& vals, double p) const {
    // interp.generic on log10(p): ascending-log-p arrays, NaN outside / masked
    if (!m_env) return NaNv;
    const size_t n = m_pres.size();
    std::vector<double> lp, v;
    for (long i = (long)n - 1; i >= 0; --i)
        if (!isn(m_pres[i]) && !isn(vals[i]) && m_pres[i] > 0) { lp.push_back(std::log10(m_pres[i])); v.push_back(vals[i]); }
    double q = std::log10(p);
    if (lp.empty() || q < lp.front() || q > lp.back()) return NaNv;
    size_t lo = 0, hi = lp.size() - 1;
    while (hi - lo > 1) { size_t m = (lo + hi) / 2; if (lp[m] <= q) lo = m; else hi = m; }
    if (lp[hi] == lp[lo]) return v[lo];
    return v[lo] + (v[hi] - v[lo]) * (q - lp[lo]) / (lp[hi] - lp[lo]);
}

void SkewTWidget::rebuildDerived() {
    const size_t n = m_prof.size();
    m_env = std::make_shared<tab::Env>(m_prof);
    const tab::Env& e = *m_env;
    m_pres = e.pres; m_tmpc = e.tmpc; m_dwpc = e.dwpc; m_vtmp = e.vtmp;
    m_wetbulb.assign(n, NaNv); m_thetae.assign(n, NaNv); m_theta.assign(n, NaNv); m_wvmr.assign(n, NaNv);
    for (size_t i = 0; i < n; ++i) {
        if (isn(m_pres[i]) || isn(m_tmpc[i])) continue;
        m_theta[i] = tab::theta(m_pres[i], m_tmpc[i]) + ZEROCNK;
        if (isn(m_dwpc[i])) continue;
        m_wetbulb[i] = tab::wetbulb(m_pres[i], m_tmpc[i], m_dwpc[i]);
        m_thetae[i] = tab::thetae(m_pres[i], m_tmpc[i], m_dwpc[i]) + ZEROCNK;
        m_wvmr[i] = tab::mixratio(m_pres[i], m_dwpc[i]);
    }
    // max lapse rate (params.max_lapse_rate: 2-6 km, 250 m interval, 2 km depth)
    m_mlrVal = m_mlrBot = m_mlrTop = NaNv;
    {
        double best = -1e300; bool any = false;
        for (double lo = 2000; lo < 6000 - 2000 + 250; lo += 250) {
            double pb = e.pres_at_hght(e.to_msl(lo)), pt = e.pres_at_hght(e.to_msl(lo + 2000));
            double lr = (e.vtmp_at(pt) - e.vtmp_at(pb)) * -1000.;
            if (isn(lr)) continue;
            if (!any || lr > best) { best = lr; any = true; m_mlrBot = pb; m_mlrTop = pt; }
        }
        if (any) m_mlrVal = best / 2000.;
    }
    // DGZ / WBZ / FRZ
    {
        double pb = tab::temp_lvl(e, -12), pt = tab::temp_lvl(e, -17);
        m_dgzBot = isn(pb) ? e.sfc_pres : pb; m_dgzTop = isn(pt) ? e.sfc_pres : pt;
        m_frzP = tab::temp_lvl(e, 0);
        tab::Env wb = e; wb.tmpc = m_wetbulb;
        m_wbzP = tab::temp_lvl(wb, 0);
    }
    // PBL top (params.pbl_top)
    {
        std::vector<double> tv(n, NaNv);
        for (size_t i = 0; i < n; ++i)
            if (!isn(m_pres[i]) && !isn(m_tmpc[i]) && !isn(m_dwpc[i]))
                tv[i] = tab::theta(m_pres[i], tab::virtemp(m_pres[i], m_tmpc[i], m_dwpc[i]));
        long lvl = (long)n - 1;
        for (size_t i = 0; i < n; ++i) if (!isn(tv[i]) && tv[e.sfc] + .5 < tv[i]) { lvl = (long)i; break; }
        m_pblTop = n ? m_pres[lvl] : NaNv;
    }
    // downdraft parcel trace
    m_dpP.clear(); m_dpT.clear();
    tab::dcape(e, &m_dpP, &m_dpT);
}

void SkewTWidget::setProfile(const Profile& prof, const ParcelResult& pcl) {
    m_prof = prof; m_hasData = !prof.empty();
    m_havePcl = false;
    if (m_hasData) {
        rebuildDerived();
        if (pcl.start_p != MISSING && pcl.start_t != MISSING && pcl.start_dp != MISSING) {
            m_pcl = tab::parcelx(*m_env, pcl.start_p, pcl.start_t, pcl.start_dp);
            m_havePcl = true;
        }
    }
    if (m_readout) updateReadout();
    update();
}

void SkewTWidget::setKinematics(const BunkersMotion& b, const InflowLayer& i) { m_bunkers = b; m_inflow = i; update(); }

// ===================================================================== plotting
void SkewTWidget::drawTitles(QPainter& qp) {
    qp.setClipping(false);
    qp.setFont(m_titleFont);
    qp.setPen(QPen(m_fg, 1));
    qp.drawText(QRectF(m_lpad, 2, 150, m_titleHeight), Qt::TextDontClip | Qt::AlignLeft, m_title);
}

void SkewTWidget::drawHeight(QPainter& qp, double h) {
    qp.setClipping(true);
    qp.setPen(QPen(m_hgtC, 1)); qp.setFont(m_hghtFont);
    const double offset = 10, txt = 15;
    double sfc = m_env->hght_at(m_env->sfc_pres);
    double p1 = m_env->pres_at_hght(h + sfc);
    if (!isn(p1)) {
        double y1 = m_originy + presToPix(p1) / m_scale;
        qp.drawLine(QPointF(m_lpad, y1), QPointF(m_lpad + offset, y1));
        qp.drawText(QRectF(m_lpad + txt, y1 - 20, m_lpad + txt, 40), Qt::AlignVCenter | Qt::AlignLeft | Qt::TextDontClip,
                    i2s(h / 1000) + " km");
    }
}

void SkewTWidget::drawSigLevels(QPainter& qp, double plevel, const QColor& color, const QString& var_id) {
    qp.setClipping(true);
    if (isn(plevel)) return;
    double z = m2ft(m_env->hght_at(plevel));
    double xa = tmpcToPix(37, 1000.), xb = tmpcToPix(45, 1000.);
    double y = m_originy + presToPix(plevel) / m_scale;
    qp.setPen(QPen(color, 1));
    qp.drawLine(QPointF(xa, y), QPointF(xb, y));
    double l0 = tmpcToPix(20, 1000), l1 = tmpcToPix(36, 1000);
    qp.drawText(QRectF(l0, y - 3, l1 - l0, 4), Qt::TextDontClip | Qt::AlignRight, var_id + i2s(z) + "'");
}

void SkewTWidget::drawPblLevel(QPainter& qp) {
    qp.setClipping(true);
    double xa = tmpcToPix(37, 1000.), xb = tmpcToPix(41, 1000.);
    if (isn(m_pblTop)) return;
    double y = m_originy + presToPix(m_pblTop) / m_scale;
    qp.setPen(QPen(Qt::gray, 2));
    qp.drawLine(QPointF(xa, y), QPointF(xb, y));
    qp.drawText(QRectF(xa, y + 6, xb - xa, 4), Qt::TextDontClip | Qt::AlignCenter, "PBL");
}

void SkewTWidget::drawParcelLevels(QPainter& qp) {
    if (!m_havePcl) return;
    qp.setClipping(true);
    double xa = tmpcToPix(37, 1000.), xb = tmpcToPix(41, 1000.);
    qp.setFont(m_hghtFont);
    auto mark = [&](double p, const QColor& c, double dy, const char* s) {
        double y = m_originy + presToPix(p) / m_scale;
        qp.setPen(QPen(c, 2));
        qp.drawLine(QPointF(xa, y), QPointF(xb, y));
        qp.drawText(QRectF(xa, y + dy, xb - xa, 4), Qt::TextDontClip | Qt::AlignCenter, s);
    };
    const double lclp = m_pcl.lclpres, lfcp = m_pcl.lfcpres, elp = m_pcl.elpres, mplp = m_pcl.mplpres;
    if (!isn(lclp)) mark(lclp, m_lclMkr, 6, "LCL");
    if (!isn(lfcp)) mark(lfcp, m_lfcMkr, -8, "LFC");
    if (!isn(elp) && elp != lclp) mark(elp, m_elMkr, -8, "EL");
    if (!isn(mplp) && mplp != lclp) mark(mplp, m_mplMkr, -8, "MPL");
}

void SkewTWidget::drawTempLevels(QPainter& qp) {
    if (!m_havePcl) return;
    double xa = tmpcToPix(37, 1000.), xb = tmpcToPix(41, 1000.);
    struct L { double p, h; const char* t; } lv[3] = {{m_pcl.p0c, m_pcl.hght0c, "0 C"}, {m_pcl.pm20c, m_pcl.hghtm20c, "-20 C"}, {m_pcl.pm30c, m_pcl.hghtm30c, "-30 C"}};
    qp.setClipping(true);
    for (const L& l : lv) {
        if (isn(l.p) || isn(l.h)) continue;
        double y = m_originy + presToPix(l.p) / m_scale;
        qp.setPen(QPen(m_sigTemp, 2));
        qp.drawLine(QPointF(xa, y), QPointF(xb, y));
        qp.drawText(QRectF(xa, y - 12, xb - xa, 4), Qt::TextDontClip | Qt::AlignLeft, QString(l.t) + "=" + i2s(m2ft(l.h)) + "'");
    }
}

void SkewTWidget::drawMaxLapseRateLayer(QPainter& qp, double bound) {
    qp.setClipping(true);
    if (isn(m_mlrTop) || isn(m_mlrBot) || !(m_mlrVal >= bound)) return;
    const double ll = 10;
    double x1 = tmpcToPix(m_env->vtmp_at(m_mlrBot) + 5, m_mlrBot);
    double y1 = m_originy + presToPix(m_mlrBot) / m_scale, y2 = m_originy + presToPix(m_mlrTop) / m_scale;
    QRectF r3(x1 - 15, y2 - m_esrhHeight, 50, m_esrhHeight);
    qp.setPen(QPen(m_bg, 0)); qp.setBrush(m_bg); qp.drawRect(r3);
    QColor c = m_mlrVal >= 8 ? m_alert[5] : m_mlrVal >= 7 ? m_alert[4] : m_mlrVal >= 6 ? m_alert[1] : m_alert[0];
    qp.setPen(QPen(c, 1.5)); qp.setFont(m_esrhFont);
    qp.drawLine(QPointF(x1 - ll, y1), QPointF(x1 + ll, y1));
    qp.drawLine(QPointF(x1 - ll, y2), QPointF(x1 + ll, y2));
    qp.drawLine(QPointF(x1, y1), QPointF(x1, y2));
    qp.setClipping(false);
    qp.drawText(r3, Qt::TextDontClip | Qt::AlignLeft, f2s(m_mlrVal, 1) + " C/km");
}

void SkewTWidget::drawEffectiveLayer(QPainter& qp) {
    qp.setClipping(true);
    const double ptop = m_inflow.top_p, pbot = m_inflow.bot_p;
    if (ptop == MISSING || pbot == MISSING) return;
    const double len = 15;
    double x1 = tmpcToPix(-20, 1000), x2 = tmpcToPix(-33, 1000);
    double y1 = m_originy + presToPix(pbot) / m_scale, y2 = m_originy + presToPix(ptop) / m_scale;
    QRectF r1(x2, y1 + 4, 25, m_esrhHeight), r2(x2, y2 - m_esrhHeight, 50, m_esrhHeight), r3(x1 - 15, y2 - m_esrhHeight, 50, m_esrhHeight);
    qp.setPen(QPen(m_bg, 0)); qp.setBrush(m_bg);
    double sfc = m_env->hght_at(m_env->sfc_pres);
    QString tb = (m_env->sfc_pres == pbot) ? QString("SFC") : i2s(m_env->hght_at(pbot) - sfc) + "m";
    QString tt = i2s(m_env->hght_at(ptop) - sfc) + "m";
    qp.drawRect(r1); qp.drawRect(r2); qp.drawRect(r3);
    qp.setPen(QPen(m_effLayer, 2)); qp.setFont(m_esrhFont);
    qp.drawLine(QPointF(x1 - len, y1), QPointF(x1 + len, y1));
    qp.drawLine(QPointF(x1 - len, y2), QPointF(x1 + len, y2));
    qp.drawLine(QPointF(x1, y1), QPointF(x1, y2));
    qp.setClipping(false);
    qp.drawText(r1, Qt::TextDontClip | Qt::AlignLeft, tb);
    qp.setClipping(true);
    qp.drawText(r2, Qt::TextDontClip | Qt::AlignLeft, tt);
    qp.drawText(r3, Qt::TextDontClip | Qt::AlignLeft, i2s(m_inflow.esrh) + " m2s2");
}

void SkewTWidget::drawVirtualParcelTrace(QPainter& qp, const std::vector<double>& t, const std::vector<double>& p, const QColor& color) {
    qp.setClipping(true);
    qp.setPen(QPen(color, 1, Qt::DashLine)); qp.setBrush(Qt::NoBrush);
    QPainterPath path; bool first = true;
    for (size_t i = 0; i < p.size() && i < t.size(); ++i) {
        if (isn(p[i]) || isn(t[i])) continue;
        double x = m_originx + tmpcToPix(t[i], p[i]) / m_scale, y = m_originy + presToPix(p[i]) / m_scale;
        if (first) { path.moveTo(x, y); first = false; } else path.lineTo(x, y);
    }
    if (!first) qp.drawPath(path);
}

void SkewTWidget::drawTrace(QPainter& qp, const std::vector<double>& data, const std::vector<double>& pres, const QColor& color,
                            double width, Qt::PenStyle style, bool label) {
    qp.setClipping(true);
    qp.setPen(QPen(color, width, style)); qp.setBrush(Qt::NoBrush);
    // Raw soundings carry >1000 levels, far more than there are pixels. Work in screen space, drop vertices that fall
    // within half a pixel of the last kept one (invisible), and, when zoomed in, vertices that are off-screen
    // (keeping the neighbour on each side so the line still enters/leaves the view correctly).
    const size_t n = std::min(data.size(), pres.size());
    std::vector<double> xs, ys, ds;
    xs.reserve(n); ys.reserve(n); ds.reserve(n);
    const double top = m_clip.top() - 8, bot = m_clip.bottom() + 8;
    const double inv = 1.0 / m_scale;
    const double ysc = (m_bry - m_tpad) / (m_logPmax - m_logPmin);
    double lastx = 0, lasty = 0;
    long pendingIdx = -1;                      // last vertex skipped for being off-screen (candidate "entering" neighbour)
    double pend_x = 0, pend_y = 0, pend_d = 0;
    bool prevOn = false;
    bool haveFirst = false; double fx = 0, fy = 0, fd = 0;      // first valid vertex: where the surface label goes
    for (size_t i = 0; i < n; ++i) {
        if (isn(data[i]) || isn(pres[i])) continue;
        const double ypix = m_bry - ((m_logPmax - std::log(pres[i])) * ysc);          // == presToPix(p)
        const double yrow = (m_bry - ypix) / (m_bry - m_tpad) * m_yrange;           // == tmpcToPix's inner term
        const double xpix = m_brx - (((m_brtmpc - yrow - data[i]) / m_xrange) * (m_brx - m_lpad));
        const double x = m_originx + xpix * inv, y = m_originy + ypix * inv;
        if (!haveFirst) { haveFirst = true; fx = x; fy = y; fd = data[i]; }
        const bool on = (y >= top && y <= bot);
        if (!on) {
            if (prevOn) { xs.push_back(x); ys.push_back(y); ds.push_back(data[i]); lastx = x; lasty = y; pendingIdx = -1; }   // leaving
            else { pendingIdx = (long)i; pend_x = x; pend_y = y; pend_d = data[i]; }
            prevOn = false;
            continue;
        }
        if (!prevOn && pendingIdx >= 0) { xs.push_back(pend_x); ys.push_back(pend_y); ds.push_back(pend_d); lastx = pend_x; lasty = pend_y; }   // entering
        pendingIdx = -1;
        prevOn = true;
        if (!xs.empty() && std::fabs(x - lastx) < 0.5 && std::fabs(y - lasty) < 0.5) continue;
        xs.push_back(x); ys.push_back(y); ds.push_back(data[i]);
        lastx = x; lasty = y;
    }
    if (xs.empty()) return;
    QVector<QPointF> pts; pts.reserve((int)xs.size());
    for (size_t i = 0; i < xs.size(); ++i) pts.push_back(QPointF(xs[i], ys[i]));
    qp.drawPolyline(pts.constData(), pts.size());
    if (label) {
        qp.setClipping(false);
        double lab = (m_sfcUnits == "Celsius") ? fd : (fd * 1.8 + 32.0);
        qp.setPen(QPen(m_bg, 0)); qp.setBrush(m_bg);
        QRectF rect(fx - 8, fy + 4, 16, 12);
        qp.drawRect(rect);
        qp.setPen(QPen(color, 3)); qp.setFont(m_envTraceFont);
        qp.drawText(rect, Qt::AlignCenter | Qt::TextDontClip, i2s(lab));
        qp.setClipping(true);
    }
}

void SkewTWidget::drawBarbs(QPainter& qp) {
    qp.setClipping(false);
    QRectF mod(m_clip.topLeft(), QSizeF(m_clip.width(), m_clip.height() + m_bpad));
    qp.setClipRect(mod);
    const bool shemis = (m_prof.latitude != MISSING && m_prof.latitude < 0);
    long top = -1; for (size_t i = 0; i < m_tmpc.size(); ++i) if (!isn(m_tmpc[i])) top = (long)i;
    if (top < 0) { qp.setClipRect(m_clip); return; }
    const double ptop = m_pres[top];
    for (double p = m_env->sfc_pres; p > ptop; p -= 40.) {
        double u, v; m_prof.wind_at_pres(p, u, v);
        if (u == MISSING || v == MISSING || p < m_pmin) continue;
        double dd, ss; comp2vec(u, v, dd, ss);
        if (!(ss == ss)) continue;
        const QColor bc = get_barb_color(ss);                 // ramp is in knots
        if (m_windUnits == "m/s") ss *= 0.514444;
        double y = m_originy + presToPix(p) / m_scale;
        drawBarb(qp, m_barbx, y, dd, ss, bc.isValid() ? bc : m_fg, shemis);
    }
    qp.setClipRect(m_clip);
}

void SkewTWidget::plotData(QPainter& qp) {
    if (!m_hasData || !m_env) return;
    qp.setClipRect(m_clip);
    qp.setRenderHint(QPainter::Antialiasing, !(m_dragging && m_liveSent));   // cheaper strokes while a live drag is running
    qp.setRenderHint(QPainter::TextAntialiasing);
    drawTitles(qp);

    drawTrace(qp, m_wetbulb, m_pres, m_wetbulbC, 1);
    drawTrace(qp, m_tmpc, m_pres, m_temp);
    if (m_showTv) drawTrace(qp, m_vtmp, m_pres, m_temp, 1, Qt::DashLine, false);

    if (m_plotDgz && m_dgzBot != m_dgzTop) {
        std::vector<double> pres, tmpc;
        for (double p = m_dgzTop; p < m_dgzBot; p += 5) pres.insert(pres.begin(), p);   // arange(ptop, pbot, 5)[::-1]
        for (double p : pres) tmpc.push_back(m_env->temp_at(p));
        qp.setFont(m_hghtFont);
        drawTrace(qp, tmpc, pres, m_dgzC, 3, Qt::SolidLine, false);
        const QColor yel("#F5D800");
        drawSigLevels(qp, m_dgzBot, yel); drawSigLevels(qp, m_dgzTop, yel);
        drawSigLevels(qp, m_wbzP, m_dewp, "WBZ="); drawSigLevels(qp, m_frzP, QColor("#FFA500"), "FRZ=");
    } else {
        drawMaxLapseRateLayer(qp);
        drawTempLevels(qp);
    }
    drawTrace(qp, m_dwpc, m_pres, m_dewp);

    for (double h : {0., 1000., 3000., 6000., 9000., 12000., 15000.}) drawHeight(qp, h);
    if (m_havePcl) {
        drawVirtualParcelTrace(qp, m_pcl.ttrace, m_pcl.ptrace, m_fg);
        drawVirtualParcelTrace(qp, m_dpT, m_dpP, QColor("#FF00FF"));
    }
    if (m_plotPbl) drawPblLevel(qp);
    drawParcelLevels(qp);

    qp.setRenderHint(QPainter::Antialiasing, false);
    drawBarbs(qp);
    qp.setRenderHint(QPainter::Antialiasing);
    drawEffectiveLayer(qp);
}

// ===================================================================== readout
QString SkewTWidget::readoutText(const QString& var, QColor& color) const {
    const double p = m_readoutPres;
    double val; QString id, unit; int rnd = 1;
    if (var == "tmpc") { val = m_env->temp_at(p); id = "T="; unit = "C"; color = m_temp; }
    else if (var == "dwpc") { val = m_env->dwpt_at(p); id = "Td="; unit = "C"; color = m_dewp; }
    else if (var == "thetae") { val = interpLevel(m_thetae, p); id = QString(QChar(0x03B8)) + "e="; unit = "K"; rnd = 0; color = m_lfcMkr; }
    else if (var == "wetbulb") { val = interpLevel(m_wetbulb, p); id = "Tw="; unit = "C"; color = m_wetbulbC; }
    else if (var == "theta") { val = interpLevel(m_theta, p); id = QString(QChar(0x03B8)) + "="; unit = "K"; rnd = 0; color = m_lfcMkr; }
    else if (var == "wvmr") { val = interpLevel(m_wvmr, p); id = "q="; unit = "g/kg"; color = m_dewp; }
    else { val = NaNv; id = QString(QChar(0x03C9)) + "="; unit = "mb/hr"; color = m_elMkr; }   // omega: not available
    return id + f2s(val, rnd) + " " + unit;
}

void SkewTWidget::updateReadout() {
    if (!m_hasData || !m_env) return;
    m_tmpText = readoutText(m_readoutVars[0], m_tmpCol);
    m_dwpText = readoutText(m_readoutVars[1], m_dwpCol);
    double hgt = m_env->to_agl(m_env->hght_at(m_readoutPres));
    m_presText = f2s(m_readoutPres, 1) + " hPa";
    m_hghtText = f2s(hgt, 1) + " m";
    emit cursorMove(hgt);
    update();
}

void SkewTWidget::drawReadout(QPainter& qp) {
    if (!m_readout || !m_hasData) return;
    qp.setClipping(false);
    qp.setRenderHint(QPainter::Antialiasing, false);
    double y = m_originy + presToPix(m_readoutPres) / m_scale;
    qp.setPen(QPen(m_fg, 1)); qp.setBrush(Qt::NoBrush);
    qp.drawLine(QPointF(m_lpad, y), QPointF(m_brx, y));
    QFont f; f.setPixelSize(11); qp.setFont(f);
    auto lab = [&](double x, double yy, double w, const QString& t, const QColor& c) {
        QRectF r(x, yy, w, 15);
        qp.fillRect(r, m_bg); qp.setPen(c); qp.drawText(r, Qt::AlignCenter, t);
    };
    lab(m_lpad, y + 2, 60, m_presText, m_fg);
    lab(m_lpad, y - 15, 65, m_hghtText, QColor("#FF0000"));
    lab(m_brx - m_rpad, y - 15, 65, m_tmpText, m_tmpCol);
    lab(m_brx - m_rpad, y + 2, 65, m_dwpText, m_dwpCol);
}

void SkewTWidget::setReadoutCursor() {
    m_readout = true; m_trackCursor = true;
    emit cursorToggle(true);
    updateReadout(); update();
}
void SkewTWidget::setNoCursor() {
    m_readout = false; m_trackCursor = false;
    emit cursorToggle(false);
    update();
}

// ===================================================================== paint
void SkewTWidget::paintContents(QPaintEvent*) {
    if (m_bgDirty || m_bgCache.size() != size()) {
        m_bgCache = QPixmap(size()); m_bgCache.fill(m_bg);
        QPainter bp(&m_bgCache); drawBackground(bp); bp.end();
        m_bgDirty = false;
    }
    QPainter qp(this);
    qp.drawPixmap(0, 0, m_bgCache);
    plotData(qp);

    // drag preview (Draggable._dragLine)
    if (m_dragging && m_dragIdx >= 0 && m_editMode == EditMode::PointSnap) {
        const std::vector<double>& arr = m_dragTemp ? m_tmpc : m_dwpc;
        qp.setClipping(false);
        qp.setPen(QPen(m_dragTemp ? QColor("#9F0101") : QColor("#019B06"), 1));
        double y = vtxPix(m_dragTemp, (size_t)m_dragIdx).y();
        long lb = m_dragIdx - 1, ub = m_dragIdx + 1;
        while (lb >= 0 && (isn(arr[lb]) || isn(m_pres[lb]))) --lb;
        while (ub < (long)arr.size() && (isn(arr[ub]) || isn(m_pres[ub]))) ++ub;
        if (lb >= 0) qp.drawLine(vtxPix(m_dragTemp, (size_t)lb), QPointF(m_dragPos.x(), y));
        if (ub < (long)arr.size()) qp.drawLine(QPointF(m_dragPos.x(), y), vtxPix(m_dragTemp, (size_t)ub));
    } else if (m_dragging && m_editMode == EditMode::LineSegment) {
        qp.setClipping(false);
        qp.setPen(QPen(m_lsTemp ? QColor("#9F0101") : QColor("#019B06"), 1, Qt::DashLine));
        double sx = m_originx + tmpcToPix(m_lsT0, m_lsP0) / m_scale, sy = m_originy + presToPix(m_lsP0) / m_scale;
        qp.drawLine(QPointF(sx, sy), m_lsCur);
    }
    drawReadout(qp);
}

// ===================================================================== interaction
QPointF SkewTWidget::vtxPix(bool temp, size_t i) const {
    const std::vector<double>& a = temp ? m_tmpc : m_dwpc;
    return QPointF(m_originx + tmpcToPix(a[i], m_pres[i]) / m_scale, m_originy + presToPix(m_pres[i]) / m_scale);
}

void SkewTWidget::mousePressEvent(QMouseEvent* e) {
    if (!m_hasData) return;
    if (e->button() == Qt::MiddleButton) {            // pan the zoomed view
        if (m_scale < 1.0) { m_panning = true; m_panLast = e->pos(); setCursor(Qt::ClosedHandCursor); }
        return;
    }
    m_wasRight = (e->button() & Qt::RightButton);
    if (m_wasRight || m_readout || !m_interactive) return;
    if (m_editMode == EditMode::LineSegment) {
        // Left = temperature; Shift/Ctrl+Left or a side (back/forward) mouse button = dewpoint.
        // Right click is left alone for the context menu.
        const bool side = e->button() == Qt::BackButton || e->button() == Qt::ForwardButton;
        if (e->button() == Qt::LeftButton || side) {
            m_lsTemp = !(side || (e->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier)));
            double y = (e->pos().y() - m_originy) * m_scale, x = (e->pos().x() - m_originx) * m_scale;
            m_lsP0 = pixToPres(y); m_lsT0 = pixToTmpc(x, y);
            m_lsCur = e->pos(); m_dragging = true;
        }
        return;
    }
    const double cutoff = 5.0;
    for (int pass = 0; pass < 2; ++pass) {              // dwpc first, then tmpc
        const bool temp = (pass == 1);
        const std::vector<double>& a = temp ? m_tmpc : m_dwpc;
        double best = 1e18; long bi = -1;
        for (size_t i = 0; i < a.size(); ++i) {
            if (isn(a[i]) || isn(m_pres[i])) continue;
            QPointF p = vtxPix(temp, i);
            double d = std::hypot(e->pos().x() - p.x(), e->pos().y() - p.y());
            if (d < best) { best = d; bi = (long)i; }
        }
        if (best <= cutoff) {
            m_dragTemp = temp; m_dragIdx = bi; m_clickStart = e->pos(); m_dragPos = e->pos(); m_dragging = true;
            m_dragBase = m_prof; m_liveSent = false; m_liveClock.start();
            return;
        }
    }
}

static double restrictX(const SkewTWidget* /*w*/, double x) { return x; }

void SkewTWidget::mouseMoveEvent(QMouseEvent* e) {
    if (!m_hasData) return;
    if (m_panning) {
        m_originx += e->pos().x() - m_panLast.x();
        m_originy += e->pos().y() - m_panLast.y();
        m_panLast = e->pos();
        double ll_x = m_originx + width() / m_scale, ll_y = m_originy + height() / m_scale;   // same clamp as the wheel zoom
        if (ll_x < width()) m_originx += (width() - ll_x); else if (m_originx > 0) m_originx = 0;
        if (ll_y < height()) m_originy += (height() - ll_y); else if (m_originy > 0) m_originy = 0;
        m_bgDirty = true;
        if (m_readout) updateReadout();
        update();
        emit viewChanged(m_scale, m_originy);
        emitZoom();
        return;
    }
    if (m_trackCursor) {
        m_readoutPres = pixToPres((e->pos().y() - m_originy) * m_scale);
        updateReadout();
    }
    if (m_dragging) {
        if (m_editMode == EditMode::LineSegment) m_lsCur = e->pos();
        else if (m_dragIdx >= 0) {
            // restrictions: T may not pass left of Td, Td may not pass right of T (nearest vertex by y)
            double x = e->pos().x(), y = vtxPix(m_dragTemp, (size_t)m_dragIdx).y();
            const std::vector<double>& oth = m_dragTemp ? m_dwpc : m_tmpc;
            double bestd = 1e18, ox = x;
            for (size_t i = 0; i < oth.size(); ++i) {
                if (isn(oth[i]) || isn(m_pres[i])) continue;
                QPointF p = vtxPix(!m_dragTemp, i);
                double d = std::fabs(y - p.y());
                if (d < bestd) { bestd = d; ox = p.x(); }
            }
            x = m_dragTemp ? std::max(ox, x) : std::min(ox, x);
            m_dragPos = QPoint((int)std::lround(x), (int)std::lround(y));
            if (m_live && m_editMode == EditMode::PointSnap && m_liveClock.elapsed() >= m_liveInterval) {
                m_liveClock.restart();
                QElapsedTimer cost; cost.start();
                const double t = pixToTmpc((m_dragPos.x() - m_originx) * m_scale, (y - m_originy) * m_scale);
                Profile live = m_dragBase;
                ProfileModifier::modify_thermo_index(live, (size_t)m_dragIdx, t, m_dragTemp, 0);
                m_prof = live; m_liveSent = true;
                emit profileLive();                         // the window recomputes and calls setProfile()
                // pace the next live update to what this machine can afford (recompute + repaint ~ 3.5x the recompute)
                m_liveInterval = std::max<qint64>(16, std::min<qint64>(250, (qint64)(3.5 * cost.nsecsElapsed() / 1e6)));
            }
        }
    }
    (void)restrictX;
    update();
}

void SkewTWidget::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() == Qt::MiddleButton && m_panning) { m_panning = false; unsetCursor(); return; }
    if (!m_hasData) return;
    if (!m_wasRight && m_readout) { m_trackCursor = !m_trackCursor; m_cursorLoc = e->pos(); m_haveCursorLoc = true; }
    if (m_dragging) {
        m_dragging = false;
        if (m_editMode == EditMode::LineSegment) {
            double y = (e->pos().y() - m_originy) * m_scale, x = (e->pos().x() - m_originx) * m_scale;
            double p1 = pixToPres(y), t1 = pixToTmpc(x, y);
            if (std::isfinite(p1) && std::isfinite(t1) && e->pos() != m_lsCur) {}
            ProfileModifier::modify_layer(m_prof, m_lsP0, m_lsT0, p1, t1, m_lsTemp);
            emit profileModified();
        } else if (m_dragIdx >= 0 && !(m_dragPos == m_clickStart)) {
            if (m_liveSent) { m_prof = m_dragBase; m_liveSent = false; }   // re-apply the final position to the original
            double x = m_dragPos.x(), y = vtxPix(m_dragTemp, (size_t)m_dragIdx).y();
            double t = pixToTmpc((x - m_originx) * m_scale, (y - m_originy) * m_scale);
            ProfileModifier::modify_thermo_index(m_prof, (size_t)m_dragIdx, t, m_dragTemp, 0);
            m_dragIdx = -1;
            emit profileModified();
        }
        if (m_liveSent) {                                  // dragged back onto the start: drop the preview edit
            m_prof = m_dragBase; m_liveSent = false;
            emit profileModified();
        }
        m_dragIdx = -1;
        update();
    }
    m_wasRight = false;
}

void SkewTWidget::liftParcelLevel(int i) {
    if (!m_hasData || !m_env) return;
    double pres = pixToPres(m_cursorLoc.y());      // sic: skew.py ignores origin/scale here
    double tmp = m_env->temp_at(pres), dwp = m_env->dwpt_at(pres);
    if (i == 0) { emit userParcel(pres, tmp, dwp); return; }
    if (i == -9999) {
        bool ok = false;
        QString s = QInputDialog::getText(nullptr, "Parcel Depth (" + QString::number((int)pres) + "to __)",
                                          "Mean Layer Depth (mb):", QLineEdit::Normal, "", &ok);
        if (!ok) return;
        i = s.toInt(&ok); if (!ok) return;
    }
    tab::LPL l = tab::define_ml(*m_env, (double)i, pres);
    emit userParcel(l.pres, l.tmpc, l.dwpc);
}

void SkewTWidget::populateMenu(QMenu* menu, const QPoint& pos) {
    if (!m_haveCursorLoc || m_trackCursor) { m_cursorLoc = pos; m_haveCursorLoc = true; }
    auto* g = new QActionGroup(menu); g->setExclusive(true);
    QAction* a = menu->addAction("No Cursor"); a->setCheckable(true); a->setChecked(!m_readout); g->addAction(a);
    connect(a, &QAction::triggered, this, &SkewTWidget::setNoCursor);
    a = menu->addAction("Readout Cursor"); a->setCheckable(true); a->setChecked(m_readout); g->addAction(a);
    connect(a, &QAction::triggered, this, &SkewTWidget::setReadoutCursor);
    menu->addSeparator();
    QMenu* pm = menu->addMenu("Lift Parcel"); pm->setEnabled(m_readout);
    connect(pm->addAction("This Parcel"), &QAction::triggered, this, [this]() { liftParcelLevel(0); });
    connect(pm->addAction("50 mb Layer Parcel"), &QAction::triggered, this, [this]() { liftParcelLevel(50); });
    connect(pm->addAction("100 mb Layer Parcel"), &QAction::triggered, this, [this]() { liftParcelLevel(100); });
    connect(pm->addAction("Custom Parcel"), &QAction::triggered, this, [this]() { liftParcelLevel(-9999); });
    connect(menu->addAction("Modify Surface"), &QAction::triggered, this, &SkewTWidget::modifySurfaceRequested);
    menu->addSeparator();
    connect(menu->addAction("Reset Skew-T"), &QAction::triggered, this, &SkewTWidget::resetRequested);
}

} // namespace sharppy
