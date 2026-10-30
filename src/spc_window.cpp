#include "sharppy/tab.hpp"
#include "sharppy/tab_params.hpp"
#include "sharppy/spc_window.hpp"
#include "sharppy/cm1_io.hpp"
#include "sharppy/sars_archive.hpp"
#include "sharppy/spc.hpp"
#include "sharppy/cm1_dialog.hpp"
#include "sharppy/winter_calc.hpp"
#include "sharppy/watch_type.hpp"
#include "sharppy/pwv.hpp"
#include <QDate>
#include <algorithm>
#include "sharppy/sanitize.hpp"
#include "sharppy/surface_dialog.hpp"
#include "sharppy/fetch_dialog.hpp"
#include "sharppy/storm_motion_dialog.hpp"
#include "sharppy/sounding_data_dialog.hpp"
#include "sharppy/modify.hpp"
#include "sharppy/bufkit.hpp"
#include "sharppy/uwyo.hpp"
#include <QVBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QLabel>
#include <QSettings>
#include <QRegularExpression>
#include <QDate>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QRadioButton>
#include <QCheckBox>
#include "sharppy/profile_interp.hpp"
#include <QClipboard>
#include <QDir>
#include <QFileInfo>
#include <QApplication>
#include "sharppy/preferences_dialog.hpp"
#include "sharppy/viz_styles.hpp"
#include <QHBoxLayout>
#include <QMenuBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QApplication>
#include <QActionGroup>
#include <QPixmap>

namespace sharppy {

SPCWindow::SPCWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("SHARPpy: Sounding and Hodograph Analysis and Research Program");
    resize(1200, 850);
    setupUI();
    createMenuBar();
    applyPreferences(m_appPrefs);   // saved style/units before the first sounding is drawn
    resize(1180, 800);
}

