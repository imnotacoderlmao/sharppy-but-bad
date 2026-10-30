#pragma once
#include <QWidget>
#include <vector>
#include "sharppy/profile.hpp"
#include "sharppy/viz_prefs.hpp"

namespace sharppy {

// Port of viz/advection.py (plotAdvection) + params.inferred_temp_adv.
class AdvBarWidget : public QWidget {
    Q_OBJECT
public:
    explicit AdvBarWidget(QWidget* parent = nullptr);
    void setProfile(const Profile& prof);
    void setPreferences(const VizPrefs& prefs);
    void setPresBounds(double, double) {}   // advection.py uses fixed 1050-100 hPa axes
protected:
    void paintEvent(QPaintEvent*) override;
private:
    struct Layer { double ptop, pbot, val; };   // pbot/ptop as in pressure_bounds[i] = (bottom, top)
    std::vector<Layer> m_layers;
    bool m_has = false;
    QColor m_bg{"#000000"}, m_fg{"#ffffff"};
    const double m_pmax = 1050., m_pmin = 100., m_adv_max = 13., m_adv_min = 0.;  // advection.py sets adv_min = 0
    double bry() const { return height() - 20.0; }
    double pres_to_pix(double p) const;
    double adv_to_pix(double a) const;
};

} // namespace sharppy
