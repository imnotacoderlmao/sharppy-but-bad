#pragma once
#include <QWidget>
#include <QPixmap>
#include <array>
#include "sharppy/viz_prefs.hpp"

namespace sharppy {

// Port of viz/ship.py (plotSHIP).
class SHIPWidget : public QWidget {
    Q_OBJECT
public:
    explicit SHIPWidget(QWidget* parent = nullptr);
    void setShip(double ship);
    void setPreferences(const VizPrefs& prefs);

protected:
    void paintEvent(QPaintEvent*) override;

private:
    struct Geo { double tpad, bpad, brx, bry, wid, hgt, plot_h; };
    Geo geo() const;
    double ship_to_pix(const Geo& g, double ship) const;
    QColor ship_color(double ship) const;

    double m_ship = 0.0;
    bool m_hasData = false;
    QColor m_bg{"#000000"}, m_fg{"#ffffff"}, m_box{"#00ff00"}, m_line{"#0080ff"};
    std::array<QColor, 6> m_alert{{QColor("#775000"), QColor("#996600"), QColor("#ffffff"),
                                   QColor("#ffff00"), QColor("#ff0000"), QColor("#e700df")}};
};

} // namespace sharppy