void SPCWindow::setupUI() {
    setStyleSheet(
        "QMainWindow { background-color: #000000; }"
        "QMenuBar { background-color: #111111; color: #d0d0d0; font-size: 11px; font-family: Monospace; }"
        "QMenuBar::item:selected { background-color: #005080; }"
        "QMenu { background-color: #1a1a1a; color: #ffffff; border: 1px solid #00a0dc; font-family: Monospace; }"
        "QMenu::item:selected { background-color: #00a0dc; color: #000000; }"
    );

    // ---- layout identical to SPCWidget.__init__/loadWidgets (SPCWindow.py) ----
    QWidget* central = new QWidget(this);
    central->setStyleSheet("QWidget#spcCentral { background-color: #000000; }");
    central->setObjectName("spcCentral");
    QGridLayout* grid = new QGridLayout(central);
    grid->setContentsMargins(1, 1, 1, 1);
    grid->setHorizontalSpacing(0);
    grid->setVerticalSpacing(2);
    for (int r = 0; r < 4; ++r) grid->setRowStretch(r, 1);       // equal rows: 3 for skew-T/ur, 1 for text
    grid->setColumnStretch(0, 1); grid->setColumnStretch(1, 1);

    m_skewt = new SkewTWidget(this);
    m_skewt->setInteractiveMode(true);

    m_speed = new SpeedWidget(this);
    m_adv = new AdvBarWidget(this);
    m_hodo = new HodoWidget(this);
    m_hodo->setInteractiveMode(true);
    m_slinky = new SlinkyWidget(this);
    m_thetae = new ThetaEWidget(this);
    m_srwinds = new SRWindsWidget(this);
    m_watch = new WatchWidget(this);

    // upper right: brand + frame holding speed/advection/hodograph/slinky/theta-e/SR winds/watch
    QFrame* urparent = new QFrame(this);
    QVBoxLayout* urparentLayout = new QVBoxLayout(urparent);
    urparentLayout->setContentsMargins(0, 0, 0, 0);
    urparentLayout->setSpacing(0);
    m_brand = new QLabel(QString("SHARPpy v%1").arg("0+cpp"), this);
    m_brand->setAlignment(Qt::AlignRight);
    m_brand->setStyleSheet("QLabel { background-color: #000000; padding-top: 4px; padding-bottom: 4px; font-size: 11px; color: #ffffff; }");
    QFrame* ur = new QFrame(this);
    ur->setStyleSheet("QFrame { background-color: #000000; border-width: 0px; margin: 0px; }");
    QGridLayout* grid2 = new QGridLayout(ur);
    grid2->setHorizontalSpacing(0);
    grid2->setVerticalSpacing(0);
    grid2->setContentsMargins(0, 0, 0, 0);
    for (int r = 0; r < 11; ++r) grid2->setRowStretch(r, 1);
    for (int c = 0; c < 29; ++c) grid2->setColumnStretch(c, 1);
    grid2->addWidget(m_speed, 0, 0, 11, 3);
    grid2->addWidget(m_adv, 0, 3, 11, 2);
    grid2->addWidget(m_hodo, 0, 5, 8, 24);
    grid2->addWidget(m_slinky, 8, 5, 3, 6);
    grid2->addWidget(m_thetae, 8, 11, 3, 6);
    grid2->addWidget(m_srwinds, 8, 17, 3, 6);
    grid2->addWidget(m_watch, 8, 23, 3, 6);
    urparentLayout->addWidget(m_brand, 0);
    urparentLayout->addWidget(ur, 1);
    grid->addWidget(m_skewt, 0, 0, 3, 1);
    grid->addWidget(urparent, 0, 1, 3, 1);

    // lower text area: convective, kinematics, left inset, right inset
    QFrame* text = new QFrame(this);
    text->setObjectName("spcText");
    text->setStyleSheet("QFrame#spcText { background-color: #000000; border-width: 2px; border-style: solid; border-color: #3399CC; }");
    m_textGrid = new QGridLayout(text);
    m_textGrid->setHorizontalSpacing(0);
    m_textGrid->setContentsMargins(0, 0, 0, 0);
    for (int c = 0; c < 4; ++c) m_textGrid->setColumnStretch(c, 1);

    m_convective = new ConvectiveTableWidget(this);
    m_kinematics = new KinematicsWidget(this);
    m_textGrid->addWidget(m_convective, 0, 0);
    m_textGrid->addWidget(m_kinematics, 0, 1);

    m_sars = new AnaloguesWidget(this);
    connect(m_sars, &AnaloguesWidget::updatematch, this, &SPCWindow::loadSarsAnalog);
    m_winter = new WinterWidget(this);
    m_fire = new FireWidget(this);
    m_stpBox = new STPBoxWidget(this);
    m_stpEf = new STPEFWidget(this);
    m_ship = new SHIPWidget(this);
    m_vrot = new VROTWidget(this);
    m_insetWidgets = {{"SARS", m_sars}, {"STP STATS", m_stpBox}, {"COND STP", m_stpEf}, {"WINTER", m_winter},
                      {"FIRE", m_fire}, {"SHIP", m_ship}, {"VROT", m_vrot}};
    for (auto it = m_insetWidgets.begin(); it != m_insetWidgets.end(); ++it) {
        it.value()->hide();
        it.value()->setContextMenuPolicy(Qt::CustomContextMenu);
        const QString key = it.key();
        connect(it.value(), &QWidget::customContextMenuRequested, this, [this, key](const QPoint& pos) {
            showInsetMenu(key, pos);
        });
    }
    {
        QSettings st("sharppy_cpp", "sharppy");
        m_leftInset = st.value("insets/left_inset", "SARS").toString();
        m_rightInset = st.value("insets/right_inset", "STP STATS").toString();
        if (!m_insetWidgets.contains(m_leftInset)) m_leftInset = "SARS";
        if (!m_insetWidgets.contains(m_rightInset) || m_rightInset == m_leftInset) m_rightInset = (m_leftInset == "STP STATS") ? "SARS" : "STP STATS";
    }
    m_textGrid->addWidget(m_insetWidgets[m_leftInset], 0, 2);
    m_textGrid->addWidget(m_insetWidgets[m_rightInset], 0, 3);
    m_insetWidgets[m_leftInset]->show();
    m_insetWidgets[m_rightInset]->show();
    if (m_leftInset == "WINTER" || m_rightInset == "WINTER") m_skewt->setDGZ(true);
    if (m_leftInset == "FIRE" || m_rightInset == "FIRE") m_skewt->setPBLLevel(true);
    grid->addWidget(text, 3, 0, 1, 2);
    setCentralWidget(central);
    m_appPrefs = loadAppPreferences();

    m_speed->setSkewT(m_skewt);   // speed column reads its y axis from the Skew-T (speed.py setSkewT)

    // Signal-Slot wiring for live updates
    connect(m_skewt, &SkewTWidget::zoomChanged, this, [this](double pBot, double pTop) {
        m_zoomPBot = pBot; m_zoomPTop = pTop;
        updateHodoWindow();
        m_speed->setPresBounds(pBot, pTop);
        m_adv->setPresBounds(pBot, pTop);
    });

    connect(m_skewt, &SkewTWidget::profileModified, this, [this]() {
        markModified();
        m_prof = m_skewt->currentProfile();
        recomputeAll();
    });

    connect(m_skewt, &SkewTWidget::profileLive, this, [this]() {
        markModified();
        m_prof = m_skewt->currentProfile();
        recomputeAll();
    });
    connect(m_hodo, &HodoWidget::windLive, this, [this]() {
        markModified();
        m_prof = m_hodo->currentProfile();
        recomputeAll();
    });

    connect(m_skewt, &SkewTWidget::cursorToggle, m_hodo, &HodoWidget::cursorToggle);
    connect(m_skewt, &SkewTWidget::cursorMove, m_hodo, &HodoWidget::cursorMove);
    connect(m_skewt, &SkewTWidget::modifySurfaceRequested, this, &SPCWindow::showSurfaceModifyDialog);
    connect(m_skewt, &SkewTWidget::resetRequested, this, &SPCWindow::resetModifications);
    connect(m_skewt, &SkewTWidget::userParcel, this, [this](double p, double t, double td) {
        if (m_prof.empty()) return;
        ParcelResult usr = lift_parcel(m_prof, p, t, td, true);
        m_usr = usr; m_hasUsr = true;
        m_skewt->setProfile(m_prof, usr);
        m_convective->setParcels(m_sb, m_ml, m_fcst, m_mu, m_eff, m_usr);
    });

    connect(m_hodo, &HodoWidget::windModified, this, [this]() {
        markModified();
        m_prof = m_hodo->currentProfile();
        recomputeAll();
    });

    connect(m_hodo, &HodoWidget::vectorModified, this, [this](bool left, double u, double v) {
        if (left != m_useLeftMover) toggleDeviantMotion(left);
        m_hasCustomStorm = true;
        m_customStormU = u;
        m_customStormV = v;
        recomputeAll();
    });
    connect(m_hodo, &HodoWidget::toggleVector, this, [this](bool left) { toggleDeviantMotion(left); });
    connect(m_hodo, &HodoWidget::resetVector, this, [this]() { m_hasCustomStorm = false; recomputeAll(); });
    connect(m_hodo, &HodoWidget::resetHodograph, this, &SPCWindow::resetModifications);

    {
        QSettings st("sharppy_cpp", "sharppy");
        const QStringList t = st.value("parcel_types", QStringList{"SFC", "ML", "FCST", "MU"}).toStringList();
        m_convective->setParcelTypes(t);          // pcl1..pcl4 of the config
    }
    connect(m_convective, &ConvectiveTableWidget::parcelSelected, this, [this](int idx) {
        QSettings("sharppy_cpp", "sharppy").setValue("parcel_types", m_convective->parcelTypes());
        m_selectedParcelIdx = idx;
        updateActiveParcel();
    });

    // Skew-T Context Menu
    m_skewt->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_skewt, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        QMenu menu(this);
        m_skewt->populateMenu(&menu, pos);
        menu.exec(m_skewt->mapToGlobal(pos));
    });

    // Hodograph Context Menu
    m_hodo->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_hodo, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        QMenu menu(this);
        m_hodo->populateMenu(&menu);
        menu.exec(m_hodo->mapToGlobal(pos));
    });
}

