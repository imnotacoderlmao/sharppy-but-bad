#pragma once

#include <cstddef>
#include <string>
#include "sharppy/profile.hpp"
#include "sharppy/thermo.hpp"

namespace sharppy {

class ProfileModifier {
public:
    // Discrete level editing by index
    static void modify_thermo_index(Profile& prof, std::size_t target_idx, double new_t_or_td, bool is_temp, std::size_t neighbor_radius = 0);
    static void modify_wind_index(Profile& prof, std::size_t target_idx, double new_u, double new_v, std::size_t neighbor_radius = 0);

    // Two-point rubber-band slab replacement
    static void modify_layer(Profile& prof, double p_start, double t_start, double p_end, double t_end, bool is_temp);

    // Surface modification with optional dry-adiabatic boundary layer mixing
    static void modify_surface(Profile& prof, double new_t, double new_td, bool dry_mix = true);

    // Physical sanity check (enforces Td <= T)
    static void sanitize(Profile& prof);

    // Export modified profile into standard SPC raw format
    static bool export_spc(const Profile& prof, const std::string& title, const std::string& filepath);
};

} // namespace sharppy