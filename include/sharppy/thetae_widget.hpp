#pragma once
#include <QWidget>
#include <vector>
#include "sharppy/profile.hpp"
#include "sharppy/winds.hpp"
#include "sharppy/viz_prefs.hpp"

namespace sharppy {

// Port of viz/thetae.py (plotThetae): theta-e vs. pressure profile.
class ThetaEWidget : public QWidget {
    Q_OBJECT
public:
    explicit ThetaEWidget(QWidget* parent = nullptr);
    void setProfile(const Profile& prof);
    // legacy overload (Python widget has no storm-motion dependency)
    void setProfile(const Profile& prof, const BunkersMotion&) { setProfile(prof); }
    void setPreferences(const VizPrefs& prefs);
protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
private:
    void updateRange();
    double pres_to_pix(double p) const;
    double theta_to_pix(double t) const;
    double bry() const { return height() - 20.0; }

    std::vector<double> m_pres, m_thetae;
    bool m_has = false;
    double m_tmin = 300., m_tmax = 360.;
    const double m_pmax = 1025., m_pmin = 400.;
    QColor m_bg{"#000000"}, m_fg{"#ffffff"}, m_thte{"#ff0000"};
};

} // namespace sharppy