void SPCWindow::createMenuBar() {
    QMenu* fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction("Sounding &Picker...", this, &SPCWindow::pickerRequested, QKeySequence("Ctrl+P"));
    fileMenu->addAction("Download / Fetch &Sounding...", this, &SPCWindow::showFetchSoundingDialog, QKeySequence("Ctrl+D"));
    fileMenu->addAction("&Open Sounding File...", this, [this]() {
        QString f = QFileDialog::getOpenFileName(this, "Open Sounding", "", "Soundings (*.txt *.raw *.buf *.snd *.cm1 *.sharppy);;All Files (*)");
        if (!f.isEmpty()) loadSoundingFile(f.toStdString());
    }, QKeySequence("Ctrl+O"));

    fileMenu->addSeparator();
    fileMenu->addAction("&Preferences...", this, &SPCWindow::showPreferences);
    fileMenu->addAction("Modify &Surface Layer...", this, &SPCWindow::showSurfaceModifyDialog, QKeySequence("Ctrl+M"));
    fileMenu->addAction("&Reset Modifications\tR", this, &SPCWindow::resetModifications);
    fileMenu->addSeparator();

    fileMenu->addAction("Save &Image (PNG)...", this, &SPCWindow::saveImage, QKeySequence("Ctrl+S"));
    fileMenu->addAction("&Copy Image", this, &SPCWindow::copyImage, QKeySequence("Ctrl+C"));
    fileMenu->addAction("Save Sounding &Text...", this, &SPCWindow::saveSoundingText, QKeySequence("Ctrl+Shift+S"));
    fileMenu->addAction("&Import CM1 Sounding...", this, &SPCWindow::importCm1);
    fileMenu->addAction("E&xport CM1 Sounding...", this, &SPCWindow::exportCm1);
    fileMenu->addSeparator();

    fileMenu->addAction("E&xit", this, &QWidget::close, QKeySequence("Ctrl+Q"));

    // Profiles menu (SPCWindow.createMenuBar): one entry per loaded profile with Focus
    m_profileMenu = menuBar()->addMenu("&Profiles");
    m_actInterp = new QAction("Interpolate Focused Profile", this);
    m_actInterp->setShortcut(QKeySequence("I"));
    connect(m_actInterp, &QAction::triggered, this, &SPCWindow::interpolateProfile);
    m_actResetInterp = new QAction("Reset Interpolation", this);
    m_actResetInterp->setShortcut(QKeySequence("I"));
    connect(m_actResetInterp, &QAction::triggered, this, &SPCWindow::resetInterpolation);
    m_actResetInterp->setVisible(false);
    rebuildProfileMenu();

    // Tools Menu
    QMenu* toolsMenu = menuBar()->addMenu("&Tools");
    toolsMenu->addAction("View &Sounding Level Data...", this, &SPCWindow::showSoundingDataDialog, QKeySequence("Ctrl+Shift+D"));
    toolsMenu->addAction("Modify Surface Layer...", this, &SPCWindow::showSurfaceModifyDialog);
    toolsMenu->addAction("Reset Profile Modifications", this, &SPCWindow::resetModifications);

    // Motion Menu (Left Mover vs Right Mover - from sharppy-but-bad!)
    QMenu* motionMenu = menuBar()->addMenu("&Motion");
    QActionGroup* motionGroup = new QActionGroup(this);

    QAction* actRM = motionMenu->addAction("&Right Mover (Bunkers RM)", this, [this]() { toggleDeviantMotion(false); });
    m_motionActs[0] = actRM;
    actRM->setCheckable(true);
    actRM->setChecked(true);
    motionGroup->addAction(actRM);

    QAction* actLM = motionMenu->addAction("&Left Mover (Bunkers LM)\tL", this, [this]() { toggleDeviantMotion(true); });
    m_motionActs[1] = actLM;
    actLM->setCheckable(true);
    motionGroup->addAction(actLM);

    motionMenu->addSeparator();
    motionMenu->addAction("Custom &Storm Motion...", this, &SPCWindow::showStormMotionDialog, QKeySequence("Ctrl+Shift+M"));
    motionMenu->addAction("&Reset Storm Motion to Bunkers", this, [this]() {
        m_hasCustomStorm = false;
        recomputeAll();
    });

    // Edit Mode Menu
    QMenu* modeMenu = menuBar()->addMenu("Mo&de");
    QActionGroup* modeGroup = new QActionGroup(this);

    QAction* actSnap = modeMenu->addAction("&Point Snap Drag", this, [this]() {
        m_skewt->setEditMode(EditMode::PointSnap);
    });
    actSnap->setCheckable(true);
    actSnap->setChecked(true);
    modeGroup->addAction(actSnap);

    QAction* actLine = modeMenu->addAction("&Line Segment Replace (Shift/Ctrl+drag = dewpoint)", this, [this]() {
        m_skewt->setEditMode(EditMode::LineSegment);
    });
    actLine->setCheckable(true);
    actLine->setToolTip("Left-drag: temperature. Shift/Ctrl+Left-drag or side mouse button: dewpoint.");
    actLine->setStatusTip(actLine->toolTip());
    modeGroup->addAction(actLine);

    modeMenu->addSeparator();
    QAction* actLive = modeMenu->addAction("&Live Editing (recompute while dragging)");
    actLive->setCheckable(true);
    actLive->setChecked(QSettings("sharppy_cpp", "sharppy").value("live_editing", true).toBool());
    auto applyLive = [this](bool on) { m_skewt->setLiveEditing(on); m_hodo->setLiveEditing(on); };
    applyLive(actLive->isChecked());
    connect(actLive, &QAction::toggled, this, [applyLive](bool on) {
        QSettings("sharppy_cpp", "sharppy").setValue("live_editing", on);
        applyLive(on);
    });
}

static const QList<QPair<QString, QString>>& insetNames() {
    // sorted by display name, as SPCWidget.makeInsetMenu does
    static const QList<QPair<QString, QString>> n = {
        {"WINTER", "Winter Weather"}, {"FIRE", "Fire Weather"}, {"COND STP", "EF-Scale Probs (Sig-Tor)"},
        {"VROT", "EF-Scale Probs (V-Rot)"}, {"SARS", "Sounding Analogues"}, {"SHIP", "Sig-Hail Stats"},
        {"STP STATS", "Sig-Tor Stats"}};
    return n;
}

