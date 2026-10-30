#include "sharppy/hazard_widget.hpp"

namespace sharppy {

HazardWidget::HazardWidget(QWidget* parent) : QWidget(parent) {
    setFixedHeight(160);
}

void HazardWidget::setParams(const SevereParams& params) {
    m_params = params;
    m_hasData = true;
    update();
}

void HazardWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), Qt::black);

    p.setPen(QPen(QColor(0, 160, 220), 1));
    p.drawRect(rect().adjusted(0, 0, -1, -1));

    QFont font("Monospace", 7);
    p.setFont(font);

    p.setPen(QColor(180, 180, 180));
    p.drawText(4, 12, "Psbl Haz. Type");

    if (!m_hasData) return;

    QString hazType;
    QColor hazCol;

    if (m_params.stp_cin >= 4.0) {
        hazType = "PDS TORNADO\nVIOLENT";
        hazCol = QColor(255, 0, 255); // Magenta
    } else if (m_params.stp_cin >= 1.0) {
        hazType = "SUPERCELL\nTORNADO";
        hazCol = QColor(255, 40, 40);  // Red
    } else if (m_params.scp >= 2.0) {
        hazType = "SUPERCELL\nHAIL/WIND";
        hazCol = QColor(255, 180, 0); // Orange
    } else {
        hazType = "MARGINAL\nSEVERE";
        hazCol = QColor(0, 220, 255); // Cyan
    }

    QFont fontHaz("Monospace", 8, QFont::Bold);
    p.setFont(fontHaz);
    p.setPen(hazCol);

    // Centered hazard text
    p.drawText(rect().adjusted(2, 20, -2, -2), Qt::AlignCenter, hazType);
}

} // namespace sharppy