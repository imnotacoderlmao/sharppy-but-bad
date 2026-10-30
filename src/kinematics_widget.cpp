#include "sharppy/kinematics_widget.hpp"
#include "sharppy/utils_fmt.hpp"
#include "sharppy/barbs.hpp"
#include <QPainter>
#include <QFontMetrics>
#include <cmath>

namespace sharppy {

namespace {
constexpr double KTS2MS = 0.514444;

bool bad(double v) { return !std::isfinite(v) || v == MISSING; }

QString i2s(double v) { return int2str(v); }   // utils.INT2STR
}

KinematicsWidget::KinematicsWidget(QWidget* parent) : QWidget(parent) {
    setMinimumSize(100, 60);
    initUI();
}

void KinematicsWidget::initUI() {
    const double hgt = height();
    const double fsize = std::floor(.06 * hgt);
    m_tpad = std::floor(.03 * hgt);
    m_labelFont = QFont("Helvetica");
    m_labelFont.setPixelSize(std::max(1, (int)fsize));
    QFontMetrics fm(m_labelFont);
    m_labelHeight = fm.xHeight() + m_tpad;
}

void KinematicsWidget::resizeEvent(QResizeEvent*) { initUI(); }

void KinematicsWidget::setPreferences(const VizPrefs& p) {
    auto it = p.find("wind_units"); if (it != p.end()) m_windUnits = it.value();
    m_bg = prefColor(p, "bg_color", m_bg);
    m_fg = prefColor(p, "fg_color", m_fg);
    update();
}

void KinematicsWidget::setProfile(const Profile& prof, const BunkersMotion& bunkers, const InflowLayer& inflow) {
    setProfile(prof, bunkers, bunkers, inflow, most_unstable_parcel(prof, 300.0));
}

void KinematicsWidget::setProfile(const Profile& prof, const BunkersMotion& bunkersRaw, const BunkersMotion& srwind,
                                  const InflowLayer& inflow, const ParcelResult& mu) {
    m_prof = prof; m_bunkersRaw = bunkersRaw; m_srwind = srwind; m_inflow = inflow; m_mu = mu;
    m_hasData = !prof.empty();
    compute();
    update();
}

void KinematicsWidget::setDeviant(bool use_left) {
    m_useLeft = use_left;
    if (m_hasData) compute();
    update();
}


void KinematicsWidget::compute() {
    const Profile& P = m_prof;
    const double su = m_useLeft ? m_srwind.left_mover.u : m_srwind.right_mover.u;
    const double sv = m_useLeft ? m_srwind.left_mover.v : m_srwind.right_mover.v;
    const double sfc = P.pres.empty() ? MISSING : P.pres[0];

    auto presAgl = [&](double h) { return P.pres_at_hght(P.to_msl(h)); };
    const double p1 = presAgl(1000), p3 = presAgl(3000), p4 = presAgl(4000), p5 = presAgl(5000),
                 p6 = presAgl(6000), p8 = presAgl(8000);

    auto vec = [](double u, double v) { Vec r; if (!bad(u) && !bad(v)) comp2vec(u, v, r.dir, r.spd); return r; };
    auto mean = [&](double pb, double pt) {
        if (bad(pb) || bad(pt)) return Vec();
        WindVector w = mean_wind_pres(P, pb, pt, true);
        return vec(w.u, w.v);
    };
    auto srw = [&](double pb, double pt) {
        if (bad(pb) || bad(pt)) return Vec();
        WindVector w = mean_wind_pres(P, pb, pt, true);
        return vec(w.u - su, w.v - sv);
    };
    auto shear = [&](double pb, double pt) {
        if (bad(pb) || bad(pt)) return (double)MISSING;
        WindVector w = wind_shear_pres(P, pb, pt);
        return w.wspd;
    };
    auto windAt = [&](double p) {
        double u, v; P.wind_at_pres(p, u, v);
        return vec(u, v);
    };
    auto helic = [&](double hb, double ht) {
        return storm_relative_helicity(P, hb, ht, su, sv);
    };

    m_wind1 = windAt(p1); m_wind6 = windAt(p6);

    m_r1 = {helic(0, 1000), shear(sfc, p1), mean(sfc, p1), srw(sfc, p1)};
    m_r3 = {helic(0, 3000), shear(sfc, p3), mean(sfc, p3), srw(sfc, p3)};
    m_r6 = {MISSING, shear(sfc, p6), mean(sfc, p6), srw(sfc, p6)};
    m_r8 = {MISSING, shear(sfc, p8), mean(sfc, p8), srw(sfc, p8)};
    m_lclel = {MISSING, shear(m_mu.lcl_p, m_mu.el_p), mean(m_mu.lcl_p, m_mu.el_p), srw(m_mu.lcl_p, m_mu.el_p)};
    m_srw45 = srw(p4, p5);

    const bool haveEff = !bad(m_inflow.bot_p) && !bad(m_inflow.top_p);
    m_eff = Row(); m_ebw = Row(); m_esrh = MISSING;
    if (haveEff) {
        double elhAgl = MISSING;
        if (!bad(m_mu.el_h_agl)) {
            double depth = (m_mu.el_h_agl - m_inflow.bot_h_agl) / 2.0;
            elhAgl = m_inflow.bot_h_agl + depth;
        }
        double elh = bad(elhAgl) ? MISSING : presAgl(elhAgl);
        m_eff = {MISSING, shear(m_inflow.bot_p, m_inflow.top_p), mean(m_inflow.bot_p, m_inflow.top_p),
                 srw(m_inflow.bot_p, m_inflow.top_p)};
        m_ebw = {MISSING, shear(m_inflow.bot_p, elh), mean(m_inflow.bot_p, elh), srw(m_inflow.bot_p, elh)};
        m_esrh = helic(m_inflow.bot_h_agl, m_inflow.top_h_agl);
    }

    // BRN shear (params.bulk_rich, parcel flag 1..3)
    m_brn = MISSING;
    {
        double ptop = p6, pbot = sfc;
        if (!bad(pbot) && !bad(ptop)) {
            double p500 = P.pres_at_hght(P.hght_at_pres(pbot) + 500.0);
            WindVector lo = mean_wind_pres(P, pbot, p500, true);
            WindVector hi = mean_wind_pres(P, pbot, ptop, true);
            if (!bad(p500) && lo.wspd == lo.wspd && hi.wspd == hi.wspd) {
                double sh = KTS2MS * std::hypot(hi.u - lo.u, hi.v - lo.v);
                m_brn = sh * sh / 2.0;
            }
        }
    }

    m_bunkR = vec(m_bunkersRaw.right_mover.u, m_bunkersRaw.right_mover.v);
    m_bunkL = vec(m_bunkersRaw.left_mover.u, m_bunkersRaw.left_mover.v);
    CorfidiMotion cm = corfidi_mcs_motion(P);
    m_up = vec(cm.upwind.u, cm.upwind.v);
    m_down = vec(cm.downwind.u, cm.downwind.v);
}

void KinematicsWidget::drawFrame(QPainter& qp) {
    qp.setPen(QPen(m_fg, 1, Qt::SolidLine));
    qp.setFont(m_labelFont);
    const double brx = width();
    const double x1 = brx / 10;
    double y1 = m_labelHeight + m_tpad;
    const int lh = (int)m_labelHeight;
    auto R = [&](double x, double y, double w) { return QRect((int)x, (int)y, (int)w, lh); };
    const int fl = Qt::TextDontClip;

    const QString disp = (m_windUnits == "m/s") ? "m/s" : "kt";
    qp.drawText(R(x1 * 2.5, 3, x1), fl | Qt::AlignCenter, "SRH (m2/s2)");
    qp.drawText(R(x1 * 5, 3, x1), fl | Qt::AlignCenter, QString("Shear (%1)").arg(disp));
    qp.drawText(R(x1 * 7, 3, x1), fl | Qt::AlignCenter, "MnWind");
    qp.drawText(R(x1 * 9 - m_rpad, 3, x1), fl | Qt::AlignCenter, "SRW");

    auto block = [&](const QStringList& texts) {
        for (const QString& t : texts) {
            qp.drawText(R(m_lpad, y1, x1), fl | Qt::AlignLeft, t);
            y1 += m_labelHeight;
        }
    };
    block({"SFC-1km", "SFC-3km", "Eff Inflow Layer"});
    y1 += m_tpad;
    block({"SFC-6km", "SFC-8km", "LCL-EL (Cloud Layer)", "Eff Shear (EBWD)"});
    y1 += m_tpad;
    block({"BRN Shear = ", "4-6km SR Wind = "});
    y1 += m_tpad;
    m_barby = y1 + m_tpad;
    block({"...Storm Motion Vectors...", "Bunkers Right = ", "Bunkers Left = ", "Corfidi Downshear = ", "Corfidi Upshear = "});

    // Python sets ylast = vspace here, so the separator sits just under the header
    qp.drawLine(0, (int)(m_labelHeight + 3), (int)brx, (int)(m_labelHeight + 3));
}

void KinematicsWidget::drawKinematics(QPainter& qp) {
    qp.setPen(QPen(m_fg, 1, Qt::SolidLine));
    qp.setFont(m_labelFont);
    const double x1 = width() / 10.0;
    double y1 = m_labelHeight + m_tpad;
    const int lh = (int)m_labelHeight;
    const int fl = Qt::TextDontClip | Qt::AlignRight;

    const bool ms = (m_windUnits == "m/s");
    const QString unit = ms ? " m/s" : " kt";
    auto conv = [&](double s) { return (ms && !bad(s)) ? s * KTS2MS : s; };
    auto vs = [&](const Vec& v) { return i2s(v.dir) + "/" + i2s(conv(v.spd)); };
    auto sh = [&](double s) { return i2s(conv(s)); };

    auto row = [&](const QStringList& texts, int count, double after) {
        for (const QString& t : texts) {
            qp.drawText(QRect((int)(x1 * count), (int)y1, (int)x1, lh), fl, t);
            count += 2;
        }
        y1 += m_labelHeight + after;
    };
    row({i2s(m_r1.srh), sh(m_r1.shear), vs(m_r1.mean), vs(m_r1.srw)}, 3, 0);
    row({i2s(m_r3.srh), sh(m_r3.shear), vs(m_r3.mean), vs(m_r3.srw)}, 3, 0);
    row({i2s(m_esrh), sh(m_eff.shear), vs(m_eff.mean), vs(m_eff.srw)}, 3, m_tpad);
    row({sh(m_r6.shear), vs(m_r6.mean), vs(m_r6.srw)}, 5, 0);
    row({sh(m_r8.shear), vs(m_r8.mean), vs(m_r8.srw)}, 5, 0);
    row({sh(m_lclel.shear), vs(m_lclel.mean), vs(m_lclel.srw)}, 5, 0);
    row({sh(m_ebw.shear), vs(m_ebw.mean), vs(m_ebw.srw)}, 5, m_tpad);

    for (const QString& t : {i2s(m_brn) + " m2/s2", vs(m_srw45) + unit}) {
        qp.drawText(QRect((int)(x1 * 5), (int)y1, (int)x1, lh), fl, t);
        y1 += m_labelHeight;
    }
    m_ylast = y1;
    y1 += m_labelHeight + m_tpad;

    const QString texts[2] = {vs(m_bunkR) + unit, vs(m_bunkL) + unit};
    const QColor colors[2] = {QColor("#0099CC"), QColor("#FF6666")};
    for (int i = 0; i < 2; ++i) {
        qp.setPen(QPen(colors[i], 1, Qt::SolidLine));
        qp.drawText(QRect((int)(x1 * 5), (int)y1, (int)x1, lh), fl, texts[i]);
        y1 += m_labelHeight;
    }
    m_ylast = y1;
    qp.setPen(QPen(m_fg, 1, Qt::SolidLine));
    for (const QString& t : {vs(m_down) + unit, vs(m_up) + unit}) {
        qp.drawText(QRect((int)(x1 * 5), (int)y1, (int)x1, lh), fl, t);
        y1 += m_labelHeight;
    }
}

void KinematicsWidget::drawBarbs(QPainter& qp) {
    const double x1 = width() / 10.0;
    const double originX = x1 * 8.;
    qp.setPen(QPen(QColor("#0A74C6"), 1, Qt::SolidLine));
    qp.setFont(m_labelFont);
    QRect rect0((int)(x1 * 7), (int)(m_ylast + m_tpad), (int)(x1 * 2), (int)m_labelHeight);
    qp.drawText(rect0, Qt::TextDontClip | Qt::AlignCenter, "1km & 6km AGL\nWind Barbs");

    const bool ms = (m_windUnits == "m/s");
    auto conv = [&](double s) { return ms ? s * KTS2MS : s; };
    const bool shemis = !bad(m_prof.latitude) && m_prof.latitude < 0;
    if (!bad(m_wind1.dir) && !bad(m_wind1.spd))
        drawBarb(qp, originX, m_barby, m_wind1.dir, conv(m_wind1.spd), QColor("#AA0000"), shemis);
    if (!bad(m_wind6.dir) && !bad(m_wind6.spd))
        drawBarb(qp, originX, m_barby, m_wind6.dir, conv(m_wind6.spd), QColor("#0A74C6"), shemis);
}

void KinematicsWidget::paintEvent(QPaintEvent*) {
    QPainter qp(this);
    qp.fillRect(rect(), m_bg);
    qp.setPen(QPen(QColor("#3399CC"), 1));
    qp.drawRect(0, 0, width() - 1, height() - 1);
    qp.translate(1, 1);
    qp.setRenderHint(QPainter::Antialiasing);
    qp.setRenderHint(QPainter::TextAntialiasing);
    drawFrame(qp);
    if (!m_hasData) return;
    drawKinematics(qp);
    drawBarbs(qp);
}

} // namespace sharppy
