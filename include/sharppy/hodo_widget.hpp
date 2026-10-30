#pragma once

#include <QWidget>
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QElapsedTimer>
#include <QMenu>
#include <QPixmap>
#include <vector>
#include <cstddef>
#include "sharppy/profile.hpp"
#include "sharppy/winds.hpp"
#include "sharppy/thermo.hpp"
#include "sharppy/viz_prefs.hpp"

namespace sharppy {

// Port of sharppy/viz/hodo.py (plotHodo + backgroundHodo).
class HodoWidget : public QWidget {
    Q_OBJECT

public:
    explicit HodoWidget(QWidget* parent = nullptr);

    // Legacy entry point (storm motion == Bunkers, no inflow/MU data)
    void setProfile(const Profile& prof, const BunkersMotion& bunkers);
    // Full data: bunkers = prof.bunkers (the '+' marks), srwind = user storm motion (the circles)
    void setKinematics(const BunkersMotion& bunkers, const BunkersMotion& srwind,
                       const InflowLayer& inflow, const ParcelResult& mu, bool useLeft);
    void setProfileOnly(const Profile& prof);

    void setCustomStormMotion(double u, double v);      // kept for MainWindow
    void setInteractiveMode(bool enabled) { m_interactive = enabled; update(); }
    bool isCustomStormMotionActive() const { return m_hasCustomStorm; }
    void setPresBounds(double, double) {}               // not used by hodo.py
    void setDeviant(bool left) { m_useLeft = left; update(); }
    void setPreferences(const VizPrefs& prefs);
    // Skew-T zoom window in metres AGL; points outside it are culled (like the speed/advection columns). NaN = unbounded.
    void setHeightWindow(double zlo, double zhi);
    void setLiveEditing(bool on) { m_live = on; }
    void populateMenu(QMenu* menu);                     // cursor / centre / vector actions from showCursorMenu

    const Profile& currentProfile() const { return m_prof; }

public slots:
    void cursorToggle(bool on) { m_readoutVisible = on; update(); }
    void cursorMove(double hght_agl) { m_readoutHght = hght_agl; update(); }
    void setNormalCenter();
    void setMWCenter();
    void setSRCenter();
    void setNoCursor();
    void setBndyCursor();

signals:
    void stormMotionChanged(double u, double v);        // active vector dragged (MainWindow)
    void vectorModified(bool left, double u, double v); // modified_vector
    void toggleVector(bool left);                       // toggle_vector
    void resetVector();                                 // reset_vector
    void resetHodograph();                              // reset(['u','v'])
    void windModified();
    void windLive();                                    // dragging a hodograph point: currentProfile() is the in-progress edit

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    // ---- backgroundHodo ----
    void initUI();
    void centerHodo(double u, double v);
    QPointF uvToPix(double u, double v) const;
    void pixToUv(double x, double y, double& u, double& v) const;
    void drawRing(QPainter& qp, double spd);
    void drawBackground(QPainter& qp);
    // ---- plotHodo ----
    void drawHodo(QPainter& qp);
    void drawSMV(QPainter& qp);
    void drawCorfidi(QPainter& qp);
    void drawLclElMW(QPainter& qp);
    void drawCriticalAngle(QPainter& qp);
    void drawBoundary(QPainter& qp);
    void plotBndyCursor(double dir);
    void rebuildDerived();
    void rebuildVertices();
    QString spdStr(double kts) const;

    // data
    Profile m_prof;
    bool m_hasData = false;
    BunkersMotion m_bunkers, m_srwind;
    InflowLayer m_inflow;
    ParcelResult m_mu;
    bool m_useLeft = false;
    bool m_hasCustomStorm = false;
    bool m_interactive = true;

    // derived (profile.py values)
    bool m_haveMeanLclEl = false; double m_mlu = 0, m_mlv = 0, m_mlDir = 0, m_mlSpd = 0;
    double m_cupU = 0, m_cupV = 0, m_cdnU = 0, m_cdnV = 0, m_upDir = 0, m_upSpd = 0, m_dnDir = 0, m_dnSpd = 0;
    double m_shear6u = 0, m_shear6v = 0;
    double m_srw911u = 0, m_srw911v = 0;
    double m_critRight = MISSING, m_critLeft = MISSING;
    double m_sfcU = MISSING, m_sfcV = MISSING, m_u500 = MISSING, m_v500 = MISSING;

    // background state
    QString m_windUnits = "knots";
    QColor m_bg{"#000000"}, m_fg{"#FFFFFF"}, m_isotach{"#555555"}, m_crit{"#00FFFF"}, m_effInflow{"#00FFFF"};
    QColor m_colors[5] = {QColor("#FF00FF"), QColor("#FF0000"), QColor("#00FF00"), QColor("#FFFF00"), QColor("#00FFFF")};
    double m_hodomag = 160.0;
    int m_ringInc = 10;
    std::vector<int> m_rings;
    double m_wid = 0, m_hgt = 0, m_centerx = 0, m_centery = 0, m_scale = 1;
    double m_pointX = 0, m_pointY = 0;
    bool m_panning = false; QPoint m_panLast;
    double m_zlo = -1e18, m_zhi = 1e18;                // AGL height window from the Skew-T zoom (culling)
    void rebuildRings();
    double defaultMag() const { return m_windUnits == "m/s" ? 80. : 160.; }
    QFont m_labelFont, m_criticalFont, m_readoutFont;
    double m_criticalHeight = 0;

    // cursor / centre
    enum class Cursor { None, Boundary } m_cursor = Cursor::None;
    bool m_trackCursor = false, m_wasRight = false;
    double m_bndyDir = 0, m_bndySpd = 0;
    bool m_bndyDrawn = false; QPoint m_bndyPos;
    QString m_bndyReadout;
    bool m_readoutVisible = false; double m_readoutHght = -999.0;

    // draggables
    struct Vtx { double u, v; long prof_idx; };
    std::vector<Vtx> m_vtx;                // hodograph vertices (<= 12 km, 12 km point inserted)
    bool m_live = true; Profile m_dragBase; bool m_liveSent = false; QElapsedTimer m_liveClock; qint64 m_liveInterval = 16;
    int m_dragKind = 0;                    // 0 none, 1 hodo, 2 rm, 3 lm
    long m_dragIdx = -1; QPoint m_clickStart, m_dragPos;
};

} // namespace sharppy
