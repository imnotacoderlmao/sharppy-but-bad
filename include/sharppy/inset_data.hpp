#pragma once
// Port of sharppy/databases/inset_data.py (box/whisker and probability tables).
#include <array>
#include <vector>
#include <string>

namespace sharppy { namespace inset {

// SHIP box/whisker (Ryan Jewell, SPC). Rows: <=1.5", >=2.5".
inline const std::vector<std::string>& shipYTexts() {
    static const std::vector<std::string> v{"5","4","3","2","1","0"," "}; return v; }
inline const std::vector<std::string>& shipXTexts() {
    static const std::vector<std::string> v{"<= 1.5\"", ">= 2.5\""}; return v; }
inline const std::array<std::array<double,5>,2>& shipDist() {
    static const std::array<std::array<double,5>,2> v{{
        {0.2, 0.3, 0.2, 0.9, 1.2},
        {1.1, 1.4, 0.8, 2.8, 4.0}}}; return v; }

// STP box/whisker (Thompson et al. 2012).
inline const std::vector<std::string>& stpYTexts() {
    static const std::vector<std::string> v{"11","10","9","8","7","6","5","4","3","2","1","0"}; return v; }
inline const std::vector<std::string>& stpXTexts() {
    static const std::vector<std::string> v{"EF4+","EF3","EF2","EF1","EF0","NONTOR"}; return v; }
inline const std::array<std::array<double,5>,6>& stpEf() {
    static const std::array<std::array<double,5>,6> v{{
        {1.2, 2.6, 5.3, 8.3, 11.0},
        {0.2, 1.0, 2.4, 4.5, 8.4},
        {0.0, 0.6, 1.7, 3.7, 5.6},
        {0.0, 0.3, 1.2, 2.6, 4.5},
        {0.0, 0.1, 0.8, 2.0, 3.7},
        {0.0, 0.0, 0.2, 0.7, 1.7}}}; return v; }

// Conditional STP vs EF (line plot), Bryan Smith / Rich Thompson (SPC).
struct CondSTP {
    std::array<double,11> ef1{{15.3,28.0,36.1,47.8,52.3,54.4,60.3,61.6,61.2,73.2,73.4}};
    std::array<double,11> ef2{{2.7,6.6,6.5,13.8,14.9,17.5,21.8,31.5,33.3,46.2,53.1}};
    std::array<double,11> ef3{{0,1.2,1.2,2.7,2.7,4.1,7.3,10.0,13.2,25.8,39.1}};
    std::array<double,11> ef4{{0,0,0,0,0.2,0.1,1.5,2.3,6.2,9.7,17.2}};
    std::array<const char*,11> xticks{{"0",".01-.5",".5-1","1-2","2-3","3-4","4-6","6-8","8-10","10-12",">12"}};
    // ytexts = [' ','0','10',...,'70'][::-1]
    std::array<const char*,9> ytexts{{"70","60","50","40","30","20","10","0"," "}};
};
inline const CondSTP& condSTP() { static const CondSTP c; return c; }

// VROT.
struct Vrot {
    std::array<double,11> xpts{{5,15,25,35,45,55,65,75,85,95,105}};
    std::array<double,11> ef01{{100.0,98.6,95.3,91.0,80.2,61.9,42.1,29.1,16.3,5.6,0.0}};
    std::array<double,11> ef23{{0.0,1.0,4.7,9.0,19.3,36.5,51.1,62.8,65.1,50.0,25.0}};
    std::array<double,11> ef45{{0.0,0.0,0.0,0.0,0.5,1.6,6.8,8.1,18.6,44.4,75.0}};
    std::array<const char*,9> ytexts{{"70","60","50","40","30","20","10","0"," "}};
};
inline const Vrot& vrot() { static const Vrot v; return v; }

}} // namespace sharppy::inset
