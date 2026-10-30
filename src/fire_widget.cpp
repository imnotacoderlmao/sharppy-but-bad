#include "sharppy/fire_widget.hpp"
#include "sharppy/utils_fmt.hpp"
#include <QFont>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <algorithm>
#include <cmath>

namespace sharppy {

static const QColor RED("#FF0000"), ORANGE("#FF4000"), YELLOW("#FFFF00"), MAGENTA("#E700DF"),
    DBROWN("#775000"), LBROWN("#996600"), GREEN("#00FF00"), DGREEN("#006000");

FireWidget::FireWidget(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(60);
}

void FireWidget::setProfile(const Profile& prof) {
    m_hasData = !prof.empty();
    if (m_hasData) {
        m_d = tab::compute_fire(prof);
        m_hainesHght = m_d.haines_hght;
    }
    update();
}

void FireWidget::setPreferences(const VizPrefs& p) {
    m_bg = prefColor(p, "bg_color", QColor("#000000"));
    m_fg = prefColor(p, "fg_color", QColor("#ffffff"));
    update();
}

FireWidget::Layout FireWidget::layout() const {
    Layout L;
    const int wid = width(), hgt = height();
    const int brx = wid;
    const double ratio = 0.0512;
    const int pt = std::max(1, int(std::nearbyint(ratio * hgt)));      // Python round(): half-even
    L.label = QFont("Helvetica"); L.label.setPointSize(pt);
    L.fosberg = QFont("Helvetica"); L.fosberg.setPointSize(pt + 2);
    QFontMetrics lm(L.label);
    L.lh = lm.xHeight() + L.tpad;
#ifdef Q_OS_WIN
    L.osMod = lm.descent();
#endif
    const int sep = 2, moswindsep = 7;
    int y1 = 2 * L.lh + L.tpad + L.osMod + 4;
    y1 += L.lh + sep + L.osMod;
    L.startDataY = y1 + 3;
    y1 += 3;
    for (int i = 0; i < 7; ++i) y1 += L.lh + moswindsep + L.osMod;
    L.fosbergY = y1 + moswindsep;
    L.hainesY = L.fosbergY + moswindsep + L.osMod + L.lh;
    (void)brx;
    return L;
}

void FireWidget::mousePressEvent(QMouseEvent* e) {
    if (!m_hasData) return;
    const Layout L = layout();
    const QPoint pos = e->pos();
    if (0 <= pos.x() && pos.x() <= width() && L.hainesY <= pos.y() && pos.y() <= L.hainesY + L.lh - L.osMod) {
        m_hainesHght = (m_hainesHght + 1) % 3;
        update();
        if (parentWidget()) parentWidget()->setFocus();
    }
}

void FireWidget::paintEvent(QPaintEvent*) {
    QPainter qp(this);
    qp.fillRect(rect(), m_bg);
    qp.setPen(QPen(QColor("#3399CC"), 1));
    qp.drawRect(0, 0, width() - 1, height() - 1);
    qp.translate(1, 1);

    const Layout L = layout();
    const int wid = width(), brx = wid;
    const int F = Qt::TextDontClip;
    const int sep = 2, moswindsep = 7;
    const int x10 = int(brx / 10.0), x2 = int(brx / 2.0), colw = int(brx / 2.0 - brx / 10.0);

    // ---- backgroundFire.draw_frame ----
    qp.setPen(QPen(m_fg, 1));
    qp.setFont(L.fosberg);
    qp.drawText(QRect(0, L.tpad, wid, L.lh), F | Qt::AlignCenter, "Fire Weather Parameters");
    int y1 = 2 * L.lh + L.tpad + L.osMod + 4;
    qp.setFont(L.label);
    qp.setPen(QPen(QColor("#00CC33"), 1));
    qp.drawText(QRect(x10, y1, colw, L.lh), F | Qt::AlignLeft, "Moisture");
    qp.setPen(QPen(QColor("#0066CC"), 1));
    qp.drawText(QRect(x2, y1, colw, L.lh), F | Qt::AlignRight, "Low-Level Wind");
    y1 += L.lh + sep + L.osMod;
    qp.setPen(QPen(m_fg, 1));
    qp.drawLine(0, y1 + 3, brx, y1 + 3);
    y1 += 3;
    qp.setPen(QPen(QColor("#FF6633"), 1));
    for (int i = 0; i < 7; ++i) {
        if (i == 6) qp.drawText(QRect(0, y1, brx, L.lh), F | Qt::AlignCenter, "Derived Indices");
        y1 += L.lh + moswindsep + L.osMod;
    }
    qp.drawLine(0, y1, brx, y1);

    if (!m_hasData) return;
    qp.setRenderHint(QPainter::Antialiasing);
    qp.setRenderHint(QPainter::TextAntialiasing);

    const tab::FireData& d = m_d;
    auto i2s = [](double v) { return int2str(v); };

    // ---- drawPBLchar ----
    // low-level wind column
    {
        const QString lab[4] = {
            "SFC = " + i2s(d.sfc_dir) + "/" + i2s(d.sfc_spd),
            "0-1 km mean = " + i2s(d.mw01_dir) + "/" + i2s(d.mw01_spd),
            "BL mean = " + i2s(d.mwpbl_dir) + "/" + i2s(d.mwpbl_spd),
            "BL max = " + i2s(d.max_dir) + "/" + i2s(d.max_spd)};
        int y = L.startDataY + 6;
        for (int i = 0; i < 4; ++i) {
            QColor c = m_fg;
            if (i == 3) {                                       // getMaxWindFormat
                const bool masked = std::isnan(d.max_spd);
                const int s = masked ? 0 : int(d.max_spd);
                if (masked || s <= 10) c = DBROWN;
                else if (s <= 20) c = LBROWN;
                else if (s <= 30) c = m_fg;
                else if (s <= 40) c = YELLOW;
                else if (s <= 50) c = RED;
                else c = MAGENTA;
            }
            qp.setFont(L.label);
            qp.setPen(QPen(c, 1));
            qp.drawText(QRect(x2, y, colw, L.lh), F | Qt::AlignRight, lab[i]);
            y += L.lh + moswindsep + L.osMod;
        }
    }
    // moisture column
    int y = L.startDataY + 6;
    {
        const QString lab[4] = {
            "SFC RH = " + i2s(d.sfc_rh) + "%",
            "0-1 km RH = " + i2s(d.rh01km) + "%",
            "BL mean RH = " + i2s(d.pblrh) + "%",
            "PW = " + float2str(d.pwat, 2) + " in"};
        for (int i = 0; i < 4; ++i) {
            QColor c = m_fg;
            if (i == 0) {                                       // getSfcRHFormat
                const double r = d.sfc_rh;
                if (r <= 10) c = MAGENTA;
                else if (r <= 15) c = RED;
                else if (r <= 20) c = YELLOW;
                else if (r <= 30) c = m_fg;
                else if (r <= 35) c = LBROWN;
                else c = DBROWN;
            } else if (i == 3) {                                // getPWColor
                if (d.pwat < 0.5 && d.bplus_fire > 50 && d.sfc_rh < 35) c = RED;
            }
            qp.setFont(L.label);
            qp.setPen(QPen(c, 1));
            qp.drawText(QRect(x10, y, colw, L.lh), F | Qt::AlignLeft, lab[i]);
            y += L.lh + moswindsep + L.osMod;
        }
    }
    qp.setFont(L.label);
    qp.setPen(QPen(m_fg, 1));
    qp.drawText(QRect(0, y, brx, L.lh), F | Qt::AlignCenter,
                "PBL Height = " + float2str(d.pbl_h * 3.2808399, 0) + "ft / " + float2str(d.pbl_h, 0) + "m");

    // ---- drawFosberg ----
    {
        const double f = d.fosberg;
        QColor c;                                               // getFosbergFormat (NaN falls through to MAGENTA)
        if (f < 30) c = DBROWN;
        else if (f < 40) c = LBROWN;
        else if (f < 50) c = m_fg;
        else if (f < 60) c = YELLOW;
        else if (f < 70) c = RED;
        else c = MAGENTA;
        qp.setPen(QPen(c, 1));
        qp.setFont(L.fosberg);
        qp.drawText(QRect(0, L.fosbergY, brx, L.lh - L.osMod), F | Qt::AlignCenter,
                    f == -9999.0 ? QString("Fosberg FWI = M") : "Fosberg FWI = " + i2s(f));
    }
    // ---- drawHainesIndex ----
    {
        static const char* lbl[3] = {"L", "M", "H"};
        const double h = d.haines[m_hainesHght];
        QColor c = h == 2 ? DGREEN : h == 3 ? GREEN : h == 4 ? YELLOW : h == 5 ? ORANGE : h == 6 ? RED : DGREEN;
        qp.setPen(QPen(c, 1));
        qp.setFont(L.fosberg);
        qp.drawText(QRect(0, L.hainesY, brx, L.lh - L.osMod), F | Qt::AlignCenter,
                    QString("Haines Index (") + lbl[m_hainesHght] + ") = " + i2s(h));
    }
}

} // namespace sharppy
