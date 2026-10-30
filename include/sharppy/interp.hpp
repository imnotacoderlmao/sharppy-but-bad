#pragma once

#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include <vector>
#include <cmath>
#include <algorithm>
#include "sharppy/constants.hpp"

namespace sharppy {

inline double linear_interp(double x, double x1, double x2, double y1, double y2) {
    if (std::abs(x2 - x1) < 1e-6) return y1;
    return y1 + (x - x1) * (y2 - y1) / (x2 - x1);
}

inline double log_pres_interp(double p, double p1, double p2, double y1, double y2) {
    if (p <= 0 || p1 <= 0 || p2 <= 0) return MISSING;
    if (std::abs(p2 - p1) < 1e-6) return y1;
    double log_p = std::log(p);
    double log_p1 = std::log(p1);
    double log_p2 = std::log(p2);
    return y1 + (log_p - log_p1) * (y2 - y1) / (log_p2 - log_p1);
}

inline void comp2vec(double u, double v, double& wdir, double& wspd) {
    wspd = std::hypot(u, v);
    if (wspd < 1e-4) {
        wdir = 0.0;
        return;
    }
    double dir = std::atan2(-u, -v) * (180.0 / M_PI);
    if (dir < 0.0) dir += 360.0;
    wdir = dir;
}

inline void vec2comp(double wdir, double wspd, double& u, double& v) {
    double rad = wdir * (M_PI / 180.0);
    u = -wspd * std::sin(rad);
    v = -wspd * std::cos(rad);
}

} // namespace sharppy
