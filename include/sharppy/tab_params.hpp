#pragma once
// Exact ports of the sharptab/params.py indices shown in the thermo panel (viz/thermo.py).
// NaN means "masked".
#include "sharppy/profile.hpp"

namespace sharppy { namespace tab {

// params.lapse_rate (virtual temperature based); NaN when masked
double lapse_rate_exact(const Profile& prof, double lower, double upper, bool pres);

struct ThermoIndices {
    double k_idx = NAN, pwat = NAN;
    double lr3 = NAN, lr36 = NAN, lr850_500 = NAN, lr700_500 = NAN;
    double convT_F = NAN, maxT_F = NAN;
    double mean_mixr = NAN, low_rh = NAN, mid_rh = NAN, tt = NAN;
    double dcape = NAN, drush_F = NAN;
    double sig_severe = NAN, mmp = NAN, esp = NAN, wndg = NAN, tei = NAN, mburst = NAN;
    double mlb3km = NAN, mlbplus = NAN;       // ML parcel 0-3 km CAPE / CAPE (3CAPE colouring)
    int pwv_flag = 0;
};

// profile.get_thermo() + the helpers it needs (esp/mmp/wndg/sig_severe/tei/mburst/convective_temp)
ThermoIndices compute_thermo_indices(const Profile& prof, int pwv_flag);

// profile.get_fire(): everything the FIRE inset shows (NaN == masked; haines_* are MISSING (-9999) when unavailable)
struct FireData {
    double fosberg = NAN;
    int haines_hght = 0;                       // 0 low, 1 mid, 2 high
    double haines[3] = {-9999., -9999., -9999.};
    double sfc_rh = NAN, rh01km = NAN, pblrh = NAN, pbl_h = NAN;
    double mw01_dir = NAN, mw01_spd = NAN;     // comp2vec(meanwind01km)
    double mwpbl_dir = NAN, mwpbl_spd = NAN;   // comp2vec(meanwindpbl)
    double sfc_dir = NAN, sfc_spd = NAN;
    double max_dir = NAN, max_spd = NAN;       // comp2vec(pblmaxwind); NaN == masked
    double pwat = NAN, bplus_fire = NAN;
};
FireData compute_fire(const Profile& prof);

}} // namespace sharppy::tab
