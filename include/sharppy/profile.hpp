#pragma once

#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include "sharppy/constants.hpp"
#include "sharppy/interp.hpp"

namespace sharppy {

struct Profile {
    std::vector<double> pres;   // hPa
    double sfc_pres = 1000.0;   // Surface pressure
    std::vector<double> hght;   // m MSL
    double sfc_hght = 0.0;      // Surface elevation
    double latitude = MISSING;  // deg N (MISSING when unknown, like Python's masked latitude)
    std::string station_id;     // ICAO/IATA/WMO id when known (PWV climatology)
    std::vector<double> temp;   // C
    std::vector<double> dwpt;   // C
    std::vector<double> wdir;   // deg
    std::vector<double> wspd;   // kts
    std::vector<double> u;      // kts
    std::vector<double> v;      // kts

    size_t size() const { return pres.size(); }
    bool empty() const { return pres.empty(); }

    double to_agl(double hght_msl) const { return hght_msl - sfc_hght; }
    double to_msl(double hght_agl) const { return hght_agl + sfc_hght; }

    // Index i with pres[i] >= target >= pres[i+1], or -1. Pressure is stored
    // surface-first (descending), so this is a binary search; if the data is not
    // monotonic around the hit it falls back to the original linear scan.
    long find_pres_bracket(double target) const {
        const size_t n = pres.size();
        // snap float round-off (e.g. exp(log(p0)) a hair above p0) onto the profile ends
        if (n > 0) {
            if (target > pres.front() && target - pres.front() < 1e-6) target = pres.front();
            if (target < pres.back() && pres.back() - target < 1e-6) target = pres.back();
        }
        if (n < 2 || !(target <= pres.front()) || !(target >= pres.back())) return -1;
        size_t lo = 0, hi = n - 1;
        while (hi - lo > 1) {
            size_t mid = lo + (hi - lo) / 2;
            if (pres[mid] >= target) lo = mid; else hi = mid;
        }
        if (pres[lo] >= target && pres[lo + 1] <= target) return static_cast<long>(lo);
        for (size_t i = 0; i + 1 < n; ++i) {
            if (pres[i] >= target && pres[i + 1] <= target) return static_cast<long>(i);
        }
        return -1;
    }

    double hght_at_pres(double target_p) const {
        if (pres.empty()) return MISSING;
        long i = find_pres_bracket(target_p);
        if (i < 0) return MISSING;
        return log_pres_interp(target_p, pres[i], pres[i + 1], hght[i], hght[i + 1]);
    }

    double pres_at_hght(double target_h) const {
        if (hght.empty() || target_h < hght.front() || target_h > hght.back()) return MISSING;
        for (size_t i = 0; i < hght.size() - 1; ++i) {
            if (hght[i] <= target_h && hght[i + 1] >= target_h) {
                double log_p = linear_interp(target_h, hght[i], hght[i + 1], 
                                             std::log(pres[i]), std::log(pres[i + 1]));
                return std::exp(log_p);
            }
        }
        return MISSING;
    }

    double temp_at_pres(double target_p) const {
        if (pres.empty()) return MISSING;
        long i = find_pres_bracket(target_p);
        if (i < 0) return MISSING;
        return log_pres_interp(target_p, pres[i], pres[i + 1], temp[i], temp[i + 1]);
    }

    double dwpt_at_pres(double target_p) const {
        if (pres.empty()) return MISSING;
        long i = find_pres_bracket(target_p);
        if (i < 0) return MISSING;
        return log_pres_interp(target_p, pres[i], pres[i + 1], dwpt[i], dwpt[i + 1]);
    }

    // u/v at a pressure (log-p interpolation, like interp.components)
    void wind_at_pres(double target_p, double& out_u, double& out_v) const {
        out_u = out_v = MISSING;
        if (pres.empty()) return;
        long i = find_pres_bracket(target_p);
        if (i < 0) return;
        out_u = log_pres_interp(target_p, pres[i], pres[i + 1], u[i], u[i + 1]);
        out_v = log_pres_interp(target_p, pres[i], pres[i + 1], v[i], v[i + 1]);
    }

    void wind_at_agl(double h_agl, double& out_u, double& out_v) const {
        double target_msl = to_msl(h_agl);
        if (hght.empty()) { out_u = out_v = MISSING; return; }
        if (target_msl <= hght.front()) {
            out_u = u.front();
            out_v = v.front();
            return;
        }
        if (target_msl >= hght.back()) {
            out_u = u.back();
            out_v = v.back();
            return;
        }
        for (size_t i = 0; i < hght.size() - 1; ++i) {
            if (hght[i] <= target_msl && hght[i + 1] >= target_msl) {
                out_u = linear_interp(target_msl, hght[i], hght[i + 1], u[i], u[i + 1]);
                out_v = linear_interp(target_msl, hght[i], hght[i + 1], v[i], v[i + 1]);
                return;
            }
        }
        out_u = MISSING;
        out_v = MISSING;
    }
};

} // namespace sharppy
