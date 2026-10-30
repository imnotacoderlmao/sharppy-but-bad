#include "sharppy/modify.hpp"
#include <cstddef>
#include <fstream>
#include <iomanip>
#include <cmath>
#include <algorithm>

namespace sharppy {

void ProfileModifier::modify_wind_index(Profile& prof, std::size_t target_idx, double new_u, double new_v, std::size_t /* neighbor_radius */) {
    if (prof.empty() || target_idx >= prof.size()) return;
    prof.u[target_idx] = new_u;
    prof.v[target_idx] = new_v;
    comp2vec(prof.u[target_idx], prof.v[target_idx], prof.wdir[target_idx], prof.wspd[target_idx]);
}

void ProfileModifier::modify_thermo_index(Profile& prof, std::size_t target_idx, double new_t_or_td, bool is_temp, std::size_t /* neighbor_radius */) {
    if (prof.empty() || target_idx >= prof.size()) return;
    if (is_temp) {
        prof.temp[target_idx] = new_t_or_td;
    } else {
        prof.dwpt[target_idx] = new_t_or_td;
    }
    sanitize(prof);
}

void ProfileModifier::modify_layer(Profile& prof, double p_start, double t_start, double p_end, double t_end, bool is_temp) {
    if (prof.empty()) return;
    double p_top = std::min(p_start, p_end);
    double p_bot = std::max(p_start, p_end);
    if (p_bot - p_top < 1e-3) return;

    for (std::size_t i = 0; i < prof.size(); ++i) {
        double p = prof.pres[i];
        if (p >= p_top && p <= p_bot) {
            double interp_t = log_pres_interp(p, p_start, p_end, t_start, t_end);
            if (is_temp) {
                prof.temp[i] = interp_t;
            } else {
                prof.dwpt[i] = interp_t;
            }
        }
    }
    sanitize(prof);
}

void ProfileModifier::modify_surface(Profile& prof, double new_t, double new_td, bool dry_mix) {
    if (prof.empty()) return;

    if (!dry_mix) {
        prof.temp.front() = new_t;
        prof.dwpt.front() = new_td;
        sanitize(prof);
        return;
    }

    // Boundary layer dry-adiabatic mixing
    double sfc_p = prof.sfc_pres;
    double sfc_th_k = theta(sfc_p, new_t);
    double sfc_w = mixing_ratio(sfc_p, new_td);

    prof.temp.front() = new_t;
    prof.dwpt.front() = new_td;

    for (std::size_t i = 1; i < prof.size(); ++i) {
        double p = prof.pres[i];
        double env_th_k = theta(p, prof.temp[i]);
        if (env_th_k <= sfc_th_k) {
            // Mixed dry-adiabatically
            double mixed_t = k2c(sfc_th_k * std::pow(p / 1000.0, ROCP));
            double mixed_td = temp_at_mixrat(sfc_w, p);
            prof.temp[i] = mixed_t;
            prof.dwpt[i] = mixed_td;
        } else {
            // Reached top of mixed layer
            break;
        }
    }

    sanitize(prof);
}

void ProfileModifier::sanitize(Profile& prof) {
    for (std::size_t i = 0; i < prof.size(); ++i) {
        if (prof.dwpt[i] > prof.temp[i]) {
            prof.dwpt[i] = prof.temp[i];
        }
    }
}

bool ProfileModifier::export_spc(const Profile& prof, const std::string& title, const std::string& filepath) {
    std::ofstream out(filepath);
    if (!out.is_open()) return false;

    out << "%TITLE%\n";
    out << title << " (MODIFIED)\n";
    out << "%RAW%\n";
    out << std::fixed << std::setprecision(1);

    for (std::size_t i = 0; i < prof.size(); ++i) {
        out << std::setw(7) << prof.pres[i] << ", "
            << std::setw(7) << static_cast<int>(std::round(prof.hght[i])) << ", "
            << std::setw(6) << prof.temp[i] << ", "
            << std::setw(6) << prof.dwpt[i] << ", "
            << std::setw(5) << static_cast<int>(std::round(prof.wdir[i])) << ", "
            << std::setw(5) << static_cast<int>(std::round(prof.wspd[i])) << "\n";
    }

    out << "%END%\n";
    return true;
}

} // namespace sharppy