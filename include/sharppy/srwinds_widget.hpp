#pragma once

#include <QWidget>
#include <QColor>
#include <QFont>
#include "sharppy/profile.hpp"
#include "sharppy/winds.hpp"
#include "sharppy/viz_prefs.hpp"

namespace sharppy {

// Faithful port of sharppy/viz/srwinds.py (plotWinds): SR wind vs. height.
class SRWindsWidget : public QWidget {
    Q_OBJECT

public:
    explicit SRWindsWidget(QWidget* parent = nullptr);

    // srwind = prof.srwind (Bunkers or the user's storm motion)
    void setProfile(const Profile& prof, const BunkersMotion& srwind);
    void setDeviant(bool use_left);
    void setPreferences(const VizPrefs& prefs);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void initUI();
    void compute();
    void drawFrame(QPainter& qp);
    void drawHeight(int h, QPainter& qp);
    void drawSpeed(int s, QPainter& qp);
    void drawProfile(QPainter& qp);
    double hgtToPix(double h) const;
    double speedToPix(double s) const;

    Profile m_prof;
    BunkersMotion m_srwind;
    bool m_hasData = false;
    bool m_useLeft = false;

    QColor m_bg{"#000000"}, m_fg{"#ffffff"}, m_clsc{"#b1019a"}, m_trace{"#ff0000"},
           m_c02{"#8b0000"}, m_c46{"#6495ed"}, m_c911{"#9400d3"};
    double m_srw02 = MISSING, m_srw46 = MISSING, m_srw911 = MISSING;

    int m_lpad = 0, m_rpad = 0, m_tpad = 0, m_bpad = 20;
    double m_brx = 0, m_bry = 0;
    double m_hmax = 16., m_hmin = 0., m_smax = 80., m_smin = 0.;
    QFont m_labelFont;
};

} // namespace sharppy
