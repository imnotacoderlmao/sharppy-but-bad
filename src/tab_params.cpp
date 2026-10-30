#include "sharppy/tab_params.hpp"
#include "sharppy/tab.hpp"
#include "sharppy/winds.hpp"
#include "sharppy/pwv.hpp"
#include <algorithm>
#include <cmath>
#include <cmath>

namespace sharppy { namespace tab {

namespace {
constexpr double KTS2MS = 0.514444;
bool nn(double v) { return !std::isfinite(v); }
double ctof(double c) { return c * 9. / 5. + 32.; }

// thermo.relh
double relh(double p, double t, double td) { return 100. * mixratio(p, td) / mixratio(p, t); }

// np.arange(pbot, ptop+dp, dp), dp = -1
long arangeCount(double pbot, double ptop) { return (long)std::ceil((pbot - (ptop - 1.0)) / 1.0); }

double mean_mixratio_default(const Env& e, double pbot, double ptop) {   // params.mean_mixratio(exact=False)
    if (nn(e.temp_at(pbot))) pbot = e.sfc_pres;
    if (nn(e.temp_at(ptop))) return NaN;
    double s = 0; long n = 0;
    for (long k = 0, c = arangeCount(pbot, ptop); k < c; ++k) {
        double p = pbot - k, d = e.dwpt_at(p);
        if (nn(d)) continue;
        s += mixratio(p, d); ++n;
    }
    return n ? s / n : NaN;
}

double mean_relh_default(const Env& e, double pbot, double ptop) {       // params.mean_relh(exact=False)
    if (nn(e.temp_at(pbot))) pbot = e.sfc_pres;
    if (nn(e.temp_at(ptop))) return NaN;
    double sw = 0, s = 0;
    for (long k = 0, c = arangeCount(pbot, ptop); k < c; ++k) {
        double p = pbot - k, t = e.temp_at(p), d = e.dwpt_at(p);
        if (nn(t) || nn(d)) continue;
        s += relh(p, t, d) * p; sw += p;
    }
    return sw > 0 ? s / sw : NaN;
}

double convective_temp(const Env& e) {                                    // params.convective_temp (mincinh = 0)
    const double mincinh = 0.;
    double pres = e.sfc_pres, tmpc = e.tmpc[e.sfc];
    double mmr = mean_mixratio_default(e, e.sfc_pres, e.sfc_pres - 100.);
    double dwpc = temp_at_mixrat(mmr, pres);
    const double ptop = e.pres.back();
    double bplus, bminus;
    cape_only(e, pres, tmpc + 25., dwpc, e.sfc_pres, ptop, true, bplus, bminus);
    if (bplus == 0. || nn(bminus) || bminus < mincinh) return NaN;
    double excess = dwpc - tmpc;
    if (excess > 0) tmpc = tmpc + excess + 4.;
    cape_only(e, pres, tmpc, dwpc, e.sfc_pres, ptop, true, bplus, bminus);
    if (bplus == 0. || nn(bminus)) bminus = NaN;
    int guard = 0;
    while ((nn(bminus) || bminus < mincinh) && guard++ < 400) {
        if (bminus < -100) tmpc += 2.; else tmpc += 0.5;
        cape_only(e, pres, tmpc, dwpc, e.sfc_pres, ptop, true, bplus, bminus);
        if (bplus == 0.) bminus = NaN;
    }
    return guard >= 400 ? NaN : tmpc;
}
}

double lapse_rate_exact(const Profile& prof, double lower, double upper, bool pres) {
    if (prof.size() < 2) return NaN;
    const auto e_hold_ = env_for(prof); const Env& e = *e_hold_;
    double p1, p2, z1, z2;
    if (pres) {
        if (e.pres.back() > upper) return NaN;
        p1 = lower; p2 = upper; z1 = e.hght_at(lower); z2 = e.hght_at(upper);
    } else {
        z1 = e.to_msl(lower); z2 = e.to_msl(upper);
        p1 = e.pres_at_hght(z1); p2 = e.pres_at_hght(z2);
    }
    double tv1 = e.vtmp_at(p1), tv2 = e.vtmp_at(p2);
    return (tv2 - tv1) / (z2 - z1) * -1000.;
}

ThermoIndices compute_thermo_indices(const Profile& prof, int pwv_flag) {
    ThermoIndices o;
    o.pwv_flag = pwv_flag;
    if (prof.size() < 2) return o;
    const auto e_hold_ = env_for(prof); const Env& e = *e_hold_;
    const double sfc = e.sfc_pres;

    LPL ls = define_sfc(e), lm = define_ml(e), lu = define_mu(e, 300.);
    Parcel sb = parcelx(e, ls.pres, ls.tmpc, ls.dwpc);
    Parcel ml = parcelx(e, lm.pres, lm.tmpc, lm.dwpc);
    Parcel mu = parcelx(e, lu.pres, lu.tmpc, lu.dwpc);
    o.mlb3km = ml.b3km; o.mlbplus = ml.bplus;

    const double t8 = e.temp_at(850.), t7 = e.temp_at(700.), t5 = e.temp_at(500.);
    const double td7 = e.dwpt_at(700.), td8 = e.dwpt_at(850.);
    o.k_idx = t8 - t5 + td8 - (t7 - td7);
    o.tt = (td8 - t5) + (t8 - t5);                                  // c_totals + v_totals
    o.pwat = sharppy::precip_water(prof);
    o.lr3 = lapse_rate_exact(prof, 0., 3000., false);
    o.lr36 = lapse_rate_exact(prof, 3000., 6000., false);
    o.lr850_500 = lapse_rate_exact(prof, 850., 500., true);
    o.lr700_500 = lapse_rate_exact(prof, 700., 500., true);
    o.convT_F = ctof(convective_temp(e));
    o.maxT_F = ctof(max_temp(e));
    o.mean_mixr = mean_mixratio_default(e, sfc, sfc - 100.);
    o.low_rh = mean_relh_default(e, sfc, sfc - 100.);
    o.mid_rh = mean_relh_default(e, sfc - 150., sfc - 350.);

    std::vector<double> pt, tt;
    double dc = dcape(e, &pt, &tt);
    o.dcape = dc;
    o.drush_F = tt.empty() ? NaN : ctof(tt.back());

    // thetae profile (K)
    std::vector<double> the(e.pres.size());
    for (size_t i = 0; i < e.pres.size(); ++i) the[i] = thetae(e.pres[i], e.tmpc[i], e.dwpc[i]) + ZEROCNK;

    // tei
    {
        double top = sfc - 400., mn = NaN, mx = NaN;
        for (size_t i = 0; i < e.pres.size(); ++i) {
            if (!(e.pres[i] >= top) || nn(the[i])) continue;
            mn = nn(mn) ? the[i] : std::min(mn, the[i]); mx = nn(mx) ? the[i] : std::max(mx, the[i]);
        }
        o.tei = mx - mn;
    }

    // sig_severe
    {
        WindVector sh = wind_shear_pres(prof, prof.pres[0], prof.pres_at_hght(prof.to_msl(6000.)));
        o.sig_severe = ml.bplus * (sh.wspd * KTS2MS);
    }

    // esp
    if (nn(o.lr3) || o.lr3 < 7. || ml.bplus < 250.) o.esp = 0;
    else o.esp = (ml.b3km / 50.) * ((o.lr3 - 7.0) / 1.0);

    // wndg
    {
        double lr03 = o.lr3;
        WindVector mw = mean_wind_pres(prof, prof.pres_at_hght(prof.to_msl(1000.)), prof.pres_at_hght(prof.to_msl(3500.)), true);
        double mws = mw.wspd * KTS2MS, mlcin = ml.bminus;
        if (lr03 < 7) lr03 = 0.;
        if (mlcin < -50) mlcin = -50.;
        o.wndg = (ml.bplus / 2000.) * (lr03 / 9.) * (mws / 15.) * ((50. + mlcin) / 40.);
    }

    // mmp
    {
        if (mu.bplus < 100.) o.mmp = 0.;
        else {
            std::vector<double> bu, bv, tu, tv;
            for (size_t i = 0; i < e.pres.size(); ++i) {
                double agl = e.to_agl(e.hght[i]);
                double p = e.pres_at_hght(e.hght[i]), u, v;
                if (agl <= 1000) { prof.wind_at_pres(p, u, v); bu.push_back(u); bv.push_back(v); }
                if (agl >= 6000 && agl < 10000) { prof.wind_at_pres(p, u, v); tu.push_back(u); tv.push_back(v); }
            }
            if (bu.empty() || tu.empty()) o.mmp = NaN;
            else {
                double mx = NaN;
                for (size_t b = 0; b < bu.size(); ++b)
                    for (size_t t = 0; t < tu.size(); ++t) {
                        if (!(b >= t)) continue;
                        double m = std::hypot(tu[t] - bu[b], tv[t] - bv[b]);
                        if (!nn(m) && (nn(mx) || m > mx)) mx = m;
                    }
                double maxShear = mx * KTS2MS;
                double lr38 = lapse_rate_exact(prof, 3000., 8000., false);
                WindVector mw = mean_wind_pres(prof, prof.pres_at_hght(prof.to_msl(3000.)), prof.pres_at_hght(prof.to_msl(12000.)), true);
                double mw312 = mw.wspd * KTS2MS;
                const double a0 = 13.0, a1 = -4.59e-2, a2 = -1.16, a3 = -6.17e-4, a4 = -0.17;
                o.mmp = 1. / (1. + std::exp(a0 + a1 * maxShear + a2 * lr38 + a3 * mu.bplus + a4 * mw312));
            }
        }
    }

    // mburst
    {
        double vt = t8 - t5;
        double te = (thetae(ls.pres, ls.tmpc, ls.dwpc) + ZEROCNK >= 355) ? 1 : 0;
        double sbc = NaN, sbli = NaN, pwt = NaN, dct = NaN, lrt = NaN, vtt = NaN, ted = NaN;
        if (!nn(sb.bplus)) {
            sbc = -5; if (sb.bplus >= 2000) sbc = 0; if (sb.bplus >= 3300) sbc = 1;
            if (sb.bplus >= 3700) sbc = 2; if (sb.bplus >= 4300) sbc = 4;
        }
        if (!nn(sb.li5)) {
            sbli = 0; if (sb.li5 <= -7.5) sbli = 1; if (sb.li5 <= -9.0) sbli = 2; if (sb.li5 <= -10.0) sbli = 3;
        }
        if (!nn(o.pwat)) pwt = o.pwat < 1.5 ? -3 : 0;
        if (!nn(dc)) dct = (o.pwat > 1.70 && dc > 900) ? 1 : 0;
        if (!nn(o.lr3)) lrt = o.lr3 <= 8.4 ? 0 : 1;
        if (!nn(vt)) vtt = vt < 27 ? 0 : vt < 28 ? 1 : vt < 29 ? 2 : 3;
        // params.thetae_diff
        double tdiff = NaN;
        {
            long imax = -1, imin = -1;
            for (size_t i = 0; i < e.pres.size(); ++i) {
                if (!(e.to_agl(e.hght[i]) <= 3000) || nn(the[i])) continue;
                if (imax < 0 || the[i] > the[imax]) imax = (long)i;
                if (imin < 0 || the[i] < the[imin]) imin = (long)i;
            }
            if (imax >= 0) tdiff = (e.pres[imax] < e.pres[imin]) ? 0. : the[imax] - the[imin];
        }
        if (!nn(tdiff)) ted = tdiff >= 35 ? 1 : 0;
        double mb = te + sbc + sbli + pwt + dct + lrt + vtt + ted;
        if (mb < 0) mb = 0;
        o.mburst = mb;                         // NaN stays NaN ("masked")
    }
    return o;
}


// ======================= fire.py / profile.get_fire =======================
namespace {
constexpr double kPi = 3.14159265358979323846;
bool isclose(double a, double b) { return std::fabs(a - b) <= 1e-8 + 1e-5 * std::fabs(b); }
void comp2vec(double u, double v, double& dir, double& spd) {          // utils.comp2vec
    if (nn(u) || nn(v)) { dir = spd = NaN; return; }
    dir = std::atan2(-u, -v) * 180.0 / kPi;
    if (dir < 0) dir += 360;
    if (std::fabs(dir) < 1e-10) dir = 0.;
    spd = std::sqrt(u * u + v * v);
}
double haines_term(const Env& e, double p1, double p2, double pd, bool dd_from_p2, double l1, double l2, double d1, double d2) {
    double t1 = e.temp_at(p1), t2 = e.temp_at(p2), td = e.dwpt_at(pd);
    if (nn(t1) || nn(t2) || nn(td)) return -9999.;
    double lapse = t1 - t2, dd = (dd_from_p2 ? t2 : t1) - td;
    int a = lapse < l1 ? 1 : (lapse <= l2 ? 2 : 3);
    int b = dd < d1 ? 1 : (dd <= d2 ? 2 : 3);
    return a + b;
}
}


static double pbl_top_pres(const Env& e) {                                   // params.pbl_top
    const size_t n = e.pres.size(), s = e.sfc;
    std::vector<double> tv(n);
    for (size_t i = 0; i < n; ++i) tv[i] = theta(e.pres[i], virtemp(e.pres[i], e.tmpc[i], e.dwpc[i]));
    size_t level = n - 1;
    for (size_t i = 0; i < n; ++i) if (tv[s] + .5 < tv[i]) { level = i; break; }
    return e.pres[level];
}

FireData compute_fire(const Profile& prof) {
    FireData f;
    if (prof.empty()) return f;
    const auto e_hold_ = env_for(prof); const Env& e = *e_hold_;
    const size_t s = e.sfc;
    // fire.fosberg
    {
        double tmpf = ctof(e.tmpc[s]);
        double fmph = prof.wspd[s] * 1.15078;
        double rh = relh(e.pres[s], e.tmpc[s], e.dwpc[s]);
        double em;
        if (rh <= 10) em = 0.03229 + 0.281073 * rh - 0.000578 * rh * tmpf;
        else if (10 < rh && rh <= 50) em = 2.22749 + 0.160107 * rh - 0.014784 * tmpf;
        else em = 21.0606 + 0.005565 * rh * rh - 0.00035 * rh * tmpf - 0.483199 * rh;
        double em30 = em / 30, u_sq = fmph * fmph;
        double fmdc = 1 - 2 * em30 + 1.5 * em30 * em30 - 0.5 * em30 * em30 * em30;
        f.fosberg = (fmdc * std::sqrt(1 + u_sq)) / 0.3002;
    }
    { double h = e.hght[s]; f.haines_hght = h < 305 ? 0 : (h <= 914 ? 1 : 2); }
    f.haines[0] = haines_term(e, 950, 850, 850, true, 4, 7, 6, 9);
    f.haines[1] = haines_term(e, 850, 700, 850, false, 6, 10, 6, 12);
    f.haines[2] = haines_term(e, 700, 500, 700, false, 18, 21, 15, 20);

    const double ppbl = pbl_top_pres(e);
    const double psfc = e.pres[s];
    f.sfc_rh = relh(psfc, e.tmpc[s], e.dwpc[s]);
    const double p1km = e.pres_at_hght(e.to_msl(1000.));
    f.pbl_h = e.to_agl(e.hght_at(ppbl));
    auto mrh = [&](double pbot, double ptop) { if (nn(ptop)) ptop = psfc - 100.; return mean_relh_default(e, pbot, ptop); };
    f.rh01km = mrh(psfc, p1km);
    f.pblrh = mrh(psfc, ppbl);
    auto mw = [&](double ptop, double& d, double& sp) {
        if (nn(ptop)) { d = sp = NaN; return; }
        WindVector w = mean_wind_pres(prof, psfc, ptop, true);
        comp2vec(w.u, w.v, d, sp);
    };
    mw(p1km, f.mw01_dir, f.mw01_spd);
    mw(ppbl, f.mwpbl_dir, f.mwpbl_spd);
    f.sfc_dir = prof.wdir[s]; f.sfc_spd = prof.wspd[s];

    // winds.max_wind(lower=0, upper=pbl_h)
    {
        double plower = e.pres_at_hght(e.to_msl(0.)), pupper = e.pres_at_hght(e.to_msl(f.pbl_h));
        if (!nn(plower) && !nn(pupper)) {
            const long n = (long)e.pres.size();
            long i1 = -1, i2 = -1;
            for (long i = 0; i < n; ++i) if (plower > e.pres[i] || isclose(plower, e.pres[i])) { i1 = i; break; }
            for (long i = 0; i < n; ++i) if (pupper < e.pres[i] || isclose(pupper, e.pres[i])) i2 = i;
            if (i1 >= 0 && i2 >= 0) {
                long pick = i1;
                if (i1 != i2) {
                    double best = -1e300; pick = -1;
                    for (long i = i1; i <= i2; ++i)       // argsort ascending, reversed: highest speed, last among ties
                        if (prof.wspd[i] != MISSING && prof.wspd[i] >= best) { best = prof.wspd[i]; pick = i; }
                }
                if (pick >= 0 && prof.wspd[pick] != MISSING && prof.wdir[pick] != MISSING) {
                    double u = prof.wspd[pick] * std::sin(prof.wdir[pick] * kPi / 180.) * -1;
                    double v = prof.wspd[pick] * std::cos(prof.wdir[pick] * kPi / 180.) * -1;
                    if (std::fabs(u) < 1e-10) u = 0.;
                    if (std::fabs(v) < 1e-10) v = 0.;
                    comp2vec(u, v, f.max_dir, f.max_spd);
                }
            }
        }
    }
    LPL mu = define_mu(e, 500.);
    f.bplus_fire = parcelx(e, mu.pres, mu.tmpc, mu.dwpc).bplus;
    f.pwat = precip_water(prof);
    return f;
}

}} // namespace sharppy::tab
