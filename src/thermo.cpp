#include "sharppy/thermo.hpp"
#include "sharppy/tab_params.hpp"
#include "sharppy/tab.hpp"
#include <algorithm>
#include <cmath>

namespace sharppy {

// Numba-accelerated fast thermo polynomials from fast_thermo.py
double wobf(double t) {
    double t_shifted = t - 20.0;
    if (t_shifted <= 0.0) {
        double npol = 1.0 + t_shifted * (-8.8416605e-3 + t_shifted * (1.4714143e-4 + t_shifted * (-9.6719890e-7 + t_shifted * (-3.2607217e-8 + t_shifted * (-3.8598073e-10)))));
        npol = 15.13 / (npol * npol * npol * npol);
        return npol;
    } else {
        double ppol = t_shifted * (4.9618922e-07 + t_shifted * (-6.1059365e-09 + t_shifted * (3.9401551e-11 + t_shifted * (-1.2588129e-13 + t_shifted * 1.6688280e-16))));
        ppol = 1.0 + t_shifted * (3.6182989e-03 + t_shifted * (-1.3603273e-05 + ppol));
        ppol = (29.93 / (ppol * ppol * ppol * ppol)) + (0.96 * t_shifted) - 14.8;
        return ppol;
    }
}

double vappres(double t) {
    double pol = t * (1.1112018e-17 + (t * -3.0994571e-20));
    pol = t * (2.1874425e-13 + (t * (-1.789232e-15 + pol)));
    pol = t * (4.3884180e-09 + (t * (-2.988388e-11 + pol)));
    pol = t * (7.8736169e-05 + (t * (-6.111796e-07 + pol)));
    pol = 0.99999683 + (t * (-9.082695e-03 + pol));
    double pol4 = pol * pol * pol * pol;
    return 6.1078 / (pol4 * pol4);
}

double vapor_pressure(double pres, double temp_c) {
    return 6.112 * std::exp((17.67 * temp_c) / (temp_c + 243.5));
}

double mixing_ratio(double pres, double dwpc) {
    double e = vapor_pressure(pres, dwpc);
    return 1000.0 * (EPS * e) / std::max(1e-4, pres - e);
}

double temp_at_mixrat(double w, double pres) {
    double e = (w * pres) / (1000.0 * EPS + w);
    e = std::max(1e-4, e);
    return (243.5 * std::log(e / 6.112)) / (17.67 - std::log(e / 6.112));
}

double theta(double pres, double temp_c, double pres_ref) {
    return c2k(temp_c) * std::pow(pres_ref / pres, ROCP);
}

void get_lcl(double pres, double temp_c, double dwpc, double& lcl_p, double& lcl_t) {
    double tk = c2k(temp_c);
    double dk = c2k(dwpc);
    double tlcl = 1.0 / (1.0 / (dk - 56.0) + std::log(tk / dk) / 800.0) + 56.0;
    lcl_t = k2c(tlcl);
    lcl_p = pres * std::pow(tlcl / tk, 1.0 / ROCP);
}

double theta_e(double pres, double temp_c, double dwpc) {
    return tab::thetae(pres, temp_c, dwpc) + 273.15;
}

double virtual_temp(double pres, double temp_c, double dwpc) {
    double tk = c2k(temp_c);
    double w = mixing_ratio(pres, dwpc) / 1000.0;
    return k2c(tk * (1.0 + 0.61 * w));
}

double wetbulb(double pres, double temp_c, double dwpc) {
    return tab::wetbulb(pres, temp_c, dwpc);
}

double moist_lapse_rate(double pres, double temp_k) {
    double tc = k2c(temp_k);
    double e = vapor_pressure(pres, tc);
    double r = (EPS * e) / std::max(1e-4, pres - e);

    double num = 1.0 + (LV * r) / (RD * temp_k);
    double den = CP + (LV * LV * r * EPS) / (RD * temp_k * temp_k);
    
    double tv = temp_k * (1.0 + 0.61 * r);
    double dp_dz = -(G * pres * 100.0) / (RD * tv);
    double dt_dz = -G * (num / den);
    
    return (dt_dz / (dp_dz / 100.0));
}

double lift_moist(double p1, double p2, double t1_c) {
    if (std::abs(p1 - p2) < 1e-4) return t1_c;

    double tk = c2k(t1_c);
    double p = p1;
    double dp = (p2 - p1);

    int steps = std::max(1, static_cast<int>(std::abs(dp) / 10.0));
    double h = dp / steps;

    for (int i = 0; i < steps; ++i) {
        double k1 = moist_lapse_rate(p, tk);
        double k2 = moist_lapse_rate(p + 0.5 * h, tk + 0.5 * h * k1);
        double k3 = moist_lapse_rate(p + 0.5 * h, tk + 0.5 * h * k2);
        double k4 = moist_lapse_rate(p + h, tk + h * k3);
        tk += (h / 6.0) * (k1 + 2.0 * k2 + 2.0 * k3 + k4);
        p += h;
    }

    return k2c(tk);
}

double precipitable_water(const Profile& prof) {
    double pwat_mm = 0.0;
    for (size_t i = 0; i < prof.size() - 1; ++i) {
        if (prof.pres[i + 1] < 100.0) break;
        double avg_w = 0.5 * (mixing_ratio(prof.pres[i], prof.dwpt[i]) + 
                              mixing_ratio(prof.pres[i + 1], prof.dwpt[i + 1])) / 1000.0;
        double dp = (prof.pres[i] - prof.pres[i + 1]) * 100.0;
        pwat_mm += (avg_w * dp) / G;
    }
    return pwat_mm / 25.4;
}

double k_index(const Profile& prof) {
    double t850 = prof.temp_at_pres(850.0);
    double td850 = prof.dwpt_at_pres(850.0);
    double t700 = prof.temp_at_pres(700.0);
    double td700 = prof.dwpt_at_pres(700.0);
    double t500 = prof.temp_at_pres(500.0);

    if (t850 == MISSING || t500 == MISSING || t700 == MISSING) return MISSING;
    return (t850 - t500) + td850 - (t700 - td700);
}

double dcape(const Profile& prof) {
    if (prof.size() < 2) return 0.0;
    const auto env_hold_ = tab::env_for(prof); const tab::Env& env = *env_hold_;
    double d = tab::dcape(env);
    return std::isfinite(d) ? d : 0.0;
}

double lapse_rate(const Profile& prof, double lower, double upper, bool pres) {
    // exact params.lapse_rate (virtual-temperature based); masked -> 0 for the legacy callers
    double v = tab::lapse_rate_exact(prof, lower, upper, pres);
    return std::isfinite(v) ? v : 0.0;
}

static double nz(double v, double d = MISSING) { return std::isfinite(v) ? v : d; }

static ParcelResult from_tab(const Profile& prof, const tab::Env& env, const tab::LPL& l, bool keep_trajectory) {
    ParcelResult res;
    res.start_p = nz(l.pres); res.start_t = nz(l.tmpc); res.start_dp = nz(l.dwpc);
    tab::Parcel pc = tab::parcelx(env, l.pres, l.tmpc, l.dwpc);
    res.lcl_p = nz(pc.lclpres); res.lcl_t = nz(pc.lcltmp); res.lcl_h_agl = nz(pc.lclhght);
    res.lfc_p = nz(pc.lfcpres); res.lfc_h_agl = nz(pc.lfchght);
    res.el_p = nz(pc.elpres); res.el_h_agl = nz(pc.elhght);
    res.cape = nz(pc.bplus, 0.0); res.cin = nz(pc.bminus, 0.0);
    res.cape_3km = nz(pc.b3km, 0.0); res.cape_6km = nz(pc.b6km, 0.0);
    res.li = nz(pc.li5); res.cap = nz(pc.cap, 0.0);
    if (res.el_h_agl != MISSING && res.lfc_h_agl != MISSING && res.el_h_agl - res.lfc_h_agl > 100.0)
        res.ncape = res.cape / (res.el_h_agl - res.lfc_h_agl);
    if (keep_trajectory)
        for (size_t i = 0; i < pc.ptrace.size(); ++i) {
            double et = env.temp_at(pc.ptrace[i]);
            res.trajectory.push_back({pc.ptrace[i], pc.ttrace[i], nz(et)});
        }
    (void)prof;
    return res;
}

ParcelResult lift_parcel(const Profile& prof, double p, double t, double dp, bool keep_trajectory) {
    if (prof.empty()) return {};
    const auto env_hold_ = tab::env_for(prof); const tab::Env& env = *env_hold_;
    return from_tab(prof, env, tab::LPL{p, t, dp}, keep_trajectory);
}

ParcelResult surface_parcel(const Profile& prof) {
    if (prof.empty()) return {};
    const auto env_hold_ = tab::env_for(prof); const tab::Env& env = *env_hold_;
    return from_tab(prof, env, tab::define_sfc(env), true);
}

ParcelResult mixed_layer_parcel(const Profile& prof, double ml_depth_hpa) {
    if (prof.empty()) return {};
    const auto env_hold_ = tab::env_for(prof); const tab::Env& env = *env_hold_;
    return from_tab(prof, env, tab::define_ml(env, ml_depth_hpa), true);
}

ParcelResult most_unstable_parcel(const Profile& prof, double search_depth_hpa) {
    if (prof.empty()) return {};
    const auto env_hold_ = tab::env_for(prof); const tab::Env& env = *env_hold_;
    return from_tab(prof, env, tab::define_mu(env, search_depth_hpa), true);
}

ParcelResult forecast_parcel(const Profile& prof) {
    if (prof.empty()) return {};
    const auto env_hold_ = tab::env_for(prof); const tab::Env& env = *env_hold_;
    return from_tab(prof, env, tab::define_fcst(env), true);
}

SoundingMetrics compute_sounding_metrics(const Profile& prof, const ParcelResult& sb, const ParcelResult& ml) {
    SoundingMetrics m;
    if (prof.empty()) return m;

    m.pwat = precipitable_water(prof);
    m.k_idx = k_index(prof);
    m.dcape_val = dcape(prof);
    m.down_t_f = (prof.temp.front() - (m.dcape_val / 200.0)) * 1.8 + 32.0;

    double sum_w = 0.0, dp_tot = 0.0;
    double p_top = prof.sfc_pres - 100.0;
    for (size_t i = 0; i < prof.size() - 1; ++i) {
        if (prof.pres[i] < p_top) break;
        double dp = prof.pres[i] - prof.pres[i + 1];
        double avg_w = 0.5 * (mixing_ratio(prof.pres[i], prof.dwpt[i]) + 
                              mixing_ratio(prof.pres[i + 1], prof.dwpt[i + 1]));
        sum_w += avg_w * dp;
        dp_tot += dp;
    }
    m.mean_w = (dp_tot > 0) ? (sum_w / dp_tot) : 10.0;

    double sum_rh_low = 0.0; int count_low = 0;
    double sum_rh_mid = 0.0; int count_mid = 0;

    for (size_t i = 0; i < prof.size(); ++i) {
        double e = vapor_pressure(prof.pres[i], prof.dwpt[i]);
        double es = vapor_pressure(prof.pres[i], prof.temp[i]);
        double rh = std::min(100.0, std::max(0.0, (e / es) * 100.0));

        double h_agl = prof.to_agl(prof.hght[i]);
        if (h_agl <= 1000.0) {
            sum_rh_low += rh;
            count_low++;
        }
        if (prof.pres[i] <= 700.0 && prof.pres[i] >= 500.0) {
            sum_rh_mid += rh;
            count_mid++;
        }
    }
    m.low_rh = (count_low > 0) ? (sum_rh_low / count_low) : 70.0;
    m.mid_rh = (count_mid > 0) ? (sum_rh_mid / count_mid) : 50.0;

    double t850 = prof.temp_at_pres(850.0);
    double td850 = prof.dwpt_at_pres(850.0);
    double t500 = prof.temp_at_pres(500.0);
    if (t850 != MISSING && td850 != MISSING && t500 != MISSING) {
        m.tt = (t850 - t500) + (td850 - t500);
    }

    double th_e_sfc = theta_e(prof.sfc_pres, prof.temp.front(), prof.dwpt.front());
    double min_th_e = 9999.0;
    for (size_t i = 0; i < prof.size(); ++i) {
        if (prof.pres[i] < prof.sfc_pres - 400.0) break;
        double th_e = theta_e(prof.pres[i], prof.temp[i], prof.dwpt[i]);
        if (th_e < min_th_e) min_th_e = th_e;
    }
    m.tei = std::max(0.0, th_e_sfc - min_th_e);

    m.lr_03km = lapse_rate(prof, 0.0, 3000.0, false);
    m.lr_36km = lapse_rate(prof, 3000.0, 6000.0, false);
    m.lr_700_500 = lapse_rate(prof, 700.0, 500.0, true);
    m.lr_850_500 = lapse_rate(prof, 850.0, 500.0, true);

    m.esp = std::max(0.0, (sb.cape_3km / 50.0) * ((m.lr_03km - 7.0) / 1.0));
    m.conv_t_f = (prof.temp.front() + 3.0) * 1.8 + 32.0;
    m.max_t_f = (prof.temp.front() + 5.0) * 1.8 + 32.0;
    m.mmp = std::min(1.0, std::max(0.0, (m.tei / 35.0) * (sb.cape / 2500.0)));
    m.wndg = std::max(0.0, (m.dcape_val / 1000.0) * (m.mid_rh / 60.0));
    m.mburst = (m.dcape_val > 800) ? 2.0 : 1.0;
    m.sig_svr = sb.cape * 25.0 * 200.0 * 0.001;

    return m;
}

} // namespace sharppy
