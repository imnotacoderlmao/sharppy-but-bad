#include "sharppy/stpbox_widget.hpp"
#include "sharppy/utils_fmt.hpp"
#include "sharppy/inset_data.hpp"
#include "sharppy/constants.hpp"
#include <QPainter>
#include <QFontMetrics>
#include <QFont>
#include <QFontMetrics>
#include <cmath>
#include <limits>

namespace sharppy {

static const double NaN = std::numeric_limits<double>::quiet_NaN();

STPBoxWidget::STPBoxWidget(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(140);
    recompute();
}

void STPBoxWidget::setPreferences(const VizPrefs& p) {
    m_bg = prefColor(p, "bg_color", m_bg);
    m_fg = prefColor(p, "fg_color", m_fg);
    m_box = prefColor(p, "stp_box_color", m_box);
    m_line = prefColor(p, "stp_line_color", m_line);
    for (int i = 0; i < 6; ++i)
        m_alert[i] = prefColor(p, QString("alert_l%1_color").arg(i + 1).toLatin1().constData(), m_alert[i]);
    recompute();
    update();
}

void STPBoxWidget::setInputs(double mlcape, double mllcl, double ebwspd,
                             double esrh, double stpc, double stpf, double latitude) {
    m_mlcape = mlcape; m_mllcl = mllcl; m_ebwd = ebwspd;
    m_esrh = esrh; m_stpc = stpc; m_stpf = stpf; m_lat = latitude;
    m_hasData = true;
    recompute();
    update();
}

void STPBoxWidget::recompute() {
    double esrh = m_esrh, stpc = m_stpc, stpf = m_stpf;
    if (m_lat < 0 && m_lat != MISSING) { esrh = -esrh; stpc = -stpc; stpf = -stpf; }
    m_cape = cape_prob(m_mlcape);
    m_lcl = lcl_prob(m_mllcl);
    m_esrhp = esrh_prob(esrh);
    m_ebwdp = ebwd_prob(m_ebwd);
    m_stpcp = stpc_prob(stpc);
    m_stpfp = stpf_prob(stpf);
}

// ---- probability tables, copied from stp.py ----------------------------------------------
STPBoxWidget::Pc STPBoxWidget::cape_prob(double c) const {
    const auto& A = m_alert;
    if (std::isnan(c)) return {NaN, A[0]};
    if (c == 0.) return {0.00, A[0]};
    if (c > 0. && c < 250.) return {.12, A[1]};
    if (c < 500.) return {.14, A[2]};
    if (c < 1000.) return {.16, A[2]};
    if (c < 1500.) return {.15, A[2]};
    if (c < 2000.) return {.13, A[2]};
    if (c < 2500.) return {.14, A[2]};
    if (c < 3000.) return {.18, A[3]};
    if (c < 4000.) return {.20, A[3]};
    if (c >= 4000.) return {.16, A[3]};
    return {NaN, A[0]};   // negative CAPE: python falls through to masked
}
STPBoxWidget::Pc STPBoxWidget::lcl_prob(double l) const {
    const auto& A = m_alert;
    if (std::isnan(l)) return {NaN, A[0]};
    if (l < 750.) return {.19, A[3]};
    if (l < 1000.) return {.19, A[3]};
    if (l < 1250.) return {.15, A[2]};
    if (l < 1500.) return {.10, A[1]};
    if (l < 1750.) return {.06, A[0]};
    if (l < 2000.) return {.06, A[0]};
    if (l < 2500.) return {.02, A[0]};
    return {0.0, A[0]};
}
STPBoxWidget::Pc STPBoxWidget::esrh_prob(double e) const {
    const auto& A = m_alert;
    if (std::isnan(e)) return {NaN, A[0]};
    if (e < 50.) return {.06, A[0]};
    if (e < 100.) return {.06, A[0]};
    if (e < 200.) return {.08, A[1]};
    if (e < 300.) return {.14, A[2]};
    if (e < 400.) return {.20, A[3]};
    if (e < 500.) return {.27, A[3]};
    if (e < 600.) return {.38, A[4]};
    if (e < 700.) return {.37, A[4]};
    return {.42, A[4]};
}
STPBoxWidget::Pc STPBoxWidget::ebwd_prob(double b) const {
    const auto& A = m_alert;
    if (std::isnan(b)) return {NaN, A[0]};
    if (b == 0.) return {0.0, A[0]};
    if (b >= .01 && b < 20.) return {.03, A[0]};
    if (b >= 20. && b < 30.) return {.05, A[0]};
    if (b >= 30. && b < 40.) return {.06, A[0]};
    if (b >= 40. && b < 50.) return {.12, A[1]};
    if (b >= 50. && b < 60.) return {.19, A[3]};
    if (b >= 60. && b < 70.) return {.27, A[3]};
    if (b >= 70. && b < 80.) return {.36, A[4]};
    if (b >= 80.) return {.26, A[3]};
    return {NaN, A[0]};
}
STPBoxWidget::Pc STPBoxWidget::stpc_prob(double s) const {
    const auto& A = m_alert;
    if (std::isnan(s)) return {NaN, A[0]};
    if (s < .1) return {.06, A[0]};
    if (s < .50) return {.08, A[1]};
    if (s < 1.0) return {.12, A[1]};
    if (s < 2.) return {.17, A[2]};
    if (s < 4.) return {.25, A[3]};
    if (s < 6.) return {.32, A[4]};
    if (s < 8.) return {.34, A[4]};
    if (s < 10.) return {.55, A[5]};
    return {.58, A[5]};
}
STPBoxWidget::Pc STPBoxWidget::stpf_prob(double s) const {
    const auto& A = m_alert;
    if (std::isnan(s)) return {NaN, A[0]};
    if (s < .1) return {.05, A[0]};
    if (s < .5) return {.06, A[0]};
    if (s < 1.) return {.11, A[1]};
    if (s < 2.) return {.17, A[2]};
    if (s < 3.) return {.25, A[3]};
    if (s < 5.) return {.25, A[3]};
    if (s < 7.) return {.39, A[4]};
    if (s < 9.) return {.55, A[5]};
    return {.59, A[5]};
}

// ---- geometry ----------------------------------------------------------------------------
STPBoxWidget::Geo STPBoxWidget::geo() const {
    const double font_ratio = 0.0512;
    Geo g;
    g.fs1 = int(std::lround(height() * font_ratio)) + 2;
    g.fs2 = int(std::lround(height() * font_ratio));
    QFont pf("Helvetica"); pf.setPointSize(std::max(1, g.fs1));
    QFont bf("Helvetica"); bf.setPointSize(std::max(1, g.fs2));
    g.plot_h = QFontMetrics(pf).xHeight();
    g.box_h = QFontMetrics(bf).xHeight() + 5;
    g.tpad = g.plot_h + 15;
    g.bpad = g.plot_h + 2;
    g.wid = width();
    g.hgt = height() - g.bpad;
    g.brx = g.wid;
    g.bry = g.hgt - g.bpad;
    return g;
}
double STPBoxWidget::stp_to_pix(const Geo& g, double v) const {
    return g.bry - (v / 11.0) * (g.bry - g.tpad);
}

static QString f2s(double v, int d) { return float2str(v, d); }       // utils.FLOAT2STR

void STPBoxWidget::paintEvent(QPaintEvent*) {
    QPainter qp(this);
    qp.fillRect(rect(), m_bg);
    qp.setPen(QPen(QColor("#3399CC"), 1));
    qp.drawRect(0, 0, width() - 1, height() - 1);
    qp.translate(1, 1);
    qp.setRenderHint(QPainter::Antialiasing);
    qp.setRenderHint(QPainter::TextAntialiasing);

    const Geo g = geo();
    const double font_ratio = 0.0512;
    QFont plot_font("Helvetica"); plot_font.setPointSize(std::max(1, g.fs1));
    QFont box_font("Helvetica"); box_font.setPointSize(std::max(1, g.fs2));

    // --- background (draw_frame) ---
    qp.setPen(QPen(m_fg, 2));
    while (plot_font.pointSize() > 5 && QFontMetrics(plot_font).horizontalAdvance("Effective Layer STP (with CIN)") > g.brx - 4)
        plot_font.setPointSize(plot_font.pointSize() - 1);
    qp.setFont(plot_font);
    qp.drawText(QRectF(0, 5, g.brx, g.plot_h), Qt::TextDontClip | Qt::AlignCenter,
                "Effective Layer STP (with CIN)");

    int ytick_fs = int(std::lround(font_ratio * g.hgt)) + 1;
    QFont yf("Helvetica"); yf.setPointSize(std::max(1, ytick_fs));
    qp.setFont(yf);
    for (const auto& t : inset::stpYTexts()) {
        int v = QString::fromStdString(t).toInt();
        double y = stp_to_pix(g, v);
        qp.setPen(QPen(m_line, 1, Qt::DashLine));
        qp.drawLine(QPointF(0, y), QPointF(g.brx, y));
        qp.setPen(QPen(m_fg, 1));
        qp.drawText(QRect(0, int(y - ytick_fs / 2), 20, ytick_fs),
                    Qt::TextDontClip | Qt::AlignCenter, QString::fromStdString(t));
    }

    QFont xf("Helvetica"); xf.setPointSize(std::max(1, int(std::lround(font_ratio * g.hgt))));
    while (xf.pointSize() > 4 && QFontMetrics(xf).horizontalAdvance("NONTOR") > g.brx / 7.0 - 2)
        xf.setPointSize(xf.pointSize() - 1);
    qp.setFont(xf);
    const auto& ef = inset::stpEf();
    const auto& xt = inset::stpXTexts();
    double width_ = g.brx / 14.0, spacing = g.brx / 7.0;
    for (size_t i = 0; i < ef.size(); ++i) {
        double cx = spacing * (i + 1);
        if (cx >= g.brx) break;
        double e[5];
        for (int k = 0; k < 5; ++k) e[k] = stp_to_pix(g, ef[i][k]);
        qp.setPen(QPen(m_box, 2));
        qp.drawLine(QPointF(cx, e[0]), QPointF(cx, e[1]));
        qp.drawLine(QPointF(cx - width_ / 2, e[3]), QPointF(cx + width_ / 2, e[3]));
        qp.drawLine(QPointF(cx - width_ / 2, e[1]), QPointF(cx + width_ / 2, e[1]));
        qp.drawLine(QPointF(cx - width_ / 2, e[1]), QPointF(cx - width_ / 2, e[3]));
        qp.drawLine(QPointF(cx + width_ / 2, e[1]), QPointF(cx + width_ / 2, e[3]));
        qp.drawLine(QPointF(cx - width_ / 2, e[2]), QPointF(cx + width_ / 2, e[2]));  // median
        qp.drawLine(QPointF(cx, e[3]), QPointF(cx, e[4]));
        qp.setPen(QPen(m_fg, 1));
        qp.drawText(QRectF(cx - spacing / 2, g.bry + std::round(g.bpad / 2), spacing, g.bpad),
                    Qt::TextDontClip | Qt::AlignCenter, QString::fromStdString(xt[i]));
    }

    if (!m_hasData) return;

    // --- draw_stp: line at the (clamped) STPC value ---
    {
        double s = m_lat < 0 ? -m_stpc : m_stpc;
        if (std::isfinite(s) && s != MISSING) {
            if (s < 0) s = 0; else if (s > 11.) s = 11.;
            QColor c = stpc_prob(s).second;
            double y = stp_to_pix(g, s);
            qp.setPen(QPen(c, 1.5));
            qp.drawLine(QPointF(0, y), QPointF(g.wid, y));
        }
    }

    // --- draw_box ---
    double left_x = width_ * 7, right_x = g.brx - 5.;
    double top_y = stp_to_pix(g, 11.);
    double vspace = g.box_h + 1;
    double bot_y = top_y + vspace * 8;
    qp.setPen(QPen(m_bg, 0));
    qp.setBrush(QBrush(m_bg));
    qp.drawRect(QRectF(left_x, top_y, right_x - left_x, bot_y - top_y));
    qp.setPen(QPen(m_fg, 2));
    qp.setBrush(Qt::NoBrush);
    qp.drawLine(QPointF(left_x, top_y), QPointF(right_x, top_y));
    qp.drawLine(QPointF(left_x, bot_y), QPointF(right_x, bot_y));
    qp.drawLine(QPointF(left_x, top_y), QPointF(left_x, bot_y));
    qp.drawLine(QPointF(right_x, top_y), QPointF(right_x, bot_y));
    double w = right_x - left_x - 3;
    while (box_font.pointSize() > 4 && QFontMetrics(box_font).horizontalAdvance("Prob EF2+ torn with supercell") > w)
        box_font.setPointSize(box_font.pointSize() - 1);
    qp.setFont(box_font);
    double y1 = top_y + 2, x1 = left_x + 3, x2 = x1 + w * .72;
    for (const char* t : {"Prob EF2+ torn with supercell", "Sample CLIMO = .15 sigtor"}) {
        qp.drawText(QRectF(x1, y1, w, g.box_h), Qt::TextDontClip | Qt::AlignLeft, t);
        y1 += g.box_h + 1;
    }
    qp.drawLine(QPointF(left_x, y1 - 1), QPointF(right_x, y1 - 1));
    const char* labels[] = {"based on CAPE: ", "based on LCL:", "based on ESRH:",
                            "based on EBWD:", "based on STPC:", "based on STP_fixed:"};
    const Pc probs[] = {m_cape, m_lcl, m_esrhp, m_ebwdp, m_stpcp, m_stpfp};
    for (int i = 0; i < 6; ++i) {
        qp.setPen(QPen(probs[i].second, 1));
        qp.drawText(QRectF(x1, y1, w, g.box_h), Qt::TextDontClip | Qt::AlignLeft, labels[i]);
        qp.drawText(QRectF(x2, y1, w, g.box_h), Qt::TextDontClip | Qt::AlignLeft, f2s(probs[i].first, 2));
        y1 += g.box_h;
    }
}

} // namespace sharppy
