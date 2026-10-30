#include "sharppy/ship_widget.hpp"
#include "sharppy/inset_data.hpp"
#include "sharppy/constants.hpp"
#include <QPainter>
#include <QFont>
#include <QFontMetrics>
#include <cmath>

namespace sharppy {

SHIPWidget::SHIPWidget(QWidget* parent) : QWidget(parent) { setMinimumHeight(140); }

void SHIPWidget::setShip(double ship) { m_ship = ship; m_hasData = true; update(); }

void SHIPWidget::setPreferences(const VizPrefs& p) {
    m_bg = prefColor(p, "bg_color", m_bg);
    m_fg = prefColor(p, "fg_color", m_fg);
    m_box = prefColor(p, "stp_box_color", m_box);
    m_line = prefColor(p, "stp_line_color", m_line);
    for (int i = 0; i < 6; ++i)
        m_alert[i] = prefColor(p, QString("alert_l%1_color").arg(i + 1).toLatin1().constData(), m_alert[i]);
    update();
}

SHIPWidget::Geo SHIPWidget::geo() const {
    const double font_ratio = 0.0512;
    int fsize1 = int(std::lround(height() * font_ratio)) + 2;
    QFont pf("Helvetica"); pf.setPointSize(std::max(1, fsize1));
    Geo g;
    g.plot_h = QFontMetrics(pf).xHeight();
    g.tpad = g.plot_h + 15;
    g.bpad = g.plot_h + 2;
    g.wid = width();
    g.hgt = height() - g.bpad;
    g.brx = g.wid;
    g.bry = g.hgt - g.bpad;
    return g;
}

double SHIPWidget::ship_to_pix(const Geo& g, double ship) const {
    return g.bry - (ship / 5.0) * (g.bry - g.tpad);
}

QColor SHIPWidget::ship_color(double ship) const {
    if (ship >= 5) return m_alert[5];
    if (ship >= 2) return m_alert[4];
    if (ship >= 1) return m_alert[3];
    if (ship >= .5) return m_alert[2];
    return m_alert[0];
}

void SHIPWidget::paintEvent(QPaintEvent*) {
    QPainter qp(this);
    qp.fillRect(rect(), m_bg);
    // QFrame border: 1px solid #3399CC
    qp.setPen(QPen(QColor("#3399CC"), 1));
    qp.drawRect(0, 0, width() - 1, height() - 1);
    qp.translate(1, 1);
    qp.setRenderHint(QPainter::Antialiasing);
    qp.setRenderHint(QPainter::TextAntialiasing);

    const Geo g = geo();
    const double font_ratio = 0.0512;
    int fsize1 = int(std::lround(height() * font_ratio)) + 2;
    QFont plot_font("Helvetica"); plot_font.setPointSize(std::max(1, fsize1));

    qp.setPen(QPen(m_fg, 2));
    qp.setFont(plot_font);
    qp.drawText(QRectF(1.5, 6, g.brx, g.plot_h), Qt::TextDontClip | Qt::AlignCenter,
                "Significant Hail Param (SHIP)");

    int ytick_fs = int(std::lround(font_ratio * g.hgt));
    QFont yf("Helvetica"); yf.setPointSize(std::max(1, ytick_fs));
    qp.setFont(yf);
    const auto& yt = inset::shipYTexts();
    for (size_t i = 0; i < yt.size(); ++i) {
        bool ok = false;
        int v = QString::fromStdString(yt[i]).toInt(&ok);
        if (!ok) continue;                       // python: int(' ') raises -> continue
        double y = ship_to_pix(g, v);
        qp.setPen(QPen(m_line, 1, Qt::DashLine));
        qp.drawLine(QPointF(0, y), QPointF(g.brx, y));
        qp.setPen(QPen(m_fg, 1));
        qp.drawText(QRect(0, int(y - ytick_fs / 2), 20, ytick_fs),
                    Qt::TextDontClip | Qt::AlignCenter, QString::fromStdString(yt[i]));
    }

    const auto& dist = inset::shipDist();
    const auto& xt = inset::shipXTexts();
    double width_ = g.brx / 3.7, spacing = g.brx / 3.0;
    qp.setFont(QFont([&]{ QFont f("Helvetica"); f.setPointSize(std::max(1, ytick_fs)); return f; }()));
    for (size_t i = 0; i < dist.size(); ++i) {
        double cx = spacing * (i + 1);
        if (cx >= g.brx) break;                  // np.arange(spacing, brx, spacing)
        double e[5];
        for (int k = 0; k < 5; ++k) e[k] = ship_to_pix(g, dist[i][k]);
        qp.setPen(QPen(m_box, 2));
        qp.drawLine(QPointF(cx, e[0]), QPointF(cx, e[1]));
        qp.drawLine(QPointF(cx - width_ / 2, e[3]), QPointF(cx + width_ / 2, e[3]));
        qp.drawLine(QPointF(cx - width_ / 2, e[1]), QPointF(cx + width_ / 2, e[1]));
        qp.drawLine(QPointF(cx - width_ / 2, e[1]), QPointF(cx - width_ / 2, e[3]));
        qp.drawLine(QPointF(cx + width_ / 2, e[1]), QPointF(cx + width_ / 2, e[3]));
        qp.drawLine(QPointF(cx, e[3]), QPointF(cx, e[4]));
        qp.setPen(QPen(m_fg, 1));
        qp.drawText(QRectF(cx - width_ / 2, g.bry + g.bpad / 2, width_, g.bpad),
                    Qt::TextDontClip | Qt::AlignCenter, QString::fromStdString(xt[i]));
    }

    if (m_hasData && std::isfinite(m_ship) && m_ship != MISSING) {
        double s = std::min(5.0, std::max(0.0, m_ship));
        double y = ship_to_pix(g, s);
        qp.setPen(QPen(ship_color(s), 1.5));
        qp.drawLine(QPointF(0, y), QPointF(g.wid, y));
    }
}

} // namespace sharppy
