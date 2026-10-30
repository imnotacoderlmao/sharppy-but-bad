#include "sharppy/watch_type.hpp"
#include "sharppy/thermo.hpp"
#include <cmath>
#include <algorithm>

namespace sharppy {

double heat_index(double temp, double rh) {
    if (temp < 40) return temp;
    double hi = 0.5 * (temp + 61.0 + ((temp - 68.0) * 1.2) + (rh * 0.094));
    double avg = (hi + temp) / 2.;
    if (avg < 80) return hi;
    double h = -42.379 + 2.04901523 * temp + 10.14333127 * rh - 0.22475541 * temp * rh
             - 6.83783e-3 * temp * temp - 5.481717e-2 * rh * rh + 1.22874e-3 * rh * temp * temp
             + 8.5282e-4 * temp * rh * rh - 1.99e-6 * rh * rh * temp * temp;
    if (rh < 13 && temp > 80 && temp < 112)
        h -= ((13 - rh) / 4.) * std::sqrt((17 - std::fabs(temp - 95)) / 17.);
    else if (rh > 85 && temp > 80 && temp < 87)
        h += ((rh - 85) / 10.) * ((87 - temp) / 5.);
    return h;
}

std::vector<std::string> possible_watch(const WatchInputs& in) {
    std::vector<std::string> w;
    double stp_eff = in.stp_eff, stp_fixed = in.stp_fixed, esrh = in.esrh, srh1km = in.srh1km;
    if (in.latitude < 0) { stp_eff = -stp_eff; stp_fixed = -stp_fixed; esrh = -esrh; srh1km = -srh1km; }
    const double srw = in.srw_4_6km, shr8 = in.sfc_8km_shear, lr1 = in.lr1;
    const bool ok0 = in.ebotm == 0;

    if (stp_eff >= 50 && stp_fixed >= 50 && srh1km >= 850 && esrh >= 1200 && srw >= 60.0 &&
        shr8 > 100.0 && in.sfc_lclhght < 1000. && in.ml_lclhght < 1200 && lr1 >= 7.0 && in.ml_bminus > -50 && ok0)
        w.push_back("are you\n in jupiter?");
    else if (stp_eff >= 10 && stp_fixed >= 10 && srh1km >= 500 && esrh >= 500 && srw >= 45.0 &&
             shr8 > 65.0 && in.sfc_lclhght < 1000. && in.ml_lclhght < 1200 && lr1 >= 7.0 && in.ml_bminus > -50 && ok0)
        w.push_back("WHAT THE FUCK");
    else if (stp_eff >= 3 && stp_fixed >= 3 && srh1km >= 200 && esrh >= 200 && srw >= 15.0 &&
             shr8 > 45.0 && in.sfc_lclhght < 1000. && in.ml_lclhght < 1200 && lr1 >= 5.0 && in.ml_bminus > -50 && ok0)
        w.push_back("BIG WEDGE\n OMGGGG");
    else if ((stp_eff >= 3 || stp_fixed >= 4) && in.ml_bminus > -125. && ok0)
        w.push_back("OMG TOR");
    else if ((stp_eff >= 1 || stp_fixed >= 1) && (srw >= 15.0 || shr8 >= 40) && in.ml_bminus > -50 && ok0)
        w.push_back("OMG TOR");
    else if ((stp_eff >= 1 || stp_fixed >= 1) && ((in.low_rh + in.mid_rh) / 2. >= 60) && lr1 >= 5.0 && in.ml_bminus > -50 && ok0)
        w.push_back("OMG TOR");
    else if ((stp_eff >= 1 || stp_fixed >= 1) && in.ml_bminus > -150 && ok0)
        w.push_back("TOR???");
    // Python precedence: A or (B and C and D) -- reproduced as written.
    else if ((stp_eff >= 0.5 && esrh >= 150) || ((stp_fixed >= 0.5 && srh1km >= 150) && in.ml_bminus > -50. && ok0))
        w.push_back("TOR???");

    const double scp = in.scp;
    if ((stp_fixed >= 1.0 || scp >= 4.0 || stp_eff >= 1.0) && in.mu_bminus >= -50)
        w.push_back("STORMS");
    else if (scp >= 2.0 && (in.ship >= 1.0 || in.dcape >= 750) && in.mu_bminus >= -50)
        w.push_back("STORMS");
    else if (in.sig_severe >= 30000 && in.mmp >= 0.6 && in.mu_bminus >= -50)
        w.push_back("STORMS");
    else if (in.mu_bminus >= -75.0 && (in.wndg >= 0.5 || in.ship >= 0.5 || scp >= 0.5))
        w.push_back("WEAKASS STORMS");

    if (in.pwv_flag >= 2 && in.upshear_dn_spd < 25) w.push_back("HUGE WATER");

    const double sfc_mph = in.sfc_wspd_kts * 1.15077945;
    if (sfc_mph > 35. && in.sfc_temp_c <= 0 && in.snow_precip) w.push_back("SNOWY WIND");

    // watch_type.py calls relh(pres, temp_F, dwpc): temperature in F is passed to relh. Reproduced as-is.
    double tf = in.sfc_temp_c * 9. / 5. + 32.;
    double rh = 100. * vappres(in.sfc_dwpt_c) / vappres(tf);
    if (heat_index(tf, rh) > 105.) w.push_back("THE SUN");

    w.push_back("NONE");
    return w;
}

} // namespace sharppy
