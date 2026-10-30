#include "sharppy/barbs.hpp"
#include <cmath>
#include <QPainterPath>
#include <QPen>

namespace sharppy {

// Speed ramp (knots): white -> green -> yellow -> red -> pink, then darkening; invalid (= use the
// default foreground) above 150 kt. Linear RGB blend between the stops.
QColor get_barb_color(double wspd) {
    struct Stop { double kt; int r, g, b; };
    static const Stop s[] = {
        {  3, 255, 255, 255}, { 20,   0, 255,   0}, { 45, 255, 255,   0}, { 70, 255,   0,   0},
        { 90, 255, 105, 180}, {120, 176,  40, 110}, {150,  90,   0,  50}};
    constexpr int n = sizeof(s) / sizeof(s[0]);
    if (!(wspd == wspd) || wspd > 150.0) return QColor();
    if (wspd <= s[0].kt) return QColor(s[0].r, s[0].g, s[0].b);
    for (int i = 1; i < n; ++i) {
        if (wspd <= s[i].kt) {
            const double t = (wspd - s[i - 1].kt) / (s[i].kt - s[i - 1].kt);
            auto m = [t](int a, int b) { return static_cast<int>(std::lround(a + (b - a) * t)); };
            return QColor(m(s[i - 1].r, s[i].r), m(s[i - 1].g, s[i].g), m(s[i - 1].b, s[i].b));
        }
    }
    return QColor();
}

static inline void drawFlag(QPainterPath& path, bool shemis) {
    double side = shemis ? -1.0 : 1.0;
    QPointF pos = path.currentPosition();
    path.lineTo(pos.x(), pos.y() + side * 10.0);
    path.lineTo(pos.x() - 4.0, pos.y());
    path.moveTo(pos.x() - 6.0, pos.y());
}

static inline void drawFullBarb(QPainterPath& path, bool shemis) {
    double side = shemis ? -1.0 : 1.0;
    QPointF pos = path.currentPosition();
    path.lineTo(pos.x(), pos.y() + side * 10.0);
    path.moveTo(pos.x() - 4.0, pos.y());
}

static inline void drawHalfBarb(QPainterPath& path, bool shemis) {
    double side = shemis ? -1.0 : 1.0;
    QPointF pos = path.currentPosition();
    path.lineTo(pos.x(), pos.y() + side * 5.0);
    path.moveTo(pos.x() - 4.0, pos.y());
}

void drawBarb(QPainter& qp, double origin_x, double origin_y, double wdir, double wspd,
              const QColor& override_color, bool shemis) {
    QColor col = override_color.isValid() ? override_color : get_barb_color(wspd);
    if (!col.isValid()) col = QColor("#FFFFFF");

    QPen pen(col, 1.0, Qt::SolidLine);
    qp.setPen(pen);
    qp.setBrush(Qt::NoBrush);

    int rounded_spd = static_cast<int>(std::round(wspd / 5.0) * 5.0);

    qp.save();
    qp.translate(origin_x, origin_y);

    if (rounded_spd > 0) {
        qp.rotate(wdir - 90.0);

        QPainterPath path;
        path.moveTo(0.0, 0.0);
        path.lineTo(25.0, 0.0);

        while (rounded_spd >= 50) {
            drawFlag(path, shemis);
            rounded_spd -= 50;
        }
        while (rounded_spd >= 10) {
            drawFullBarb(path, shemis);
            rounded_spd -= 10;
        }
        while (rounded_spd >= 5) {
            drawHalfBarb(path, shemis);
            rounded_spd -= 5;
        }

        qp.drawPath(path);
    } else {
        qp.drawEllipse(QPointF(0.0, 0.0), 3.0, 3.0);
    }

    qp.restore();
}

} // namespace sharppy