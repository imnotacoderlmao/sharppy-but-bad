#pragma once

#include <string>
#include "sharppy/profile.hpp"

namespace sharppy {

struct SoundingMetadata {
    std::string title;
    std::string station_id;
    std::string valid_time;
    double latitude = MISSING;
    double longitude = MISSING;
    double elevation_m = 0.0;
};

class SpcParser {
public:
    static bool parse_file(const std::string& filepath, Profile& out_profile, SoundingMetadata& out_meta);
    static bool parse_string(const std::string& content, Profile& out_profile, SoundingMetadata& out_meta);
};

} // namespace sharppy
