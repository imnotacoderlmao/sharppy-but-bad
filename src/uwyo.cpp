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

namespace {
std::string trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\r\n"), e = s.find_last_not_of(" \t\r\n");
    return b == std::string::npos ? std::string() : s.substr(b, e - b + 1);
}
std::string stripTags(const std::string& s) {
    std::string o; bool in = false;
    for (char c : s) { if (c == '<') in = true; else if (c == '>') in = false; else if (!in) o += c; }
    return o;
}
std::vector<std::string> splitWs(const std::string& s) {
    std::vector<std::string> t; std::istringstream is(s); std::string w;
    while (is >> w) t.push_back(w);
    return t;
}
std::string valueAfterColon(const std::string& line) {
    size_t i = line.find(':');
    return i == std::string::npos ? std::string() : trim(line.substr(i + 1));
}
}

// Handles both the legacy cgi-bin text list (PRES HGHT TEMP DWPT RELH MIXR DRCT SKNT ...) and the current
// wsgi/sounding output (same layout, but wind speed is SPED in m/s; "Aviation Units" gives SKNT/knots and
// heights in feet).  The table is 7-character fixed width with blanks for missing values, so columns are
// located from the header line instead of assuming a fixed order.
bool UwyoParser::parse_string(const std::string& content, Profile& out_profile, SoundingMetadata& out_meta) {
    std::istringstream stream(content);
    std::string raw;

    out_profile.pres.clear(); out_profile.hght.clear(); out_profile.temp.clear(); out_profile.dwpt.clear();
    out_profile.wdir.clear(); out_profile.wspd.clear(); out_profile.u.clear(); out_profile.v.clear();

    constexpr double MS_TO_KT = 1.0 / 0.514444, FT_TO_M = 0.3048;
    int cP = -1, cH = -1, cT = -1, cD = -1, cDir = -1, cSpd = -1;
    bool spdMs = false, hghtFeet = false;
    bool in_table = false, have_header = false, pending_units = false;
    std::string stnNum, stnId, obsTime, heading;

    while (std::getline(stream, raw)) {
        const std::string line = stripTags(raw);
        const std::string t = trim(line);

        if (raw.find("<h2") != std::string::npos || raw.find("<H2") != std::string::npos) { if (!t.empty()) heading = t; }

        // header line: PRES HGHT TEMP ...
        if (!in_table && t.rfind("PRES", 0) == 0 && t.find("HGHT") != std::string::npos) {
            const auto names = splitWs(t);
            cP = cH = cT = cD = cDir = cSpd = -1; spdMs = false;
            for (size_t i = 0; i < names.size(); ++i) {
                const std::string& n = names[i];
                if (n == "PRES") cP = (int)i; else if (n == "HGHT") cH = (int)i; else if (n == "TEMP") cT = (int)i;
                else if (n == "DWPT") cD = (int)i; else if (n == "DRCT") cDir = (int)i;
                else if (n == "SKNT") cSpd = (int)i;
                else if (n == "SPED") { cSpd = (int)i; spdMs = true; }
                else if (n == "SMPS") { cSpd = (int)i; spdMs = true; }
            }
            if (cP < 0 || cH < 0) continue;
            in_table = have_header = pending_units = true;
            continue;
        }

        if (in_table) {
            if (t.empty() || raw.find("</pre>") != std::string::npos || raw.find("</PRE>") != std::string::npos ||
                (t.find("Station") != std::string::npos)) {
                in_table = false;
                // fall through so a station line directly after the table is still read below
            } else {
                if (t.find_first_not_of("- ") == std::string::npos) continue;        // dashed separator
                if (pending_units && (t.find("hPa") != std::string::npos || t.find("mb") != std::string::npos)) {
                    pending_units = false;
                    if (t.find("feet") != std::string::npos || t.find(" ft") != std::string::npos) hghtFeet = true;
                    if (spdMs && t.find("knot") != std::string::npos) spdMs = false;
                    continue;
                }
                auto field = [&](int col, double& v) {
                    if (col < 0) return false;
                    const size_t start = (size_t)col * 7;
                    if (start >= line.size()) return false;
                    std::string f = line.substr(start, 7);
                    size_t b0 = f.find_first_not_of(' ');
                    if (b0 == std::string::npos) return false;
                    char* end = nullptr;
                    v = std::strtod(f.c_str() + b0, &end);
                    return end != f.c_str() + b0 && std::string(end).find_first_not_of(" \r\n\t") == std::string::npos;
                };
                double p = 0, h = 0, tt = MISSING, td = MISSING, drct = MISSING, spd = MISSING;
                if (!field(cP, p) || p <= 0.0) continue;
                if (!field(cH, h)) continue;                       // a level without height is unusable
                if (!field(cT, tt)) tt = MISSING;
                if (!field(cD, td)) td = MISSING;
                if (!field(cDir, drct)) drct = MISSING;
                if (!field(cSpd, spd)) spd = MISSING;
                if (hghtFeet) h *= FT_TO_M;
                if (spd != MISSING && spdMs) spd *= MS_TO_KT;
                if (drct == MISSING || spd == MISSING || drct < 0 || spd < 0) { drct = MISSING; spd = MISSING; }
                out_profile.pres.push_back(p);
                out_profile.hght.push_back(h);
                out_profile.temp.push_back(tt);
                out_profile.dwpt.push_back(td);
                out_profile.wdir.push_back(drct);
                out_profile.wspd.push_back(spd);
                double u_val = MISSING, v_val = MISSING;
                if (drct != MISSING) vec2comp(drct, spd, u_val, v_val);
                out_profile.u.push_back(u_val);
                out_profile.v.push_back(v_val);
                continue;
            }
        }

        if (t.find("Station identifier:") != std::string::npos) stnId = valueAfterColon(t);
        else if (t.find("Station number:") != std::string::npos) stnNum = valueAfterColon(t);
        else if (t.find("Observation time:") != std::string::npos) obsTime = valueAfterColon(t);
        else if (t.find("Station latitude:") != std::string::npos) out_meta.latitude = std::atof(valueAfterColon(t).c_str());
        else if (t.find("Station longitude:") != std::string::npos) out_meta.longitude = std::atof(valueAfterColon(t).c_str());
        else if (t.find("Station elevation:") != std::string::npos) out_meta.elevation_m = std::atof(valueAfterColon(t).c_str());
    }
    (void)have_header;

    if (!stnId.empty()) out_meta.station_id = stnId; else if (!stnNum.empty()) out_meta.station_id = stnNum;
    if (!obsTime.empty()) out_meta.valid_time = obsTime;       // yymmdd/hhmm
    if (out_meta.title.empty()) {
        if (!heading.empty()) out_meta.title = heading;
        else if (!out_meta.station_id.empty()) out_meta.title = out_meta.station_id + " " + obsTime + " UWYO";
    }

    if (!out_profile.empty()) {
        out_profile.sfc_pres = out_profile.pres.front();
        out_profile.sfc_hght = out_profile.hght.front();
        return sanitize_profile(out_profile);
    }
    return false;
}

} // namespace sharppy
