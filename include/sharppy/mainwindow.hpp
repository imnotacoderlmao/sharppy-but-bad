#pragma once

#include <QMainWindow>
#include "sharppy/spc.hpp"
#include "sharppy/skewt_widget.hpp"
#include "sharppy/windbar_widget.hpp"
#include "sharppy/advbar_widget.hpp"
#include "sharppy/hodo_widget.hpp"
#include "sharppy/slinky_widget.hpp"
#include "sharppy/thetae_widget.hpp"
#include "sharppy/srwind_widget.hpp"
#include "sharppy/hazard_widget.hpp"
#include "sharppy/table_widget.hpp"
#include "sharppy/stpbox_widget.hpp"
#include "sharppy/header_widget.hpp"

namespace sharppy {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    bool loadSoundingFile(const std::string& path);

private slots:
    void onSoundingModified();
    void onCustomStormMotion(double u, double v);

private:
    HeaderWidget* m_header;
    SkewTWidget* m_skewt;
    WindBarWidget* m_windbar;
    AdvBarWidget* m_advbar;
    HodoWidget* m_hodo;

    // 4 Inset Widgets below Hodograph
    SlinkyWidget* m_slinky;
    ThetaEWidget* m_thetae;
    SRWindWidget* m_srwind;
    HazardWidget* m_hazard;

    TableWidget* m_table;
    STPBoxWidget* m_stpbox;

    Profile m_origProfile;
    SoundingMetadata m_meta;
    void recomputeDiagnostics(double custom_u = -9999.0, double custom_v = -9999.0);
};

} // namespace sharppy