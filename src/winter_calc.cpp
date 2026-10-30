#include "sharppy/winter_calc.hpp"
#include "sharppy/thermo.hpp"
#include "sharppy/pwv.hpp"
#include <cmath>
#include <algorithm>
#include <vector>

namespace sharppy {

namespace {
double relh(double p, double t, double td) {
    double ws = mixing_ratio(p, t), w = mixing_ratio(p, td);
    return ws > 0 ? 100. * w / ws : 0.;
}
bool ok(double v) { return v != MISSING && std::isfinite(v); }

// params.mean_relh / mean_mixratio: 1 hPa steps, pressure-weighted
double mean_layer(const Profile& prof, double pbot, double ptop, bool rh) {
    double s = 0, w = 0;
    for (double p = pbot; p >= ptop - 1e-9; p -= 1.0) {
        double t = prof.temp_at_pres(p), d = prof.dwpt_at_pres(p);
        if (!ok(t) || !ok(d)) continue;
        s += (rh ? relh(p, t, d) : mixing_ratio(p, d)) * p;
        w += p;
    }
    return w > 0 ? s / w : 0.;
}

struct PosNeg { double pos = 0, neg = 0, top = 0, bot = 0; };

// Port of thermo._fast.posneg_temperature_loop / posneg_wetbulb_loop (fast_thermo.py), driven the way
// watch_type.posneg_temperature/posneg_wetbulb drive them.
PosNeg posneg(const Profile& prof, double upper, bool wet) {
    PosNeg r;
    if (!ok(upper) || prof.size() < 2) return r;      // start = missing: result is unused (precip type "None.")
    const long lptr = 0;
    long uptr = 0;
    for (size_t i = 0; i < prof.size(); ++i) if (prof.pres[i] > upper) uptr = (long)i;
    double pe1 = upper;
    double h1 = prof.hght_at_pres(pe1), t1 = prof.temp_at_pres(pe1), d1 = prof.dwpt_at_pres(pe1);
    if (!ok(h1) || !ok(t1) || !ok(d1)) return r;
    double te1 = wet ? wetbulb(pe1, t1, d1) : t1;

    int warmlayer = 0, coldlayer = 0;
    double totp = 0, totn = 0, ptop = 0, pbot = 0;
    for (long i = uptr; i >= lptr; --i) {
        double pe2 = prof.pres[i], h2 = prof.hght[i];
        double te2 = wet ? wetbulb(pe2, prof.temp[i], prof.dwpt[i]) : prof.temp[i];
        double tdef1 = (0. - te1) / (te1 + 273.15);
        double tdef2 = (0. - te2) / (te2 + 273.15);
        double lyre = 9.8 * (tdef1 + tdef2) / 2.0 * (h2 - h1);
        if (te2 > 0 && warmlayer == 0) { warmlayer = 1; ptop = pe2; }
        if (te2 < 0 && warmlayer == 1 && coldlayer == 0) { coldlayer = 1; pbot = pe2; }
        if (warmlayer > 0) { if (lyre > 0) totp += lyre; else totn += lyre; }
        pe1 = pe2; h1 = h2; te1 = te2;
    }
    if (warmlayer == 1 && coldlayer == 1) { r.pos = totp; r.neg = totn; r.top = ptop; r.bot = pbot; }
    return r;
}
} // namespace

double temp_lvl(const Profile& prof, double temp) {
    const size_t n = prof.size();
    if (n < 2) return MISSING;
    bool any_le = false, any_ge = false;
    for (double t : prof.temp) { if (t - temp <= 0) any_le = true; if (t - temp >= 0) any_ge = true; }
    if (!any_le || !any_ge) return MISSING;
    for (size_t i = 0; i < n; ++i) if (prof.temp[i] == temp) return prof.pres[i];
    for (size_t i = 0; i + 1 < n; ++i) {
        double a = prof.temp[i] - temp, b = prof.temp[i + 1] - temp;
        if (a * b < 0) {
            double lp = std::log10(prof.pres[i]) + (temp - prof.temp[i]) *
                        (std::log10(prof.pres[i + 1]) - std::log10(prof.pres[i])) / (prof.temp[i + 1] - prof.temp[i]);
            return std::pow(10., lp);
        }
    }
    return MISSING;
}

WinterData compute_winter(const Profile& prof) {
    WinterData w;
    if (prof.size() < 2) return w;
    const double sfc = prof.sfc_pres;

    // params.dgz
    w.dgz_pbot = temp_lvl(prof, -12); w.dgz_ptop = temp_lvl(prof, -17);
    if (!ok(w.dgz_pbot)) w.dgz_pbot = sfc;
    if (!ok(w.dgz_ptop)) w.dgz_ptop = sfc;
    double zb = prof.hght_at_pres(w.dgz_pbot), zt = prof.hght_at_pres(w.dgz_ptop);
    w.dgz_zbot_ft = ok(zb) ? zb * 3.28084 : 0; w.dgz_ztop_ft = ok(zt) ? zt * 3.28084 : 0;
    w.dgz_depth_ft = w.dgz_ztop_ft - w.dgz_zbot_ft;
    w.dgz_meanrh = mean_layer(prof, w.dgz_pbot, w.dgz_ptop, true);
    w.dgz_meanq = mean_layer(prof, w.dgz_pbot, w.dgz_ptop, false);
    w.dgz_pw = precip_water(prof, w.dgz_ptop, w.dgz_pbot);

    // watch_type.init_phase (saturation path: no omega data)
    {
        std::vector<double> newp;
        for (size_t i = 0; i < prof.size(); ++i) {
            double agl = prof.to_agl(prof.hght[i]);
            if (!(agl < 5000. && agl >= 0)) continue;
            if (relh(prof.pres[i], prof.temp[i], prof.dwpt[i]) > 80) newp.push_back(prof.pres[i] + 50.);
        }
        long top = -1;
        for (size_t k = 0; k < newp.size(); ++k) {
            double t = prof.temp_at_pres(newp[k]), d = prof.dwpt_at_pres(newp[k]);
            if (ok(t) && ok(d) && relh(newp[k], t, d) > 80) top = std::max<long>(top, (long)k);
        }
        if (top < 0) { w.plevel = MISSING; w.phase = -1; w.tmp = MISSING; w.init_st = "N/A"; }
        else {
            w.plevel = newp[top] - 25.;
            w.tmp = prof.temp_at_pres(w.plevel);
            if (w.tmp > 0) { w.phase = 0; w.init_st = "Rain"; }
            else if (w.tmp > -5) { w.phase = 1; w.init_st = "Freezing Rain"; }
            else if (w.tmp > -9) { w.phase = 1; w.init_st = "ZR/S Mix"; }
            else { w.phase = 3; w.init_st = "Snow"; }
        }
    }

    PosNeg t = posneg(prof, w.plevel, false), b = posneg(prof, w.plevel, true);
    w.tpos = t.pos; w.tneg = t.neg; w.ttop = t.top; w.tbot = t.bot;
    w.wpos = b.pos; w.wneg = b.neg; w.wtop = b.top; w.wbot = b.bot;

    // best_guess_precip
    const double sfct = prof.temp.front();
    w.sfc_tmpf = sfct * 9. / 5. + 32.;
    std::string pt;
    if (w.phase < 0) pt = "None.";
    else if (w.phase == 0 && w.tneg >= 0 && sfct > 0) pt = "Rain.";
    else if (w.phase == 3 && w.tpos <= 0 && sfct <= 0) pt = "Snow.";
    else if (w.phase == 1 && w.tpos <= 0 && sfct > 0) pt = "Rain.";
    else if (w.phase == 1 && w.tpos <= 0 && sfct <= 0) {
        double h = prof.hght_at_pres(w.plevel);
        if (ok(h) && prof.to_agl(h) >= 3000) pt = (w.tmp <= -4) ? "Sleet and Snow." : "Sleet.";
        else pt = "Freezing Rain/Drizzle.";
    }
    else if (w.phase == 3 && w.tpos <= 0 && sfct > 0) pt = (sfct > 4) ? "Rain." : "Snow.";
    else if (w.tpos > 0) {
        double y1 = -w.tneg, y2 = 0.62 * w.tpos + 60.0;
        pt = (y1 > y2) ? "Sleet." : (sfct <= 0 ? "Freezing Rain." : "Rain.");
    } else pt = "Unknown.";
    w.precip_type = pt;
    return w;
}

} // namespace sharppy