void SPCWindow::showInsetMenu(const QString& key, const QPoint& pos) {
    const bool isLeft = (key == m_leftInset);
    if (!isLeft && key != m_rightInset) return;
    QMenu menu(this);
    QList<QPair<QString, QString>> items = insetNames();
    std::sort(items.begin(), items.end(), [](const auto& a, const auto& b) { return a.second < b.second; });
    QActionGroup* grp = new QActionGroup(&menu);
    grp->setExclusive(true);
    for (const auto& it : items) {
        if (it.first == m_leftInset || it.first == m_rightInset) continue;     // makeInsetMenu(*exclude)
        QAction* a = menu.addAction(it.second);
        a->setCheckable(true);
        grp->addAction(a);
        const QString k = it.first;
        connect(a, &QAction::triggered, this, [this, isLeft, k]() { swapInset(isLeft, k); });
    }
    QWidget* w = m_insetWidgets[key];
    menu.exec(w->mapToGlobal(pos));
    setFocus();
}

void SPCWindow::swapInset(bool left, const QString& key) {
    QString& cur = left ? m_leftInset : m_rightInset;
    if (key == cur || !m_insetWidgets.contains(key)) return;
    if (cur == "WINTER") m_skewt->setDGZ(false);
    if (cur == "FIRE") m_skewt->setPBLLevel(false);
    QWidget* old = m_insetWidgets[cur];
    m_textGrid->removeWidget(old);
    old->hide();
    cur = key;
    m_textGrid->addWidget(m_insetWidgets[cur], 0, left ? 2 : 3);
    m_insetWidgets[cur]->show();
    if (key == "WINTER") m_skewt->setDGZ(true);
    if (key == "FIRE") m_skewt->setPBLLevel(true);
    QSettings st("sharppy_cpp", "sharppy");
    st.setValue(left ? "insets/left_inset" : "insets/right_inset", cur);
    setFocus();
    update();
}

// SPCWidget.updateProfs: right mover in the northern hemisphere, left mover in the southern
// Skew-T zoom (pressure window) -> AGL height window for the hodograph
void SPCWindow::updateHodoWindow() {
    double zlo = std::nan(""), zhi = std::nan("");
    if (!m_prof.empty() && std::isfinite(m_zoomPBot) && std::isfinite(m_zoomPTop) && m_skewt->isZoomed()) {
        const double hb = m_prof.hght_at_pres(m_zoomPBot), ht = m_prof.hght_at_pres(m_zoomPTop);
        if (hb != MISSING) zlo = m_prof.to_agl(hb);
        if (ht != MISSING) zhi = m_prof.to_agl(ht);
    }
    m_hodo->setHeightWindow(zlo, zhi);
}

void SPCWindow::applyHemisphere() {
    if (m_prof.latitude != MISSING) m_useLeftMover = (m_prof.latitude < 0);
    if (m_motionActs[0]) { m_motionActs[m_useLeftMover ? 1 : 0]->setChecked(true); }
}

void SPCWindow::toggleDeviantMotion(bool use_left) {
    if (m_motionActs[0]) m_motionActs[use_left ? 1 : 0]->setChecked(true);
    m_useLeftMover = use_left;
    recomputeAll();
}

void SPCWindow::showFetchSoundingDialog() {
    FetchSoundingDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        m_collection.clear();
        loadSoundingFromData(dlg.getProfile(), dlg.getTitle());
    }
}

QString SPCWindow::displayTitle() const {
    QString t = QString::fromStdString(m_currentTitle);
    if (!m_modified) return t;
    // "(Observed: SPC)" -> "(Observed: SPC, Modified)"; titles with no source group get " (Modified)"
    static const QRegularExpression src(R"(\(([^()]*: [^()]*)\))");
    const QRegularExpressionMatch m = src.match(t);
    if (m.hasMatch()) { t.insert(m.capturedEnd(0) - 1, ", Modified"); return t; }
    return t + " (Modified)";
}

void SPCWindow::refreshTitle() {
    m_skewt->setTitle(m_currentTitle.empty() ? QString() : displayTitle());
}

void SPCWindow::markModified() {
    if (m_modified) return;
    m_modified = true;
    refreshTitle();
}

void SPCWindow::loadSoundingFromData(const Profile& prof, const QString& title) {
    if (m_collection.isEmpty()) m_preInterp.clear();
    m_prof = prof;
    sanitize_profile(m_prof); // idempotent; leaves the profile alone if it can't be cleaned
    m_origProfile = prof;
    m_currentTitle = title.toStdString();
    m_modified = false;
    refreshTitle();
    m_hasCustomStorm = false;
    applyHemisphere();
    if (m_collection.isEmpty() && m_profileMenu) rebuildProfileMenu();
    if (m_actInterp) syncInterpActions();
    recomputeAll();
}

void SPCWindow::loadProfileCollection(const QVector<Profile>& profiles, const QStringList& titles) {
    m_preInterp.clear();
    m_collection = profiles;
    m_collectionTitles = titles;
    m_collectionIdx = 0;
    if (m_profileMenu) rebuildProfileMenu();
    if (m_collection.isEmpty()) return;
    showCollectionIndex(0);
}

void SPCWindow::showCollectionIndex(int idx) {
    if (m_collection.isEmpty()) return;
    idx = std::max(0, std::min(idx, static_cast<int>(m_collection.size()) - 1));
    m_collectionIdx = idx;
    QString t = idx < m_collectionTitles.size() ? m_collectionTitles[idx] : QString("Profile %1").arg(idx + 1);
    if (m_collection.size() > 1) t += QString("  (%1/%2)").arg(idx + 1).arg(m_collection.size());
    loadSoundingFromData(m_collection[idx], t);
}

