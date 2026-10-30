#pragma once
#include <QWidget>
#include <QColor>
#include <QString>
#include "sharppy/profile.hpp"
#include "sharppy/viz_prefs.hpp"

namespace sharppy {

class SkewTWidget;

// Port of viz/speed.py (plotSpeed): wind speed vs. height column beside the Skew-T.
class SpeedWidget : public QWidget {
    Q_OBJECT
public:
    explicit SpeedWidget(QWidget* parent = nullptr);
    void setProfile(const Profile& prof);
    void setSkewT(const SkewTWidget* skewt) { m_skewt = skewt; }
    void setPresBounds(double, double) { update(); }   // Skew-T zoom changed: redraw (y is read live from the Skew-T)
    void setPreferences(const VizPrefs& prefs);
protected:
    void paintEvent(QPaintEvent*) override;
private:
    Profile m_prof; bool m_has = false;
    const SkewTWidget* m_skewt = nullptr;
    QColor m_bg{"#000000"}, m_fg{"#ffffff"}, m_isotach{"#9D5736"};
    QColor m_c01{"#FF00FF"}, m_c13{"#FF0000"}, m_c36{"#00FF00"}, m_c69{"#FFFF00"}, m_c912{"#00FFFF"};
    QString m_units = "knots";
    double smax() const { return m_units == "m/s" ? 80. : 140.; }
    double delta() const { return m_units == "m/s" ? 10. : 20.; }
    double bry() const { return height() - 20.0; }
    double pres_to_pix(double p) const;
    double speed_to_pix(double s) const;
};

} // namespace sharppy
