#include "sharppy/winds.hpp"
#include "sharppy/tab.hpp"
#include "sharppy/thermo.hpp"
#include <cmath>
#include <algorithm>
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace sharppy {

constexpr double KTS_TO_MS = 0.514444;

static WindVector mk(double u, double v) {
    WindVector r; r.u = u; r.v = v; comp2vec(u, v, r.wdir, r.wspd); return r;
}

// ---- exact sharptab/winds.py (pressure coordinates) ----
WindVector mean_wind_pres(const Profile& prof, double pbot, double ptop, bool weighted) {
    if (prof.empty() || !std::isfinite(pbot) || !std::isfinite(ptop) || pbot == MISSING || ptop == MISSING) return {};
    // np.arange(pbot, ptop+dp, dp), dp = -1
    long cnt = (long)std::ceil((pbot - ptop) + 1.0);
    double su = 0, sv = 0, sw = 0; long n = 0;
    // Same math as Profile::wind_at_pres (log-p interpolation), but the bracketing level is tracked as p
    // decreases and its logs are cached, so each sample costs one log() instead of four plus a search.
    const size_t np = prof.pres.size();
    long bi = -2;                                    // -2: no bracket yet
    double lp1 = 0, lp2 = 0;
    for (long k = 0; k < cnt; ++k) {
        const double pw = pbot - (double)k;          // the weight uses the unsnapped pressure, as the original loop did
        double p = pw, u, v;
        if (np > 0) {                                // snapping done by find_pres_bracket
            if (p > prof.pres.front() && p - prof.pres.front() < 1e-6) p = prof.pres.front();
            if (p < prof.pres.back() && prof.pres.back() - p < 1e-6) p = prof.pres.back();
        }
        if (bi < 0 || !(prof.pres[bi] >= p && p >= prof.pres[bi + 1])) {
            // advance while the next interval still starts at/above p, else fall back to the full search
            long j = (bi >= 0) ? bi : -1;
            if (j >= 0) { while (j + 2 < (long)np && prof.pres[j + 1] > p) ++j; }
            if (j >= 0 && prof.pres[j] >= p && p >= prof.pres[j + 1]) bi = j;
            else bi = prof.find_pres_bracket(p);
            if (bi >= 0) { lp1 = std::log(prof.pres[bi]); lp2 = std::log(prof.pres[bi + 1]); }
        }
        if (bi < 0) continue;                        // outside the profile -> MISSING
        const double p1 = prof.pres[bi], p2 = prof.pres[bi + 1];
        if (pw <= 0 || p1 <= 0 || p2 <= 0) continue;
        if (std::abs(p2 - p1) < 1e-6) { u = prof.u[bi]; v = prof.v[bi]; }
        else {
            const double lp = std::log(pw);             // identical operation order to log_pres_interp
            u = prof.u[bi] + (lp - lp1) * (prof.u[bi + 1] - prof.u[bi]) / (lp2 - lp1);
            v = prof.v[bi] + (lp - lp1) * (prof.v[bi + 1] - prof.v[bi]) / (lp2 - lp1);
        }
        if (u == MISSING || v == MISSING) continue;
        double w = weighted ? pw : 1.0;
        su += u * w; sv += v * w; sw += w; ++n;
    }
    if (n == 0 || sw == 0) return {};
    return mk(su / sw, sv / sw);
}
WindVector wind_shear_pres(const Profile& prof, double pbot, double ptop) {
    double u1, v1, u2, v2;
    prof.wind_at_pres(pbot, u1, v1); prof.wind_at_pres(ptop, u2, v2);
    if (u1 == MISSING || u2 == MISSING) return {};
    return mk(u2 - u1, v2 - v1);
}
static double ph(const Profile& prof, double h_agl) { return prof.pres_at_hght(prof.to_msl(h_agl)); }

WindVector wind_shear(const Profile& prof, double bottom_agl, double top_agl) {
    return wind_shear_pres(prof, ph(prof, bottom_agl), ph(prof, top_agl));
}
WindVector mean_wind(const Profile& prof, double bottom_agl, double top_agl) {
    return mean_wind_pres(prof, ph(prof, bottom_agl), ph(prof, top_agl), true);
}
WindVector sr_wind(const Profile& prof, double bottom_agl, double top_agl, double storm_u, double storm_v) {
    WindVector mw = mean_wind(prof, bottom_agl, top_agl);
    return mk(mw.u - storm_u, mw.v - storm_v);
}

double storm_relative_helicity(const Profile& prof, double bottom_agl, double top_agl, double storm_u, double storm_v) {
    if (prof.empty() || bottom_agl == top_agl) return 0.0;
    double pl = ph(prof, bottom_agl), pu = ph(prof, top_agl);
    if (pl == MISSING || pu == MISSING || !std::isfinite(pl) || !std::isfinite(pu)) return 0.0;
    long i1 = -1, i2 = -1;
    for (size_t i = 0; i < prof.pres.size(); ++i) if (pl >= prof.pres[i]) { i1 = (long)i; break; }
    for (size_t i = 0; i < prof.pres.size(); ++i) if (pu <= prof.pres[i]) i2 = (long)i;
    if (i1 < 0 || i2 < 0) return 0.0;
    double u1, v1, u2, v2; prof.wind_at_pres(pl, u1, v1); prof.wind_at_pres(pu, u2, v2);
    if (u1 == MISSING || u2 == MISSING) return 0.0;
    std::vector<double> u{u1}, v{v1};
    for (long i = i1; i <= i2; ++i) if (prof.u[i] != MISSING && prof.v[i] != MISSING) { u.push_back(prof.u[i]); v.push_back(prof.v[i]); }
    u.push_back(u2); v.push_back(v2);
    double ph_ = 0, nh = 0;
    for (size_t i = 0; i + 1 < u.size(); ++i) {
        double sa = (u[i + 1] - storm_u) * KTS_TO_MS, sb = (v[i] - storm_v) * KTS_TO_MS;
        double sc = (u[i] - storm_u) * KTS_TO_MS, sd = (v[i + 1] - storm_v) * KTS_TO_MS;
        double l = sa * sb - sc * sd;
        if (l > 0) ph_ += l; else if (l < 0) nh += l;
    }
    return ph_ + nh;
}

static void mean_wind_p(const Profile& prof, double pbot, double ptop, double& mu, double& mv) {
    WindVector w = mean_wind_pres(prof, pbot, ptop, true); mu = w.u; mv = w.v;
    if (w.u == 0 && w.v == 0 && w.wspd == 0 && prof.empty()) mu = mv = MISSING;
}
static void shear_p(const Profile& prof, double pbot, double ptop, double& du, double& dv) {
    WindVector w = wind_shear_pres(prof, pbot, ptop); du = w.u; dv = w.v;
}

BunkersMotion bunkers_storm_motion(const Profile& prof) {
    BunkersMotion res;
    if (prof.size() > 1) {   // params.bunkers_storm_motion (parcel based, effective inflow base)
        tab::Env env(prof);
        tab::LPL l = tab::define_mu(env);
        tab::Parcel mu = tab::parcelx(env, l.pres, env.temp_at(l.pres), env.dwpt_at(l.pres));
        double pb, pt; tab::effective_inflow(env, mu.bplus, mu.bminus, pb, pt);
        if (std::isfinite(pb) && std::isfinite(pt) && mu.bplus > 100. && std::isfinite(mu.elhght)) {
            const double d = 7.5 / 0.514444;
            double base = env.to_agl(env.hght_at(pb));
            double htop = base + (mu.elhght - base) * 0.65;
            double ptop = env.pres_at_hght(env.to_msl(htop));
            double mnu, mnv, sru, srv;
            mean_wind_p(prof, pb, ptop, mnu, mnv); shear_p(prof, pb, ptop, sru, srv);
            double mag = std::hypot(sru, srv);
            if (mnu != MISSING && mag > 0 && std::isfinite(ptop)) {
                double uchg = d / mag * srv, vchg = d / mag * sru;
                res.right_mover.u = mnu + uchg; res.right_mover.v = mnv - vchg;
                res.left_mover.u = mnu - uchg;  res.left_mover.v = mnv + vchg;
                res.mean_wind.u = mnu; res.mean_wind.v = mnv;
                comp2vec(mnu, mnv, res.mean_wind.wdir, res.mean_wind.wspd);
                comp2vec(res.right_mover.u, res.right_mover.v, res.right_mover.wdir, res.right_mover.wspd);
                comp2vec(res.left_mover.u, res.left_mover.v, res.left_mover.wdir, res.left_mover.wspd);
                return res;
            }
        }
    }

    // winds.non_parcel_bunkers_motion
    const double d = 7.5 / KTS_TO_MS;
    double psfc = prof.sfc_pres, p6 = ph(prof, 6000.0);
    WindVector mw = mean_wind_pres(prof, psfc, p6, false);
    WindVector sh = wind_shear_pres(prof, psfc, p6);
    res.mean_wind = mw;
    double mag = std::hypot(sh.u, sh.v);
    if (mag < 1e-9) { res.right_mover = mw; res.left_mover = mw; return res; }
    double tmp = d / mag;
    res.right_mover = mk(mw.u + tmp * sh.v, mw.v - tmp * sh.u);
    res.left_mover = mk(mw.u - tmp * sh.v, mw.v + tmp * sh.u);
    return res;
}

CorfidiMotion corfidi_mcs_motion(const Profile& prof) {
    CorfidiMotion cm;
    if (prof.empty()) return cm;
    WindVector m1 = mean_wind_pres(prof, prof.sfc_pres < 850.0 ? prof.sfc_pres : 850.0, 300.0, false);
    WindVector m2 = mean_wind_pres(prof, prof.sfc_pres, ph(prof, 1500.0), false);
    cm.upwind = mk(m1.u - m2.u, m1.v - m2.v);
    cm.downwind = mk(m1.u + cm.upwind.u, m1.v + cm.upwind.v);
    return cm;
}

InflowLayer effective_inflow_layer(const Profile& prof, double storm_u, double storm_v) {
    InflowLayer layer;
    if (prof.size() < 2) return layer;
    tab::Env env(prof);
    tab::Parcel mu = tab::parcelx(env, tab::define_mu(env).pres, env.temp_at(tab::define_mu(env).pres), env.dwpt_at(tab::define_mu(env).pres));
    double pb, pt;
    tab::effective_inflow(env, mu.bplus, mu.bminus, pb, pt);
    if (!std::isfinite(pb) || !std::isfinite(pt)) return layer;
    layer.bot_p = pb; layer.top_p = pt;
    double hb = env.hght_at(pb), ht = env.hght_at(pt);
    if (!std::isfinite(hb) || !std::isfinite(ht)) return layer;
    layer.bot_h_agl = env.to_agl(hb); layer.top_h_agl = env.to_agl(ht);
    layer.depth_m = std::max(0.0, layer.top_h_agl - layer.bot_h_agl);
    layer.esrh = storm_relative_helicity(prof, layer.bot_h_agl, layer.top_h_agl, storm_u, storm_v);
    return layer;
}

double critical_angle(const Profile& prof, double storm_u, double storm_v) {
    double u5, v5, su, sv;
    prof.wind_at_pres(ph(prof, 500.0), u5, v5);
    prof.wind_at_pres(prof.sfc_pres, su, sv);
    if (u5 == MISSING || su == MISSING) return MISSING;
    double a1 = u5 - su, b1 = v5 - sv, a2 = storm_u - su, b2 = storm_v - sv;
    double m = std::hypot(a1, b1) * std::hypot(a2, b2);
    double c = (a1 * a2 + b1 * b2) / m;
    return std::acos(c) * 180.0 / M_PI;     // NaN if m == 0, as numpy
}

} // namespace sharppy
