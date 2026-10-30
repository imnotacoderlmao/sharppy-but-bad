#pragma once

#include <QPainter>
#include <QColor>

namespace sharppy {

// Wind barb speed ramp (knots); invalid QColor above 150 kt -> caller uses its foreground color
QColor get_barb_color(double wspd);

// Direct 1:1 port of drawBarb from barbs.py
void drawBarb(QPainter& qp, double origin_x, double origin_y, double wdir, double wspd,
              const QColor& override_color = QColor(), bool shemis = false);

} // namespace sharppy