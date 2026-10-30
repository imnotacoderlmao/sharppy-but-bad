#pragma once

#include <string>
#include <vector>
#include "sharppy/profile.hpp"
#include "sharppy/thermo.hpp"
#include "sharppy/winds.hpp"

namespace sharppy {

struct SarsMatch {
    std::string date_str;
    std::string location;
    std::string storm_type;
    double hail_size_in = 0.0;
    int f_scale = -1;
    double match_score = 0.0;
};

struct SarsResult {
    std::string supercell_match_summary;
    std::string hail_match_summary;
    std::vector<SarsMatch> top_matches;
    int loose_matches_tor = 0;
    int loose_matches_hail = 0;
    int pct_tor = 0;
    int pct_sig_tor = 0;
    int pct_sig_hail = 0;
};

class SarsEngine {
public:
    static SarsResult query(const Profile& prof,
                            const ParcelResult& mu,
                            const ParcelResult& ml,
                            double shear06_kts,
                            double srh01,
                            double srh03);
};

} // namespace sharppy
