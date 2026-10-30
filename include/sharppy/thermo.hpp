#pragma once

#include "sharppy/profile.hpp"
#include <vector>

namespace sharppy {

struct ParcelTrajectoryPoint {
    double pres;
    double parcel_t;
    double env_t;
};

struct ParcelResult {
    double start_p = MISSING;
    double start_t = MISSING;
    double start_dp = MISSING;

    double lcl_p = MISSING;
    double lcl_t = MISSING;
    double lcl_h_agl = MISSING;
    double lfc_p = MISSING;
    double lfc_h_agl = MISSING;
    double el_p = MISSING;
    double el_h_agl = MISSING;

    double cape = 0.0;
    double cin = 0.0;
    double cape_3km = 0.0;
    double cape_6km = 0.0;
    double ncape = 0.0;
    double li = MISSING;
    double cap = 0.0;

    std::vector<ParcelTrajectoryPoint> trajectory;
};

struct SoundingMetrics {
    double mean_w = 0.0;      // Lowest 100mb mean mixing ratio (g/kg)
    double low_rh = 0.0;      // Lowest 1km mean RH (%)
    double mid_rh = 0.0;      // 700-500mb mean RH (%)
    double tt = 0.0;          // Total Totals (T850 + Td850 - 2*T500)
    double k_idx = 0.0;       // K-Index
    double pwat = 0.0;        // Precipitable Water (inches)
    double dcape_val = 0.0;   // Downdraft CAPE (J/kg)
    double down_t_f = 0.0;    // Downdraft temperature (F)
    double conv_t_f = 0.0;    // Convective Temperature (F)
    double max_t_f = 0.0;     // Max Temperature (F)
    double tei = 0.0;         // Theta-E Index (ThE_sfc - ThE_min)
    double esp = 0.0;         // Enhanced Stretching Potential
    double mmp = 0.0;         // MCS Maintenance Parameter
    double wndg = 0.0;        // Wind Damage Parameter
    double mburst = 0.0;      // Microburst Index
    double sig_svr = 0.0;     // Significant Severe Parameter
    double lr_03km = 0.0;     // 0-3km lapse rate (C/km)
    double lr_36km = 0.0;     // 3-6km lapse rate (C/km)
    double lr_700_500 = 0.0;  // 700-500mb lapse rate (C/km)
    double lr_850_500 = 0.0;  // 850-500mb lapse rate (C/km)
};

// Fast polynomial approximations from fast_thermo.py
double wobf(double t);
double vappres(double t);

double vapor_pressure(double pres, double temp_c);
double mixing_ratio(double pres, double dwpc);
double temp_at_mixrat(double w, double pres);
double theta(double pres, double temp_c, double pres_ref = 1000.0);
double theta_e(double pres, double temp_c, double dwpc);
double virtual_temp(double pres, double temp_c, double dwpc);
double wetbulb(double pres, double temp_c, double dwpc);
void get_lcl(double pres, double temp_c, double dwpc, double& lcl_p, double& lcl_t);
double moist_lapse_rate(double pres, double temp_k);
double lift_moist(double p1, double p2, double t1_c);

double precipitable_water(const Profile& prof);
double k_index(const Profile& prof);
double dcape(const Profile& prof);
double lapse_rate(const Profile& prof, double lower, double upper, bool pres = true);

SoundingMetrics compute_sounding_metrics(const Profile& prof, const ParcelResult& sb, const ParcelResult& ml);
ParcelResult surface_parcel(const Profile& prof);
ParcelResult mixed_layer_parcel(const Profile& prof, double ml_depth_hpa = 100.0);
ParcelResult most_unstable_parcel(const Profile& prof, double search_depth_hpa = 300.0);
ParcelResult forecast_parcel(const Profile& prof);
ParcelResult lift_parcel(const Profile& prof, double p, double t, double dp, bool keep_trajectory = true);

} // namespace sharppy
