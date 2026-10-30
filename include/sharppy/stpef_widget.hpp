#pragma once
#include <QWidget>
#include <array>
#include "sharppy/viz_prefs.hpp"

namespace sharppy {

// Port of viz/stpef.py (plotSTPEF): conditional EF-rating probability vs STP(cin).
class STPEFWidget : public QWidget {
    Q_OBJECT
public:
    explicit STPEFWidget(QWidget* parent = nullptr);
    void setStpCin(double stpc, double latitude = 0.0);
    void setPreferences(const VizPrefs& prefs);
protected:
    void paintEvent(QPaintEvent*) override;
private:
    struct Geo { double tpad, bpad, brx, bry, wid, hgt, plot_h; int fs; };
    Geo geo() const;
    double prob_to_pix(const Geo&, double p) const;
    double stpc_to_pix(const Geo&, double s) const;
    double m_stpc = 0; double m_lat = 0; bool m_hasData = false;
    QColor m_bg{"#000000"}, m_fg{"#ffffff"}, m_line{"#0080ff"};
};

} // namespace sharppy
