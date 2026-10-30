#include "sharppy/sanitize.hpp"
#include "sharppy/winds.hpp"

#include <cmath>
#include <vector>

namespace sharppy {

namespace {
inline bool bad(double v) { return !std::isfinite(v) || v <= -999.0; }
}

bool sanitize_profile(Profile& p) {
    const size_t n = p.pres.size();
    if (n == 0) return false;
    if (p.hght.size() != n || p.temp.size() != n || p.dwpt.size() != n ||
        p.wdir.size() != n || p.wspd.size() != n) {
        return false; // malformed: the parsers always fill these in parallel
    }

    const bool surface_dropped = bad(p.pres[0]) || bad(p.hght[0]) || bad(p.temp[0]) || bad(p.dwpt[0]);

    Profile out;
    out.sfc_pres = p.sfc_pres;
    out.sfc_hght = p.sfc_hght;
    std::vector<char> wind_ok;
    for (size_t i = 0; i < n; ++i) {
        if (bad(p.pres[i]) || p.pres[i] <= 0.0 || bad(p.hght[i]) || bad(p.temp[i]) || bad(p.dwpt[i])) continue;
        out.pres.push_back(p.pres[i]);
        out.hght.push_back(p.hght[i]);
        out.temp.push_back(p.temp[i]);
        out.dwpt.push_back(p.dwpt[i]);
        out.wdir.push_back(p.wdir[i]);
        out.wspd.push_back(p.wspd[i]);
        wind_ok.push_back(!bad(p.wdir[i]) && !bad(p.wspd[i]) && p.wspd[i] >= 0.0);
    }

    const size_t m = out.pres.size();
    if (m < 2) return false;

    // Winds: work in u/v so direction wraparound is not an issue.
    std::vector<double> u(m, 0.0), v(m, 0.0);
    long first_ok = -1, last_ok = -1;
    for (size_t i = 0; i < m; ++i) {
        if (!wind_ok[i]) continue;
        vec2comp(out.wdir[i], out.wspd[i], u[i], v[i]);
        if (first_ok < 0) first_ok = static_cast<long>(i);
        last_ok = static_cast<long>(i);
    }

    if (first_ok < 0) {
        for (size_t i = 0; i < m; ++i) { out.wdir[i] = 0.0; out.wspd[i] = 0.0; }
    } else {
        for (size_t i = 0; i < m; ++i) {
            if (wind_ok[i]) continue;
            if (static_cast<long>(i) < first_ok) {
                u[i] = u[first_ok]; v[i] = v[first_ok];
            } else if (static_cast<long>(i) > last_ok) {
                u[i] = u[last_ok]; v[i] = v[last_ok];
            } else {
                size_t lo = i, hi = i;
                while (!wind_ok[lo]) --lo;          // exists: first_ok < i
                while (!wind_ok[hi]) ++hi;          // exists: last_ok > i
                double dz = out.hght[hi] - out.hght[lo];
                double f = (dz > 0.0) ? (out.hght[i] - out.hght[lo]) / dz : 0.0;
                u[i] = u[lo] + f * (u[hi] - u[lo]);
                v[i] = v[lo] + f * (v[hi] - v[lo]);
            }
            comp2vec(u[i], v[i], out.wdir[i], out.wspd[i]);
        }
    }

    out.u = u;
    out.v = v;
    if (first_ok < 0) { out.u.assign(m, 0.0); out.v.assign(m, 0.0); }

    if (surface_dropped || bad(out.sfc_pres) || bad(out.sfc_hght)) {
        out.sfc_pres = out.pres.front();
        out.sfc_hght = out.hght.front();
    }

    p = std::move(out);
    return true;
}

} // namespace sharppy
