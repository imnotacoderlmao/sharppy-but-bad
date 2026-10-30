#include "sharppy/cm1_io.hpp"
#include "sharppy/tab.hpp"
#include "sharppy/sanitize.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <sstream>

namespace sharppy {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double ROCP_ = 0.28571426, ZEROCNK_ = 273.15;

bool parseNums(const std::string& line, std::vector<double>& v) {
    v.clear();
    std::istringstream ss(line);
    std::string tok;
    while (ss >> tok) {
        char* end = nullptr;
        double d = std::strtod(tok.c_str(), &end);
        if (end == tok.c_str() || *end != '\0') return false;
        v.push_back(d);
    }
    return true;
}

std::string nowTitle() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char b[32];
    std::strftime(b, sizeof b, "%y%m%d/%H%M", &tm);
    return std::string("****   ") + b;
}

std::string fmt(const char* f, double v) { char b[64]; std::snprintf(b, sizeof b, f, v); return b; }
std::string pad12(const std::string& s) { return s.size() >= 12 ? s : std::string(12 - s.size(), ' ') + s; }

// np.arange(start, stop, step) length
long arangeLen(double start, double stop, double step) { return std::max(0L, (long)std::ceil((stop - start) / step)); }
}

// ===================== cm12snd =====================
bool read_cm1(const std::string& path, Profile& out, std::string* err) {
    std::ifstream f(path);
    if (!f) { if (err) *err = "cannot open file"; return false; }
    const double c_g = 9.81, cp = 1005.7, rd = 287.04, reps = 1. / 0.62197, p00 = 1000.;
    double sfc_pres = 0, sfc_theta = 0, sfc_qv = 0;
    std::vector<double> hght{0.}, theta, qv, u{0.}, v{0.};
    std::string line; int nline = 0; std::vector<double> n;
    while (std::getline(f, line)) {
        if (!parseNums(line, n)) { if (err) *err = "non-numeric data"; return false; }
        if (n.empty()) continue;
        if (nline == 0) {
            if (n.size() != 3) { if (err) *err = "first line must hold surface pressure, theta and qv"; return false; }
            sfc_pres = n[0]; sfc_theta = n[1]; sfc_qv = n[2];
            theta.push_back(sfc_theta); qv.push_back(sfc_qv);
        } else {
            if (n.size() < 5) { if (err) *err = "expected z theta qv u v"; return false; }
            hght.push_back(n[0]); theta.push_back(n[1]); qv.push_back(n[2]);
            u.push_back(n[3] * 1.94384449); v.push_back(n[4] * 1.94384449);   // m/s -> kts
        }
        ++nline;
    }
    if (nline < 2 || !(sfc_pres > 0)) { if (err) *err = "not a CM1 sounding"; return false; }
    const size_t N = hght.size();
    std::vector<double> q(N), thetav(N), pi(N), pres(N), tmpc(N), dwpc(N), wdir(N), wspd(N);
    for (size_t k = 0; k < N; ++k) q[k] = qv[k] / 1000.;                      // g/g
    for (size_t k = 0; k < N; ++k) thetav[k] = theta[k] * (1.0 + reps * q[k]) / (1.0 + q[k]);
    pi[0] = std::pow(sfc_pres / p00, ROCP_);
    for (size_t k = 1; k < N; ++k)
        pi[k] = pi[k - 1] - c_g * (hght[k] - hght[k - 1]) / (cp * 0.5 * (thetav[k] + thetav[k - 1]));
    for (size_t k = 0; k < N; ++k) {
        pres[k] = p00 * std::pow(pi[k], cp / rd);
        tmpc[k] = theta[k] * pi[k] - 273.15;
        dwpc[k] = tab::temp_at_mixrat(q[k] * 1000., pres[k]);
        wspd[k] = std::sqrt(u[k] * u[k] + v[k] * v[k]);
        double d = std::atan2(-u[k], -v[k]) * 180.0 / kPi;
        if (d < 0) d += 360;
        wdir[k] = d;
    }
    Profile p;
    p.pres = pres; p.hght = hght; p.temp = tmpc; p.dwpt = dwpc; p.wdir = wdir; p.wspd = wspd;
    p.u.resize(N); p.v.resize(N);
    for (size_t k = 0; k < N; ++k) vec2comp(wdir[k], wspd[k], p.u[k], p.v[k]);
    p.sfc_pres = pres[0]; p.sfc_hght = hght[0];
    if (!sanitize_profile(p)) { if (err) *err = "fewer than two usable levels"; return false; }
    out = p;
    return true;
}

