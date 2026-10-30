#pragma once

#include <string>
#include "sharppy/profile.hpp"
#include "sharppy/spc.hpp"

namespace sharppy {

class UwyoParser {
public:
    static bool parse_file(const std::string& filepath, Profile& out_profile, SoundingMetadata& out_meta);
    static bool parse_string(const std::string& content, Profile& out_profile, SoundingMetadata& out_meta);
};

} // namespace sharppy