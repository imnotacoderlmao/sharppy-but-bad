#pragma once
#include <QWidget>
#include "sharppy/profile.hpp"
#include "sharppy/winter_calc.hpp"
#include "sharppy/viz_prefs.hpp"

namespace sharppy {

// Port of viz/winter.py (plotWinter).
class WinterWidget : public QWidget {
    Q_OBJECT
public:
    explicit WinterWidget(QWidget* parent = nullptr);
    void setProfile(const Profile& prof);
    void setPreferences(const VizPrefs& prefs);
protected:
    void paintEvent(QPaintEvent*) override;
private:
    Profile m_prof; WinterData m_w; bool m_has = false;
    QColor m_bg{"#000000"}, m_fg{"#ffffff"}, m_dgz{"#ffff00"};
};

} // namespace sharppy
