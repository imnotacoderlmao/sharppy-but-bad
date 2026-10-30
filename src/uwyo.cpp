#include "sharppy/uwyo.hpp"
#include "sharppy/sanitize.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <cstdlib>

namespace sharppy {

bool UwyoParser::parse_file(const std::string& filepath, Profile& out_profile, SoundingMetadata& out_meta) {
    std::ifstream file(filepath);
    if (!file.is_open()) return false;
    std::stringstream buffer;
    buffer << file.rdbuf();
    return parse_string(buffer.str(), out_profile, out_meta);
}

bool UwyoParser::parse_string(const std::string& content, Profile& out_profile, SoundingMetadata& out_meta) {
    std::istringstream stream(content);
    std::string line;

    bool in_table = false;
    out_profile.pres.clear();
    out_profile.hght.clear();
    out_profile.temp.clear();
    out_profile.dwpt.clear();
    out_profile.wdir.clear();
    out_profile.wspd.clear();
    out_profile.u.clear();
    out_profile.v.clear();

    while (std::getline(stream, line)) {
        if (line.find("<PRE>") != std::string::npos || line.find("PRES   HGHT   TEMP") != std::string::npos) {
            in_table = true;
            continue;
        }

        if (line.find("</PRE>") != std::string::npos) {
            in_table = false;           // keep reading: the station info (latitude/longitude) follows in a second <PRE>
            continue;
        }

        if (line.find("Station latitude:") != std::string::npos) {
            size_t idx = line.find(':');
            if (idx != std::string::npos) {
                out_meta.latitude = std::atof(line.substr(idx + 1).c_str());
            }
            continue;
        }

        if (line.find("Station longitude:") != std::string::npos) {
            size_t idx = line.find(':');
            if (idx != std::string::npos) {
                out_meta.longitude = std::atof(line.substr(idx + 1).c_str());
            }
            continue;
        }

        if (in_table) {
            // University of Wyoming text tables are fixed width (7 characters per column) and leave missing
            // values blank, so whitespace splitting would shift every later column.
            if (line.length() < 7) continue;
            auto field = [&](size_t col, double& v) {
                const size_t start = col * 7;
                if (start >= line.size()) return false;
                std::string s = line.substr(start, 7);
                size_t b0 = s.find_first_not_of(' ');
                if (b0 == std::string::npos) return false;
                char* end = nullptr;
                v = std::strtod(s.c_str() + b0, &end);
                return end != s.c_str() + b0 && std::string(end).find_first_not_of(" \r\n\t") == std::string::npos;
            };
            double p = 0, h = 0, t = MISSING, td = MISSING, relh = 0, mixr = 0, drct = MISSING, sknt = MISSING;
            if (!field(0, p) || p <= 0.0) continue;            // header / separator / blank lines
            if (!field(1, h)) continue;                        // a level without height is unusable
            if (!field(2, t)) t = MISSING;
            if (!field(3, td)) td = MISSING;
            field(4, relh); field(5, mixr);
            if (!field(6, drct)) drct = MISSING;
            if (!field(7, sknt)) sknt = MISSING;
            if (drct == MISSING || sknt == MISSING || drct < 0 || sknt < 0) { drct = MISSING; sknt = MISSING; }
            out_profile.pres.push_back(p);
            out_profile.hght.push_back(h);
            out_profile.temp.push_back(t);
            out_profile.dwpt.push_back(td);
            out_profile.wdir.push_back(drct);
            out_profile.wspd.push_back(sknt);
            double u_val = MISSING, v_val = MISSING;
            if (drct != MISSING) vec2comp(drct, sknt, u_val, v_val);
            out_profile.u.push_back(u_val);
            out_profile.v.push_back(v_val);
        }
    }

    if (!out_profile.empty()) {
        out_profile.sfc_pres = out_profile.pres.front();
        out_profile.sfc_hght = out_profile.hght.front();
        return sanitize_profile(out_profile);
    }
    return false;
}

} // namespace sharppy