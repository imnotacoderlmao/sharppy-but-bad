#include "sharppy/profile_interp.hpp"
#include <cmath>

namespace sharppy {

bool interpolate_profile(const Profile& src, Profile& out, InterpMode mode, double dp, double pbot, double ptop) {
    if (src.size() < 2) return false;
    std::vector<double> np;
    if (mode == InterpMode::FillMissing) {
        np = src.pres;
    } else {
        double p_sfc = src.pres.front(), p_top = src.pres.back();
        if (pbot > 0) p_sfc = pbot;
        if (ptop > 0) p_top = ptop;
        const double step = std::fabs(dp);
        if (step <= 0 || p_sfc <= p_top) return false;
        const long n = static_cast<long>(std::lround((p_sfc - p_top) / step)) + 1;
        if (n < 2) return false;
        np.resize(n);
        for (long i = 0; i < n; ++i) np[i] = p_sfc + (p_top - p_sfc) * static_cast<double>(i) / static_cast<double>(n - 1);  // linspace
    }
    Profile o;
    o.latitude = src.latitude;
    o.station_id = src.station_id;
    for (double p : np) {
        const double h = src.hght_at_pres(p), t = src.temp_at_pres(p), d = src.dwpt_at_pres(p);
        double u, v; src.wind_at_pres(p, u, v);
        if (h == MISSING || t == MISSING || d == MISSING || u == MISSING || v == MISSING) continue;
        double wd, ws; comp2vec(u, v, wd, ws);
        o.pres.push_back(p); o.hght.push_back(h); o.temp.push_back(t); o.dwpt.push_back(d);
        o.u.push_back(u); o.v.push_back(v); o.wdir.push_back(wd); o.wspd.push_back(ws);
    }
    if (o.size() < 2) return false;
    o.sfc_pres = o.pres.front();
    o.sfc_hght = (mode == InterpMode::FillMissing) ? src.sfc_hght : o.hght.front();
    if (mode == InterpMode::FillMissing) o.sfc_pres = src.sfc_pres;
    out = std::move(o);
    return true;
}

} // namespace sharppy
