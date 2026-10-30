#pragma once

namespace sharppy {

constexpr double ROCP = 0.28571426;           // R over Cp (sharptab constants.py)
constexpr double CP = 1004.0;                 // Specific heat of dry air (J / kg / K)
constexpr double RD = 287.058;                // Gas constant for dry air (J / kg / K)
constexpr double RV = 461.5;                  // Gas constant for water vapor (J / kg / K)
constexpr double EPS = RD / RV;               // 0.62197
constexpr double G = 9.80665;                 // Gravity (m / s^2)
constexpr double LV = 2.501e6;                // Latent heat of vaporization (J / kg)
constexpr double ZEROCNK = 273.15;            // 0 C in Kelvin
constexpr double MISSING = -9999.0;

inline double c2k(double c) { return c + ZEROCNK; }
inline double k2c(double k) { return k - ZEROCNK; }

} // namespace sharppy
