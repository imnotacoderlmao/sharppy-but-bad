#pragma once
#include <QWidget>
#include <array>
#include "sharppy/viz_prefs.hpp"

namespace sharppy {

// Port of viz/vrot.py (plotVROT): tornado-EF probability vs rotational velocity.
class VROTWidget : public QWidget {
    Q_OBJECT
public:
    explicit VROTWidget(QWidget* parent = nullptr);
    void setVrot(double kts);
    void setPreferences(const VizPrefs& prefs);
protected:
    void paintEvent(QPaintEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
private:
    struct Geo { double tpad, bpad, brx, bry, wid, hgt, plot_h; int fs; };
    Geo geo() const;
    double prob_to_pix(const Geo&, double) const;
    double vrot_to_pix(const Geo&, double) const;
    double interp(const std::array<double, 11>&, double v) const;
    double m_vrot = 0; bool m_has = false;
    QColor m_bg{"#000000"}, m_fg{"#ffffff"}, m_line{"#0080ff"};
};

} // namespace sharppy
