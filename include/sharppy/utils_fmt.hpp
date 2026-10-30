#pragma once
// Ports of sharptab/utils.py INT2STR / FLOAT2STR with the same rounding and string forms.
#include <QString>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <string>
#include "sharppy/constants.hpp"

namespace sharppy {

// INT2STR: str(int(np.round(val, 0))); NaN / masked -> '--'.  np.round is round-half-to-even (2.5 -> 2, 3.5 -> 4).
inline QString int2str(double v) {
    if (std::isnan(v) || v == MISSING) return QStringLiteral("--");
    if (std::isinf(v)) return v > 0 ? QStringLiteral("inf") : QStringLiteral("-inf");
    return QString::number(static_cast<long long>(std::nearbyint(v)));
}

namespace detail {
// Python repr(float): shortest round-trip digits; fixed notation for 1e-4 <= |x| < 1e16, else d.ddde+XX; always has '.' or 'e'.
inline std::string pyRepr(double x) {
    if (x == 0.0) return std::signbit(x) ? "-0.0" : "0.0";
    char buf[64];
    auto r = std::to_chars(buf, buf + sizeof buf, x, std::chars_format::scientific);
    std::string s(buf, r.ptr);
    bool neg = false;
    if (s[0] == '-') { neg = true; s.erase(0, 1); }
    const size_t e = s.find('e');
    std::string mant = s.substr(0, e);
    const int exp10 = std::atoi(s.c_str() + e + 1);
    std::string digits;
    for (char c : mant) if (c != '.') digits += c;
    std::string out;
    if (exp10 >= -4 && exp10 < 16) {
        if (exp10 >= 0) {
            if ((int)digits.size() <= exp10 + 1) { digits.append(exp10 + 1 - digits.size(), '0'); out = digits + ".0"; }
            else out = digits.substr(0, exp10 + 1) + "." + digits.substr(exp10 + 1);
        } else {
            out = "0." + std::string(-exp10 - 1, '0') + digits;
        }
    } else {
        out = digits.substr(0, 1);
        if (digits.size() > 1) out += "." + digits.substr(1);
        char eb[16]; std::snprintf(eb, sizeof eb, "e%c%02d", exp10 < 0 ? '-' : '+', std::abs(exp10));
        out += eb;
    }
    return neg ? "-" + out : out;
}
}  // namespace detail

// FLOAT2STR: str(np.round(val, precision)) -> e.g. 2.0 -> "2.0", 1.5 -> "1.5", 85.0 (precision 0) -> "85.0"; NaN / masked -> '--'.
inline QString float2str(double v, int precision) {
    if (std::isnan(v) || v == MISSING) return QStringLiteral("--");
    if (std::isinf(v)) return v > 0 ? QStringLiteral("inf") : QStringLiteral("-inf");
    double r;
    if (precision >= 0) { const double f = std::pow(10.0, precision); r = std::nearbyint(v * f) / f; }
    else { const double f = std::pow(10.0, -precision); r = std::nearbyint(v / f) * f; }
    return QString::fromStdString(detail::pyRepr(r));
}

}  // namespace sharppy
