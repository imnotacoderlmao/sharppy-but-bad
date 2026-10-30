#include "sharppy/params.hpp"
#include <cmath>
#include <algorithm>

namespace sharppy {

constexpr double KTS_TO_MS = 0.514444;

double temp_level_agl(const Profile& prof, double target_temp_c) {
    if (prof.size() < 2) return MISSING;
    for (size_t i = 0; i < prof.size() - 1; ++i) {
        if ((prof.temp[i] >= target_temp_c && prof.temp[i + 1] <= target_temp_c) ||
            (prof.temp[i] <= target_temp_c && prof.temp[i + 1] >= target_temp_c)) {
            double h_msl = linear_interp(target_temp_c, prof.temp[i], prof.temp[i + 1],
                                         prof.hght[i], prof.hght[i + 1]);
            return prof.to_agl(h_msl);
        }
    }
    return MISSING;
}

double compute_stp_cin(double mlcape, double esrh, double ebwd_ms, double mllcl_agl, double mlcin) {
    double cape_term = std::max(0.0, mlcape) / 1500.0;
    double eshr_term = std::max(0.0, esrh) / 150.0;

    double ebwd_term = 0.0;
    if (ebwd_ms >= 30.0) {
        ebwd_term = 1.5;
    } else if (ebwd_ms >= 12.5) {
        ebwd_term = ebwd_ms / 20.0;
    }

    double lcl_term = 0.0;
    if (mllcl_agl <= 1000.0) {
        lcl_term = 1.0;
    } else if (mllcl_agl < 2000.0) {
        lcl_term = (2000.0 - mllcl_agl) / 1000.0;
    }

    double cin_term = 0.0;
    if (mlcin >= -50.0) {
        cin_term = 1.0;
    } else if (mlcin > -200.0) {
        cin_term = (mlcin + 200.0) / 150.0;
    }

    return std::max(0.0, cape_term * eshr_term * ebwd_term * lcl_term * cin_term);
}

double compute_stp_fixed(double sbcape, double sblcl_agl, double srh01, double shear06_ms) {
    double cape_term = std::max(0.0, sbcape) / 1500.0;
    double srh_term = srh01 / 150.0;                 // params.stp_fixed: not clamped (left-mover STP is negative)

    double shear_term = 0.0;
    if (shear06_ms >= 30.0) {
        shear_term = 1.5;
    } else if (shear06_ms >= 12.5) {
        shear_term = shear06_ms / 20.0;
    }

    double lcl_term = 0.0;
    if (sblcl_agl <= 1000.0) {
        lcl_term = 1.0;
    } else if (sblcl_agl < 2000.0) {
        lcl_term = (2000.0 - sblcl_agl) / 1000.0;
    }

    return cape_term * lcl_term * srh_term * shear_term;
}

double compute_scp(double mucape, double esrh, double ebwd_ms) {
    // params.scp: no clamping at zero (left-mover SCP is negative by construction)
    double cape_term = mucape / 1000.0;
    double srh_term = esrh / 50.0;

    double ebwd_term = 0.0;
    if (ebwd_ms > 20.0) {
        ebwd_term = 1.0;
    } else if (ebwd_ms >= 10.0) {
        ebwd_term = ebwd_ms / 20.0;
    }

    return cape_term * srh_term * ebwd_term;
}

double compute_ship(const Profile& prof, const ParcelResult& mupcl, double shear06_ms) {
    double mucape = std::max(0.0, mupcl.cape);
    double mumr = mixing_ratio(mupcl.start_p, mupcl.start_dp);
    // params.ship uses the MSL height of the freezing level (interp.hght of temp_lvl)
    double frz_lvl = temp_level_agl(prof, 0.0);
    if (frz_lvl != MISSING) frz_lvl += prof.sfc_hght;
    double h5_temp = prof.temp_at_pres(500.0);
    double lr75 = lapse_rate(prof, 700.0, 500.0, true);

    if (frz_lvl == MISSING) frz_lvl = 3000.0 + prof.sfc_hght;
    if (h5_temp == MISSING) h5_temp = -12.0;
    if (lr75 <= 0.0) lr75 = 6.5;

    double shr = std::min(27.0, std::max(7.0, shear06_ms));
    double mr = std::min(13.6, std::max(11.0, mumr));
    double t5 = std::min(-5.5, h5_temp);

    double ship = -1.0 * (mucape * mr * lr75 * t5 * shr) / 42000000.0;
    if (mucape < 1300.0) ship *= (mucape / 1300.0);
    if (lr75 < 5.8) ship *= (lr75 / 5.8);
    if (frz_lvl < 2400.0) ship *= (frz_lvl / 2400.0);

    return std::max(0.0, ship);
}

SevereParams compute_severe_params(
    const Profile& prof,
    const ParcelResult& sbpcl,
    const ParcelResult& mlpcl,
    const ParcelResult& mupcl,
    const BunkersMotion& bunkers,
    bool use_left_mover,
    const InflowLayer* inflow_for_motion
) {
    SevereParams p;
    p.is_left_mover = use_left_mover;
    if (prof.empty()) return p;

    const WindVector& sm = use_left_mover ? bunkers.left_mover : bunkers.right_mover;

    WindVector shear01 = wind_shear(prof, 0.0, 1000.0);
    WindVector shear06 = wind_shear(prof, 0.0, 6000.0);
    WindVector shear08 = wind_shear(prof, 0.0, 8000.0);
    p.sfc_8km_shear_kts = shear08.wspd;

    double srh01 = storm_relative_helicity(prof, 0.0, 1000.0, sm.u, sm.v);
    double srh03 = storm_relative_helicity(prof, 0.0, 3000.0, sm.u, sm.v);
    // Left-mover (and southern-hemisphere) SRH is negative by construction; flip it so the composites read positive
    const double sg = use_left_mover ? -1.0 : 1.0;
    srh01 *= sg; srh03 *= sg;
    p.ehi_01 = (sbpcl.cape * srh01) / 160000.0;
    p.ehi_03 = (sbpcl.cape * srh03) / 160000.0;

    WindVector mw_46 = mean_wind(prof, 4000.0, 6000.0);
    p.srw_4_6km_kts = std::hypot(mw_46.u - sm.u, mw_46.v - sm.v);

    const InflowLayer inflow = inflow_for_motion ? *inflow_for_motion : effective_inflow_layer(prof, sm.u, sm.v);
    const bool haveEff = inflow.bot_p != MISSING && inflow.top_p != MISSING && inflow.bot_h_agl != MISSING;
    double esrh = 0.0, ebwd_ms = 0.0;
    if (haveEff) {
        // profile.py: EBWD = shear from the inflow base to (base + (MU EL - base)/2)
        esrh = sg * inflow.esrh;
        double elh_agl = inflow.bot_h_agl;
        if (mupcl.el_h_agl != MISSING) elh_agl = inflow.bot_h_agl + (mupcl.el_h_agl - inflow.bot_h_agl) / 2.0;
        WindVector ebw = wind_shear(prof, inflow.bot_h_agl, elh_agl);
        ebwd_ms = ebw.wspd * KTS_TO_MS;
        p.ebwd_kts = ebw.wspd;
    }

    p.hght_frz = temp_level_agl(prof, 0.0);
    p.hght_m10c = temp_level_agl(prof, -10.0);
    p.hght_m20c = temp_level_agl(prof, -20.0);

    double mllcl_agl = (mlpcl.lcl_h_agl != MISSING) ? mlpcl.lcl_h_agl : 1000.0;
    double sblcl_agl = (sbpcl.lcl_h_agl != MISSING) ? sbpcl.lcl_h_agl : 1000.0;

    p.stp_fixed = compute_stp_fixed(sbpcl.cape, sblcl_agl, srh01, shear06.wspd * KTS_TO_MS);
    if (!haveEff) {
        p.scp = 0.0; p.stp_cin = 0.0;           // profile.py: no effective layer -> 0
    } else {
        p.scp = compute_scp(mupcl.cape, esrh, ebwd_ms);
        p.stp_cin = compute_stp_cin(mlpcl.cape, esrh, ebwd_ms, mllcl_agl, mlpcl.cin);
    }
    p.ship = compute_ship(prof, mupcl, shear06.wspd * KTS_TO_MS);
    p.critical_angle_deg = critical_angle(prof, sm.u, sm.v);

    // BRN calculation
    WindVector sfc_wind = mean_wind(prof, 0.0, 500.0);
    WindVector mid_wind = mean_wind(prof, 0.0, 6000.0);
    double du_ms = (mid_wind.u - sfc_wind.u) * KTS_TO_MS;
    double dv_ms = (mid_wind.v - sfc_wind.v) * KTS_TO_MS;
    p.brn_shear = 0.5 * (du_ms * du_ms + dv_ms * dv_ms);
    p.brn = (p.brn_shear > 1.0) ? (sbpcl.cape / p.brn_shear) : 0.0;

    return p;
}

} // namespace sharppy
