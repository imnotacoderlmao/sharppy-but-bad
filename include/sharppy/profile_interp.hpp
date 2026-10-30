#pragma once
#include "sharppy/profile.hpp"
namespace sharppy {

enum class InterpMode { FillMissing, PressureGrid };

// ProfCollection.interp: 'fill_missing' re-evaluates every variable at the original pressure levels
// (log-p interpolation between valid points); 'pressure_grid' resamples onto
// np.linspace(pbot, ptop, round((pbot-ptop)/|dp|)+1). pbot/ptop <= 0 mean "surface"/"top of sounding".
// Levels the source cannot cover (outside its pressure range) are dropped. Returns false if < 2 levels remain.
bool interpolate_profile(const Profile& src, Profile& out, InterpMode mode, double dp = -25.0,
                         double pbot = 0.0, double ptop = 0.0);

} // namespace sharppy
