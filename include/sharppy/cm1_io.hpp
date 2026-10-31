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

// The exported levels, as written (winds in m/s; nuk/nvk are the same winds in knots).
struct Cm1Sounding { std::vector<double> nh, np, nt, th, mixr, nu, nv, nuk, nvk; };
// sndmod up to the point of writing: Profile + options -> levels. write_cm1 uses this, so a preview matches the file.
bool build_cm1(const Profile& prof, const Cm1ExportOptions& opt, Cm1Sounding& out, std::string* err = nullptr);

// CM1's own startup check (base.F, "Ri check"): per level pair, N^2 (must be > 0) and Ri (must be > 0.25).
struct Cm1StabRow { int line; double z, n2, ri; bool n2bad, ribad; };   // line = file line of the upper level
struct Cm1Stability { std::vector<Cm1StabRow> rows; int nN2bad = 0, nRibad = 0; };
Cm1Stability cm1_stability(const Cm1Sounding& snd);

// cm12snd: CM1 input_sounding text -> Profile. Returns false (with err) when the file isn't CM1 format.
bool read_cm1(const std::string& path, Profile& out, std::string* err = nullptr);
// sndmod: Profile -> CM1 input_sounding file. ".cm1" is appended when missing (as sndmod does).
bool write_cm1(const Profile& prof, const std::string& path, const Cm1ExportOptions& opt, std::string* err = nullptr);

} // namespace sharppy
