#include "sharppy/winter_widget.hpp"
#include "sharppy/utils_fmt.hpp"
#include <QPainter>
#include <QFontMetrics>
#include <cmath>
#include <algorithm>

namespace sharppy {

WinterWidget::WinterWidget(QWidget* parent) : QWidget(parent) { setMinimumHeight(140); }

void WinterWidget::setProfile(const Profile& prof) {
    m_prof = prof; m_has = prof.size() >= 2;
    if (m_has) m_w = compute_winter(prof);
    update();
}
void WinterWidget::setPreferences(const VizPrefs& p) {
    m_bg = prefColor(p, "bg_color", m_bg);
    m_fg = prefColor(p, "fg_color", m_fg);
    m_dgz = prefColor(p, "winter_dgz_color", m_dgz);
    update();
}

static QString i2s(double v) { return int2str(v); }
static QString f2s(double v, int d) { return float2str(v, d); }

void WinterWidget::paintEvent(QPaintEvent*) {
    QPainter qp(this);
    qp.fillRect(rect(), m_bg);
    qp.setPen(QPen(QColor("#3399CC"), 1));
    qp.drawRect(0, 0, width() - 1, height() - 1);
    qp.translate(1, 1);
    qp.setRenderHint(QPainter::Antialiasing);
    qp.setRenderHint(QPainter::TextAntialiasing);

    const double wid = width(), hgt = height(), brx = wid;
    const int lpad = 5, tpad = 3, sep = 5;
    const double ratio = 0.0512;
    QFont label("Helvetica"); label.setPointSize(std::max(1, int(std::lround(hgt * ratio))));
    const int lh = QFontMetrics(label).xHeight() + tpad;
    const int F = Qt::TextDontClip;

    // ---- background (draw_frame) ----
    qp.setFont(label);
    qp.setPen(QPen(m_dgz, 1));
    qp.drawText(QRectF(0, tpad, wid, lh), F | Qt::AlignCenter, "*** DENDRITIC GROWTH ZONE (-12 TO -17 C) ***");
    const double oprh_y1 = 2 * lh + tpad;
    qp.setPen(QPen(m_fg, 1));
    double y1 = 3 * lh + tpad + sep;
    const double layers_y1 = y1;
    y1 = 3.0 * lh + tpad + sep + lh + sep;
    double begin = y1;
    y1 += 2 * (lh + sep);
    qp.drawLine(QPointF(0, y1), QPointF(brx, y1));
    qp.drawLine(QPointF(brx * .48, y1), QPointF(brx * .48, begin));
    y1 += 3;
    const double init_phase_y1 = y1;
    y1 += lh + sep;
    qp.drawLine(QPointF(0, y1), QPointF(brx, y1));
    y1 += 3;
    const double backup = y1;
    y1 += 4 * (lh + sep);
    const double energy_y1 = backup;
    qp.drawLine(QPointF(0, y1), QPointF(brx, y1));
    qp.drawLine(QPointF(brx * .48, y1), QPointF(brx * .48, backup));
    y1 += 4;
    qp.drawText(QRectF(0, y1, wid, lh), F | Qt::AlignCenter, "*** BEST GUESS PRECIP TYPE ***");
    const double precip_type_y1 = y1 + lh + 3;
    const double ptype_tmpf_y1 = precip_type_y1 + lh + 8;

    if (!m_has) return;
    const WinterData& w = m_w;

    // ---- data ----
    {   // precip type
        QFont big("Helvetica"); big.setPointSize(std::max(1, int(std::lround(hgt * ratio)) + 5));
        qp.setPen(QPen(m_fg, 2)); qp.setFont(big);
        qp.drawText(QRectF(0, precip_type_y1, wid, QFontMetrics(big).xHeight() + tpad), F | Qt::AlignCenter,
                    QString::fromStdString(w.precip_type));
        QFont small("Helvetica"); small.setPointSize(std::max(1, int(std::lround(hgt * ratio)) - 1));
        qp.setFont(small);
        qp.drawText(QRectF(0, ptype_tmpf_y1, wid, QFontMetrics(small).xHeight() + tpad), F | Qt::AlignCenter,
                    "Based on SFC Temperature of " + f2s(w.sfc_tmpf, 2) + " F");
    }
    qp.setPen(QPen(m_fg, 1)); qp.setFont(label);
    qp.drawText(QRectF(0, oprh_y1, wid, lh), F | Qt::AlignCenter, "OPRH (Omega*PW*RH): N/A");   // no omega data

    if (w.plevel > 100) {
        double h = m_prof.hght_at_pres(w.plevel) * 3.28084;
        qp.drawText(QRectF(lpad, init_phase_y1, wid / 10, lh), F | Qt::AlignLeft,
                    "Inital Phase: " + QString::fromStdString(w.init_st) + " from: " + i2s(w.plevel) + " mb (" +
                    i2s(h) + " ft msl; " + f2s(w.tmp, 1) + " C)");
    } else {
        qp.drawText(QRectF(lpad, init_phase_y1, wid / 10, lh), F | Qt::AlignLeft,
                    "Initial Phase:  No Precipitation layers found.");
    }

    {   // DGZ layer
        double y = layers_y1;
        const QStringList L = {"Layer Depth: " + i2s(w.dgz_depth_ft) + " ft (" + i2s(w.dgz_zbot_ft) + "-" +
                                   i2s(w.dgz_ztop_ft) + " ft msl)",
                               "Mean Layer RH: " + f2s(w.dgz_meanrh, 0) + " %",
                               "Mean Layer PW: " + f2s(w.dgz_pw, 1) + " in"};
        for (const QString& s : L) { qp.drawText(QRectF(lpad, y, wid / 10, lh), F | Qt::AlignLeft, s); y += lh + sep; }
        y = layers_y1 + lh + sep;
        const QStringList R = {"Mean Layer MixRat: " + f2s(w.dgz_meanq, 1) + " g/kg", "Mean Layer Omega: N/A"};
        for (const QString& s : R) { qp.drawText(QRectF(brx / 2 + 2, y, wid / 10, lh), F | Qt::AlignLeft, s); y += lh + sep; }
    }

    auto energy = [&](double x, const QString& head, double pos, double neg, double top, double bot) {
        QStringList L;
        if (pos > 0 && neg < 0)
            L = QStringList{head, "P/N: " + f2s(pos, 0) + " / " + f2s(neg, 0) + " J/kg",
                 "Melt Lyr: " + QString::number((int)top) + "-" + QString::number((int)bot) + " mb",
                 "Frz Lyr: " + QString::number((int)bot) + "-" + QString::number((int)m_prof.sfc_pres) + " mb"};
        else L = QStringList{head, "", "Warm/Cold layers not found.", ""};
        double y = energy_y1;
        for (const QString& s : L) { qp.drawText(QRectF(x, y, wid / 10, lh), F | Qt::AlignLeft, s); y += lh + sep; }
    };
    energy(lpad, "TEMPERATURE PROFILE", w.tpos, w.tneg, w.ttop, w.tbot);
    energy(brx / 2 + 2, "WETBULB PROFILE", w.wpos, w.wneg, w.wtop, w.wbot);
}

} // namespace sharppy
