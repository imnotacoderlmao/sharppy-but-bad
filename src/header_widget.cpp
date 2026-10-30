#include "sharppy/header_widget.hpp"
#include <QString>

namespace sharppy {

HeaderWidget::HeaderWidget(QWidget* parent) : QWidget(parent) {
    setFixedHeight(26);
}

void HeaderWidget::setText(const std::string& title, size_t num_levels) {
    m_title = title;
    m_levels = num_levels;
    update();
}

void HeaderWidget::setEditMode(bool enabled) {
    m_editMode = enabled;
    update();
}

void HeaderWidget::setModified(bool is_modified) {
    m_isModified = is_modified;
    update();
}

void HeaderWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), QColor(10, 10, 10));

    // Cyan top & bottom border lines
    p.setPen(QPen(QColor(0, 160, 220), 1.5));
    p.drawLine(0, 0, width(), 0);
    p.drawLine(0, height() - 1, width(), height() - 1);

    QFont fontTitle("Monospace", 9, QFont::Bold);
    p.setFont(fontTitle);

    // 1. Base Title (White)
    p.setPen(Qt::white);
    QString baseText = QString::fromStdString(m_title);
    p.drawText(8, 17, baseText);

    QFontMetrics fm(fontTitle);
    int cur_x = 8 + fm.horizontalAdvance(baseText) + 12;

    // 2. MODIFIED Flag (Bright Yellow/Orange)
    if (m_isModified) {
        p.setPen(QColor(255, 200, 0));
        QString modStr = "(MODIFIED)";
        p.drawText(cur_x, 17, modStr);
        cur_x += fm.horizontalAdvance(modStr) + 10;
    }

    // 3. EDIT MODE Active Badge (Bright Red/Magenta)
    if (m_editMode) {
        p.setPen(QColor(255, 60, 60));
        QString editStr = "[EDIT MODE: ON]";
        p.drawText(cur_x, 17, editStr);
    }

    // 4. Right side software badge
    p.setPen(QColor(160, 160, 160));
    QFont fontSmall("Monospace", 8);
    p.setFont(fontSmall);
    p.drawText(width() - 150, 17, "SHARPpy v0+cpp");
}

} // namespace sharppy