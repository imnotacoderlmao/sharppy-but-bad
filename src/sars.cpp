#include "sharppy/sars.hpp"
#include <cmath>
#include <algorithm>
#include <QString>

namespace sharppy {

struct SevereCaseArchive {
    const char* date;
    const char* loc;
    int f_scale;
    double hail_in;
    double mucape;
    double shear06;
    double srh01;
    double srh03;
    double mllcl;
};

static const SevereCaseArchive SARS_DATABASE[] = {
    {"05/03/99", "OUN (Bridge Creek-Moore)", 5, 2.75, 4200.0, 55.0, 280.0, 420.0,  950.0},
    {"04/27/11", "BMX (Tuscaloosa-Birm)",   4, 3.00, 3200.0, 68.0, 350.0, 520.0,  750.0},
    {"05/22/11", "SGF (Joplin EF5)",        5, 2.50, 3900.0, 48.0, 220.0, 380.0,  850.0},
    {"05/04/07", "DDC (Greensburg EF5)",    5, 4.00, 3800.0, 52.0, 310.0, 450.0, 1100.0},
    {"05/20/13", "OUN (Newcastle-Moore)",   5, 2.00, 4100.0, 46.0, 240.0, 390.0,  880.0},
    {"04/26/91", "ICT (Andover F5)",        5, 3.50, 3500.0, 50.0, 260.0, 410.0,  920.0},
    {"06/22/03", "ABR (Aurora 7in Hail)",  -1, 7.00, 4500.0, 60.0, 180.0, 310.0, 1300.0},
    {"05/10/10", "OUN (Norman High-End)",   4, 4.25, 3600.0, 62.0, 340.0, 490.0,  800.0},
    {"05/31/13", "OUN (El Reno 2.6mi)",     3, 2.50, 4600.0, 58.0, 290.0, 440.0,  850.0},
    {"08/28/90", "ILX (Plainfield F5)",     5, 2.75, 5200.0, 52.0, 210.0, 340.0,  980.0},
    {"05/27/97", "FWD (Jarrell F5)",        5, 3.00, 4800.0, 32.0, 140.0, 210.0,  820.0},
    {"03/13/90", "TOP (Hesston F5)",        5, 3.00, 3100.0, 65.0, 320.0, 480.0,  700.0},
    {"02/05/08", "LZK (Super Tuesday EF4)", 4, 2.00, 2400.0, 72.0, 390.0, 560.0,  620.0},
    {"03/03/19", "BMX (Beauregard EF4)",    4, 2.00, 2200.0, 64.0, 310.0, 440.0,  710.0},
    {"12/10/21", "PAH (Mayfield EF4)",      4, 2.50, 2100.0, 75.0, 380.0, 510.0,  680.0},
    {"03/24/23", "JAN (Rolling Fork EF4)",  4, 2.25, 2600.0, 68.0, 360.0, 490.0,  720.0},
    {"05/24/11", "OUN (El Reno-Piedmont)",  5, 4.00, 4800.0, 65.0, 340.0, 510.0,  760.0},
    {"06/07/07", "MPX (Cokato Hail 4in)",  -1, 4.00, 3800.0, 45.0, 110.0, 220.0, 1250.0},
    {"05/08/03", "OUN (Moore F4)",          4, 4.50, 3600.0, 54.0, 290.0, 410.0,  880.0},
    {"04/14/12", "ICT (Wichita EF3)",       3, 3.00, 3300.0, 58.0, 270.0, 390.0,  910.0}
};

SarsResult SarsEngine::query(const Profile& prof,
                            const ParcelResult& mu,
                            const ParcelResult& ml,
                            double shear06_kts,
                            double srh01,
                            double srh03) {
    SarsResult res;

    double mllcl_agl = 1000.0;
    double lcl_h = prof.hght_at_pres(ml.lcl_p);
    if (lcl_h != MISSING) {
        mllcl_agl = prof.to_agl(lcl_h);
    }

    std::vector<SarsMatch> matches;

    const double sigma_cape  = 1200.0;
    const double sigma_shear = 15.0;
    const double sigma_srh01 = 100.0;
    const double sigma_srh03 = 140.0;
    const double sigma_lcl   = 400.0;

    int tor_count = 0;
    int sig_tor_count = 0;
    int sig_hail_count = 0;
    int loose_count = 0;

    for (const auto& arch : SARS_DATABASE) {
        double d_cape  = (mu.cape - arch.mucape) / sigma_cape;
        double d_shear = (shear06_kts - arch.shear06) / sigma_shear;
        double d_srh01 = (srh01 - arch.srh01) / sigma_srh01;
        double d_srh03 = (srh03 - arch.srh03) / sigma_srh03;
        double d_lcl   = (mllcl_agl - arch.mllcl) / sigma_lcl;

        double dist = std::sqrt(d_cape * d_cape +
                                d_shear * d_shear +
                                d_srh01 * d_srh01 +
                                d_srh03 * d_srh03 +
                                d_lcl * d_lcl);

        if (dist <= 3.2) {
            loose_count++;
            if (arch.f_scale >= 0) tor_count++;
            if (arch.f_scale >= 2) sig_tor_count++;
            if (arch.hail_in >= 2.0) sig_hail_count++;
        }

        if (dist <= 2.8) {
            SarsMatch m;
            m.date_str = arch.date;
            m.location = arch.loc;
            m.f_scale = arch.f_scale;
            m.hail_size_in = arch.hail_in;
            m.match_score = dist;
            matches.push_back(m);
        }
    }

    std::sort(matches.begin(), matches.end(), [](const SarsMatch& a, const SarsMatch& b) {
        return a.match_score < b.match_score;
    });

    res.top_matches = matches;
    res.loose_matches_tor = loose_count;
    res.loose_matches_hail = loose_count;
    res.pct_tor = (loose_count > 0) ? static_cast<int>((tor_count * 100.0) / loose_count) : 0;
    res.pct_sig_tor = (loose_count > 0) ? static_cast<int>((sig_tor_count * 100.0) / loose_count) : 0;
    res.pct_sig_hail = (loose_count > 0) ? static_cast<int>((sig_hail_count * 100.0) / loose_count) : 0;

    if (!matches.empty()) {
        const auto& best = matches.front();
        res.supercell_match_summary = best.location + " (" + best.date_str + ")";
        res.hail_match_summary = QString("Hail: %1\" (%2)").arg(best.hail_size_in, 0, 'f', 2).arg(QString::fromStdString(best.date_str)).toStdString();
    } else {
        res.supercell_match_summary = "No Quality Matches";
        res.hail_match_summary = "No Quality Matches";
    }

    return res;
}

} // namespace sharppy
