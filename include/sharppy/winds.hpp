#pragma once

#include "sharppy/profile.hpp"

namespace sharppy {

struct WindVector {
    double u = 0.0;
    double v = 0.0;
    double wspd = 0.0;
    double wdir = 0.0;
};

struct BunkersMotion {
    WindVector right_mover;
    WindVector left_mover;
    WindVector mean_wind;
};

struct CorfidiMotion {
    WindVector upwind;
    WindVector downwind;
};

struct InflowLayer {
    double bot_p = MISSING;
    double top_p = MISSING;
    double bot_h_agl = MISSING;
    double top_h_agl = MISSING;
    double depth_m = 0.0;
    double esrh = 0.0;
};

WindVector mean_wind_pres(const Profile& prof, double pbot, double ptop, bool pressure_weighted = true);
WindVector wind_shear_pres(const Profile& prof, double pbot, double ptop);
WindVector wind_shear(const Profile& prof, double bottom_agl, double top_agl);
WindVector mean_wind(const Profile& prof, double bottom_agl, double top_agl);
WindVector sr_wind(const Profile& prof, double bottom_agl, double top_agl, double storm_u, double storm_v);
double storm_relative_helicity(const Profile& prof, double bottom_agl, double top_agl, 
                              double storm_u, double storm_v);
BunkersMotion bunkers_storm_motion(const Profile& prof);
CorfidiMotion corfidi_mcs_motion(const Profile& prof);
InflowLayer effective_inflow_layer(const Profile& prof, double storm_u, double storm_v);
double critical_angle(const Profile& prof, double storm_u, double storm_v);

} // namespace sharppy
