#pragma once

#include <QWidget>
#include "sharppy/canvas_base.hpp"
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QMenu>
#include <QPixmap>
#include <QElapsedTimer>
#include <memory>
#include <vector>
#include "sharppy/profile.hpp"
#include "sharppy/thermo.hpp"
#include "sharppy/winds.hpp"
#include "sharppy/tab.hpp"
#include "sharppy/viz_prefs.hpp"

namespace sharppy {

enum class EditMode {
    PointSnap,   // Python behaviour: drag a T / Td vertex horizontally
    LineSegment  // rubber-band slab replacement (C++ addition)
};

// Port of sharppy/viz/skew.py (backgroundSkewT + plotSkewT)
class SkewTWidget : public CanvasBase {
    Q_OBJECT

public:
    explicit SkewTWidget(QWidget* parent = nullptr);

    // pcl = the parcel to draw (its start values are re-lifted with the exact parcelx)
    void setProfile(const Profile& prof, const ParcelResult& pcl);
    void setKinematics(const BunkersMotion& bunkers, const InflowLayer& inflow);
    void setDeviant(bool left) { m_useLeft = left; update(); }
    void setVirtualTempEnabled(bool enabled) { m_showTv = enabled; update(); }
    void setInteractiveMode(bool enabled) { m_interactive = enabled; update(); }
    void setEditMode(EditMode mode) { m_editMode = mode; update(); }
    EditMode editMode() const { return m_editMode; }
    void setLiveEditing(bool on) { m_live = on; }
    void setDGZ(bool on) { m_plotDgz = on; update(); }
    void setPBLLevel(bool on) { m_plotPbl = on; update(); }
    void setTitle(const QString& t) { m_title = t; update(); }
    void setPreferences(const VizPrefs& prefs);
    void populateMenu(QMenu* menu, const QPoint& pos);   // showCursorMenu(): cursor type, parcel lift, modify sfc, reset

    bool isVirtualTempEnabled() const { return m_showTv; }
    const Profile& currentProfile() const { return m_prof; }

    void resetZoom();
    bool isZoomed() const { return m_scale < 0.999; }
    // widget-local y of a pressure level in the current (zoomed) view; used by the speed/advection columns
    double presToY(double pres) const { return m_originy + presToPix(pres) / m_scale; }

public slots:
    void setNoCursor();
    void setReadoutCursor();

signals:
    void profileModified();
    void profileLive();                                  // while dragging a point: currentProfile() holds the in-progress edit
    void zoomChanged(double pBot, double pTop);
    void viewChanged(double scale, double originy);
    void cursorToggle(bool on);
    void cursorMove(double hght_agl);
    void userParcel(double pres, double tmpc, double dwpc);
    void modifySurfaceRequested();
    void resetRequested();

protected:
    SHARPPY_PAINT_HOOK(SkewTWidget)
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    // ---- backgroundSkewT ----
    void initUI();
    void drawBackground(QPainter& qp);
    void drawDryAdiabat(QPainter& qp, double theta);
    void drawMixingRatio(QPainter& qp, double w, double pmin);
    void drawFrame(QPainter& qp);
    void drawIsothermLabel(QPainter& qp, double t);
    void drawIsotherm(QPainter& qp, double t);
    void drawIsobar(QPainter& qp, double p, bool flag);
    double tmpcToPix(double t, double p) const;
    double pixToTmpc(double x, double y) const;
    double presToPix(double p) const;
    double pixToPres(double y) const;
    // ---- plotSkewT ----
    void plotData(QPainter& qp);
    void drawBarbs(QPainter& qp);
    void drawTitles(QPainter& qp);
    void drawHeight(QPainter& qp, double h);
    void drawSigLevels(QPainter& qp, double plevel, const QColor& color, const QString& var_id = "");
    void drawPblLevel(QPainter& qp);
    void drawParcelLevels(QPainter& qp);
    void drawTempLevels(QPainter& qp);
    void drawMaxLapseRateLayer(QPainter& qp, double bound = 4.5);
    void drawEffectiveLayer(QPainter& qp);
    void drawVirtualParcelTrace(QPainter& qp, const std::vector<double>& t, const std::vector<double>& p, const QColor& color);
    void drawTrace(QPainter& qp, const std::vector<double>& data, const std::vector<double>& pres, const QColor& color,
                   double width = 3, Qt::PenStyle style = Qt::SolidLine, bool label = true);
    void drawReadout(QPainter& qp);
    void updateReadout();
    QString readoutText(const QString& var, QColor& color) const;
    double interpLevel(const std::vector<double>& vals, double p) const;
    void rebuildDerived();
    void liftParcelLevel(int i);
    void emitZoom();
    QPointF vtxPix(bool temp, size_t i) const;