void SPCWindow::showStormMotionDialog() {
    double meanU = 0.0, meanV = 0.0;
    if (!m_prof.empty()) {
        WindVector mw = mean_wind(m_prof, 0.0, 6000.0);
        meanU = mw.u;
        meanV = mw.v;
    }
    CorfidiMotion corfidi = corfidi_mcs_motion(m_prof);
    double curU = m_hasCustomStorm ? m_customStormU : (m_useLeftMover ? m_bunkers.left_mover.u : m_bunkers.right_mover.u);
    double curV = m_hasCustomStorm ? m_customStormV : (m_useLeftMover ? m_bunkers.left_mover.v : m_bunkers.right_mover.v);

    StormMotionDialog dlg(m_bunkers, corfidi, meanU, meanV, curU, curV, this);
    if (dlg.exec() == QDialog::Accepted) {
        m_hasCustomStorm = true;
        m_customStormU = dlg.getMotionU();
        m_customStormV = dlg.getMotionV();
        recomputeAll();
    }
}

void SPCWindow::showSoundingDataDialog() {
    if (m_prof.empty()) return;
    SoundingDataDialog dlg(m_prof, QString::fromStdString(m_currentTitle), this);
    dlg.exec();
}

void SPCWindow::showSurfaceModifyDialog() {
    if (m_prof.empty()) return;
    double cur_t = m_prof.temp.front();
    double cur_td = m_prof.dwpt.front();

    SurfaceModifyDialog dlg(cur_t, cur_td, this);
    if (dlg.exec() == QDialog::Accepted) {
        ProfileModifier::modify_surface(m_prof, dlg.getTemp(), dlg.getDewpoint(), dlg.isDryMix());
        markModified();
        recomputeAll();
    }
}

void SPCWindow::resetModifications() {
    if (m_origProfile.empty()) return;
    m_prof = m_origProfile;
    m_hasCustomStorm = false;
    m_modified = false;
    refreshTitle();
    recomputeAll();
}

void SPCWindow::saveImage() {
    QSettings st("sharppy_cpp", "sharppy");
    const QString dir = st.value("paths/save_img", QDir::homePath()).toString();
    QString path = QFileDialog::getSaveFileName(this, "Save Image", dir + "/sounding.png", "PNG Images (*.png)");
    if (!path.isEmpty()) {
        st.setValue("paths/save_img", QFileInfo(path).absolutePath());
        centralWidget()->grab().save(path, "PNG", 100);
    }
}

void SPCWindow::copyImage() {
    QApplication::clipboard()->setPixmap(centralWidget()->grab());
}

void SPCWindow::rebuildProfileMenu() {
    m_profileMenu->clear();
    m_profileMenu->addAction(m_actInterp);
    m_profileMenu->addAction(m_actResetInterp);
    m_profileMenu->addSeparator();
    if (m_collection.isEmpty()) {
        QAction* a = m_profileMenu->addAction(QString::fromStdString(m_currentTitle));
        a->setEnabled(false);
        return;
    }
    for (int i = 0; i < m_collection.size(); ++i) {
        const QString t = i < m_collectionTitles.size() ? m_collectionTitles[i] : QString("Profile %1").arg(i + 1);
        QMenu* sub = m_profileMenu->addMenu(t);
        sub->addAction("Focus", this, [this, i]() { showCollectionIndex(i); });
    }
}

// SPCWindow.updateSARS: open the clicked analog sounding. It is appended to the loaded profiles and shown
// (the Python version adds it as another profile collection).
void SPCWindow::loadSarsAnalog(const QString& match, const QString& type) {
    QString fn;
    const QByteArray raw = sars_read_sounding(default_sars_dir(), type, match, &fn);
    if (raw.isEmpty()) {
        QMessageBox::warning(this, "SARS", "Analog sounding " + match + " (" + type + ") was not found in the SARS archive.");
        return;
    }
    Profile p; SoundingMetadata meta;
    if (!SpcParser::parse_string(raw.toStdString(), p, meta) || !sanitize_profile(p)) {
        QMessageBox::warning(this, "SARS", "Could not decode analog sounding " + fn + ".");
        return;
    }
    const QString title = "Analog (" + type + "): " + (meta.title.empty() ? fn : QString::fromStdString(meta.title));
    if (m_collection.isEmpty()) {
        m_collection.push_back(m_origProfile);
        m_collectionTitles << QString::fromStdString(m_currentTitle);
    }
    int idx = m_collectionTitles.indexOf(title);
    if (idx < 0) { m_collection.push_back(p); m_collectionTitles << title; idx = m_collection.size() - 1; }
    if (m_profileMenu) rebuildProfileMenu();
    showCollectionIndex(idx);
}

void SPCWindow::importCm1() {
    QSettings st("sharppy_cpp", "sharppy");
    const QString f = QFileDialog::getOpenFileName(this, "Import CM1 Sounding", st.value("paths/cm1", QDir::homePath()).toString(),
                                                   "CM1 soundings (*.cm1 *input_sounding* *);;All Files (*)");
    if (f.isEmpty()) return;
    st.setValue("paths/cm1", QFileInfo(f).absolutePath());
    Profile p; std::string err;
    if (!read_cm1(f.toStdString(), p, &err)) { QMessageBox::critical(this, "Import CM1", "Could not read CM1 sounding: " + QString::fromStdString(err)); return; }
    m_collection.clear();
    loadSoundingFromData(p, "CM1 " + QFileInfo(f).fileName());
}

void SPCWindow::exportCm1() {
    if (m_prof.empty()) return;
    Cm1ExportDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) return;
    QSettings st("sharppy_cpp", "sharppy");
    QString path = QFileDialog::getSaveFileName(this, "Export CM1 Sounding", st.value("paths/cm1", QDir::homePath()).toString() + "/input_sounding.cm1", "CM1 input sounding (*.cm1)");
    if (path.isEmpty()) return;
    st.setValue("paths/cm1", QFileInfo(path).absolutePath());
    std::string err;
    if (!write_cm1(m_prof, path.toStdString(), dlg.options(), &err))
        QMessageBox::critical(this, "Export CM1", "Could not export: " + QString::fromStdString(err));
}

void SPCWindow::saveSoundingText() {
    QSettings st("sharppy_cpp", "sharppy");
    const QString dir = st.value("paths/save_txt", QDir::homePath()).toString();
    QString path = QFileDialog::getSaveFileName(this, "Save Sounding Text", dir + "/sounding.txt", "Text Files (*.txt *.snd)");
    if (!path.isEmpty()) {
        st.setValue("paths/save_txt", QFileInfo(path).absolutePath());
        ProfileModifier::export_spc(m_prof, m_currentTitle, path.toStdString());
    }
}

