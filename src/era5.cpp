#include "sharppy/era5.hpp"
#include "sharppy/grib.hpp"
#include <QJsonArray>
#include <algorithm>
#include <cmath>
#include <map>

namespace sharppy {

namespace {
const int kLevels[] = {1, 2, 3, 5, 7, 10, 20, 30, 50, 70, 100, 125, 150, 175, 200, 225, 250, 300, 350, 400, 450, 500,
                       550, 600, 650, 700, 750, 775, 800, 825, 850, 875, 900, 925, 950, 975, 1000};

QJsonObject common(const QDateTime& t, double lat, double lon) {
    const QDateTime u = t.toUTC();
    // The 0.25 degree cell around the point: the nearest grid point is always one of its four corners.
    const double s = std::floor(lat / 0.25) * 0.25, w = std::floor(lon / 0.25) * 0.25;
    QJsonObject o;
    o["product_type"] = QJsonArray{"reanalysis"};
    o["year"] = QJsonArray{u.toString("yyyy")};
    o["month"] = QJsonArray{u.toString("MM")};
    o["day"] = QJsonArray{u.toString("dd")};
    o["time"] = QJsonArray{u.toString("HH") + ":00"};
    o["area"] = QJsonArray{std::min(s + 0.25, 90.0), w, s, std::min(w + 0.25, 360.0)};   // N, W, S, E
    o["data_format"] = "grib";
    o["download_format"] = "unarchived";
    return o;
}

// ECMWF table 128 parameters
enum { P_Z = 129, P_T = 130, P_U = 131, P_V = 132, P_R = 157, P_SP = 134, P_10U = 165, P_10V = 166, P_2T = 167, P_2D = 168 };

// value of the grid point nearest to (lat, lon); longitude differences wrap at 360 degrees
double nearest(const GribField& f, double lat, double lon) {
    int bi = 0, bj = 0; double bd = 1e30;
    for (int j = 0; j < f.nj; ++j) {
        const double dla = f.lat(j) - lat;
        for (int i = 0; i < f.ni; ++i) {
            double dlo = std::fmod(std::fabs(f.lon(i) - lon), 360.0);
            if (dlo > 180.0) dlo = 360.0 - dlo;
            const double d = dla * dla + dlo * dlo;
            if (d < bd) { bd = d; bi = i; bj = j; }
        }
    }
    return f.at(bi, bj);
}

QString latlonTag(double lat, double lon) {
    return QString("%1%2,%3%4").arg(lat >= 0 ? "N" : "S").arg(std::fabs(lat), 0, 'f', 2).arg(lon >= 0 ? "E" : "W").arg(std::fabs(lon), 0, 'f', 2);
}

void wind(double u, double v, double& wdir, double& wspd) {
    wdir = std::fmod(std::atan2(u, v) * 180.0 / M_PI + 180.0, 360.0);   // np.degrees(arctan2(u, v)) + 180
    wspd = std::hypot(u, v) * 1.944;
}
}

QJsonObject era5_pressure_inputs(const QDateTime& utc, double lat, double lon) {
    QJsonObject o = common(utc, lat, lon);
    o["variable"] = QJsonArray{"geopotential", "relative_humidity", "temperature", "u_component_of_wind", "v_component_of_wind"};
    QJsonArray lv;
    for (int l : kLevels) lv.append(QString::number(l));
    o["pressure_level"] = lv;
    return o;
}

QJsonObject era5_surface_inputs(const QDateTime& utc, double lat, double lon) {
    QJsonObject o = common(utc, lat, lon);
    o["variable"] = QJsonArray{"surface_pressure", "geopotential", "2m_temperature", "2m_dewpoint_temperature",
                               "10m_u_component_of_wind", "10m_v_component_of_wind"};
    return o;
}

double era5_dewpoint(double tempC, double rh) {
    // MetPy 1.x dewpoint_from_relative_humidity: e = rh * es(T) with es from Ambaum (2020) eq. 13 (liquid water), then the
    // Bolton (1980) inversion  Td = 243.5 ln(e/611.2 Pa) / (17.67 - ln(e/611.2 Pa)).
    const double Rv = 461.52311572606084, Lv = 2500840.0, Cpl = 4219.400000000001, Cpv = 1860.078011865639;
    const double T0 = 273.16, es0 = 611.2;
    const double T = tempC + 273.15;
    const double latent = Lv - (Cpl - Cpv) * (T - T0);
    const double es = es0 * std::pow(T0 / T, (Cpl - Cpv) / Rv) * std::exp((Lv / T0 - latent / T) / Rv);
    const double val = std::log(rh / 100.0 * es / es0);
    return 243.5 * val / (17.67 - val);
}

bool era5_build_text(const QByteArray& pressureGrib, const QByteArray& surfaceGrib, double lat, double lon,
                     QString& text, QString& title, QString* err, QString* note) {
    std::vector<GribField> pf;
    QString e;
    if (!grib1_decode(pressureGrib, pf, &e)) { if (err) *err = "pressure-level file: " + e; return false; }

    struct Lv { double z = NAN, t = NAN, u = NAN, v = NAN, r = NAN; };
    std::map<int, Lv> lev;           // hPa -> values
    int date = 0, time = 0;
    for (const GribField& f : pf) {
        if (f.levelType != 100) continue;
        Lv& l = lev[f.level];
        const double x = nearest(f, lat, lon);
        switch (f.param) {
            case P_Z: l.z = x; break; case P_T: l.t = x; break; case P_U: l.u = x; break;
            case P_V: l.v = x; break; case P_R: l.r = x; break; default: continue;
        }
        date = f.date; time = f.time;
    }
    if (lev.empty()) { if (err) *err = "the pressure-level file has no isobaric fields"; return false; }

    struct Row { double p, z, t, td, wd, ws; };
    std::vector<Row> rows;
    double psfc = NAN;
    if (!surfaceGrib.isEmpty()) {
        std::vector<GribField> sf; QString se;
        if (!grib1_decode(surfaceGrib, sf, &se)) { if (note) *note = "surface file not usable (" + se + "); the profile starts at 1000 hPa"; }
        else {
            double sp = NAN, z = NAN, t2 = NAN, d2 = NAN, u10 = NAN, v10 = NAN;
            for (const GribField& f : sf) {
                const double x = nearest(f, lat, lon);
                switch (f.param) {
                    case P_SP: sp = x; break; case P_Z: z = x; break; case P_2T: t2 = x; break; case P_2D: d2 = x; break;
                    case P_10U: u10 = x; break; case P_10V: v10 = x; break; default: break;
                }
            }
            if (std::isfinite(sp) && std::isfinite(z) && std::isfinite(t2) && std::isfinite(d2) && std::isfinite(u10) && std::isfinite(v10)) {
                double wd, ws; wind(u10, v10, wd, ws);
                psfc = sp / 100.0;
                rows.push_back({psfc, z / 9.81, t2 - 273.15, d2 - 273.15, wd, ws});
            } else if (note) *note = "surface file is missing fields; the profile starts at 1000 hPa";
        }
    }
    int skipped = 0;
    for (auto it = lev.rbegin(); it != lev.rend(); ++it) {          // 1000 hPa first
        const Lv& l = it->second;
        if (!std::isfinite(l.z) || !std::isfinite(l.t) || !std::isfinite(l.u) || !std::isfinite(l.v) || !std::isfinite(l.r)) { ++skipped; continue; }
        if (std::isfinite(psfc) && it->first >= psfc - 0.5) continue;  // at or below the ground
        const double tc = l.t - 273.15;
        double wd, ws; wind(l.u, l.v, wd, ws);
        rows.push_back({double(it->first), l.z / 9.81, tc, era5_dewpoint(tc, std::max(l.r, 0.01)), wd, ws});
    }
    if (rows.size() < 2) { if (err) *err = "fewer than two usable levels in the ERA5 data"; return false; }
    if (skipped && note) *note = (note->isEmpty() ? "" : *note + "; ") + QString("%1 incomplete level(s) skipped").arg(skipped);

    const int y = date / 10000, mo = (date / 100) % 100, d = date % 100;
    title = QString("ERA5_(%1)   %2%3%4/%5").arg(latlonTag(lat, lon)).arg(y % 100, 2, 10, QChar('0')).arg(mo, 2, 10, QChar('0'))
                .arg(d, 2, 10, QChar('0')).arg(time, 4, 10, QChar('0'));
    text = QString("%TITLE%\n%1\n-------------------------------------------------------------------\n%RAW%\n").arg(title);
    for (const Row& r : rows)
        text += QString("%1, %2,  %3, %4, %5, %6\n").arg(r.p, 0, 'f', 2).arg(r.z, 0, 'f', 2).arg(r.t, 0, 'f', 2).arg(r.td, 0, 'f', 2)
                    .arg(r.wd, 0, 'f', 2).arg(r.ws, 0, 'f', 2);
    text += "%END%\n";
    return true;
}

} // namespace sharppy
