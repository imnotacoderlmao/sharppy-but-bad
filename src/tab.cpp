#include "sharppy/tab.hpp"
#include "sharppy/constants.hpp"
#include <algorithm>

namespace sharppy { namespace tab {

static inline bool isn(double x) { return x != x; }

// ======================= fast_thermo.py kernels =======================
double wobf(double t) {
    t = t - 20.0;
    if (t <= 0.0) {
        double npol = 1. + t * (-8.841660499999999e-3 + t * (1.4714143e-4 + t * (-9.671989000000001e-7 + t * (-3.2607217e-8 + t * (-3.8598073e-10)))));
        const double n2 = npol * npol;
        return 15.13 / (n2 * n2);                       // == pow(npol, 4), without the libm call
    }
    double ppol = t * (4.9618922e-07 + t * (-6.1059365e-09 + t * (3.9401551e-11 + t * (-1.2588129e-13 + t * (1.6688280e-16)))));
    ppol = 1 + t * (3.6182989e-03 + t * (-1.3603273e-05 + ppol));
    const double p2 = ppol * ppol;
    return (29.93 / (p2 * p2)) + (0.96 * t) - 14.8;
}
double theta(double p, double t, double p2) { return ((t + ZEROCNK) * std::pow(p2 / p, ROCP)) - ZEROCNK; }
double vappres(double t) {
    double pol = t * (1.1112018e-17 + (t * -3.0994571e-20));
    pol = t * (2.1874425e-13 + (t * (-1.789232e-15 + pol)));
    pol = t * (4.3884180e-09 + (t * (-2.988388e-11 + pol)));
    pol = t * (7.8736169e-05 + (t * (-6.111796e-07 + pol)));
    pol = 0.99999683 + (t * (-9.082695e-03 + pol));
    const double q2 = pol * pol, q4 = q2 * q2;
    return 6.1078 / (q4 * q4);
}
double mixratio(double p, double t) {
    double x = 0.02 * (t - 12.5 + (7500. / p));
    double wfw = 1. + (0.0000045 * p) + (0.0014 * x * x);
    double fwesw = wfw * vappres(t);
    return 621.97 * (fwesw / (p - fwesw));
}
double virtemp(double p, double t, double td) {
    const double eps = 0.62197;
    double tk = t + ZEROCNK;
    double w = 0.001 * mixratio(p, td);
    double vt = (tk * (1. + w / eps) / (1. + w)) - ZEROCNK;
    return isn(vt) ? t : vt;
}
double lcltemp(double t, double td) {
    double s = t - td;
    double dlt = s * (1.2185 + 0.001278 * t + s * (-0.00219 + 1.173e-5 * s - 0.0000052 * t));
    return t - dlt;
}
double thalvl(double theta_, double t) {
    t = t + ZEROCNK; theta_ = theta_ + ZEROCNK;
    return 1000. / std::pow(theta_ / t, 1. / ROCP);
}
void drylift(double p, double t, double td, double& p2, double& t2) {
    t2 = lcltemp(t, td);
    p2 = thalvl(theta(p, t, 1000.), t2);
}
double satlift(double p, double thetam, double conv) {
    if (std::fabs(p - 1000.) - 0.001 <= 0) return thetam;
    double eor = 999.0, t1 = 0, e1 = 0, t2 = 0, e2 = 0, rate = 1.0;
    double pwrp = std::pow(p / 1000., ROCP);
    bool first = true;
    while (std::fabs(eor) - conv > 0) {
        if (first) {
            t1 = (thetam + ZEROCNK) * pwrp - ZEROCNK;
            e1 = wobf(t1) - wobf(thetam);
            rate = 1.0; first = false;
        } else {
            rate = (t2 - t1) / (e2 - e1);
            t1 = t2; e1 = e2;
        }
        t2 = t1 - (e1 * rate);
        e2 = (t2 + ZEROCNK) / pwrp - ZEROCNK;
        e2 += wobf(t2) - wobf(e2) - thetam;
        eor = e2 * rate;
        if (isn(eor)) break;     // numba kernel would spin on NaN; bail out
    }
    return t2 - eor;
}
double wetlift(double p, double t, double p2) {
    double thta = theta(p, t, 1000.);
    double thetam = thta - wobf(thta) + wobf(t);
    return satlift(p2, thetam);
}
double temp_at_mixrat(double w, double p) {
    const double c1 = 0.0498646455, c2 = 2.4082965, c3 = 7.07475, c4 = 38.9114, c5 = 0.0915, c6 = 1.2035;
    double x = std::log10(w * p / (622. + w));
    return (std::pow(10., (c1 * x) + c2) - c3 + (c4 * std::pow(std::pow(10., c5 * x) - c6, 2.0))) - ZEROCNK;
}
double thetae(double p, double t, double td) {
    double p2, t2; drylift(p, t, td, p2, t2);
    return theta(100., wetlift(p2, t2, 100.), 1000.);
}
double wetbulb(double p, double t, double td) {
    double p2, t2; drylift(p, t, td, p2, t2);
    return wetlift(p2, t2, p);
}

// ======================= interpolation helpers =======================
static double interp1(double q, const std::vector<double>& x, const std::vector<double>& y) {
    const size_t n = x.size();
    if (n == 0 || isn(q) || q < x[0] || q > x[n - 1]) return NaN;
    size_t lo = 0, hi = n - 1;
    while (hi - lo > 1) { size_t mid = (lo + hi) / 2; if (x[mid] <= q) lo = mid; else hi = mid; }
    double x0 = x[lo], x1 = x[hi];
    if (x1 == x0) return y[lo];
    return y[lo] + (y[hi] - y[lo]) * (q - x0) / (x1 - x0);
}
static double fix(double v) { return (v == MISSING || !std::isfinite(v)) ? NaN : v; }

Env::Env(const Profile& prof) {
    const size_t n = prof.size();
    pres.resize(n); tmpc.resize(n); dwpc.resize(n); hght.resize(n); vtmp.resize(n);
    for (size_t i = 0; i < n; ++i) {
        pres[i] = fix(prof.pres[i]); tmpc[i] = fix(prof.temp[i]); dwpc[i] = fix(prof.dwpt[i]); hght[i] = fix(prof.hght[i]);
        // profile.py: vtmp = virtemp(p, t, td); where dwpc is masked, vtmp = tmpc
        vtmp[i] = isn(dwpc[i]) ? tmpc[i] : virtemp(pres[i], tmpc[i], dwpc[i]);
    }
    sfc = 0;
    for (size_t i = 0; i < n; ++i) if (!isn(pres[i]) && !isn(tmpc[i]) && !isn(hght[i])) { sfc = i; break; }
    sfc_pres = n ? pres[sfc] : NaN; sfc_hght = n ? hght[sfc] : NaN;
    auto build = [&](const std::vector<double>& f, std::vector<double>& lp, std::vector<double>& v) {
        for (long i = (long)n - 1; i >= 0; --i)
            if (!isn(pres[i]) && !isn(f[i]) && pres[i] > 0) { lp.push_back(std::log10(pres[i])); v.push_back(f[i]); }
    };
    build(hght, lp_h, v_h); build(tmpc, lp_t, v_t); build(dwpc, lp_d, v_d); build(vtmp, lp_v, v_v);
    for (size_t i = 0; i < n; ++i)
        if (!isn(pres[i]) && !isn(hght[i]) && pres[i] > 0) { hh_valid.push_back(hght[i]); lp_hh.push_back(std::log10(pres[i])); }
}
double Env::hght_at(double p) const { return interp1(std::log10(p), lp_h, v_h); }
double Env::temp_at(double p) const { return interp1(std::log10(p), lp_t, v_t); }
double Env::dwpt_at(double p) const { return interp1(std::log10(p), lp_d, v_d); }
double Env::vtmp_at(double p) const { return interp1(std::log10(p), lp_v, v_v); }
double Env::pres_at_hght(double h) const {
    double lp = interp1(h, hh_valid, lp_hh);
    return isn(lp) ? NaN : std::pow(10., lp);
}

// ======================= params.py helpers =======================
double temp_lvl(const Env& e, double temp) {
    const size_t n = e.pres.size();
    bool le = false, ge = false;
    for (size_t i = 0; i < n; ++i) { double d = e.tmpc[i] - temp; if (d <= 0) le = true; if (d >= 0) ge = true; }
    if (!le || !ge) return NaN;
    for (size_t i = 0; i < n; ++i) if (e.tmpc[i] - temp == 0) return e.pres[i];
    // valid (non-masked) entries only
    std::vector<size_t> ix;
    for (size_t i = 0; i < n; ++i) if (!isn(e.tmpc[i]) && !isn(e.pres[i])) ix.push_back(i);
    for (size_t k = 0; k + 1 < ix.size(); ++k) {
        double a = e.tmpc[ix[k]] - temp, b = e.tmpc[ix[k + 1]] - temp;
        if (a * b < 0) {
            // np.interp(temp, [T[k+1], T[k]], [logp[k+1], logp[k]]) incl. its behaviour for descending xp
            double xa = e.tmpc[ix[k + 1]], xb = e.tmpc[ix[k]];
            double fa = std::log10(e.pres[ix[k + 1]]), fb = std::log10(e.pres[ix[k]]);
            double lp;
            if (temp < xa) lp = fa; else if (temp > xb) lp = fb; else if (temp == xb) lp = fb;
            else lp = fa + (fb - fa) * (temp - xa) / (xb - xa);
            return std::pow(10., lp);
        }
    }
    return NaN;
}

double max_temp(const Env& e, double mixlayer) {
    double ml = e.sfc_pres - mixlayer;
    double temp = ctok(e.temp_at(ml)) + 2;
    return temp * std::pow(e.sfc_pres / ml, ROCP) - ZEROCNK;
}

static bool layer_exact_indices(const Env& e, double pbot, double ptop, long& i1, long& i2) {
    i1 = -1; i2 = -1;
    for (size_t i = 0; i < e.pres.size(); ++i) if (pbot > e.pres[i]) { i1 = (long)i; break; }
    for (size_t i = 0; i < e.pres.size(); ++i) if (ptop < e.pres[i]) i2 = (long)i;
    return i1 >= 0 && i2 >= 0;
}
double mean_theta_exact(const Env& e, double pbot, double ptop) {
    if (isn(e.temp_at(pbot))) pbot = e.sfc_pres;
    if (isn(e.temp_at(ptop))) return NaN;
    long i1, i2; if (!layer_exact_indices(e, pbot, ptop, i1, i2)) return NaN;
    double t1 = theta(pbot, e.temp_at(pbot)), t2 = theta(ptop, e.temp_at(ptop));
    double sum = 0; long m = 0;
    for (long i = i1; i <= i2; ++i) { double th = theta(e.pres[i], e.tmpc[i]); if (isn(th)) continue; sum += th; ++m; }
    double tott = (t1 + t2 + 2 * sum) / 2.;       // concatenate [t1, th, th, t2].sum()/2
    double num = (2.0 + 2.0 * m) / 2.;
    return tott / num;
}
double mean_mixratio_exact(const Env& e, double pbot, double ptop) {
    if (isn(e.temp_at(pbot))) pbot = e.sfc_pres;
    if (isn(e.temp_at(ptop))) return NaN;
    long i1, i2; if (!layer_exact_indices(e, pbot, ptop, i1, i2)) return NaN;
    double d1 = e.dwpt_at(pbot), d2 = e.dwpt_at(ptop);
    double sd = 0, sp = 0; long m = 0;
    for (long i = i1; i <= i2; ++i) { if (isn(e.dwpc[i]) || isn(e.pres[i])) continue; sd += e.dwpc[i]; sp += e.pres[i]; ++m; }
    double totd = (d1 + d2 + 2 * sd) / 2., totp = (pbot + ptop + 2 * sp) / 2.;
    double num = (2.0 + 2.0 * m) / 2.;
    return mixratio(totp / num, totd / num);
}
double most_unstable_level(const Env& e, double pbot, double ptop) {
    if (isn(e.temp_at(pbot))) pbot = e.sfc_pres;
    if (isn(e.temp_at(ptop))) return NaN;
    std::vector<double> p, mt;
    for (double q = pbot; q > ptop - 1.0; q -= 1.0) {
        double t = e.temp_at(q), d = e.dwpt_at(q);
        p.push_back(q);
        if (isn(t) || isn(d)) { mt.push_back(NaN); continue; }
        double p2, t2; drylift(q, t, d, p2, t2);
        mt.push_back(wetlift(p2, t2, 1000.));
    }
    double mx = -1e300; for (double v : mt) if (!isn(v) && v > mx) mx = v;
    for (size_t i = 0; i < mt.size(); ++i) if (!isn(mt[i]) && std::fabs(mt[i] - mx) < 1e-10) return p[i];
    return NaN;
}

LPL define_sfc(const Env& e) { return {e.pres[e.sfc], e.tmpc[e.sfc], e.dwpc[e.sfc]}; }
LPL define_fcst(const Env& e) {
    LPL l; l.tmpc = max_temp(e); l.pres = e.sfc_pres;
    l.dwpc = temp_at_mixrat(mean_mixratio_exact(e, l.pres, l.pres - 100.), l.pres);
    return l;
}
LPL define_mu(const Env& e, double presval) {
    LPL l; double pbot = e.sfc_pres, ptop = pbot - presval;
    l.pres = most_unstable_level(e, pbot, ptop);
    l.tmpc = e.temp_at(l.pres); l.dwpc = e.dwpt_at(l.pres);
    return l;
}
LPL define_ml(const Env& e, double presval, double pbot) {
    LPL l; if (isn(pbot)) pbot = e.sfc_pres;
    double ptop = pbot - presval; l.pres = pbot;
    l.tmpc = theta(1000., mean_theta_exact(e, pbot, ptop), l.pres);
    l.dwpc = temp_at_mixrat(mean_mixratio_exact(e, pbot, ptop), l.pres);
    return l;
}

// ======================= cape / parcelx =======================
static double safe_div(double a, double b) {
    if (b == 0.0) return a > 0 ? INFINITY : (a < 0 ? -INFINITY : NaN);
    return a / b;
}

void cape_only(const Env& e, double pres, double tmpc, double dwpc, double pbot, double ptop, bool trunc,
               double& bplus, double& bminus) {
    bplus = NaN; bminus = NaN;
    if (isn(pbot)) pbot = e.sfc_pres;
    if (isn(ptop)) ptop = e.pres.back();
    if (pbot > pres) pbot = pres;
    if (isn(e.vtmp_at(pbot)) || isn(e.vtmp_at(ptop))) return;
    if (isn(pres) || isn(tmpc) || isn(dwpc)) return;
    const size_t n = e.pres.size();
    double pe2, tp2; drylift(pres, tmpc, dwpc, pe2, tp2);
    if (isn(pe2)) return;
    double blupper = pe2;
    double theta_parcel = theta(pe2, tp2, 1000.);
    double blmr = mixratio(pres, dwpc);

    double totn = 0.0, prev_h = 0, prev_tdef = 0; bool have_prev = false;
    for (double p = pbot; p > blupper - 1.0; p -= 1.0) {
        double hh = e.hght_at(p), t = e.temp_at(p), d = e.dwpt_at(p);
        if (!isn(hh) && !isn(t) && !isn(d)) {
            double th_env = theta(p, t, 1000.);
            double tv_env = virtemp(p, th_env, d);
            double tmp1 = virtemp(p, theta_parcel, temp_at_mixrat(blmr, p));
            double tdef = safe_div(tmp1 - tv_env, ctok(tv_env));
            if (have_prev) { double lyre = G * (prev_tdef + tdef) / 2. * (hh - prev_h); if (lyre < 0) totn += lyre; }
            prev_h = hh; prev_tdef = tdef; have_prev = true;
        } else have_prev = false;
    }
    if (pbot > pe2) pbot = pe2;
    if (pbot < ptop) return;

    size_t lptr = 0; while (lptr < n && !(e.pres[lptr] < pbot)) ++lptr;
    size_t uptr = 0; for (size_t i = 0; i < n; ++i) if (e.pres[i] > ptop) uptr = i;

    double pe1 = pbot;
    double h1 = e.hght_at(pe1), te1 = e.vtmp_at(pe1), tp1 = tp2;
    double thta_lcl = theta(pe1, tp1, 1000.);
    double thetam = thta_lcl - wobf(thta_lcl) + wobf(tp1);

    double totp = 0, lyre = 0, totn2 = 0; bool truncated = false;
    for (size_t i = lptr; i < n; ++i) {
        if (isn(e.tmpc[i])) continue;
        double pe2i = e.pres[i], h2 = e.hght[i], te2 = e.vtmp[i];
        double tpi = satlift(pe2i, thetam);
        double tdef1 = (virtemp(pe1, tp1, tp1) - te1) / (te1 + ZEROCNK);
        double tdef2 = (virtemp(pe2i, tpi, tpi) - te2) / (te2 + ZEROCNK);
        lyre = G * (tdef1 + tdef2) / 2. * (h2 - h1);
        if (lyre > 0) totp += lyre; else if (pe2i > 500.) totn2 += lyre;
        pe1 = pe2i; h1 = h2; te1 = te2; tp1 = tpi;
        if ((trunc && pe2i <= 500.) || (i >= uptr)) { truncated = true; break; }
    }
    totn += totn2;
    if (!truncated) return;
    double bp, bm;
    if (lyre > 0) { bp = totp - lyre; bm = totn; }
    else { bp = totp; bm = (pe1 > 500.) ? totn + lyre : totn; }
    double pe3 = pe1, h3 = h1, te3 = te1, tp3 = tp1, pe2c = ptop;
    if (pe3 != pe2c) {
        double h2c = e.hght_at(pe2c), te2c = e.vtmp_at(pe2c), tp2c = wetlift(pe3, tp3, pe2c);
        double tdef3 = safe_div(virtemp(pe3, tp3, tp3) - te3, ctok(te3));
        double tdef2 = safe_div(virtemp(pe2c, tp2c, tp2c) - te2c, ctok(te2c));
        double lyrf2 = G * (tdef3 + tdef2) / 2. * (h2c - h3);
        if (lyrf2 > 0) bp += lyrf2; else if (pe2c > 500.) bm += lyrf2;
    }
    if (bp == 0) bm = 0.;
    bplus = bp; bminus = bm;
}

Parcel parcelx(const Env& e, double pres, double tmpc, double dwpc, double pbot, double ptop) {
    Parcel pcl; pcl.pres = pres; pcl.tmpc = tmpc; pcl.dwpc = dwpc;
    const size_t n = e.pres.size();
    if (n < 2 || isn(pres) || isn(tmpc) || isn(dwpc)) return pcl;
    double cap_strength = -9999., cap_strengthpres = -9999., li_max = -9999., li_maxpres = -9999.;
    double totp = 0, totn = 0, tote = 0;
    if (isn(pbot)) { pbot = e.sfc_pres; pcl.blayer = pbot; }
    if (isn(ptop)) { ptop = e.pres[n - 1]; pcl.tlayer = ptop; }
    if (pbot > pres) { pbot = pres; pcl.blayer = pbot; }

    double pe1 = pbot;
    double h1 = e.hght_at(pe1); (void)h1;
    double tp1 = virtemp(pres, tmpc, dwpc);
    std::vector<double> ttrace{tp1}, ptrace{pe1};
    double pe2, tp2; drylift(pres, tmpc, dwpc, pe2, tp2);
    if (isn(pe2)) return pcl;
    double blupper = pe2;
    double h2 = e.hght_at(pe2);
    pcl.lclpres = std::min(pe2, e.sfc_pres);
    pcl.lclhght = e.to_agl(h2);
    pcl.lcltmp = tp2;
    ptrace.push_back(pe2); ttrace.push_back(virtemp(pe2, tp2, tp2));

    double theta_parcel = theta(pe2, tp2, 1000.);
    double blmr = mixratio(pres, dwpc);
    {   // CINH in the mixing layer
        std::vector<double> hh, tdef;
        for (double p = pbot; p > blupper - 1.0; p -= 1.0) {
            double h = e.hght_at(p), t = e.temp_at(p), d = e.dwpt_at(p);
            double th_env = theta(p, t, 1000.);
            double tv_env = virtemp(p, th_env, d);
            double tmp1 = virtemp(p, theta_parcel, temp_at_mixrat(blmr, p));
            hh.push_back(h); tdef.push_back((tmp1 - tv_env) / ctok(tv_env));
        }
        totn = 0;
        for (size_t i = 0; i + 1 < tdef.size(); ++i) {
            double lyre = G * (tdef[i] + tdef[i + 1]) / 2 * (hh[i + 1] - hh[i]);
            if (lyre < 0) totn += lyre;
        }
    }
    if (pbot > pe2) { pbot = pe2; pcl.blayer = pbot; }

    double p0c = temp_lvl(e, 0.), pm10c = temp_lvl(e, -10.), pm20c = temp_lvl(e, -20.), pm30c = temp_lvl(e, -30.);
    double hgt0c = e.hght_at(p0c), hgtm10c = e.hght_at(pm10c), hgtm20c = e.hght_at(pm20c), hgtm30c = e.hght_at(pm30c);
    pcl.p0c = p0c; pcl.pm10c = pm10c; pcl.pm20c = pm20c; pcl.pm30c = pm30c;
    pcl.hght0c = hgt0c; pcl.hghtm10c = hgtm10c; pcl.hghtm20c = hgtm20c; pcl.hghtm30c = hgtm30c;
    if (pbot < e.pres[n - 1]) return pcl;      // LCL above the data

    size_t lptr = n; for (size_t i = 0; i < n; ++i) if (pbot >= e.pres[i]) { lptr = i; break; }
    if (lptr == n) return pcl;
    size_t uptr = 0; for (size_t i = 0; i < n; ++i) if (ptop <= e.pres[i]) uptr = i;

    pe1 = pbot; h1 = e.hght_at(pe1);
    double te1 = e.vtmp_at(pe1);
    tp1 = wetlift(pe2, tp2, pe1);
    double thta_lcl_px = theta(pe1, tp1, 1000.);
    double thetam_px = thta_lcl_px - wobf(thta_lcl_px) + wobf(tp1);
    std::vector<double> tp_pre(n, NaN);
    for (size_t i = lptr; i < n; ++i) tp_pre[i] = isn(e.pres[i]) ? NaN : satlift(e.pres[i], thetam_px);
    double vtp1 = virtemp(pe1, tp1, tp1);
    const double sfc_hght = e.sfc_hght;
    double h_3km = e.to_msl(3000.), pe_3km = e.pres_at_hght(h_3km);
    double h_6km = e.to_msl(6000.), pe_6km = e.pres_at_hght(h_6km);

    // ---------------- parcelx_main_loop ----------------
    const double lclhght = pcl.lclhght, lclpres = pcl.lclpres;
    double bplus = NaN, bminus = NaN, bfzl = NaN, wm10c = NaN, wm20c = NaN, wm30c = NaN;
    double b3km = lclhght < 3000. ? NaN : 0., b6km = lclhght < 6000. ? NaN : 0.;
    double lfcpres = NaN, lfchght = NaN, elpres = NaN, elhght = NaN, mplpres = NaN, mplhght = NaN;
    double cap = NaN, cappres = NaN, limax = NaN, limaxpres = NaN, li5 = NaN, li3 = NaN;
    double lyre = 0.0, lyrlast = 0.0, pelast = pe1, level_pres_i = pe1;
    std::vector<double> ptraces(n - lptr, NaN), ttraces(n - lptr, NaN);
    auto LH = [&](double p) { return e.hght_at(p); };
    auto LV = [&](double p) { return e.vtmp_at(p); };

    // helper for the layer-crossing blocks (freezing, -10/-20/-30, 3 km, 6 km)
    for (size_t i = lptr; i < n; ++i) {
        double tmpc_i = e.tmpc[i];
        if (isn(tmpc_i)) continue;
        double pe2i = e.pres[i]; level_pres_i = pe2i;
        double h2i = e.hght[i], te2 = e.vtmp[i], tp2i = tp_pre[i];
        double vtp2 = virtemp(pe2i, tp2i, tp2i);
        double tdef1 = safe_div(vtp1 - te1, ctok(te1));
        double tdef2 = safe_div(vtp2 - te2, ctok(te2));
        ptraces[i - lptr] = pe2i; ttraces[i - lptr] = vtp2;
        lyrlast = lyre;
        lyre = G * (tdef1 + tdef2) / 2. * (h2i - h1);
        if (lyre > 0) totp += lyre; else if (pe2i > 500.) totn += lyre;
        double mli = vtp2 - te2;
        if (mli > li_max) { li_max = mli; li_maxpres = pe2i; }
        double mcap = te2 - mli;
        if (mcap > cap_strength) { cap_strength = mcap; cap_strengthpres = pe2i; }
        tote += lyre;
        pelast = pe1; pe1 = pe2i; te1 = te2; tp1 = tp2i; vtp1 = vtp2;

        if (i >= uptr && isn(bplus)) {
            double lyrf = lyre;
            if (lyrf > 0) { bplus = totp - lyrf; bminus = totn; }
            else { bplus = totp; bminus = (pe2i > 500.) ? totn + lyrf : totn; }
        }

        auto crossing = [&](double& out, double lim, double pthr, double hgt, bool useLcl) {
            double pe3_ = pelast;
            double h3 = LH(pe3_), te3 = LV(pe3_), tp3 = wetlift(pe1, tp1, pe3_);
            double lyrf = lyre;
            out = (lyrf > 0.) ? totp - lyrf : totp;
            bool bad = isn(pthr) || (useLcl ? pthr > lclpres : pthr > pe3_);
            if (bad) out = 0.;
            else {
                double te2b = LV(pe2i), tp2b = wetlift(pe3_, tp3, pe2i);
                double tdef3 = safe_div(virtemp(pe3_, tp3, tp3) - te3, ctok(te3));
                double tdef2b = safe_div(virtemp(pe2i, tp2b, tp2b) - te2b, ctok(te2b));
                double lyrf2 = G * (tdef3 + tdef2b) / 2. * (hgt - h3);
                if (lyrf2 > 0) out += lyrf2;
            }
            (void)lim;
        };
        if (te2 < 0. && isn(bfzl)) crossing(bfzl, 0, p0c, hgt0c, false);
        if (te2 < -10. && isn(wm10c)) crossing(wm10c, 0, pm10c, hgtm10c, true);
        if (te2 < -20. && isn(wm20c)) crossing(wm20c, 0, pm20c, hgtm20c, true);
        if (te2 < -30. && isn(wm30c)) crossing(wm30c, 0, pm30c, hgtm30c, true);

        auto kmlevel = [&](double& out, double km, double pe_km, double h_km) {
            double pe3_ = pelast;
            double h3 = LH(pe3_), te3 = LV(pe3_), tp3 = wetlift(pe1, tp1, pe3_);
            double lyrf = lyre;
            out = (lyrf > 0) ? totp - lyrf : totp;
            double te2b = LV(pe_km), tp2b = wetlift(pe3_, tp3, pe_km);
            double tdef3 = safe_div(virtemp(pe3_, tp3, tp3) - te3, ctok(te3));
            double tdef2b = safe_div(virtemp(pe_km, tp2b, tp2b) - te2b, ctok(te2b));
            double lyrf2 = G * (tdef3 + tdef2b) / 2. * (h_km - h3);
            if (lyrf2 > 0) out += lyrf2;
            (void)km;
        };
        if (lclhght < 3000.) {
            if (isn(b3km) && (h1 - sfc_hght) <= 3000. && (h2i - sfc_hght) >= 3000.) kmlevel(b3km, 3000., pe_3km, h_3km);
        } else b3km = 0.;
        if (lclhght < 6000.) {
            if (isn(b6km) && (h1 - sfc_hght) <= 6000. && (h2i - sfc_hght) >= 6000.) kmlevel(b6km, 6000., pe_6km, h_6km);
        } else b6km = 0.;

        h1 = h2i;

        // LFC
        if (lyre >= 0. && lyrlast <= 0.) {
            double tp3 = tp1, pe2_ = pe1, pe3_ = pelast;
            double wl = wetlift(pe2_, tp3, pe3_);
            if (LV(pe3_) < virtemp(pe3_, wl, wl)) {
                lfcpres = pe3_; lfchght = LH(pe3_) - sfc_hght;
                elpres = NaN; elhght = NaN; mplpres = NaN;
            } else {
                while (true) {
                    double wl2 = wetlift(pe2_, tp3, pe3_);
                    if (!(LV(pe3_) > virtemp(pe3_, wl2, wl2) && pe3_ > 0)) break;
                    pe3_ -= 5;
                }
                if (pe3_ > 0) {
                    lfcpres = pe3_; lfchght = LH(pe3_) - sfc_hght;
                    tote = 0.; li_max = -9999.;
                    if (cap_strength < 0.) cap_strength = 0.;
                    cap = cap_strength; cappres = cap_strengthpres;
                    elpres = NaN; elhght = NaN; mplpres = NaN;
                }
            }
            if (!isn(lfcpres) && lfcpres >= lclpres) { lfcpres = lclpres; lfchght = lclhght; }
        }
        // EL
        if (lyre <= 0. && lyrlast >= 0.) {
            double tp3 = tp1, pe2_ = pe1, pe3_ = pelast;
            while (true) {
                double wl3 = wetlift(pe2_, tp3, pe3_);
                if (!(LV(pe3_) < virtemp(pe3_, wl3, wl3))) break;
                pe3_ -= 5;
            }
            elpres = pe3_; elhght = LH(elpres) - sfc_hght;
            mplpres = NaN; limax = -li_max; limaxpres = li_maxpres;
        }
        // MPL
        if (tote < 0. && isn(mplpres) && !isn(elpres)) {
            double pe3_ = pelast, h3 = LH(pe3_), te3 = LV(pe3_), tp3 = wetlift(pe1, tp1, pe3_);
            double totx = tote - lyre, pe2_ = pelast;
            while (totx > 0) {
                pe2_ -= 1;
                double te2b = LV(pe2_), tp2b = wetlift(pe3_, tp3, pe2_), h2b = LH(pe2_);
                double tdef3 = safe_div(virtemp(pe3_, tp3, tp3) - te3, ctok(te3));
                double tdef2b = safe_div(virtemp(pe2_, tp2b, tp2b) - te2b, ctok(te2b));
                double lyrf = G * (tdef3 + tdef2b) / 2. * (h2b - h3);
                totx += lyrf; tp3 = tp2b; te3 = te2b; pe3_ = pe2_;
            }
            mplpres = pe2_; mplhght = LH(pe2_) - sfc_hght;
        }
        if (level_pres_i <= 500. && isn(li5)) { double a = LV(500.), b_ = wetlift(pe1, tp1, 500.); li5 = a - virtemp(500., b_, b_); }
        if (level_pres_i <= 300. && isn(li3)) { double a = LV(300.), b_ = wetlift(pe1, tp1, 300.); li3 = a - virtemp(300., b_, b_); }
    }

    pcl.bplus = !isn(bplus) ? bplus : totp;
    pcl.bminus = bminus; pcl.bfzl = bfzl; pcl.wm10c = wm10c; pcl.wm20c = wm20c; pcl.wm30c = wm30c;
    pcl.b3km = b3km; pcl.b6km = b6km;
    pcl.lfcpres = lfcpres; pcl.lfchght = lfchght; pcl.elpres = elpres; pcl.elhght = elhght;
    pcl.mplpres = mplpres; pcl.mplhght = mplhght; pcl.cap = cap; pcl.cappres = cappres;
    pcl.limax = limax; pcl.limaxpres = limaxpres; pcl.li5 = li5; pcl.li3 = li3;
    if (std::floor(pcl.bplus) == 0) pcl.bminus = 0.;
    for (size_t i = 0; i < ptraces.size(); ++i) if (!isn(ptraces[i])) { ptrace.push_back(ptraces[i]); ttrace.push_back(ttraces[i]); }
    pcl.ptrace = ptrace; pcl.ttrace = ttrace;
    double bm = NaN; size_t bi = 0; bool any = false;
    for (size_t i = 0; i < ptrace.size(); ++i) if (ptrace[i] >= 500.) {
        double v = e.vtmp_at(ptrace[i]); if (isn(v)) continue;
        double b = ttrace[i] - v;
        if (!any || b < bm) { bm = b; bi = i; any = true; }
    }
    if (any) { pcl.bmin = bm; pcl.bminpres = ptrace[bi]; }
    return pcl;
}


void effective_inflow(const Env& e, double mucape, double mucinh, double& pbot, double& ptop, double ecape, double ecinh) {
    pbot = NaN; ptop = NaN;
    if (isn(mucape) || mucape == 0 || !(mucape >= ecape && mucinh > ecinh)) return;
    const size_t n = e.pres.size();
    long top = -1; for (size_t i = 0; i < n; ++i) if (!isn(e.tmpc[i])) top = (long)i;
    if (top < 0) return;
    const double ptop_val = e.pres[n - 1];
    auto co = [&](size_t i, double& bp, double& bm) {
        if (isn(e.tmpc[i]) || isn(e.dwpc[i])) { bp = bm = NaN; return; }
        cape_only(e, e.pres[i], e.tmpc[i], e.dwpc[i], e.sfc_pres, ptop_val, false, bp, bm);
    };
    long ib = -1;
    for (long i = (long)e.sfc; i < top; ++i) {
        double bp, bm; co((size_t)i, bp, bm);
        if (bp >= ecape && bm > ecinh) { pbot = e.pres[i]; ib = i; break; }
    }
    if (ib < 0) { pbot = NaN; return; }
    for (long i = ib + 1; i < top; ++i) {
        double d = e.dwpc[i], t = e.tmpc[i];
        if (d == 0. || isn(d) || t == 0. || isn(t)) continue;
        double bp, bm; co((size_t)i, bp, bm);
        if (bp < ecape || bm <= ecinh) {
            long j = 1;
            while (i - j > 0 && isn(e.dwpc[i - j]) && isn(e.tmpc[i - j])) ++j;
            ptop = e.pres[i - j];
            if (ptop > pbot) ptop = pbot;
            break;
        }
    }
}

double dcape(const Env& e, std::vector<double>* ptr_out, std::vector<double>* ttr_out) {
    const size_t n = e.pres.size();
    std::vector<double> pres, hght, dwpc, tmpc, the;
    for (size_t i = 0; i < n; ++i) {
        if (isn(e.pres[i]) || isn(e.tmpc[i]) || isn(e.dwpc[i])) continue;
        double te = thetae(e.pres[i], e.tmpc[i], e.dwpc[i]);
        if (isn(te)) continue;
        pres.push_back(e.pres[i]); hght.push_back(e.hght[i]); dwpc.push_back(e.dwpc[i]); tmpc.push_back(e.tmpc[i]); the.push_back(te);
    }
    if (pres.size() < 2) return NaN;
    std::vector<double> lp, tv;     // ascending log p
    for (long i = (long)pres.size() - 1; i >= 0; --i) { lp.push_back(std::log10(pres[i])); tv.push_back(the[i]); }
    double mine = 1000., minp = -999.;
    for (size_t k = 0; k < pres.size(); ++k) {
        if (!(pres[k] >= e.sfc_pres - 400.)) continue;
        double pb = pres[k], pt = pb - 100., nw = 0, dw = 0; bool valid = true;
        for (double p = pb; p > pt - 1.0; p -= 1.0) {
            double te = interp1(std::log10(p), lp, tv);
            if (isn(te)) { valid = false; break; }
            nw += te * p; dw += p;
        }
        if (valid && dw > 0) { double m = nw / dw; if (m < mine) { minp = pb - 50.; mine = m; } }
    }
    double upper = minp;
    long uptr = -1; for (size_t i = 0; i < pres.size(); ++i) if (pres[i] >= upper) uptr = (long)i;
    if (uptr < 0) return NaN;
    double tp1 = wetbulb(upper, e.temp_at(upper), e.dwpt_at(upper));
    double pe1 = upper, te1 = e.temp_at(pe1), h1 = e.hght_at(pe1), tote = 0, lyre = 0;
    double thta = theta(pe1, tp1, 1000.), thetam = thta - wobf(thta) + wobf(tp1);
    std::vector<double> tt{tp1}, pp{upper}, tts, pps;
    for (long i = uptr; i >= 0; --i) {
        double pe2 = pres[i], te2 = tmpc[i], h2 = hght[i], tp2 = satlift(pe2, thetam);
        if (!isn(te1) && !isn(te2)) {
            double tdef1 = (tp1 - te1) / ctok(te1), tdef2 = (tp2 - te2) / ctok(te2);
            lyre = 9.8 * (tdef1 + tdef2) / 2.0 * (h2 - h1);
            tote += lyre;
        }
        tts.push_back(tp2); pps.push_back(pe2);
        pe1 = pe2; te1 = te2; h1 = h2; tp1 = tp2;
    }
    if (ptr_out) { *ptr_out = pp; ptr_out->insert(ptr_out->end(), pps.rbegin(), pps.rend()); }
    if (ttr_out) { *ttr_out = tt; ttr_out->insert(ttr_out->end(), tts.rbegin(), tts.rend()); }
    return tote;
}

}} // namespace sharppy::tab
