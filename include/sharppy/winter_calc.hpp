#pragma once
#include <string>
#include "sharppy/profile.hpp"

namespace sharppy {

// Quantities shown by the winter inset (profile.py get_winter + watch_type.py precip routines).
struct WinterData {
    double dgz_pbot = 0, dgz_ptop = 0;
    double dgz_zbot_ft = 0, dgz_ztop_ft = 0, dgz_depth_ft = 0;   // MSL feet
    double dgz_meanrh = 0, dgz_pw = 0, dgz_meanq = 0;
    // omega isn't carried by Profile, so dgz_meanomeg is always "missing" (OPRH shown as N/A)
    double plevel = MISSING, tmp = MISSING; int phase = -1; std::string init_st = "N/A";
    double tpos = 0, tneg = 0, ttop = 0, tbot = 0;
    double wpos = 0, wneg = 0, wtop = 0, wbot = 0;
    std::string precip_type;
    double sfc_tmpf = 0;
};

double temp_lvl(const Profile& prof, double temp_c);             // first level (hPa) at temp, or MISSING
WinterData compute_winter(const Profile& prof);

} // namespace sharppy
