#pragma once

#include <QWidget>
#include <QColor>
#include <QFont>
#include <vector>
#include "sharppy/profile.hpp"
#include "sharppy/thermo.hpp"
#include "sharppy/winds.hpp"
#include "sharppy/viz_prefs.hpp"

namespace sharppy {

struct SlinkyPoint { double x, y, z; };   // m (storm-relative x/y, AGL z)

// Faithful port of sharppy/viz/slinky.py (plotSlinky) + params.parcelTraj.
class SlinkyWidget : public QWidget {
    Q_OBJECT

public:
    explicit SlinkyWidget(QWidget* parent = nullptr);

    // srwind = prof.srwind (Bunkers or user storm motion)
    void setProfile(const Profile& prof, const ParcelResult& pcl, const BunkersMotion& srwind);
    void setDeviant(bool use_left);
    void setPreferences(const VizPrefs& prefs);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void initUI();
    void computeTrajectory();
    QPointF xyToPix(double x, double y) const;

    Profile m_prof;
    ParcelResult m_pcl;
    BunkersMotion m_srwind;
    bool m_hasData = false;
    bool m_useLeft = false;

    QColor m_bg{"#000000"}, m_fg{"#ffffff"};
    QColor m_c01{"#FF00FF"}, m_c13{"#FF0000"}, m_c36{"#00FF00"}, m_c69{"#FFFF00"}, m_c912{"#00FFFF"};

    // parcelTraj results
    bool m_hasTraj = false;
    bool m_hasEl = false;
    double m_tilt = 0.0;               // NaN when no trajectory
    std::vector<SlinkyPoint> m_traj;

    // layout
    int m_lpad = 5, m_rpad = 0, m_tpad = 0, m_bpad = 20, m_fpad = 5;
    double m_wid = 0, m_hgt = 0, m_centerx = 0, m_centery = 0, m_scale = 1, m_mag = 7000. * 1.7;
    double m_titleHeight = 0;
    QFont m_titleFont, m_plotFont;
};

} // namespace sharppy