void SPCWindow::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right) {
        // advanceTime: step through the loaded profiles, wrapping around
        const int n = m_collection.size();
        if (n > 1) showCollectionIndex(((m_collectionIdx + (event->key() == Qt::Key_Right ? 1 : -1)) % n + n) % n);
    } else if (event->key() == Qt::Key_W) {
        emit pickerRequested();       // return focus to the Sounding Picker
    } else if (event->key() == Qt::Key_L) {
        // 'L' toggles left mover motion mode
        toggleDeviantMotion(!m_useLeftMover);
    } else if (event->key() == Qt::Key_R) {
        // 'R' resets modifications
        resetModifications();
    } else if (event->key() == Qt::Key_1) {
        m_selectedParcelIdx = 0; // SFC
        m_convective->selectParcelType("SFC");
        updateActiveParcel();
    } else if (event->key() == Qt::Key_2) {
        m_selectedParcelIdx = 1; // ML
        m_convective->selectParcelType("ML");
        updateActiveParcel();
    } else if (event->key() == Qt::Key_3) {
        m_selectedParcelIdx = 2; // FCST
        m_convective->selectParcelType("FCST");
        updateActiveParcel();
    } else if (event->key() == Qt::Key_4) {
        m_selectedParcelIdx = 3; // MU
        m_convective->selectParcelType("MU");
        updateActiveParcel();
    } else {
        QMainWindow::keyPressEvent(event);
    }
}

bool SPCWindow::loadSoundingFile(const std::string& path) {
    SoundingMetadata meta;

    m_collection.clear();
    m_preInterp.clear();
    if (SpcParser::parse_file(path, m_prof, meta)) {
        m_origProfile = m_prof;
        m_currentTitle = meta.title.empty() ? path : meta.title;
        m_modified = false;
        refreshTitle();
        m_hasCustomStorm = false;
        applyHemisphere();
        recomputeAll();
        return true;
    }

    std::vector<BufkitRecord> buf_recs;
    if (BufkitParser::parse_file(path, buf_recs)) {
        m_prof = buf_recs.front().profile;
        m_origProfile = m_prof;
        m_currentTitle = buf_recs.front().station_id + " " + buf_recs.front().valid_time;
        m_modified = false;
        refreshTitle();
        m_hasCustomStorm = false;
        applyHemisphere();
        recomputeAll();
        return true;
    }

    if (UwyoParser::parse_file(path, m_prof, meta)) {
        m_origProfile = m_prof;
        m_currentTitle = path;
        m_modified = false;
        refreshTitle();
        m_hasCustomStorm = false;
        applyHemisphere();
        recomputeAll();
        return true;
    }

    {   // CM1 input_sounding (cm12snd)
        Profile cm1;
        if (read_cm1(path, cm1)) {
            m_origProfile = m_prof = cm1;
            const QString base = QFileInfo(QString::fromStdString(path)).fileName();
            m_currentTitle = "CM1 " + base.toStdString();
            m_modified = false;
        refreshTitle();
            m_hasCustomStorm = false;
            applyHemisphere();
            recomputeAll();
            return true;
        }
    }
    QMessageBox::critical(this, "Error", "Could not decode sounding file.");
    return false;
}

void SPCWindow::updateActiveParcel() {
    const ParcelResult* active_pcl = &m_mu;
    if (m_selectedParcelIdx == 0) active_pcl = &m_sb;
    else if (m_selectedParcelIdx == 1) active_pcl = &m_ml;
    else if (m_selectedParcelIdx == 2) active_pcl = &m_fcst;
    else if (m_selectedParcelIdx == 3) active_pcl = &m_mu;
    else if (m_selectedParcelIdx == 4) active_pcl = &m_eff;
    else if (m_selectedParcelIdx == 5) active_pcl = &m_usr;

    m_skewt->setDeviant(m_useLeftMover);
    m_skewt->setProfile(m_prof, *active_pcl);
    updateHodoWindow();
    m_slinky->setProfile(m_prof, *active_pcl, m_bunkers);
    m_slinky->setDeviant(m_useLeftMover);
}

// Month of the sounding from its title: "OUN 190521/0000" (yymmdd/hhmm), "...2019-05-21..." or "...20190521...";
// today's month when the title carries no date (as pwv_climo's month=None).
static int monthFromTitle(const QString& t) {
    QRegularExpressionMatch m = QRegularExpression("(?<!\\d)(\\d{2})(\\d{2})(\\d{2})/(\\d{4})").match(t);
    if (m.hasMatch()) { int mo = m.captured(2).toInt(); if (mo >= 1 && mo <= 12) return mo; }
    m = QRegularExpression("(?<!\\d)(19|20)\\d{2}[-/ ]?(\\d{2})[-/ ]?(\\d{2})(?!\\d)").match(t);
    if (m.hasMatch()) { int mo = m.captured(2).toInt(); if (mo >= 1 && mo <= 12) return mo; }
    return QDate::currentDate().month();
}

