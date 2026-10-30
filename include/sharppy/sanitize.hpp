#pragma once

#include "sharppy/profile.hpp"

namespace sharppy {

// Cleans a freshly parsed profile so the rest of the app never sees sentinel
// values (-9999, NaN, inf):
//  * levels with a missing pressure, height, temperature or dewpoint are dropped
//    (the surface is re-derived from the first remaining level if the original
//    surface level was one of them);
//  * missing winds are interpolated in height between valid neighbours, and
//    copied from the nearest valid level at the ends of the profile (calm if the
//    whole profile has no wind);
//  * u/v are recomputed from direction/speed.
// Returns false if fewer than two usable levels remain. Idempotent.
bool sanitize_profile(Profile& prof);

} // namespace sharppy
