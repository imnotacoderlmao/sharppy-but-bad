#include "sharppy/mainwindow.hpp"
#include "sharppy/spc.hpp"
#include "sharppy/modify.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMenuBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QApplication>
#include <QStatusBar>
#include <QAction>
#include <QActionGroup>

namespace sharppy {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("SHARPpy C++ (Native)");
    resize(1400, 920);

    setStyleSheet(
        "QMainWindow { background-color: #000000; }"
        "QMenuBar { background-color: #111111; color: #d0d0d0; font-size: 11px; font-family: Monospace; }"
        "QMenuBar::item:selected { background-color: #005080; }"
        "QMenu { background-color: #1a1a1a; color: #ffffff; border: 1px solid #00a0dc; font-family: Monospace; }"
        "QMenu::item:selected { background-color: #00a0dc; color: #000000; }"
    );

    QWidget* central = new QWidget(this);
    QVBoxLayout* rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(4, 4, 4, 4);
    rootLayout->setSpacing(4);

    m_header = new HeaderWidget(this);
    rootLayout->addWidget(m_header);

    QHBoxLayout* midLayout = new QHBoxLayout();
    midLayout->setSpacing(4);

    m_skewt = new SkewTWidget(this);
    m_windbar = new WindBarWidget(this);
    m_advbar = new AdvBarWidget(this);

    // Right Column: Hodograph on top, 4-Inset Row on bottom
    QWidget* rightCol = new QWidget(this);
    QVBoxLayout* rightLayout = new QVBoxLayout(rightCol);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(2);

    m_hodo = new HodoWidget(this);

    // 4 Inset widgets side by side: [ Slinky ] [ ThetaE ] [ SRWind ] [ Hazard ]
    QHBoxLayout* insetRow = new QHBoxLayout();
    insetRow->setContentsMargins(0, 0, 0, 0);
    insetRow->setSpacing(2);

    m_slinky = new SlinkyWidget(this);
    m_thetae = new ThetaEWidget(this);
    m_srwind = new SRWindWidget(this);
    m_hazard = new HazardWidget(this);

    insetRow->addWidget(m_slinky, 1);
    insetRow->addWidget(m_thetae, 1);
    insetRow->addWidget(m_srwind, 1);
    insetRow->addWidget(m_hazard, 1);

    rightLayout->addWidget(m_hodo, 1);
    rightLayout->addLayout(insetRow, 0);
    rightCol->setFixedWidth(540);
    
    midLayout->addWidget(m_skewt, 1);
    midLayout->addWidget(m_windbar, 0);
    midLayout->addWidget(m_advbar, 0);
    midLayout->addWidget(rightCol, 0);

    rootLayout->addLayout(midLayout, 1);

    QHBoxLayout* bottomLayout = new QHBoxLayout();
    bottomLayout->setSpacing(4);

    m_table = new TableWidget(this);
    m_stpbox = new STPBoxWidget(this);
    m_stpbox->setFixedWidth(320);

    bottomLayout->addWidget(m_table, 1);
    bottomLayout->addWidget(m_stpbox, 0);

    rootLayout->addLayout(bottomLayout, 0);

    // Set seamless layout margins:
    rootLayout->setContentsMargins(2, 2, 2, 2);
    rootLayout->setSpacing(2);

    midLayout->setContentsMargins(0, 0, 0, 0);
    midLayout->setSpacing(2);

    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(2);

    insetRow->setContentsMargins(0, 0, 0, 0);
    insetRow->setSpacing(2);

    bottomLayout->setContentsMargins(0, 0, 0, 0);
    bottomLayout->setSpacing(2);

    setCentralWidget(central);

    QMenu* fileMenu = menuBar()->addMenu("&File");
    QAction* openAct = fileMenu->addAction("&Open Sounding...");
    connect(openAct, &QAction::triggered, this, [this]() {
        QString file = QFileDialog::getOpenFileName(this, "Open Sounding File", "", "SPC Raw (*.txt *.raw);;All Files (*)");
        if (!file.isEmpty()) {
            loadSoundingFile(file.toStdString());
        }
    });

