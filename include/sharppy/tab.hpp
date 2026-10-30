#pragma once
// Exact C++ ports of SHARPpy's sharptab thermodynamics: fast_thermo.py scalar kernels,
// the interp.* log-pressure interpolation, params.DefineParcel / parcelx / cape / temp_lvl / mean_*.
// NaN is used for "masked" values, as in the numba kernels.
#include <vector>
#include <cmath>
#include <limits>
#include "sharppy/profile.hpp"

namespace sharppy { namespace tab {

constexpr double NaN = std::numeric_limits<double>::quiet_NaN();
constexpr double ZEROCNK = 273.15;
constexpr double ROCP = 0.28571426;
constexpr double G = 9.80665;

// ---- fast_thermo.py scalar kernels (Celsius unless stated) ----
double wobf(double t);
double theta(double p, double t, double p2 = 1000.);          // returns C
inline double ctok(double t) { return t + ZEROCNK; }
double vappres(double t);
double mixratio(double p, double t);                           // g/kg
double virtemp(double p, double t, double td);
double lcltemp(double t, double td);
double thalvl(double theta_c, double t);
void   drylift(double p, double t, double td, double& p2, double& t2);
double satlift(double p, double thetam, double conv = 0.1);
double wetlift(double p, double t, double p2);
double temp_at_mixrat(double w, double p);
double thetae(double p, double t, double td);                  // C (profile adds 273.15)
double wetbulb(double p, double t, double td);

// ---- profile view with the interp.* behaviour ----
struct Env {
    std::vector<double> pres, tmpc, dwpc, hght, vtmp;
    size_t sfc = 0;
    double sfc_pres = NaN, sfc_hght = NaN;
    std::vector<double> lp_h, v_h, lp_t, v_t, lp_d, v_d, lp_v, v_v, hh_valid, lp_hh;
    explicit Env(const Profile& prof);
    double hght_at(double p) const;
    double temp_at(double p) const;
    double dwpt_at(double p) const;
    double vtmp_at(double p) const;
    double pres_at_hght(double h_msl) const;
    double to_agl(double h) const { return h - sfc_hght; }
    double to_msl(double h) const { return h + sfc_hght; }
};

struct Parcel {
    double pres = NaN, tmpc = NaN, dwpc = NaN;
    double lclpres = NaN, lclhght = NaN, lcltmp = NaN;
    double lfcpres = NaN, lfchght = NaN, elpres = NaN, elhght = NaN, mplpres = NaN, mplhght = NaN;
    double bplus = NaN, bminus = NaN, bfzl = NaN, b3km = NaN, b6km = NaN;
    double p0c = NaN, pm10c = NaN, pm20c = NaN, pm30c = NaN;
    double hght0c = NaN, hghtm10c = NaN, hghtm20c = NaN, hghtm30c = NaN;
    double wm10c = NaN, wm20c = NaN, wm30c = NaN, li5 = NaN, li3 = NaN;
    double limax = NaN, limaxpres = NaN, cap = NaN, cappres = NaN, bmin = NaN, bminpres = NaN;
    double blayer = NaN, tlayer = NaN;
    std::vector<double> ptrace, ttrace;
};

double temp_lvl(const Env& e, double temp);                       // hPa or NaN
double mean_theta_exact(const Env& e, double pbot, double ptop);  // C
double mean_mixratio_exact(const Env& e, double pbot, double ptop);
double most_unstable_level(const Env& e, double pbot, double ptop);
double max_temp(const Env& e, double mixlayer = 100.);

struct LPL { double pres = NaN, tmpc = NaN, dwpc = NaN; };
LPL define_sfc(const Env& e);
LPL define_fcst(const Env& e);
LPL define_mu(const Env& e, double presval = 300.);
LPL define_ml(const Env& e, double presval = 100., double pbot = NaN);

Parcel parcelx(const Env& e, double pres, double tmpc, double dwpc, double pbot = NaN, double ptop = NaN);
// params.cape(): only B+/B- (used by the effective inflow search)
void cape_only(const Env& e, double pres, double tmpc, double dwpc, double pbot, double ptop, bool trunc,
               double& bplus, double& bminus);

// params.effective_inflow_layer: pbot/ptop NaN when no layer. mucape/mucinh from the MU parcel (flag=3, 300 hPa)
void effective_inflow(const Env& e, double mucape, double mucinh, double& pbot, double& ptop,
                      double ecape = 100., double ecinh = -250.);
// params.dcape: returns tote (J/kg, positive); traces optional
double dcape(const Env& e, std::vector<double>* ptrace = nullptr, std::vector<double>* ttrace = nullptr);

}} // namespace sharppy::tab
