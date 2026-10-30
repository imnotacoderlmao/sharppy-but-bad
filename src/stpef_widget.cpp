#include "sharppy/stpef_widget.hpp"
#include "sharppy/inset_data.hpp"
#include "sharppy/constants.hpp"
#include <QPainter>
#include <QFontMetrics>
#include <cmath>

namespace sharppy {

STPEFWidget::STPEFWidget(QWidget* parent) : QWidget(parent) { setMinimumHeight(140); }

void STPEFWidget::setPreferences(const VizPrefs& p) {
    m_bg = prefColor(p, "bg_color", m_bg);
    m_fg = prefColor(p, "fg_color", m_fg);
    m_line = prefColor(p, "stp_line_color", m_line);
    update();
}
void STPEFWidget::setStpCin(double s, double lat) { m_stpc = s; m_lat = lat; m_hasData = true; update(); }

STPEFWidget::Geo STPEFWidget::geo() const {
    Geo g;
    g.fs = int(std::lround(height() * 0.0512)) + 2;
    QFont f("Helvetica"); f.setPointSize(std::max(1, g.fs));
    g.plot_h = QFontMetrics(f).xHeight();
    g.tpad = g.plot_h + 15;
    g.bpad = g.plot_h + 2;
    g.wid = width();
    g.hgt = height() - g.bpad;
    g.brx = g.wid;
    g.bry = g.hgt - g.bpad;
    return g;
}
double STPEFWidget::prob_to_pix(const Geo& g, double p) const {
    return g.bry - (p / 70.0) * (g.bry - g.tpad);
}
// stpef.py stpc_to_pix: piecewise bins 0,.01,.5,1,2,3,4,6,8,10,12 mapped across the x axis.
double STPEFWidget::stpc_to_pix(const Geo& g, double s) const {
    static const double b[] = {0, .01, .5, 1, 2, 3, 4, 6, 8, 10, 12};
    const int n = 11;
    double step = g.brx / n;
    if (s <= b[0]) return step * 0.5;
    for (int i = 0; i < n - 1; ++i)
        if (s < b[i + 1]) return step * (i + 0.5) + step * (s - b[i]) / (b[i + 1] - b[i]);
    return step * (n - 0.5);
}

void STPEFWidget::paintEvent(QPaintEvent*) {
    QPainter qp(this);
    qp.fillRect(rect(), m_bg);
    qp.setPen(QPen(QColor("#3399CC"), 1));
    qp.drawRect(0, 0, width() - 1, height() - 1);
    qp.translate(1, 1);
    qp.setRenderHint(QPainter::Antialiasing);
    const Geo g = geo();
    QFont pf("Helvetica"); pf.setPointSize(std::max(1, g.fs));
    while (pf.pointSize() > 5 && QFontMetrics(pf).horizontalAdvance("Conditional EF-scale Probs based on STPC") > g.brx - 4)
        pf.setPointSize(pf.pointSize() - 1);
    qp.setFont(pf);
    qp.setPen(QPen(m_fg, 2));
    qp.drawText(QRectF(0, 5, g.brx, g.plot_h), Qt::TextDontClip | Qt::AlignCenter,
                "Conditional EF-scale Probs based on STPC");

    const auto& d = inset::condSTP();
    int tfs = int(std::lround(0.0512 * g.hgt)) + 1;
    QFont yf("Helvetica"); yf.setPointSize(std::max(1, tfs));
    qp.setFont(yf);
    for (size_t i = 0; i < d.ytexts.size(); ++i) {
        QString t = d.ytexts[i];
        if (t.trimmed().isEmpty()) continue;
        double y = prob_to_pix(g, t.toInt());
        qp.setPen(QPen(m_line, 1, Qt::DashLine));
        qp.drawLine(QPointF(0, y), QPointF(g.brx, y));
        qp.setPen(QPen(m_fg, 1));
        qp.drawText(QRect(0, int(y - tfs / 2), 20, tfs), Qt::TextDontClip | Qt::AlignCenter, t);
    }
    QFont xf("Helvetica"); xf.setPointSize(std::max(1, int(std::lround(0.0512 * g.hgt)) - 1));
    while (xf.pointSize() > 4 && QFontMetrics(xf).horizontalAdvance(".01-.5") > g.brx / 11.0)
        xf.setPointSize(xf.pointSize() - 1);
    qp.setFont(xf);
    double step = g.brx / 11.0;
    for (int i = 0; i < 11; ++i)
        qp.drawText(QRectF(step * i, g.bry + 2, step, g.bpad), Qt::TextDontClip | Qt::AlignCenter, d.xticks[i]);

    struct L { const std::array<double, 11>* v; const char* n; QColor c; };
    const L lines[] = {{&d.ef1, "EF1+", QColor("#ffff00")}, {&d.ef2, "EF2+", QColor("#ff9900")},
                       {&d.ef3, "EF3+", QColor("#ff0000")}, {&d.ef4, "EF4+", QColor("#e700df")}};
    QFont lf(xf); lf.setPointSize(std::max(4, xf.pointSize()));
    qp.setFont(lf);
    for (const auto& l : lines) {
        qp.setPen(QPen(l.c, 2));
        QPointF prev;
        for (int i = 0; i < 11; ++i) {
            QPointF p(step * (i + 0.5), prob_to_pix(g, (*l.v)[i]));
            if (i) qp.drawLine(prev, p);
            prev = p;
        }
        qp.drawText(QPointF(prev.x() - 28, prev.y() + (l.n[2]=='1' ? 12 : -3)), l.n);
    }

    if (!m_hasData) return;
    double s = m_lat < 0 ? -m_stpc : m_stpc;
    if (!std::isfinite(s) || s == MISSING) return;
    double x = stpc_to_pix(g, s);
    qp.setPen(QPen(QColor("#00ff00"), 1.5));
    qp.drawLine(QPointF(x, g.tpad), QPointF(x, g.bry));
}

} // namespace sharppy