    QAction* saveAct = fileMenu->addAction("&Save Modified Sounding...");
    connect(saveAct, &QAction::triggered, this, [this]() {
        QString file = QFileDialog::getSaveFileName(this, "Save SPC Sounding", "", "SPC Raw (*.txt *.raw)");
        if (!file.isEmpty()) {
            ProfileModifier::export_spc(m_skewt->currentProfile(), m_meta.title, file.toStdString());
        }
    });

    QMenu* editMenu = menuBar()->addMenu("&Edit");
    QAction* resetAct = editMenu->addAction("&Reset Modifications");
    connect(resetAct, &QAction::triggered, this, [this]() {
        if (!m_origProfile.empty()) {
            m_skewt->setProfile(m_origProfile, surface_parcel(m_origProfile));
            m_header->setModified(false); // Clears (MODIFIED) tag
            recomputeDiagnostics();
        }
    });

    QAction* editModeAct = editMenu->addAction("&Enable Modification Mode");
    editModeAct->setCheckable(true);
    editModeAct->setChecked(false);
    editModeAct->setShortcut(QKeySequence("Ctrl+E"));

    QActionGroup* modeGroup = new QActionGroup(this);

    QAction* snapModeAct = editMenu->addAction("  • Mode: Snap-to-Point Drag");
    snapModeAct->setCheckable(true);
    snapModeAct->setChecked(true); // Default
    snapModeAct->setActionGroup(modeGroup);

    QAction* lineModeAct = editMenu->addAction("  • Mode: Line Segment (Rubber-Band)");
    lineModeAct->setCheckable(true);
    lineModeAct->setActionGroup(modeGroup);

    m_skewt->setInteractiveMode(false);
    m_hodo->setInteractiveMode(false); // Locked by default

    connect(editModeAct, &QAction::toggled, this, [this](bool enabled) {
        m_skewt->setInteractiveMode(enabled);
        m_hodo->setInteractiveMode(enabled);
        m_header->setEditMode(enabled); // Updates top bar!
    });

    connect(m_hodo, &HodoWidget::windModified, this, [this]() {
        const Profile& modified_prof = m_hodo->currentProfile();
        // Update Skew-T profile and recalculate all diagnostics
        m_skewt->setProfile(modified_prof, surface_parcel(modified_prof));
        recomputeDiagnostics();
    });

    connect(snapModeAct, &QAction::triggered, this, [this]() {
        m_skewt->setEditMode(EditMode::PointSnap);
        statusBar()->showMessage("Edit Mode: Snap-to-Point Drag", 3000);
    });

    connect(lineModeAct, &QAction::triggered, this, [this]() {
        m_skewt->setEditMode(EditMode::LineSegment);
        statusBar()->showMessage("Edit Mode: Line Segment (Rubber-Band)", 3000);
    });

    QMenu* viewMenu = menuBar()->addMenu("&View");
    QAction* tvAct = viewMenu->addAction("Show &Virtual Temp (Tv)");
    tvAct->setCheckable(true);
    tvAct->setChecked(true);
    connect(tvAct, &QAction::toggled, this, [this](bool checked) {
        m_skewt->setVirtualTempEnabled(checked);
    });

