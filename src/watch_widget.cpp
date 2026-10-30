#include "sharppy/watch_widget.hpp"
#include <QPainter>
#include <QFontMetrics>
#include <cmath>

namespace sharppy {

WatchWidget::WatchWidget(QWidget* parent) : QWidget(parent) { setMinimumHeight(60); }

void WatchWidget::setWatchType(const QString& t) { m_type = t; m_has = true; update(); }

void WatchWidget::setPreferences(const VizPrefs& p) {
    m_bg = prefColor(p, "bg_color", m_bg);
    m_fg = prefColor(p, "fg_color", m_fg);
    m_pds = prefColor(p, "watch_pdstor_color", m_pds);
    m_tor = prefColor(p, "watch_tor_color", m_tor);
    m_svr = prefColor(p, "watch_svr_color", m_svr);
    m_mrgl = prefColor(p, "watch_mrglsvr_color", m_mrgl);
    m_flood = prefColor(p, "watch_flood_color", m_flood);
    m_blizz = prefColor(p, "watch_blizzard_color", m_blizz);
    m_fire = prefColor(p, "watch_fire_color", m_fire);
    m_heat = prefColor(p, "watch_heat_color", m_heat);
    m_none = prefColor(p, "watch_none_color", m_none);
    update();
}

QColor WatchWidget::colorFor(const QString& t) const {
    if (t == "are you\n in jupiter?") return m_wtf;
    if (t == "WHAT THE FUCK" || t == "BIG WEDGE\n OMGGGG") return m_pds;
    if (t == "OMG TOR" || t == "TOR???") return m_tor;
    if (t == "STORMS") return m_svr;
    if (t == "WEAKASS STORMS") return m_mrgl;
    if (t == "HUGE WATER") return m_flood;
    if (t == "SNOWY WIND" || t == "ABSOLUTE ZERO" || t == "ANTARTICA") return m_blizz;
    if (t == "COMBUSTION") return m_fire;
    if (t == "THE SUN") return m_heat;
    return m_none;
}

void WatchWidget::paintEvent(QPaintEvent*) {
    QPainter qp(this);
    qp.fillRect(rect(), m_bg);
    qp.setPen(QPen(QColor("#3399CC"), 1));
    qp.drawRect(0, 0, width() - 1, height() - 1);
    qp.translate(1, 1);
    qp.setRenderHint(QPainter::Antialiasing);
    qp.setRenderHint(QPainter::TextAntialiasing);

    const double ratio = 0.0512;
    QFont title("Helvetica"); title.setPointSize(std::max(1, int(std::lround(height() * ratio)) + 5));
    QFont plot("Helvetica");  plot.setPointSize(std::max(1, int(std::lround(height() * ratio)) + 4));
    const int th = QFontMetrics(title).height();
    const double bry = height() - 20, brx = width();
    const double pad = bry / 100.;

    // frame (draw_frame): 2px lines in fg + title + underline
    qp.setPen(QPen(m_fg, 2));
    qp.setFont(title);
    qp.drawLine(QPointF(0, 0), QPointF(brx, 0));
    qp.drawLine(QPointF(brx, 0), QPointF(brx, bry));
    qp.drawLine(QPointF(brx, bry), QPointF(0, bry));
    qp.drawLine(QPointF(0, bry), QPointF(0, 0));
    qp.drawText(QRectF(0, pad * 4, brx, th), Qt::TextDontClip | Qt::AlignCenter, "Psbl Haz. Type");
    qp.setPen(QPen(m_fg, 1));
    qp.drawLine(QPointF(0, pad * 4 + th + 3), QPointF(brx, pad * 4 + th + 3));

    if (!m_has) return;
    qp.setPen(QPen(colorFor(m_type), 1));
    qp.setFont(plot);
    qp.drawText(QRectF(0, bry / 2., brx, th), Qt::TextDontClip | Qt::AlignCenter, m_type);
}

} // namespace sharppy
