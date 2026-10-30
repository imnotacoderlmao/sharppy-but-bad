#include "sharppy/hodo_widget.hpp"
#include "sharppy/utils_fmt.hpp"
#include "sharppy/modify.hpp"
#include "sharppy/interp.hpp"
#include "sharppy/tab.hpp"
#include <QActionGroup>
#include <QCursor>
#include <QPainterPath>
#include <QFontMetrics>
#include <algorithm>
#include <cmath>

namespace sharppy {

static constexpr double K2MS = 0.514444;
static const QColor GOLD("#B8860B");

static QString i2s(double v) { return int2str(v); }                  // utils.INT2STR
static double lin(double h, const std::vector<double>& z, const std::vector<double>& a) {
    for (size_t i = 0; i + 1 < z.size(); ++i)
        if (z[i] <= h && h <= z[i + 1]) {
            if (z[i + 1] == z[i]) return a[i];
            return a[i] + (a[i + 1] - a[i]) * (h - z[i]) / (z[i + 1] - z[i]);
        }
    return std::nan("");
}

HodoWidget::HodoWidget(QWidget* parent) : CanvasBase(parent) {
    setMinimumSize(250, 250);
    setMouseTracking(true);
    initUI();
}

// ------------------------------------------------------------------ background
void HodoWidget::initUI() {
    m_wid = width(); m_hgt = height();
    m_pointX = m_pointY = 0;
    m_centerx = m_wid / 2; m_centery = m_hgt / 2;
    m_scale = m_wid / m_hodomag;
    m_rings.clear();
    for (int r = m_ringInc; r < 100 + m_ringInc; r += m_ringInc) m_rings.push_back(r);
    const double fsize = (physicalDpiX() > 75) ? 7 : 9;
    m_labelFont = QFont("Helvetica"); m_labelFont.setPointSizeF(fsize + m_hgt * 0.0045); m_labelFont.setBold(true);
    m_criticalFont = QFont("Helvetica"); m_criticalFont.setPointSizeF(fsize + 2 + m_hgt * 0.0045); m_criticalFont.setBold(true);
    m_readoutFont = QFont("Helvetica"); m_readoutFont.setPointSizeF(11 + m_hgt * 0.0045); m_readoutFont.setBold(true);
    m_criticalHeight = QFontMetrics(m_criticalFont).xHeight() + 5 + m_hgt * 0.0045;
}

void HodoWidget::resizeEvent(QResizeEvent* e) { QWidget::resizeEvent(e); initUI(); }

QPointF HodoWidget::uvToPix(double u, double v) const {
    const double c = (m_windUnits == "m/s") ? K2MS : 1.0;
    return QPointF(m_centerx + u * c * m_scale, m_centery - v * c * m_scale);
}
void HodoWidget::pixToUv(double x, double y, double& u, double& v) const {
    const double c = (m_windUnits == "m/s") ? 1.0 / K2MS : 1.0;
    u = c * (x - m_centerx) / m_scale;
    v = c * (m_centery - y) / m_scale;
}

void HodoWidget::centerHodo(double u, double v) {
    const double cx = m_wid / 2, cy = m_hgt / 2;
    QPointF pt = uvToPix(u, v);
    if (!(m_pointX == pt.x() && m_pointY == pt.y())) {
        m_pointX = pt.x(); m_pointY = pt.y();
        m_centerx += cx - pt.x(); m_centery += cy - pt.y();
    }
}

void HodoWidget::drawRing(QPainter& qp, double spd) {
    const double vv = -spd * m_scale;                       // vec2comp(0, spd) -> v = -spd
    const double rad = std::fabs(vv);
    QPen pen(m_isotach, 1); pen.setStyle(Qt::DashLine);
    qp.setPen(pen); qp.setBrush(Qt::NoBrush);
    qp.drawEllipse(QPointF(m_centerx, m_centery), rad, rad);
    qp.setFont(m_labelFont);
    const double off = 5, w = 15, h = 15;
    QRectF top(m_centerx + off, m_centery + vv - off, w, h), bottom(m_centerx + off, m_centery - vv - off, w, h);
    QRectF right(m_centerx + vv - off, m_centery + off, w, h), left(m_centerx - vv - off, m_centery + off, w, h);
    qp.setPen(m_fg);
    const QString s = i2s(spd);
    for (const QRectF& r : {top, right, bottom, left})
        qp.drawText(r, Qt::TextDontClip | Qt::AlignCenter, s);
}

void HodoWidget::drawBackground(QPainter& qp) {
    qp.fillRect(rect(), m_bg);
    for (int r : m_rings) drawRing(qp, r);
    QPen pen(m_fg, 2); qp.setPen(pen);                       // axes
    qp.drawLine(QPointF(m_centerx, 0), QPointF(m_centerx, m_hgt));
    qp.drawLine(QPointF(0, m_centery), QPointF(m_wid, m_centery));
    qp.drawLine(QPointF(0, 0), QPointF(m_wid, 0));           // frame
    qp.drawLine(QPointF(m_wid, 0), QPointF(m_wid, m_hgt));
    qp.drawLine(QPointF(m_wid, m_hgt), QPointF(0, m_hgt));
    qp.drawLine(QPointF(0, m_hgt), QPointF(0, 0));
}

void HodoWidget::rebuildRings() {
    // rings out to the farthest widget corner (the view can be panned, so not just the bottom-right one)
    const double conv = (m_windUnits == "m/s") ? K2MS : 1.0;
    double far = 0;
    for (QPointF c : {QPointF(0, 0), QPointF(m_wid, 0), QPointF(0, m_hgt), QPointF(m_wid, m_hgt)}) {
        double u, v; pixToUv(c.x(), c.y(), u, v);
        far = std::max(far, std::hypot(u, v));
    }
    const int max_uv = static_cast<int>(conv * far);
    m_rings.clear();
    for (int r = m_ringInc; r < max_uv + m_ringInc; r += m_ringInc) m_rings.push_back(r);
}

void HodoWidget::wheelEvent(QWheelEvent* e) {
    m_hodomag = m_hodomag - (e->angleDelta().y() * (m_hodomag / 1000));
    if (m_hodomag > m_ringInc * 25) m_ringInc *= 2;
    else if (m_hodomag < m_ringInc * 12.5) m_ringInc = static_cast<int>(std::max(m_ringInc / 2.0, 5.0));
    m_scale = m_wid / m_hodomag;
    rebuildRings();
    update();
}

void HodoWidget::setHeightWindow(double zlo, double zhi) {
    m_zlo = std::isfinite(zlo) ? zlo : -1e18;
    m_zhi = std::isfinite(zhi) ? zhi : 1e18;
    update();
}

void HodoWidget::setPreferences(const VizPrefs& p) {
    auto it = p.find("wind_units"); if (it != p.end()) m_windUnits = it.value();
    m_bg = prefColor(p, "bg_color", m_bg); m_fg = prefColor(p, "fg_color", m_fg);
    m_isotach = prefColor(p, "hodo_itach_color", m_isotach); m_crit = prefColor(p, "hodo_crit_color", m_crit);
    static const char* k[5] = {"0_1_color", "1_3_color", "3_6_color", "6_9_color", "9_12_color"};
    for (int i = 0; i < 5; ++i) m_colors[i] = prefColor(p, k[i], m_colors[i]);
    m_effInflow = prefColor(p, "eff_inflow_color", m_effInflow);
    if (m_windUnits == "m/s") { m_hodomag = 80.; m_ringInc = 5; } else { m_hodomag = 160.; m_ringInc = 10; }
    initUI();
    update();
}

// ------------------------------------------------------------------ data
void HodoWidget::rebuildVertices() {
    m_vtx.clear();
    std::vector<double> z, u, v;
    std::vector<long> idx;
    for (size_t i = 0; i < m_prof.size(); ++i) {
        if (m_prof.u[i] == MISSING || m_prof.v[i] == MISSING || m_prof.hght[i] == MISSING) continue;
        z.push_back(m_prof.to_agl(m_prof.hght[i])); u.push_back(m_prof.u[i]); v.push_back(m_prof.v[i]); idx.push_back((long)i);
    }
    for (size_t i = 0; i < z.size(); ++i) if (z[i] <= 12000.) m_vtx.push_back({u[i], v[i], idx[i]});
    double u12 = lin(12000., z, u), v12 = lin(12000., z, v);
    if (std::isfinite(u12) && std::isfinite(v12)) m_vtx.push_back({u12, v12, -1});
}

void HodoWidget::rebuildDerived() {
    m_haveMeanLclEl = false;
    if (m_mu.lcl_p != MISSING && m_mu.el_p != MISSING) {
        WindVector w = mean_wind_pres(m_prof, m_mu.lcl_p, m_mu.el_p, true);
        if (w.wspd != 0 || w.u != 0 || w.v != 0) {
            m_mlu = w.u; m_mlv = w.v; comp2vec(w.u, w.v, m_mlDir, m_mlSpd); m_haveMeanLclEl = true;
        }
    }
    CorfidiMotion c = corfidi_mcs_motion(m_prof);
    m_cupU = c.upwind.u; m_cupV = c.upwind.v; m_cdnU = c.downwind.u; m_cdnV = c.downwind.v;
    comp2vec(m_cupU, m_cupV, m_upDir, m_upSpd); comp2vec(m_cdnU, m_cdnV, m_dnDir, m_dnSpd);
    WindVector s6 = wind_shear(m_prof, 0.0, 6000.0); m_shear6u = s6.u; m_shear6v = s6.v;
    const WindVector& sm = m_useLeft ? m_srwind.left_mover : m_srwind.right_mover;
    WindVector sr = sr_wind(m_prof, 9000.0, 11000.0, sm.u, sm.v); m_srw911u = sr.u; m_srw911v = sr.v;
    m_critRight = critical_angle(m_prof, m_srwind.right_mover.u, m_srwind.right_mover.v);
    m_critLeft = critical_angle(m_prof, m_srwind.left_mover.u, m_srwind.left_mover.v);
    m_prof.wind_at_pres(m_prof.sfc_pres, m_sfcU, m_sfcV);
    m_prof.wind_at_pres(m_prof.pres_at_hght(m_prof.to_msl(500.0)), m_u500, m_v500);
    rebuildVertices();
}

void HodoWidget::setKinematics(const BunkersMotion& b, const BunkersMotion& sr, const InflowLayer& infl,
                               const ParcelResult& mu, bool left) {
    m_bunkers = b; m_srwind = sr; m_inflow = infl; m_mu = mu; m_useLeft = left;
    if (m_hasData) rebuildDerived();
    update();
}

void HodoWidget::setProfileOnly(const Profile& prof) {
    m_prof = prof; m_hasData = !prof.empty();
    if (m_hasData) rebuildDerived();
    update();
}

void HodoWidget::setProfile(const Profile& prof, const BunkersMotion& bunkers) {
    m_prof = prof; m_hasData = !prof.empty();
    m_bunkers = bunkers; m_srwind = bunkers;
    if (m_hasData) {
        const auto env_hold_ = tab::env_for(prof); const tab::Env& env = *env_hold_;
        tab::LPL l = tab::define_mu(env);
        m_mu = ParcelResult();
        tab::Parcel pc = tab::parcelx(env, l.pres, env.temp_at(l.pres), env.dwpt_at(l.pres));
        if (std::isfinite(pc.lclpres)) m_mu.lcl_p = pc.lclpres;
        if (std::isfinite(pc.elpres)) m_mu.el_p = pc.elpres;
        m_inflow = effective_inflow_layer(prof, bunkers.right_mover.u, bunkers.right_mover.v);
        rebuildDerived();
    }
    update();
}

void HodoWidget::setCustomStormMotion(double u, double v) {
    m_hasCustomStorm = true;
    BunkersMotion& s = m_srwind;
    WindVector& w = m_useLeft ? s.left_mover : s.right_mover;
    w.u = u; w.v = v; comp2vec(u, v, w.wdir, w.wspd);
    update();
}

QString HodoWidget::spdStr(double kts) const { return i2s(m_windUnits == "m/s" ? kts * K2MS : kts); }

// ------------------------------------------------------------------ drawing
void HodoWidget::drawHodo(QPainter& qp) {
    std::vector<double> z, xx, yy;
    for (size_t i = 0; i < m_prof.size(); ++i) {
        if (m_prof.u[i] == MISSING || m_prof.v[i] == MISSING || m_prof.hght[i] == MISSING) continue;
        QPointF p = uvToPix(m_prof.u[i], m_prof.v[i]);
        z.push_back(m_prof.to_agl(m_prof.hght[i])); xx.push_back(p.x()); yy.push_back(p.y());
    }
    if (z.size() < 2) return;
    if (m_zlo > -1e17 || m_zhi < 1e17) {
        // cull to the Skew-T's visible height window; cut points are interpolated onto the window edges
        const double dmin = *std::min_element(z.begin(), z.end()), dmax = *std::max_element(z.begin(), z.end());
        const double lo = std::max(m_zlo, dmin), hi = std::min(m_zhi, dmax);
        if (!(lo < hi)) return;
        std::vector<double> z2, x2, y2;
        z2.push_back(lo); x2.push_back(lin(lo, z, xx)); y2.push_back(lin(lo, z, yy));
        for (size_t i = 0; i < z.size(); ++i) if (z[i] > lo && z[i] < hi) { z2.push_back(z[i]); x2.push_back(xx[i]); y2.push_back(yy[i]); }
        z2.push_back(hi); x2.push_back(lin(hi, z, xx)); y2.push_back(lin(hi, z, yy));
        z.swap(z2); xx.swap(x2); yy.swap(y2);
    }
    const double zmin = *std::min_element(z.begin(), z.end()), zmax = *std::max_element(z.begin(), z.end());
    double bnds[6] = {0., 1000., 3000., 6000., 9000., 12000.};
    for (double& b : bnds) b = std::max(b, zmin);
    std::vector<double> sx, sy;
    for (double b : bnds) if (b <= zmax) { sx.push_back(lin(b, z, xx)); sy.push_back(lin(b, z, yy)); }
    long sidx[6];
    for (int i = 0; i < 6; ++i) sidx[i] = std::lower_bound(z.begin(), z.end(), bnds[i]) - z.begin();
    qp.setBrush(Qt::NoBrush);
    for (size_t k = 0; k + 1 < sx.size(); ++k) {
        QPen pen(m_colors[k], 2); qp.setPen(pen);
        QPainterPath path; path.moveTo(sx[k], sy[k]);
        double lx = sx[k], ly = sy[k];                       // drop vertices within half a pixel of the last one (raw soundings)
        for (long j = sidx[k]; j < sidx[k + 1]; ++j) {
            if (std::fabs(xx[j] - lx) < 0.5 && std::fabs(yy[j] - ly) < 0.5) continue;
            path.lineTo(xx[j], yy[j]); lx = xx[j]; ly = yy[j];
        }
        path.lineTo(sx[k + 1], sy[k + 1]);
        qp.drawPath(path);
    }
    double mx = *std::max_element(bnds, bnds + 6);
    if (zmax < mx && !sx.empty()) {
        size_t k = sx.size() - 1;
        QPen pen(m_colors[std::min<size_t>(k, 4)], 2); qp.setPen(pen);
        QPainterPath path; path.moveTo(sx[k], sy[k]);
        double lx = sx[k], ly = sy[k];
        for (size_t j = (size_t)sidx[k]; j < xx.size(); ++j) {
            if (std::fabs(xx[j] - lx) < 0.5 && std::fabs(yy[j] - ly) < 0.5) continue;
            path.lineTo(xx[j], yy[j]); lx = xx[j]; ly = yy[j];
        }
        qp.drawPath(path);
    }
}

void HodoWidget::drawSMV(QPainter& qp) {
    QPen pen(m_fg, 1); qp.setPen(pen); qp.setBrush(Qt::NoBrush);
    const WindVector &rm = m_srwind.right_mover, &lm = m_srwind.left_mover;
    QPointF b_r = uvToPix(m_bunkers.right_mover.u, m_bunkers.right_mover.v);
    QPointF b_l = uvToPix(m_bunkers.left_mover.u, m_bunkers.left_mover.v);
    for (QPointF c : {b_r, b_l}) {
        qp.drawLine(c - QPointF(2, 0), c + QPointF(2, 0));
        qp.drawLine(c - QPointF(0, 2), c + QPointF(0, 2));
    }
    QPointF cr = uvToPix(rm.u, rm.v), cl = uvToPix(lm.u, lm.v);
    qp.drawEllipse(cr, 5, 5); qp.drawEllipse(cl, 5, 5);

    if (m_inflow.bot_p != MISSING && m_inflow.top_p != MISSING) {
        double ut, vt, ub, vb;
        m_prof.wind_at_pres(m_inflow.top_p, ut, vt); m_prof.wind_at_pres(m_inflow.bot_p, ub, vb);
        if (ut != MISSING && ub != MISSING) {
            QPointF pt = uvToPix(ut, vt), pb = uvToPix(ub, vb);
            QPen ip(m_effInflow, 1); qp.setPen(ip);
            QPointF c = m_useLeft ? cl : cr;
            qp.drawLine(c, pb); qp.drawLine(c, pt);
        }
    }
    qp.setPen(m_fg); qp.setFont(m_labelFont);
    double rd, rs, ld, ls; comp2vec(rm.u, rm.v, rd, rs); comp2vec(lm.u, lm.v, ld, ls);
    QRectF rr(cr.x() + 2, cr.y() + 5, 55, 12), lr(cl.x() + 2, cl.y() + 5, 55, 12);
    qp.drawText(rr, Qt::TextDontClip | Qt::AlignCenter, i2s(rd) + "/" + spdStr(rs) + " RM");
    qp.drawText(lr, Qt::TextDontClip | Qt::AlignCenter, i2s(ld) + "/" + spdStr(ls) + " LM");
}

void HodoWidget::drawCorfidi(QPainter& qp) {
    if (!std::isfinite(m_cupU) || !std::isfinite(m_cupV) || !std::isfinite(m_cdnU) || !std::isfinite(m_cdnV)) return;
    QColor c("#00BFFF");
    QPen pen(c, 1); qp.setPen(pen); qp.setBrush(Qt::NoBrush);
    QPointF up = uvToPix(m_cupU, m_cupV), dn = uvToPix(m_cdnU, m_cdnV);
    qp.drawEllipse(up, 3, 3); qp.drawEllipse(dn, 3, 3);
    qp.setFont(m_labelFont);
    QRectF ur(up.x() + 1, up.y() + 3, 60, 10), dr(dn.x() + 1, dn.y() + 3, 60, 10);
    qp.drawText(ur, Qt::TextDontClip | Qt::AlignCenter, "UP=" + i2s(m_upDir) + "/" + spdStr(m_upSpd));
    qp.drawText(dr, Qt::TextDontClip | Qt::AlignCenter, "DN=" + i2s(m_dnDir) + "/" + spdStr(m_dnSpd));
}

void HodoWidget::drawLclElMW(QPainter& qp) {
    if (!m_haveMeanLclEl) return;
    QPen pen(GOLD, 2); qp.setPen(pen); qp.setBrush(Qt::NoBrush);
    QPointF m = uvToPix(m_mlu, m_mlv);
    qp.drawRect(QRectF(m.x() - 4, m.y() + 4, 8, 8));
    qp.setFont(m_labelFont);
    QRectF r(m.x() + 1, m.y() + 5, 40, 12);
    qp.drawText(r, Qt::TextDontClip | Qt::AlignCenter, i2s(m_mlDir) + "/" + spdStr(m_mlSpd));
}

void HodoWidget::drawCriticalAngle(QPainter& qp) {
    if (m_inflow.bot_p == MISSING || m_inflow.top_p == MISSING) return;
    if (m_inflow.bot_p != m_prof.sfc_pres) return;
    if (m_sfcU == MISSING || m_u500 == MISSING) return;
    QPen pen(QColor("#FF00FF"), 1.0); qp.setPen(pen);
    qp.drawLine(uvToPix(m_sfcU, m_sfcV), uvToPix(m_u500, m_v500));
    QRectF r(15, m_hgt - 36, 100, m_criticalHeight + 5);
    QColor bg = m_bg; bg.setAlphaF(0.5);
    qp.setPen(Qt::NoPen); qp.setBrush(bg); qp.drawRect(r);
    qp.setPen(QPen(m_crit, 1.0)); qp.setFont(m_criticalFont);
    double ca = m_useLeft ? m_critLeft : m_critRight;
    QString s = (std::isfinite(ca) && ca != MISSING) ? QString::number((long)ca) : QString("--");
    qp.drawText(r, Qt::TextDontClip | Qt::AlignLeft, "Critical Angle = " + s + QChar(0x00B0));
}

void HodoWidget::drawBoundary(QPainter& qp) {
    const double x = m_bndyPos.x(), y = m_bndyPos.y(), rad = m_bndyDir * M_PI / 180.0;
    const double y1 = 400 * std::sin(rad) + y, x1 = 400 * std::cos(rad) + x;
    const double y2 = y - 400 * std::sin(rad), x2 = x - 400 * std::cos(rad);
    QColor bc("#CC9900");
    qp.setPen(QPen(bc, 2)); qp.setBrush(Qt::NoBrush);
    qp.drawLine(QPointF(x1, y1), QPointF(x2, y2));
    qp.drawEllipse(QPointF(x, y), 3, 3);
    QColor bg = m_bg; bg.setAlphaF(0.5);
    qp.setFont(m_criticalFont);
    // 0-6 km shear
    QRectF r1(3, m_hgt - 35, 150, 14);
    qp.setPen(Qt::NoPen); qp.setBrush(bg); qp.drawRect(r1);
    double tu, tv; pixToUv(x, y, tu, tv);
    QColor sc("#0099CC"); qp.setPen(QPen(sc, 2));
    qp.drawLine(QPointF(x, y), uvToPix(m_shear6u + tu, m_shear6v + tv));
    double d, s; comp2vec(m_shear6u, m_shear6v, d, s);
    qp.drawText(r1, Qt::TextDontClip | Qt::AlignLeft, "0 - 6 km Shear: " + i2s(d) + "/" + i2s(s) + " kts");
    // 9-11 km SR wind
    QRectF r2(3, m_hgt - 20, 200, 14);
    qp.setPen(Qt::NoPen); qp.setBrush(bg); qp.drawRect(r2);
    QColor mg("#FF00FF"); qp.setPen(QPen(mg, 2));
    qp.drawLine(QPointF(x, y), uvToPix(m_srw911u + tu, m_srw911v + tv));
    comp2vec(m_srw911u, m_srw911v, d, s);
    QString type = (s >= 70) ? "LP" : ((s < 70 && s > 40) ? "Classic" : "HP");
    qp.drawText(r2, Qt::TextDontClip | Qt::AlignLeft, "9 - 11 km SR-Wind: " + i2s(d) + "/" + i2s(s) + " kts - (" + type + ")");
}

void HodoWidget::paintContents(QPaintEvent*) {
    QPainter qp(this);
    qp.setRenderHint(QPainter::Antialiasing);
    qp.setRenderHint(QPainter::TextAntialiasing);
    drawBackground(qp);
    if (!m_hasData) return;

    if (m_bndyDrawn) drawBoundary(qp);
    // ONLY DRAW A HODOGRAPH IF THERE'S WIND DATA
    int nwind = 0; for (double w : m_prof.wdir) if (w != MISSING) ++nwind;
    if (nwind > 1) {
        drawHodo(qp);
        drawSMV(qp);
        drawCorfidi(qp);
        drawLclElMW(qp);
        if (m_cursor == Cursor::None) drawCriticalAngle(qp);
    }

    // drag preview (Draggable._dragLine/_dragPoint)
    if (m_dragKind != 0) {
        qp.setPen(QPen(m_fg, 1)); qp.setBrush(Qt::NoBrush);
        const double dx = m_dragPos.x(), dy = m_dragPos.y();
        if (m_dragKind == 1 && m_dragIdx >= 0) {
            if (m_dragIdx > 0) { QPointF a = uvToPix(m_vtx[m_dragIdx - 1].u, m_vtx[m_dragIdx - 1].v); qp.drawLine(a, QPointF(dx, dy)); }
            if (m_dragIdx + 1 < (long)m_vtx.size()) { QPointF b = uvToPix(m_vtx[m_dragIdx + 1].u, m_vtx[m_dragIdx + 1].v); qp.drawLine(QPointF(dx, dy), b); }
        } else qp.drawEllipse(QPointF(dx, dy), 3, 3);
    }

    // skew-T cursor readout
    if (m_readoutVisible && m_readoutHght >= 0 && m_readoutHght <= 12000.) {
        std::vector<double> z, u, v;
        for (const Vtx& vt : m_vtx) {
            (void)vt;
        }
        for (size_t i = 0; i < m_prof.size(); ++i) {
            if (m_prof.u[i] == MISSING || m_prof.hght[i] == MISSING) continue;
            z.push_back(m_prof.to_agl(m_prof.hght[i])); u.push_back(m_prof.u[i]); v.push_back(m_prof.v[i]);
        }
        double ui = lin(m_readoutHght, z, u), vi = lin(m_readoutHght, z, v);
        if (std::isfinite(ui) && std::isfinite(vi)) {
            double wd, ws; comp2vec(ui, vi, wd, ws);
            const bool ms = (m_windUnits == "m/s");
            if (ms) ws *= K2MS;
            QPointF p = uvToPix(ui, vi);
            QRectF tr(p.x() + 2, p.y() + 5, 55, 16);
            qp.fillRect(tr, m_bg);
            qp.setPen(QPen(m_fg, 1)); qp.setBrush(Qt::NoBrush);
            qp.drawEllipse(p, 4, 4);
            qp.setFont(m_readoutFont);
            qp.drawText(tr, Qt::TextDontClip | Qt::AlignCenter,
                        QString("%1/%2 %3").arg((int)wd, 3, 10, QChar('0')).arg((int)ws, 2, 10, QChar('0')).arg(ms ? "m/s" : "kts"));
        }
    }

    if (m_cursor == Cursor::Boundary && !m_bndyReadout.isEmpty()) {
        QRectF r(m_wid - 130, m_hgt - 30, 120, 16);
        qp.fillRect(r, QColor(0, 0, 0));
        QFont f; f.setPixelSize(11); qp.setFont(f);
        qp.setPen(QColor("#00FF00"));
        qp.drawText(r, Qt::AlignLeft | Qt::AlignVCenter, m_bndyReadout);
    }
}

// ------------------------------------------------------------------ interaction
void HodoWidget::plotBndyCursor(double dir) {
    const int length = 40;
    const double rad = dir * M_PI / 180.0;
    double y1 = length * std::sin(rad), x1 = length * std::cos(rad);
    QPixmap pm(length, length); pm.fill(Qt::transparent);
    QPainter p(&pm); p.setRenderHint(QPainter::Antialiasing);
    QColor bc("#CC9900"); p.setPen(QPen(bc, 2));
    p.drawLine(QPointF(x1 + length / 2.0, y1 + length / 2.0), QPointF(length / 2.0 - x1, length / 2.0 - y1));
    p.drawEllipse(QPointF(length / 2.0, length / 2.0), 3, 3);
    p.end();
    setCursor(QCursor(pm));
}

void HodoWidget::mousePressEvent(QMouseEvent* e) {
    if (!m_hasData) return;
    if (e->button() == Qt::MiddleButton) {            // pan (always enabled)
        m_panning = true; m_panLast = e->pos(); setCursor(Qt::ClosedHandCursor);
        return;
    }
    m_wasRight = (e->button() & Qt::RightButton);
    if (m_cursor != Cursor::None || m_wasRight) return;
    const QPointF c = e->localPos();
    const double cutoff = 5.0;
    m_dragKind = 0; m_dragIdx = -1;
    // Draggable.click: nearest vertex within cutoff; release order is hodo, rm, lm
    double best = 1e18; long bi = -1;
    for (size_t i = 0; i < m_vtx.size(); ++i) {
        QPointF p = uvToPix(m_vtx[i].u, m_vtx[i].v); double d = std::hypot(c.x() - p.x(), c.y() - p.y());
        if (d < best) { best = d; bi = (long)i; }
    }
    if (best <= cutoff && m_interactive) { m_dragKind = 1; m_dragIdx = bi; }
    else {
        QPointF r = uvToPix(m_srwind.right_mover.u, m_srwind.right_mover.v), l = uvToPix(m_srwind.left_mover.u, m_srwind.left_mover.v);
        if (std::hypot(c.x() - r.x(), c.y() - r.y()) <= cutoff) m_dragKind = 2;
        else if (std::hypot(c.x() - l.x(), c.y() - l.y()) <= cutoff) m_dragKind = 3;
    }
    if (m_dragKind) { m_clickStart = e->pos(); m_dragPos = m_clickStart; m_dragBase = m_prof; m_liveSent = false; m_liveClock.start(); }
}

void HodoWidget::mouseMoveEvent(QMouseEvent* e) {
    if (!m_hasData) return;
    if (m_panning) {
        m_centerx += e->pos().x() - m_panLast.x();
        m_centery += e->pos().y() - m_panLast.y();
        m_panLast = e->pos();
        m_pointX = m_pointY = 0;
        rebuildRings();
        update();
        return;
    }
    if (m_cursor == Cursor::Boundary) {
        double u, v, d, s; pixToUv(e->localPos().x(), e->localPos().y(), u, v);
        comp2vec(u, v, d, s);
        plotBndyCursor(d);
        m_bndyReadout = "Bndy Motion: " + i2s(d) + "/" + i2s(s);
        update();
    } else if (m_dragKind) {
        m_dragPos = e->pos();
        if (m_live && m_cursor == Cursor::None && m_liveClock.elapsed() >= m_liveInterval) {
            m_liveClock.restart();
            QElapsedTimer cost; cost.start();
            double u, v; pixToUv(m_dragPos.x(), m_dragPos.y(), u, v);
            if (m_dragKind == 1) {
                long pi = (m_dragIdx >= 0 && m_dragIdx < (long)m_vtx.size()) ? m_vtx[m_dragIdx].prof_idx : -1;
                if (pi >= 0) {
                    Profile live = m_dragBase;
                    ProfileModifier::modify_wind_index(live, (size_t)pi, u, v, 0);
                    m_prof = live; m_liveSent = true;
                    emit windLive();
                }
            } else {
                m_liveSent = true;
                emit vectorModified(m_dragKind == 3, u, v);
            }
            // pace the next live update to what this machine can afford (recompute + repaint ~ 3.5x the recompute)
            m_liveInterval = std::max<qint64>(16, std::min<qint64>(250, (qint64)(3.5 * cost.nsecsElapsed() / 1e6)));
        }
        update();
    }
}

void HodoWidget::mouseReleaseEvent(QMouseEvent* e) {
    if (e->button() == Qt::MiddleButton && m_panning) { m_panning = false; unsetCursor(); return; }
    if (!m_hasData) return;
    const QPoint pos = e->pos();
    if (m_cursor == Cursor::Boundary && !m_wasRight) {
        if (m_trackCursor) {
            double u, v, d, s; pixToUv(pos.x(), pos.y(), u, v);
            comp2vec(u, v, d, s); m_bndyDir = d; m_bndySpd = s;
            m_bndyPos = pos; m_bndyDrawn = true; m_trackCursor = false;
        } else {
            plotBndyCursor(m_bndyDir); m_bndyDrawn = false; m_trackCursor = true;
        }
        update();
        return;
    }
    if (m_cursor == Cursor::None && m_dragKind) {
        const bool moved = !(pos == m_clickStart);
        double u, v; pixToUv(pos.x(), pos.y(), u, v);
        const int kind = m_dragKind; const long idx = m_dragIdx;
        m_dragKind = 0; m_dragIdx = -1;
        const bool liveSent = m_liveSent; m_liveSent = false;
        if (!moved && liveSent && kind == 1) { m_prof = m_dragBase; rebuildDerived(); emit windModified(); }
        if (moved) {
            if (kind == 1) {
                long pi = (idx >= 0 && idx < (long)m_vtx.size()) ? m_vtx[idx].prof_idx : -1;
                if (pi >= 0) {
                    if (m_liveSent) m_prof = m_dragBase;           // final position re-applied to the original profile
                    ProfileModifier::modify_wind_index(m_prof, (size_t)pi, u, v, 0);
                    rebuildDerived();
                    emit windModified();
                }
            } else {
                const bool left = (kind == 3);
                emit vectorModified(left, u, v);
                if (left == m_useLeft) emit stormMotionChanged(u, v);
            }
        }
        update();
    }
}

void HodoWidget::mouseDoubleClickEvent(QMouseEvent* e) {
    if (!m_hasData) return;
    const double tol = 5;
    QPointF lm = uvToPix(m_srwind.left_mover.u, m_srwind.left_mover.v), rm = uvToPix(m_srwind.right_mover.u, m_srwind.right_mover.v);
    const QPointF c = e->localPos();
    if (std::hypot(c.x() - lm.x(), c.y() - lm.y()) < tol) emit toggleVector(true);
    else if (std::hypot(c.x() - rm.x(), c.y() - rm.y()) < tol) emit toggleVector(false);
}

void HodoWidget::setNormalCenter() { centerHodo(0, 0); update(); }
void HodoWidget::setMWCenter() { if (!m_haveMeanLclEl) return; centerHodo(m_mlu, m_mlv); update(); }
void HodoWidget::setSRCenter() { centerHodo(m_srwind.right_mover.u, m_srwind.right_mover.v); update(); }
void HodoWidget::setNoCursor() { m_trackCursor = false; m_cursor = Cursor::None; unsetCursor(); m_bndyDrawn = false; m_bndyReadout.clear(); update(); }
void HodoWidget::setBndyCursor() { m_trackCursor = true; m_cursor = Cursor::Boundary; plotBndyCursor(m_bndyDir); update(); }

void HodoWidget::populateMenu(QMenu* menu) {
    auto* g1 = new QActionGroup(menu); g1->setExclusive(true);
    QAction* a = menu->addAction("No Cursor"); a->setCheckable(true); a->setChecked(m_cursor == Cursor::None); g1->addAction(a);
    connect(a, &QAction::triggered, this, &HodoWidget::setNoCursor);
    a = menu->addAction("Bndy Cursor"); a->setCheckable(true); a->setChecked(m_cursor == Cursor::Boundary); g1->addAction(a);
    connect(a, &QAction::triggered, this, &HodoWidget::setBndyCursor);
    menu->addSeparator();
    auto* g2 = new QActionGroup(menu); g2->setExclusive(true);
    a = menu->addAction("Normal"); a->setCheckable(true); a->setChecked(true); g2->addAction(a);
    connect(a, &QAction::triggered, this, &HodoWidget::setNormalCenter);
    a = menu->addAction("Storm Relative"); a->setCheckable(true); g2->addAction(a);
    connect(a, &QAction::triggered, this, &HodoWidget::setSRCenter);
    a = menu->addAction("Mean Wind"); a->setCheckable(true); g2->addAction(a);
    connect(a, &QAction::triggered, this, &HodoWidget::setMWCenter);
    menu->addSeparator();
    a = menu->addAction("Reset Hodograph"); connect(a, &QAction::triggered, this, &HodoWidget::resetHodograph);
    a = menu->addAction("Reset Storm Motion"); connect(a, &QAction::triggered, this, &HodoWidget::resetVector);
}

} // namespace sharppy