    QAction* zoomInAct = viewMenu->addAction("Zoom &In (Skew-T)");
    zoomInAct->setShortcut(QKeySequence::ZoomIn);
    connect(zoomInAct, &QAction::triggered, this, [this]() {
        // Trigger zoom in
        QWheelEvent event(QPointF(200, 200), QPointF(200, 200), QPoint(0, 120), QPoint(0, 120),
                          Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(m_skewt, &event);
    });

    QAction* zoomOutAct = viewMenu->addAction("Zoom &Out (Skew-T)");
    zoomOutAct->setShortcut(QKeySequence::ZoomOut);
    connect(zoomOutAct, &QAction::triggered, this, [this]() {
        QWheelEvent event(QPointF(200, 200), QPointF(200, 200), QPoint(0, -120), QPoint(0, -120),
                          Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(m_skewt, &event);
    });

    QAction* resetZoomAct = viewMenu->addAction("&Reset Zoom");
    resetZoomAct->setShortcut(QKeySequence(Qt::Key_R));
    connect(resetZoomAct, &QAction::triggered, this, [this]() {
        m_skewt->resetZoom();
    });

    connect(m_skewt, &SkewTWidget::zoomChanged, this, [this](double pBot, double pTop) {
        m_windbar->setPresBounds(pBot, pTop);
        m_advbar->setPresBounds(pBot, pTop);
        m_hodo->setPresBounds(pBot, pTop);
    });

    connect(m_skewt, &SkewTWidget::profileModified, this, &MainWindow::onSoundingModified);
    connect(m_hodo, &HodoWidget::stormMotionChanged, this, &MainWindow::onCustomStormMotion);
}

bool MainWindow::loadSoundingFile(const std::string& path) {
    Profile prof;
    SoundingMetadata meta;

    if (!SpcParser::parse_file(path, prof, meta)) {
        QMessageBox::critical(this, "Error", "Failed to parse sounding file.");
        return false;
    }

    m_origProfile = prof;
    m_meta = meta;
    m_header->setText(meta.title, prof.size());
    m_header->setModified(false); // Reset modified state on fresh load
    m_skewt->setProfile(prof, surface_parcel(prof));
    recomputeDiagnostics();
    return true;
}

void MainWindow::onSoundingModified() {
    m_header->setModified(true); // Flags title as (MODIFIED)
    recomputeDiagnostics();
}

void MainWindow::onCustomStormMotion(double u, double v) {
    recomputeDiagnostics(u, v);
}

void MainWindow::recomputeDiagnostics(double custom_u, double custom_v) {
    const Profile& prof = m_skewt->currentProfile();
    if (prof.empty()) return;

    ParcelResult sb = surface_parcel(prof);
    ParcelResult ml = mixed_layer_parcel(prof, 100.0);
    ParcelResult mu = most_unstable_parcel(prof, 300.0);
    BunkersMotion bunkers = bunkers_storm_motion(prof);

    double storm_u = (custom_u != -9999.0) ? custom_u : bunkers.right_mover.u;
    double storm_v = (custom_v != -9999.0) ? custom_v : bunkers.right_mover.v;

    SevereParams params = compute_severe_params(prof, sb, ml, mu, bunkers);
    InflowLayer inflow = effective_inflow_layer(prof, storm_u, storm_v);
    WindVector shear06 = wind_shear(prof, 0, 6000);
    double srh01 = storm_relative_helicity(prof, 0, 1000, storm_u, storm_v);
    double srh03 = storm_relative_helicity(prof, 0, 3000, storm_u, storm_v);
    SarsResult sars = SarsEngine::query(prof, mu, ml, shear06.wspd, srh01, srh03);

    m_skewt->setProfile(prof, sb);
    m_skewt->setKinematics(bunkers, inflow);
    m_windbar->setProfile(prof);
    m_advbar->setProfile(prof);
    m_hodo->setProfile(prof, bunkers);
    m_table->setResults(prof, sb, ml, mu, bunkers, params, inflow);
    m_table->setSars(sars);
    m_stpbox->setInputs(ml.cape, ml.lcl_h_agl, (params.ebwd_kts == MISSING ? 0.0 : params.ebwd_kts), inflow.esrh, params.stp_cin, params.stp_fixed, 0.0);
    m_slinky->setProfile(prof, sb, bunkers);
    m_thetae->setProfile(prof, bunkers);
    m_srwind->setResults(prof, bunkers, params);
    m_hazard->setParams(params);
}

} // namespace sharppy
