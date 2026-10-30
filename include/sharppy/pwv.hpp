#pragma once
#include <QString>
#include "sharppy/profile.hpp"

namespace sharppy {

// Port of sharptab/pwv.py + params.precip_water (non-exact path).
// precip_water: PWV in inches from the surface to ptop (hPa), 1 hPa steps.
double precip_water(const Profile& prof, double ptop = 400.0, double pbot = 0.0);

// Returns -3..3 sigma flag of surface-300 hPa PWV vs. the station/month climatology, or 0 when the
// station/month is unknown or PW-mean-inches.txt / PW-stdev-inches.txt can't be found
// (looked for in data/, data/sharptab/, next to the executable and their parents).
int pwv_climo(const Profile& prof, const QString& station, int month);

} // namespace sharppy
