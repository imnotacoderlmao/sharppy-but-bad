#pragma once
// CM1 input_sounding import (port of cm12snd) and export (port of sndmod).
#include <string>
#include "sharppy/profile.hpp"

namespace sharppy {

struct Cm1ExportOptions {
    double dz = 100.;                 // --dz
    double desat = 0.;                // --desat  (RH >= 97% reduced by this much)
    bool   desatLayer = false;        // --desat-layer bottom top pct
    double dlBottom = 0., dlTop = 0., dlPct = 0.;
    double fillTop = 0.;              // --fill-top (m)
    double adiabaticHeight = 0.;      // --adiabatic height stability  (0 = off)
    double adiabaticStability = 0.04; // K/km
    bool   alsoSharppy = false;       // --output-sharppy
};

// cm12snd: CM1 input_sounding text -> Profile. Returns false (with err) when the file isn't CM1 format.
bool read_cm1(const std::string& path, Profile& out, std::string* err = nullptr);
// sndmod: Profile -> CM1 input_sounding file. ".cm1" is appended when missing (as sndmod does).
bool write_cm1(const Profile& prof, const std::string& path, const Cm1ExportOptions& opt, std::string* err = nullptr);

} // namespace sharppy