void SPCWindow::recomputeAll() {
    if (m_prof.empty()) return;

    m_sb = surface_parcel(m_prof);
    m_ml = mixed_layer_parcel(m_prof, 100.0);
    m_fcst = forecast_parcel(m_prof);
    m_mu = most_unstable_parcel(m_prof, 300.0);

    m_bunkers = bunkers_storm_motion(m_prof);
    const BunkersMotion bunkersRaw = m_bunkers;      // prof.bunkers (before user storm motion)
    if (m_hasCustomStorm) {
        if (m_useLeftMover) {
            m_bunkers.left_mover.u = m_customStormU;
            m_bunkers.left_mover.v = m_customStormV;
            comp2vec(m_bunkers.left_mover.u, m_bunkers.left_mover.v, m_bunkers.left_mover.wdir, m_bunkers.left_mover.wspd);
        } else {
            m_bunkers.right_mover.u = m_customStormU;
            m_bunkers.right_mover.v = m_customStormV;
            comp2vec(m_bunkers.right_mover.u, m_bunkers.right_mover.v, m_bunkers.right_mover.wdir, m_bunkers.right_mover.wspd);
        }
    }

    const WindVector& active_sm = m_useLeftMover ? m_bunkers.left_mover : m_bunkers.right_mover;
    m_inflow = effective_inflow_layer(m_prof, active_sm.u, active_sm.v);
    m_params = compute_severe_params(m_prof, m_sb, m_ml, m_mu, m_bunkers, m_useLeftMover, &m_inflow);

    WindVector shear06 = wind_shear(m_prof, 0, 6000);
    double srh01 = storm_relative_helicity(m_prof, 0, 1000, active_sm.u, active_sm.v);
    double srh03 = storm_relative_helicity(m_prof, 0, 3000, active_sm.u, active_sm.v);

    // Profile.location / Profile.date: station id and month come from the sounding when known
    QString pwvStation = !m_stationId.isEmpty() ? m_stationId : QString::fromStdString(m_prof.station_id);
    const QString titleQ = QString::fromStdString(m_currentTitle).trimmed();
    if (pwvStation.isEmpty()) pwvStation = titleQ.section(QRegularExpression("\\s+"), 0, 0);
    const int pwvMonth = monthFromTitle(titleQ);
    const int pwvFlag = pwv_climo(m_prof, pwvStation, pwvMonth);
    const tab::ThermoIndices tix = tab::compute_thermo_indices(m_prof, pwvFlag);
    auto fz = [](double v) { return std::isfinite(v) ? v : 0.0; };

    // effective inflow layer parcel (profile.get_parcels): sfc parcel when there is no layer
    m_eff = m_sb;
    if (m_inflow.bot_p != MISSING && m_inflow.top_p != MISSING) {
        tab::Env env(m_prof);
        double mtha = tab::mean_theta_exact(env, m_inflow.bot_p, m_inflow.top_p);
        double mmr = tab::mean_mixratio_exact(env, m_inflow.bot_p, m_inflow.top_p);
        double effpres = (m_inflow.bot_p + m_inflow.top_p) / 2.;
        double efftmpc = tab::theta(1000., mtha, effpres);
        double effdwpc = tab::temp_at_mixrat(mmr, effpres);
        if (std::isfinite(efftmpc) && std::isfinite(effdwpc)) m_eff = lift_parcel(m_prof, effpres, efftmpc, effdwpc, true);
    }
    if (!m_hasUsr) m_usr = m_sb;

    updateActiveParcel();

    m_skewt->setKinematics(m_bunkers, m_inflow);
    m_speed->setProfile(m_prof);
    m_adv->setProfile(m_prof);

    m_hodo->setProfileOnly(m_prof);
    m_hodo->setKinematics(bunkersRaw, m_bunkers, m_inflow, m_mu, m_useLeftMover);

    m_thetae->setProfile(m_prof, m_bunkers);
    m_srwinds->setProfile(m_prof, m_bunkers);
    m_srwinds->setDeviant(m_useLeftMover);

    m_convective->setProfile(m_prof);
    m_convective->setParcels(m_sb, m_ml, m_fcst, m_mu, m_eff, m_usr);
    m_convective->setIndices(tix);
    m_convective->setSevere(m_params.ship, m_params.stp_fixed, m_params.stp_cin, m_params.scp);

    m_kinematics->setProfile(m_prof, bunkersRaw, m_bunkers, m_inflow, m_mu);
    m_kinematics->setDeviant(m_useLeftMover);

    {
        WatchInputs wi;
        wi.stp_eff = m_params.stp_cin; wi.stp_fixed = m_params.stp_fixed; wi.scp = m_params.scp;
        wi.srw_4_6km = m_params.srw_4_6km_kts;
        wi.esrh = (m_useLeftMover ? -1.0 : 1.0) * m_inflow.esrh; wi.srh1km = (m_useLeftMover ? -1.0 : 1.0) * srh01;   // LM SRH sign-flipped (composites are, too)
        wi.latitude = 0.0;   // hemisphere sign handling is done upstream (left-mover SRH/composites are sign-flipped)
        wi.sfc_8km_shear = m_params.sfc_8km_shear_kts;
        wi.sfc_lclhght = m_sb.lcl_h_agl; wi.ml_lclhght = m_ml.lcl_h_agl;
        wi.lr1 = lapse_rate(m_prof, 0.0, 1000.0, false);
        wi.ml_bminus = m_ml.cin; wi.mu_bminus = m_mu.cin;
        wi.ebotm = (m_inflow.bot_h_agl == MISSING) ? 1.0 : m_inflow.bot_h_agl;
        wi.low_rh = fz(tix.low_rh); wi.mid_rh = fz(tix.mid_rh);
        wi.ship = m_params.ship; wi.dcape = fz(tix.dcape); wi.sig_severe = fz(tix.sig_severe);
        wi.mmp = fz(tix.mmp); wi.wndg = fz(tix.wndg);
        CorfidiMotion cf = corfidi_mcs_motion(m_prof);
        wi.upshear_dn_spd = cf.upwind.wspd;
        wi.sfc_wspd_kts = m_prof.wspd.empty() ? 0.0 : m_prof.wspd.front();
        wi.sfc_temp_c = m_prof.temp.empty() ? 0.0 : m_prof.temp.front();
        wi.sfc_dwpt_c = m_prof.dwpt.empty() ? 0.0 : m_prof.dwpt.front();
        wi.sfc_pres = m_prof.sfc_pres;
        // "Snow" in prof.precip_type (only evaluated when the other blizzard conditions hold)
        if (wi.sfc_wspd_kts * 1.15078 > 35. && wi.sfc_temp_c <= 0)
            wi.snow_precip = QString::fromStdString(compute_winter(m_prof).precip_type).contains("Snow");
        wi.pwv_flag = pwvFlag;
        m_watch->setWatchType(QString::fromStdString(possible_watch(wi).front()));
    }
    m_sars->setSars(compute_sars(m_prof, m_mu, m_ml, m_bunkers, default_sars_dir()));
    m_sars->setDeviant(m_useLeftMover);
    m_winter->setProfile(m_prof);
    m_fire->setProfile(m_prof);

    {
        const double ebwd_kt = (m_params.ebwd_kts == MISSING) ? 0.0 : m_params.ebwd_kts;
        const double lat = 0.0;     // sign handling is upstream: left-mover (incl. southern-hemisphere) values arrive flipped
        m_stpBox->setInputs(m_ml.cape, m_ml.lcl_h_agl, ebwd_kt, (m_useLeftMover ? -1.0 : 1.0) * m_inflow.esrh,
                            m_params.stp_cin, m_params.stp_fixed, lat);
        m_stpEf->setStpCin(m_params.stp_cin, lat);
    }
    m_ship->setShip(m_params.ship);
}

