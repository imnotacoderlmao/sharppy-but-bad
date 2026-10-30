#include "sharppy/srwind_widget.hpp"
#include <cmath>
#include <QPolygonF>

namespace sharppy {

SRWindWidget::SRWindWidget(QWidget* parent) : QWidget(parent) {
    setFixedHeight(160);
}

void SRWindWidget::setResults(const Profile& prof, const BunkersMotion& bunkers, const SevereParams& params) {
    m_prof = prof;
    m_bunkers = bunkers;
    m_params = params;
    m_hasData = !prof.empty();
    update();
}

void SRWindWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), Qt::black);

    p.setPen(QPen(QColor(0, 160, 220), 1));
    p.drawRect(rect().adjusted(0, 0, -1, -1));

    QFont font("Monospace", 6);
    p.setFont(font);

    p.setPen(QColor(180, 180, 180));
    p.drawText(3, 10, "SR Wind");
    p.drawText(3, 18, "v. Height");

    // Height levels (km)
    for (int km = 2; km <= 14; km += 2) {
        double y_norm = km / 16.0;
        int y = height() - static_cast<int>(y_norm * (height() - 25)) - 5;
        p.setPen(QPen(QColor(40, 40, 40), 1, Qt::DotLine));
        p.drawLine(0, y, width(), y);

        p.setPen(QColor(130, 130, 130));
        p.drawText(2, y - 1, QString::number(km));
    }

    if (!m_hasData) return;

    // Classic supercell storm-relative speed window (~15-25 kts at 4-6km)
    p.setPen(QColor(255, 60, 220));
    p.drawText(width() - 48, 12, "Classic");
    p.drawText(width() - 52, 20, "Supercell");

    double max_sr_spd = 50.0;
    QPolygonF poly;

    for (double h = 0.0; h <= 14000.0; h += 200.0) {
        double u, v;
        m_prof.wind_at_agl(h, u, v);

        double sr_spd = std::hypot(u - m_bunkers.right_mover.u, v - m_bunkers.right_mover.v);
        double x_norm = std::min(1.0, sr_spd / max_sr_spd);

        double y_norm = (h / 1000.0) / 16.0;
        double px = 14 + x_norm * (width() - 18);
        double py = height() - y_norm * (height() - 25) - 5;

        poly.append(QPointF(px, py));
    }

    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(QColor(255, 30, 30), 1.8));
    p.drawPolyline(poly);
}

} // namespace sharppy