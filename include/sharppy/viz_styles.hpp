#pragma once
#include <string>
#include "sharppy/viz_prefs.hpp"
namespace sharppy {
// name: "standard" | "inverted" | "protanopia"
VizPrefs vizStyle(const std::string& name);
}