// SPCWidget.updateConfig: push every preference (colors of the selected style, units, readout vars) into all widgets.
void SPCWindow::applyPreferences(const AppPreferences& a) {
    m_appPrefs = a;
    const VizPrefs v = toVizPrefs(a);
    const QColor bg(v.value("bg_color", "#000000"));
    const QString bgHex = bg.name();
    setStyleSheet(QString(
        "QMainWindow { background-color: %1; }"
        "QMenuBar { background-color: #111111; color: #d0d0d0; font-size: 11px; font-family: Monospace; }"
        "QMenuBar::item:selected { background-color: #005080; }"
        "QMenu { background-color: #1a1a1a; color: #ffffff; border: 1px solid #00a0dc; font-family: Monospace; }"
        "QMenu::item:selected { background-color: #00a0dc; color: #000000; }").arg(bgHex));
    if (centralWidget()) centralWidget()->setStyleSheet(QString("QWidget#spcCentral { background-color: %1; }").arg(bgHex));
    if (m_brand) m_brand->setStyleSheet(QString("QLabel { background-color: %1; padding-top: 4px; padding-bottom: 4px; font-size: 11px; color: %2; }")
                                            .arg(bgHex, v.value("fg_color", "#ffffff")));
    m_skewt->setPreferences(v);
    m_hodo->setPreferences(v);
    m_slinky->setPreferences(v);
    m_adv->setPreferences(v);
    m_speed->setPreferences(v);
    m_srwinds->setPreferences(v);
    m_thetae->setPreferences(v);
    m_watch->setPreferences(v);
    m_convective->setPreferences(v);
    m_kinematics->setPreferences(v);
    m_sars->setPreferences(v);
    m_winter->setPreferences(v);
    m_fire->setPreferences(v);
    m_stpBox->setPreferences(v);
    m_stpEf->setPreferences(v);
    m_ship->setPreferences(v);
    m_vrot->setPreferences(v);
    update();
    for (QWidget* w : findChildren<QWidget*>()) w->update();
}

void SPCWindow::showPreferences() {
    PreferencesDialog dlg(m_appPrefs, this);
    if (dlg.exec() == QDialog::Accepted) {
        saveAppPreferences(dlg.getPreferences());
        applyPreferences(dlg.getPreferences());
        recomputeAll();
    }
}

// ---- interpolation (SPCWindow.interpProf / resetProf; options dialog replaces the Tkinter one) ----
bool SPCWindow::isInterpolated() const { return m_preInterp.contains(m_collection.isEmpty() ? -1 : m_collectionIdx); }

void SPCWindow::syncInterpActions() {
    const bool on = isInterpolated();
    m_actResetInterp->setVisible(on);
    m_actInterp->setVisible(!on);
}

void SPCWindow::clearInterpolation() { m_preInterp.clear(); if (m_actInterp) syncInterpActions(); }

void SPCWindow::interpolateProfile() {
    if (m_prof.empty() || isInterpolated()) return;
    QDialog dlg(this);
    dlg.setWindowTitle("Interpolation Options");
    QVBoxLayout* lay = new QVBoxLayout(&dlg);
    QRadioButton* fill = new QRadioButton("Fill missing values only  (linear interp between valid points)");
    QRadioButton* grid = new QRadioButton("Interpolate to regular pressure grid");
    fill->setChecked(true);
    lay->addWidget(new QLabel("Interpolation mode:"));
    lay->addWidget(fill);
    lay->addWidget(grid);
    QFormLayout* form = new QFormLayout;
    QDoubleSpinBox* dp = new QDoubleSpinBox; dp->setRange(1, 500); dp->setValue(25); dp->setDecimals(1);
    QDoubleSpinBox* pb = new QDoubleSpinBox; pb->setRange(0, 1100); pb->setValue(0); pb->setSpecialValueText("surface");
    QDoubleSpinBox* pt = new QDoubleSpinBox; pt->setRange(0, 1100); pt->setValue(0); pt->setSpecialValueText("top");
    form->addRow("Pressure step (hPa):", dp);
    form->addRow("Bottom pressure (hPa):", pb);
    form->addRow("Top pressure (hPa):", pt);
    lay->addLayout(form);
    auto toggle = [=]() { dp->setEnabled(grid->isChecked()); pb->setEnabled(grid->isChecked()); pt->setEnabled(grid->isChecked()); };
    connect(grid, &QRadioButton::toggled, &dlg, toggle);
    toggle();
    QDialogButtonBox* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    lay->addWidget(bb);
    if (dlg.exec() != QDialog::Accepted) return;

    Profile out;
    const bool ok = interpolate_profile(m_prof, out, grid->isChecked() ? InterpMode::PressureGrid : InterpMode::FillMissing,
                                        -dp->value(), pb->value(), pt->value());
    if (!ok) { QMessageBox::warning(this, "Interpolation", "The requested pressure grid leaves fewer than two levels."); return; }
    const int key = m_collection.isEmpty() ? -1 : m_collectionIdx;
    m_preInterp.insert(key, m_prof);            // _orig_profs / _interp_profs
    if (key >= 0) m_collection[key] = out;
    m_prof = out;
    m_origProfile = out;                         // resetModification returns to the interpolated profile
    m_hasCustomStorm = false;
    syncInterpActions();
    recomputeAll();
}

void SPCWindow::resetInterpolation() {
    const int key = m_collection.isEmpty() ? -1 : m_collectionIdx;
    auto it = m_preInterp.find(key);
    if (it == m_preInterp.end()) return;
    m_prof = it.value();
    m_origProfile = m_prof;
    if (key >= 0) m_collection[key] = m_prof;
    m_preInterp.erase(it);
    m_hasCustomStorm = false;
    syncInterpActions();
    recomputeAll();
}

} // namespace sharppy
