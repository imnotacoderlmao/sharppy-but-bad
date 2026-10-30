#pragma once

#include "sharppy/profile.hpp"
#include "sharppy/thermo.hpp"
#include "sharppy/winds.hpp"

namespace sharppy {

struct SevereParams {
    double stp_fixed = 0.0;
    double stp_cin = 0.0;
    double scp = 0.0;
    double ship = 0.0;
    double ehi_01 = 0.0;
    double ehi_03 = 0.0;
    double brn = 0.0;
    double brn_shear = 0.0;
    double hght_frz = MISSING;
    double hght_m10c = MISSING;
    double hght_m20c = MISSING;
    double critical_angle_deg = 0.0;
    double sfc_8km_shear_kts = 0.0;
    double srw_4_6km_kts = 0.0;
    double ebwd_kts = MISSING;     // effective bulk wind difference (kts); MISSING without an effective layer
    bool is_left_mover = false;
};

double temp_level_agl(const Profile& prof, double target_temp_c);

SevereParams compute_severe_params(
    const Profile& prof,
    const ParcelResult& sbpcl,
    const ParcelResult& mlpcl,
    const ParcelResult& mupcl,
    const BunkersMotion& bunkers,
    bool use_left_mover = false,
    const InflowLayer* inflow_for_motion = nullptr   // effective inflow for the active storm motion, if already known
);

double compute_ship(const Profile& prof, const ParcelResult& mupcl, double shear06_ms);
double compute_stp_cin(double mlcape, double esrh, double ebwd_ms, double mllcl_agl, double mlcin);
double compute_stp_fixed(double sbcape, double sblcl_agl, double srh01, double shear06_ms);
double compute_scp(double mucape, double esrh, double ebwd_ms);

} // namespace sharppy
