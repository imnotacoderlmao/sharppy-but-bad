#pragma once

#include <QWidget>
#include <QFont>
#include <QPixmap>
#include <QColor>
#include <QString>
#include "sharppy/profile.hpp"
#include "sharppy/winds.hpp"
#include "sharppy/thermo.hpp"
#include "sharppy/viz_prefs.hpp"

namespace sharppy {

// Faithful port of sharppy/viz/kinematics.py (plotKinematics).
class KinematicsWidget : public QWidget {
    Q_OBJECT

public:
    explicit KinematicsWidget(QWidget* parent = nullptr);

    // bunkersRaw = prof.bunkers (shown in the storm motion block),
    // srwind     = prof.srwind  (user override or bunkers; used for SRH/SRW)
    void setProfile(const Profile& prof, const BunkersMotion& bunkersRaw, const BunkersMotion& srwind,
                    const InflowLayer& inflow, const ParcelResult& mu);
    // legacy: same vectors for both, MU parcel computed here
    void setProfile(const Profile& prof, const BunkersMotion& bunkers, const InflowLayer& inflow);
    void setDeviant(bool use_left);
    void setPreferences(const VizPrefs& prefs);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    struct Vec { double dir = MISSING, spd = MISSING; };
    struct Row { double srh = MISSING; double shear = MISSING; Vec mean, srw; };

    void initUI();
    void compute();
    void drawFrame(QPainter& qp);
    void drawKinematics(QPainter& qp);
    void drawBarbs(QPainter& qp);

    Profile m_prof;
    BunkersMotion m_bunkersRaw, m_srwind;
    InflowLayer m_inflow;
    ParcelResult m_mu;
    bool m_hasData = false;
    bool m_useLeft = false;

    QString m_windUnits = "knots";
    QColor m_bg{"#000000"}, m_fg{"#ffffff"};

    // computed
    Row m_r1, m_r3, m_eff, m_r6, m_r8, m_lclel, m_ebw;
    double m_esrh = MISSING, m_brn = MISSING;
    Vec m_srw45, m_bunkR, m_bunkL, m_up, m_down;
    Vec m_wind1, m_wind6;

    // layout (initUI)
    int m_lpad = 5, m_rpad = 5;
    double m_tpad = 0, m_labelHeight = 0, m_barby = 0, m_ylast = 0;
    QFont m_labelFont;
    QPixmap m_framePix; QString m_frameKey;   // cached static layer (background, border, labels)
};

} // namespace sharppy
