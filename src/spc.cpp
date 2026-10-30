#include "sharppy/spc.hpp"
#include "sharppy/sanitize.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <algorithm>

namespace sharppy {

static inline std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

static inline std::vector<std::string> split_fields(const std::string& line) {
    std::vector<std::string> tokens;
    std::stringstream ss(line);
    std::string token;
    bool has_commas = (line.find(',') != std::string::npos);

    if (has_commas) {
        while (std::getline(ss, token, ',')) {
            tokens.push_back(trim(token));
        }
    } else {
        while (ss >> token) {
            tokens.push_back(trim(token));
        }
    }
    return tokens;
}

bool SpcParser::parse_file(const std::string& filepath, Profile& out_profile, SoundingMetadata& out_meta) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "[SpcParser] Error: Could not open file: " << filepath << "\n";
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return parse_string(buffer.str(), out_profile, out_meta);
}

bool SpcParser::parse_string(const std::string& content, Profile& out_profile, SoundingMetadata& out_meta) {
    // BUFKIT / other tabular formats also have digit-led rows with >= 6 columns: never mistake them for SPC text
    if (content.find("SNPARM") != std::string::npos || content.find("STNPRM") != std::string::npos ||
        content.find("STID =") != std::string::npos ||
        content.find("<PRE>") != std::string::npos || content.find("PRES   HGHT   TEMP") != std::string::npos)
        return false;
    std::istringstream stream(content);
    std::string line;
    bool in_raw_block = false;

    struct RawLevel {
        double p, h, t, td, wdir, wspd;
    };
    std::vector<RawLevel> levels;

    while (std::getline(stream, line)) {
        line = trim(line);
        if (line.empty()) continue;

        if (line.find("%TITLE%") != std::string::npos) {
            if (std::getline(stream, line)) {
                out_meta.title = trim(line);
            }
            continue;
        }

        if (line.find("%RAW%") != std::string::npos) {
            in_raw_block = true;
            continue;
        }

        if (line.find("%END%") != std::string::npos) {
            in_raw_block = false;
            break;
        }

        if (in_raw_block || (!line.empty() && std::isdigit(static_cast<unsigned char>(line[0])))) {
            auto fields = split_fields(line);
            if (fields.size() < 6) continue;

            try {
                double p    = std::stod(fields[0]);
                double h    = std::stod(fields[1]);
                double t    = std::stod(fields[2]);
                double td   = std::stod(fields[3]);
                double wdir = std::stod(fields[4]);
                double wspd = std::stod(fields[5]);

                if (p <= 0.0 || p > 1080.0 || h < -500.0) continue;

                if (t <= -999.0)  t = MISSING;
                if (td <= -999.0) td = MISSING;
                // Missing winds stay missing here; sanitize_profile() fills them
                // in properly instead of silently turning them into calm.
                if (wdir < 0.0 || wspd < 0.0) {
                    wdir = MISSING;
                    wspd = MISSING;
                }

                levels.push_back({p, h, t, td, wdir, wspd});
            } catch (const std::exception&) {
                continue;
            }
        }
    }

    if (levels.empty()) {
        std::cerr << "[SpcParser] Error: No valid sounding levels parsed.\n";
        return false;
    }

    std::sort(levels.begin(), levels.end(), [](const RawLevel& a, const RawLevel& b) {
        return a.p > b.p;
    });

    out_profile.pres.clear();
    out_profile.hght.clear();
    out_profile.temp.clear();
    out_profile.dwpt.clear();
    out_profile.wdir.clear();
    out_profile.wspd.clear();
    out_profile.u.clear();
    out_profile.v.clear();

    out_profile.sfc_pres = levels.front().p;
    out_profile.sfc_hght = levels.front().h;

    for (const auto& lvl : levels) {
        out_profile.pres.push_back(lvl.p);
        out_profile.hght.push_back(lvl.h);
        out_profile.temp.push_back(lvl.t);
        out_profile.dwpt.push_back(lvl.td);
        out_profile.wdir.push_back(lvl.wdir);
        out_profile.wspd.push_back(lvl.wspd);

        double u_val, v_val;
        vec2comp(lvl.wdir, lvl.wspd, u_val, v_val);
        out_profile.u.push_back(u_val);
        out_profile.v.push_back(v_val);
    }

    if (!sanitize_profile(out_profile)) {
        std::cerr << "[SpcParser] Error: fewer than two usable levels after removing missing data.\n";
        return false;
    }
    return true;
}

} // namespace sharppy
