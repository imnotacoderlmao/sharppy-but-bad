#pragma once
#include <QWidget>
#include <array>
#include <utility>
#include "sharppy/viz_prefs.hpp"

namespace sharppy {

// Port of viz/stp.py (plotSTP): effective-layer STP box/whisker + probability box.
class STPBoxWidget : public QWidget {
    Q_OBJECT
public:
    explicit STPBoxWidget(QWidget* parent = nullptr);

    // Inputs for the active mover: esrh = effective SRH, stpc/stpf = STP(cin)/STP(fixed).
    // ebwspd in knots, mllcl in m AGL. latitude < 0 flips the signs like stp.py.
    void setInputs(double mlcape, double mllcl, double ebwspd,
                   double esrh, double stpc, double stpf, double latitude);
    void setPreferences(const VizPrefs& prefs);

protected:
    void paintEvent(QPaintEvent*) override;

private:
    struct Geo { double tpad, bpad, brx, bry, wid, hgt, plot_h, box_h; int fs1, fs2; };
    Geo geo() const;
    double stp_to_pix(const Geo& g, double v) const;
    using Pc = std::pair<double, QColor>;
    Pc cape_prob(double) const; Pc lcl_prob(double) const; Pc esrh_prob(double) const;
    Pc ebwd_prob(double) const; Pc stpc_prob(double) const; Pc stpf_prob(double) const;
    void recompute();

    bool m_hasData = false;
    double m_mlcape = 0, m_mllcl = 0, m_ebwd = 0, m_esrh = 0, m_stpc = 0, m_stpf = 0, m_lat = 0;
    Pc m_cape, m_lcl, m_esrhp, m_ebwdp, m_stpcp, m_stpfp;
    QColor m_bg{"#000000"}, m_fg{"#ffffff"}, m_box{"#00ff00"}, m_line{"#0080ff"};
    std::array<QColor, 6> m_alert{{QColor("#775000"), QColor("#996600"), QColor("#ffffff"),
                                   QColor("#ffff00"), QColor("#ff0000"), QColor("#e700df")}};
};

} // namespace sharppy
