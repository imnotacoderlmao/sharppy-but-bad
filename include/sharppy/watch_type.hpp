#pragma once
#include <string>
#include <vector>

namespace sharppy {

// Inputs to possible_watch() (port of sharptab/watch_type.py in the sharppy-but-bad fork).
// Left/right-mover specific values must already be those of the active mover.
struct WatchInputs {
    double stp_eff = 0, stp_fixed = 0, scp = 0;
    double srw_4_6km = 0;      // |SR wind 4-6 km| kts
    double esrh = 0, srh1km = 0;
    double latitude = 0;       // < 0 flips STP/SRH signs
    double sfc_8km_shear = 0;  // kts
    double sfc_lclhght = 0, ml_lclhght = 0;   // m AGL
    double lr1 = 0;            // 0-1 km lapse rate (C/km)
    double ml_bminus = 0, mu_bminus = 0;      // CIN (J/kg, negative)
    double ebotm = 0;          // effective inflow base (m AGL)
    double low_rh = 0, mid_rh = 0;
    double ship = 0, dcape = 0, sig_severe = 0, mmp = 0, wndg = 0;
    int    pwv_flag = 0;       // PWV climatology flag (needs pwv.py; 0 until ported)
    double upshear_dn_spd = 99;// |Corfidi upshear| kts
    double sfc_wspd_kts = 0, sfc_temp_c = 0, sfc_dwpt_c = 0, sfc_pres = 1000;
    bool   snow_precip = false;// prof.precip_type contains "Snow" (set from compute_winter)
};

double heat_index(double temp_f, double rh);
std::vector<std::string> possible_watch(const WatchInputs& in);

} // namespace sharppy
