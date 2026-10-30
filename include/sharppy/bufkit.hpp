#pragma once

#include <string>
#include <vector>
#include "sharppy/profile.hpp"
#include "sharppy/spc.hpp"

namespace sharppy {

struct BufkitRecord {
    std::string station_id;
    std::string wmo_id;
    std::string valid_time;
    double lat = 0.0;
    double lon = 0.0;
    double elev_m = 0.0;
    int    stim = -1;            // STIM: forecast hour of this record (-1 when absent)
    Profile profile;
};

class BufkitParser {
public:
    static bool parse_file(const std::string& filepath, std::vector<BufkitRecord>& out_records);
    static bool parse_string(const std::string& content, std::vector<BufkitRecord>& out_records);
};

} // namespace sharppy