    // data
    Profile m_prof;
    bool m_hasData = false;
    std::shared_ptr<tab::Env> m_env;
    std::vector<double> m_pres, m_tmpc, m_dwpc, m_vtmp, m_wetbulb, m_thetae, m_theta, m_wvmr;
    tab::Parcel m_pcl; bool m_havePcl = false;
    std::vector<double> m_dpP, m_dpT;
    double m_mlrVal = std::nan(""), m_mlrBot = std::nan(""), m_mlrTop = std::nan("");
    double m_dgzBot = std::nan(""), m_dgzTop = std::nan(""), m_pblTop = std::nan("");
    double m_wbzP = std::nan(""), m_frzP = std::nan("");
    BunkersMotion m_bunkers; InflowLayer m_inflow;
    bool m_useLeft = false, m_showTv = true, m_interactive = true;
    EditMode m_editMode = EditMode::PointSnap;
    bool m_plotDgz = false, m_plotPbl = false;
    QString m_title;

    // geometry (backgroundSkewT.initUI)
    double m_lpad = 30, m_rpad = 65, m_tpad = 25, m_bpad = 20;
    double m_tlx = 0, m_tly = 0, m_wid = 0, m_hgt = 0, m_brx = 0, m_bry = 0, m_barbx = 0;
    double m_pmax = 1050, m_pmin = 100, m_logPmax = 0, m_logPmin = 0;
    double m_bltmpc = -50, m_brtmpc = 50, m_dt = 10, m_xskew = 100. / 3., m_xrange = 100, m_yrange = 0;
    QRectF m_clip;
    double m_originx = 0, m_originy = 0, m_scale = 1.0;
    QFont m_titleFont, m_labelFont, m_envTraceFont, m_inPlotFont, m_esrhFont, m_hghtFont;
    double m_titleHeight = 0, m_esrhHeight = 0;
    QPixmap m_bgCache; bool m_bgDirty = true;

    // colours
    QColor m_bg{"#000000"}, m_fg{"#FFFFFF"}, m_isotherm{"#555555"}, m_isothermHgz{"#0000FF"}, m_adiab{"#333333"}, m_mixr{"#006600"};
    QColor m_temp{"#FF0000"}, m_dewp{"#00FF00"}, m_wetbulbC{"#00FFFF"}, m_effLayer{"#00FFFF"}, m_hgtC{"#FF0000"}, m_dgzC{"#F5D800"};
    QColor m_lclMkr{"#00FF00"}, m_lfcMkr{"#FFFF00"}, m_elMkr{"#FF00FF"}, m_mplMkr{"#4400FF"}, m_sigTemp{"#0A63FF"};
    QColor m_alert[6] = {QColor("#775000"), QColor("#996600"), QColor("#ffffff"), QColor("#ffff00"), QColor("#ff0000"), QColor("#e700df")};
    QString m_sfcUnits = "Fahrenheit", m_windUnits = "knots";
    QString m_readoutVars[2] = {"tmpc", "dwpc"};

    // cursor / readout
    bool m_readout = false, m_trackCursor = false, m_wasRight = false;
    bool m_panning = false; QPoint m_panLast;          // middle-button pan while zoomed
    double m_readoutPres = 1000.;
    QPoint m_cursorLoc; bool m_haveCursorLoc = false;
    QString m_presText, m_hghtText, m_tmpText, m_dwpText; QColor m_tmpCol, m_dwpCol;

    // draggables (lock_dim='y')
    bool m_dragTemp = false; long m_dragIdx = -1; QPoint m_clickStart, m_dragPos;
    bool m_dragging = false;
    // live editing: the profile is re-modified from the press-time copy and pushed out while dragging (throttled)
    bool m_live = true; Profile m_dragBase; bool m_liveSent = false; QElapsedTimer m_liveClock; qint64 m_liveInterval = 16;
    double m_lsP0 = 0, m_lsT0 = 0; QPoint m_lsCur;
    bool m_lsTemp = true;
};

} // namespace sharppy
