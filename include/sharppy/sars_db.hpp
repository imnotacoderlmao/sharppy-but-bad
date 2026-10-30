#pragma once

#include <string>
#include <vector>
#include "sharppy/profile.hpp"
#include "sharppy/thermo.hpp"
#include "sharppy/winds.hpp"

namespace sharppy {

// Return tuple of sharppy.databases.sars.hail()/supercell():
// (quality match names, types-or-sizes, #loose matches, #weak/sig matches, probability)
struct SarsMatches {
    std::vector<std::string> names;    // "yymmddhh.LOC"
    std::vector<std::string> types;    // supercell: SIGTOR / WEAKTOR / NONTOR
    std::vector<double> sizes;         // hail: inches
    double nloose = 0;
    double nmatch = 0;
    double prob = 0;
};

struct SarsAll {
    SarsMatches rightHail, leftHail, rightSupercell, leftSupercell;
};

// sars.supercell(): units are kts / m^2 s^-2
SarsMatches sars_supercell(const std::string& dbfile, double mlcape, double mllcl, double h5temp, double lr,
                           double shr, double srh, double shr3k, double shr9k, double srh3);
// sars.hail(): shears in m/s, srh = 0-3 km SRH
SarsMatches sars_hail(const std::string& dbfile, double mumr, double mucape, double h5_temp, double lr,
                      double shr6, double shr9, double shr3, double srh);

// profile.get_sars(): empty matches when a database file is missing (Python's try/except fallback)
SarsAll compute_sars(const Profile& prof, const ParcelResult& mu, const ParcelResult& ml,
                     const BunkersMotion& srwind, const std::string& dbdir);

// directory searched for sars_hail.txt / sars_supercell.txt (<exe dir>/data/sars by default)
std::string default_sars_dir();

} // namespace sharppy
