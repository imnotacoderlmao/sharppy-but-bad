#include "sharppy/bufkit.hpp"
#include "sharppy/sanitize.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>

namespace sharppy {

namespace {

std::vector<std::string> tokens(const std::string& s) {
    std::vector<std::string> t;
    std::istringstream ss(s);
    std::string w;
    while (ss >> w) t.push_back(w);
    return t;
}

bool toNum(const std::string& s, double& v) {
    char* end = nullptr;
    v = std::strtod(s.c_str(), &end);
    return end != s.c_str() && *end == '\0';
}

bool allNumeric(const std::vector<std::string>& t) {
    double v;
    if (t.empty()) return false;
    for (const auto& w : t) if (!toNum(w, v)) return false;
    return true;
}

bool allNames(const std::vector<std::string>& t) {      // column names: upper-case letters/digits, never numbers
    double v;
    if (t.empty()) return false;
    for (const auto& w : t) {
        if (toNum(w, v)) return false;
        for (char c : w) if (!(std::isupper(static_cast<unsigned char>(c)) || std::isdigit(static_cast<unsigned char>(c)))) return false;
    }
    return true;
}

// "KEY = value" pairs anywhere on a line ("STID = KOKC STNM = 72353 TIME = 261029/0000")
std::map<std::string, std::string> keyValues(const std::string& line) {
    std::map<std::string, std::string> kv;
    const auto t = tokens(line);
    for (size_t i = 0; i + 2 < t.size() + 0; ++i)
        if (t[i + 1] == "=" ) kv[t[i]] = t[i + 2];
    // "KEY=value" glued form
    for (const auto& w : t) {
        const auto eq = w.find('=');
        if (eq != std::string::npos && eq > 0 && eq + 1 < w.size()) kv[w.substr(0, eq)] = w.substr(eq + 1);
    }
    return kv;
}

constexpr double MISSING_BUF = -9998.0;   // BUFKIT writes -9999.00 for missing

}  // namespace

bool BufkitParser::parse_file(const std::string& filepath, std::vector<BufkitRecord>& out_records) {
    std::ifstream file(filepath);
    if (!file.is_open()) return false;
    std::stringstream buffer;
    buffer << file.rdbuf();
    return parse_string(buffer.str(), out_records);
}

bool BufkitParser::parse_string(const std::string& content, std::vector<BufkitRecord>& out_records) {
    out_records.clear();
    // Cheap format check: BUFKIT always declares its sounding parameters or starts a block with STID
    if (content.find("SNPARM") == std::string::npos && content.find("STID") == std::string::npos) return false;

    std::istringstream stream(content);
    std::vector<std::string> lines;
    for (std::string l; std::getline(stream, l);) {
        while (!l.empty() && (l.back() == '\r' || l.back() == '\n')) l.pop_back();
        lines.push_back(l);
    }

    BufkitRecord cur;
    bool haveBlock = false;
    auto flush = [&]() {
        if (haveBlock && !cur.profile.empty()) out_records.push_back(cur);
        cur = BufkitRecord();
        haveBlock = false;
    };

    for (size_t i = 0; i < lines.size(); ++i) {
        const std::string& line = lines[i];
        const auto tk = tokens(line);
        if (tk.empty()) continue;

        // ---- block header keys ----
        if (line.find('=') != std::string::npos) {
            const auto kv = keyValues(line);
            auto has = [&](const char* k) { return kv.find(k) != kv.end(); };
            if (has("STID") || (has("TIME") && has("STNM"))) {
                // a new STID line starts a new block; a TIME on its own line belongs to the current one
                if (has("STID")) flush();
                haveBlock = true;
            }
            if (has("STID")) cur.station_id = kv.at("STID");
            if (has("STNM")) cur.wmo_id = kv.at("STNM");
            if (has("TIME")) { cur.valid_time = kv.at("TIME"); haveBlock = true; }
            double v;
            if (has("SLAT") && toNum(kv.at("SLAT"), v)) { cur.lat = v; cur.profile.latitude = v; }
            if (has("SLON") && toNum(kv.at("SLON"), v)) cur.lon = v;
            if (has("SELV") && toNum(kv.at("SELV"), v)) cur.elev_m = v;
            if (has("STIM") && toNum(kv.at("STIM"), v)) cur.stim = static_cast<int>(std::lround(v));
            if (has("SLAT") || has("STIM")) haveBlock = true;
            continue;
        }

        // ---- profile header: "PRES TMPC TMWC DWPC THTE DRCT SKNT OMEG" (+ optional second names line "CFRL HGHT") ----
        if (tk[0] == "PRES" && allNames(tk)) {
            std::vector<std::string> names = tk;
            size_t j = i + 1;
            std::vector<size_t> rowLen{names.size()};
            if (j < lines.size()) {
                const auto t2 = tokens(lines[j]);
                if (allNames(t2) && t2[0] != "PRES" && t2[0] != "STN") {
                    names.insert(names.end(), t2.begin(), t2.end());
                    rowLen.push_back(t2.size());
                    ++j;
                }
            }
            auto col = [&](const char* n) -> int {
                for (size_t c = 0; c < names.size(); ++c) if (names[c] == n) return static_cast<int>(c);
                return -1;
            };
            const int cP = col("PRES"), cT = col("TMPC"), cD = col("DWPC"), cDir = col("DRCT"), cSpd = col("SKNT"), cH = col("HGHT");
            if (cP < 0 || cT < 0 || cD < 0 || cDir < 0 || cSpd < 0 || cH < 0) { i = j - 1; continue; }

            // gather numeric tokens until a blank / non-numeric line, then chunk them into levels
            std::vector<double> nums;
            for (; j < lines.size(); ++j) {
                const auto t = tokens(lines[j]);
                if (t.empty()) { if (nums.empty()) continue; else break; }
                if (!allNumeric(t)) break;
                for (const auto& w : t) { double v; toNum(w, v); nums.push_back(v); }
            }
            const size_t ncol = names.size();
            Profile& pr = cur.profile;
            pr.pres.clear(); pr.hght.clear(); pr.temp.clear(); pr.dwpt.clear();
            pr.wdir.clear(); pr.wspd.clear(); pr.u.clear(); pr.v.clear();
            for (size_t k = 0; k + ncol <= nums.size(); k += ncol) {
                auto at = [&](int c) { double x = nums[k + static_cast<size_t>(c)]; return x <= MISSING_BUF ? MISSING : x; };
                const double p = at(cP), h = at(cH);
                if (p == MISSING || p <= 0.0 || h == MISSING) continue;
                const double t = at(cT), td = at(cD);
                double wd = at(cDir), ws = at(cSpd);
                if (wd == MISSING || ws == MISSING || wd < 0 || ws < 0) { wd = MISSING; ws = MISSING; }
                pr.pres.push_back(p); pr.hght.push_back(h);
                pr.temp.push_back(t); pr.dwpt.push_back(td);
                pr.wdir.push_back(wd); pr.wspd.push_back(ws);
                double u = MISSING, v = MISSING;
                if (wd != MISSING) vec2comp(wd, ws, u, v);
                pr.u.push_back(u); pr.v.push_back(v);
            }
            haveBlock = true;
            i = j - 1;
            continue;
        }
        // everything else (SHOW/LIFT lines, the STN surface table, ...) is not needed for the profile
    }
    flush();

    for (auto it = out_records.begin(); it != out_records.end();) {
        Profile& p = it->profile;
        if (p.pres.empty()) { it = out_records.erase(it); continue; }
        // the first BUFKIT level is the surface (pressure, station elevation)
        p.sfc_pres = p.pres.front();
        p.sfc_hght = p.hght.front();
        if (it->elev_m == 0.0 && p.sfc_hght != 0.0) it->elev_m = p.sfc_hght;
        if (sanitize_profile(p)) ++it; else it = out_records.erase(it);
    }
    return !out_records.empty();
}

}  // namespace sharppy
