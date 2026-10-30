#pragma once

#include <QMainWindow>
#include <QStackedLayout>
#include <cmath>
#include <QGridLayout>
#include <QHash>
#include "sharppy/preferences_dialog.hpp"
#include <QLabel>
#include <QMap>
#include <QMenu>
#include <QAction>
#include <QKeyEvent>
#include <QStringList>
#include <QVector>
#include "sharppy/profile.hpp"
#include "sharppy/thermo.hpp"
#include "sharppy/winds.hpp"
#include "sharppy/params.hpp"
#include "sharppy/sars.hpp"
#include "sharppy/spc.hpp"

#include "sharppy/header_widget.hpp"
#include "sharppy/skewt_widget.hpp"
#include "sharppy/speed_widget.hpp"
#include "sharppy/advbar_widget.hpp"
#include "sharppy/hodo_widget.hpp"
#include "sharppy/slinky_widget.hpp"
#include "sharppy/thetae_widget.hpp"
#include "sharppy/srwinds_widget.hpp"
#include "sharppy/watch_widget.hpp"

#include "sharppy/convective_table_widget.hpp"
#include "sharppy/kinematics_widget.hpp"
#include "sharppy/analogues_widget.hpp"
#include "sharppy/winter_widget.hpp"
#include "sharppy/fire_widget.hpp"
#include "sharppy/stpbox_widget.hpp"
#include "sharppy/stpef_widget.hpp"
#include "sharppy/ship_widget.hpp"
#include "sharppy/vrot_widget.hpp"

namespace sharppy {

class SPCWindow : public QMainWindow {
    Q_OBJECT

public:
    void setStationId(const QString& id) { m_stationId = id; }
    explicit SPCWindow(QWidget* parent = nullptr);

    bool loadSoundingFile(const std::string& path);
    void loadSoundingFromData(const Profile& prof, const QString& title);
    // Several profiles (e.g. model forecast hours); Left/Right arrows step through them.
    void loadProfileCollection(const QVector<Profile>& profiles, const QStringList& titles);
    void resetModifications();
    void toggleDeviantMotion(bool use_left);
    void showSurfaceModifyDialog();
    void showFetchSoundingDialog();
    void showStormMotionDialog();
    void showSoundingDataDialog();
    void saveImage();
    void copyImage();
    void saveSoundingText();
    void importCm1();
    void loadSarsAnalog(const QString& match, const QString& type);
    void exportCm1();

signals:
    // Emitted by File > Sounding Picker...; main() shows/raises the picker.
    void pickerRequested();

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    Profile m_prof;
    Profile m_origProfile;
    std::string m_currentTitle = "Sample Sounding";

    QVector<Profile> m_collection;
    QStringList m_collectionTitles;
    int m_collectionIdx = 0;
    void showCollectionIndex(int idx);

    // Active state
    bool m_useLeftMover = false;
    int m_selectedParcelIdx = 3; // 0=SFC, 1=ML, 2=FCST, 3=MU, 4=EFF, 5=USER (default MU, as thermo.py)
    bool m_hasCustomStorm = false;
    double m_customStormU = 0.0;
    double m_customStormV = 0.0;

    // Cached computed parcels
    ParcelResult m_sb, m_ml, m_fcst, m_mu, m_eff, m_usr;
    bool m_hasUsr = false;
    QString m_stationId;   // ICAO/IATA/WMO id used for PWV climatology
    BunkersMotion m_bunkers;
    InflowLayer m_inflow;
    SevereParams m_params;

    // Widgets
    SkewTWidget* m_skewt;
    SpeedWidget* m_speed;
    AdvBarWidget* m_adv;
    HodoWidget* m_hodo;
    SlinkyWidget* m_slinky;
    ThetaEWidget* m_thetae;
    SRWindsWidget* m_srwinds;
    WatchWidget* m_watch;

    ConvectiveTableWidget* m_convective;
    KinematicsWidget* m_kinematics;

    // Swappable Insets (Left & Right)
    AppPreferences m_appPrefs;
    QGridLayout* m_textGrid = nullptr;
    QLabel* m_brand = nullptr;
    QMap<QString, QWidget*> m_insetWidgets;
    QString m_leftInset, m_rightInset;

    AnaloguesWidget* m_sars;
    WinterWidget* m_winter;
    FireWidget* m_fire;

    STPBoxWidget* m_stpBox;
    STPEFWidget* m_stpEf;
    SHIPWidget* m_ship;
    VROTWidget* m_vrot;

    void setupUI();
    void createMenuBar();
    void recomputeAll();
    void updateActiveParcel();
    QHash<int, Profile> m_preInterp;   // pre-interpolation profile per collection index (-1 = single profile)
    QAction* m_actInterp = nullptr;
    QAction* m_actResetInterp = nullptr;
    bool isInterpolated() const;
    void syncInterpActions();
    void clearInterpolation();
    void interpolateProfile();
    void resetInterpolation();
    double m_zoomPBot = std::nan(""), m_zoomPTop = std::nan("");
    void updateHodoWindow();
    void applyHemisphere();
    void rebuildProfileMenu();
    QMenu* m_profileMenu = nullptr;
    QAction* m_motionActs[2] = {nullptr, nullptr};
    void showPreferences();
    void applyPreferences(const AppPreferences& a);
    void showInsetMenu(const QString& key, const QPoint& pos);
    void swapInset(bool left, const QString& key);
};

} // namespace sharppy