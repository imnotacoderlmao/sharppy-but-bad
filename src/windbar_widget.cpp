#include "sharppy/windbar_widget.hpp"
#include <cmath>

namespace sharppy {

WindBarWidget::WindBarWidget(QWidget* parent) : QWidget(parent) {
    setFixedWidth(50);
}

void WindBarWidget::setProfile(const Profile& prof) {
    m_prof = prof;
    m_hasData = !prof.empty();
    update();
}

double WindBarWidget::presToY(double pres) const {
    double h = height() - 25.0;
    double log_bot = std::log(m_pBot);
    double log_top = std::log(m_pTop);
    double y_norm = (std::log(pres) - log_bot) / (log_top - log_bot);
    return (1.0 - y_norm) * h;
}

void WindBarWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), Qt::black);

    p.setPen(QPen(QColor(0, 160, 220), 1.5));
    p.drawRect(rect().adjusted(0, 0, -1, -1));

    QFont font("Monospace", 6);
    p.setFont(font);

    // Title & Dotted Reference Speed Lines (40, 80, 120 kts)
    double plot_w = width() - 2;
    double x40  = 2 + (40.0 / 120.0) * (plot_w - 4);
    double x80  = 2 + (80.0 / 120.0) * (plot_w - 4);

    p.setPen(QPen(QColor(60, 60, 60), 1, Qt::DotLine));
    p.drawLine(x40, 14, x40, height() - 25);
    p.drawLine(x80, 14, x80, height() - 25);

    p.setPen(QColor(180, 180, 180));
    p.drawText(2, 10, "Wind Speed");
    p.drawText(x40 - 6, height() - 8, "40");
    p.drawText(x80 - 6, height() - 8, "80");
    p.drawText(width() - 18, height() - 8, "120");

    if (!m_hasData || m_prof.size() < 2) return;

    // Draw horizontal colored speed bars per discrete level
    for (size_t i = 0; i < m_prof.size(); ++i) {
        double pres = m_prof.pres[i];
        if (pres < m_pTop || pres > m_pBot) continue;

        double spd = m_prof.wspd[i];
        if (spd == MISSING) continue;

        double y = presToY(pres);
        double h_agl = m_prof.to_agl(m_prof.hght[i]);

        // Exact SHARPpy Layer Colors
        QColor col;
        if (h_agl < 1000.0)       col = QColor(255, 0, 255); // Magenta
        else if (h_agl < 3000.0)  col = QColor(255, 255, 0); // Yellow
        else if (h_agl < 6000.0)  col = QColor(255, 60, 0);   // Red/Orange
        else if (h_agl < 9000.0)  col = QColor(0, 230, 255); // Cyan
        else                      col = QColor(0, 140, 255); // Blue

        double len = std::min(plot_w - 4.0, (spd / 120.0) * (plot_w - 4.0));

        p.setPen(QPen(col, 1.5));
        p.drawLine(2, y, 2 + len, y);
    }
}

} // namespace sharppy