// ===================== sndmod =====================
bool write_cm1(const Profile& prof, const std::string& pathIn, const Cm1ExportOptions& o, std::string* err) {
    using tab::mixratio; using tab::temp_at_mixrat;
    std::string path = pathIn;
    if (path.size() < 4 || path.compare(path.size() - 4, 4, ".cm1") != 0) path += ".cm1";

    // only levels where everything that matters is valid
    std::vector<double> P, H, T, Td, U, V, RH;
    for (size_t i = 0; i < prof.size(); ++i) {
        auto bad = [](double x) { return x == MISSING || !std::isfinite(x); };
        if (bad(prof.pres[i]) || bad(prof.hght[i]) || bad(prof.temp[i]) || bad(prof.dwpt[i]) || bad(prof.u[i]) || bad(prof.v[i])) continue;
        P.push_back(prof.pres[i]); H.push_back(prof.hght[i]); T.push_back(prof.temp[i]); Td.push_back(prof.dwpt[i]);
        U.push_back(prof.u[i]); V.push_back(prof.v[i]);
        RH.push_back(100. * mixratio(prof.pres[i], prof.dwpt[i]) / mixratio(prof.pres[i], prof.temp[i]));   // thermo.relh
    }
    if (P.size() < 2) { if (err) *err = "profile has fewer than two valid levels"; return false; }
    const double dz = o.dz;
    if (!(dz > 0)) { if (err) *err = "dz must be positive"; return false; }

    // --fill-top (kept exactly as sndmod computes it)
    if (o.fillTop > 0.0) {
        const long n = arangeLen(H.back(), o.fillTop + 3 * dz, dz);
        std::vector<double> th(n), tp(n, 0.), tt(n, T.back()), ttd(n, 0.), tmr(n), trh(n), tu(n, U.back()), tv(n, V.back());
        for (long i = 0; i < n; ++i) th[i] = H.back() + i * dz;
        tp[0] = P.back(); ttd[0] = Td.back();
        for (long i = 0; i + 1 < n; ++i) tp[i + 1] = tp[i] * std::exp((dz * 9.81) / (tt[i] * 287.04));
        const double mr0 = mixratio(tp[0], ttd[0]);
        for (long i = 0; i < n; ++i) {
            tmr[i] = mr0;
            trh[i] = (tmr[i] / mixratio(tp[i], tt[i])) * 100.0;
            ttd[i] = temp_at_mixrat(tmr[i], tp[i]);
        }
        for (long i = 1; i < n; ++i) {
            H.push_back(th[i]); P.push_back(tp[i]); T.push_back(tt[i]); Td.push_back(ttd[i]);
            RH.push_back(trh[i]); U.push_back(tu[i]); V.push_back(tv[i]);
        }
    }
    auto rebuildTd = [&]() {
        for (size_t i = 0; i < P.size(); ++i) {
            double mr = (RH[i] / 100.0) * mixratio(P[i], T[i]);
            if (mr < 0.003) mr = 0.003;
            Td[i] = temp_at_mixrat(mr, P[i]);
        }
    };
    if (o.desat > 0.0) {
        for (double& r : RH) { if (r >= 97.0) r -= o.desat; if (r < 0.0001) r = 0.0001; }
        rebuildTd();
    }
    if (o.desatLayer) {
        for (size_t i = 0; i < H.size(); ++i) if (H[i] >= o.dlBottom && H[i] <= o.dlTop) RH[i] -= o.dlPct;
        for (double& r : RH) if (r < 0.0001) r = 0.0001;
        rebuildTd();
    }
    if (!o.desatLayer && o.desat == 0.0) rebuildTd();

    Profile pr;
    pr.pres = P; pr.hght = H; pr.temp = T; pr.dwpt = Td; pr.u = U; pr.v = V;
    pr.wdir.resize(P.size()); pr.wspd.resize(P.size());
    for (size_t i = 0; i < P.size(); ++i) comp2vec(U[i], V[i], pr.wdir[i], pr.wspd[i]);
    pr.sfc_pres = P[0]; pr.sfc_hght = H[0];
    tab::Env env(pr);

    const long n = arangeLen(H.front(), H.back(), dz);
    if (n < 2) { if (err) *err = "profile too shallow for this dz"; return false; }
    std::vector<double> nh(n), np(n), nt(n), ntd(n), nu(n), nv(n), nuk(n), nvk(n), th(n), mixr(n);
    for (long i = 0; i < n; ++i) {
        nh[i] = H.front() + i * dz;
        np[i] = env.pres_at_hght(nh[i]);
        nt[i] = env.temp_at(np[i]);
        ntd[i] = env.dwpt_at(np[i]);
        double uu, vv; pr.wind_at_pres(np[i], uu, vv);
        nu[i] = nuk[i] = uu; nv[i] = nvk[i] = vv;
        th[i] = tab::theta(np[i], nt[i]) + ZEROCNK_;                       // interp.theta (K)
    }
    if (o.adiabaticHeight > 0.0) {
        long zmax = -1;
        for (long i = 0; i < n; ++i) if (nh[i] >= o.adiabaticHeight) { zmax = i; break; }
        if (zmax < 0) { if (err) *err = "adiabatic height is above the profile"; return false; }
        for (long z = 0; z <= zmax && z + 1 < n; ++z) {
            const double dzt = (nh[z + 1] - nh[z]) / 1000.0, lo = th[z], mod = dzt * o.adiabaticStability;
            if (th[z + 1] < lo + mod) th[z + 1] = lo + mod;
        }
    }
    for (long i = 0; i < n; ++i) {
        nt[i] = (th[i] / std::pow(1000.0 / np[i], ROCP_)) - ZEROCNK_;
        double mr = mixratio(np[i], ntd[i]);                                 // interp.mixratio
        if (mr < 0.003) mr = 0.003;
        mixr[i] = mr;
        nu[i] *= 0.514444; nv[i] *= 0.514444;                                // KTS2MS
    }

    std::ofstream out(path);
    if (!out) { if (err) *err = "cannot write " + path; return false; }
    const std::string D = "\t\t";                                           // delimiter "\t" + "\t"
    for (long i = 0; i < n; ++i) {
        if (i == 0) {
            out << pad12(fmt("%0.6f", np[0])) << D << pad12(fmt("%0.6f", th[0])) << D << pad12(fmt("%0.6f", mixr[0])) << "\t\n";
        } else {
            out << pad12(fmt("%0.6f", nh[i] - nh[0])) << D << pad12(fmt("%0.6f", th[i])) << D << pad12(fmt("%0.6f", mixr[i]))
                << D << pad12(fmt("%0.6f", nu[i])) << D << pad12(fmt("%0.6f", nv[i])) << "\n";
        }
    }
    out.close();

    if (o.alsoSharppy) {                                                     // write_sharp
        std::string sp = path.substr(0, path.size() - 4) + ".sharppy";
        std::ofstream s(sp);
        if (!s) { if (err) *err = "cannot write " + sp; return false; }
        s << "%TITLE%\n" << nowTitle() << "\n"
          << "   LEVEL       HGHT       TEMP       DWPT       WDIR       WSPD\n"
          << "-------------------------------------------------------------------\n%RAW%\n";
        for (long i = 0; i < n; ++i) {
            double wd, ws; comp2vec(nuk[i], nvk[i], wd, ws);   // interp.vec (kts)
            s << fmt("%12.6f", np[i]) << ",  " << fmt("%12.6f", nh[i] - nh[0]) << ",  " << fmt("%12.6f", nt[i]) << ",  "
              << fmt("%12.6f", tab::temp_at_mixrat(mixr[i], np[i])) << ",  " << fmt("%12.6f", wd) << ",  " << fmt("%12.6f", ws) << "\n";
        }
        s << "%END%\n";
    }
    return true;
}

} // namespace sharppy
