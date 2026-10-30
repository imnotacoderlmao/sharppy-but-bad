#include "sharppy/map_mapper.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QtEndian>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace sharppy {

namespace {

constexpr double kPi = 3.14159265358979323846;
inline double rad(double d) { return d * (kPi / 180.0); }
inline double deg(double r) { return r * (180.0 / kPi); }

QString g_dataDir;

double minLat(MapProj p) {
    switch (p) {
        case MapProj::NPStere: return 0.0;
        case MapProj::Merc:    return -35.0;
        case MapProj::SPStere: return -90.0;
    }
    return 0.0;
}
double maxLat(MapProj p) {
    switch (p) {
        case MapProj::NPStere: return 90.0;
        case MapProj::Merc:    return 35.0;
        case MapProj::SPStere: return 0.0;
    }
    return 0.0;
}

} // namespace

QString mapProjName(MapProj p) {
    switch (p) {
        case MapProj::NPStere: return "npstere";
        case MapProj::Merc:    return "merc";
        case MapProj::SPStere: return "spstere";
    }
    return "npstere";
}

bool mapProjFromName(const QString& s, MapProj& out) {
    if (s == "npstere") { out = MapProj::NPStere; return true; }
    if (s == "merc")    { out = MapProj::Merc;    return true; }
    if (s == "spstere") { out = MapProj::SPStere; return true; }
    return false;
}

Mapper::Mapper(double lambda0, double phi0, MapProj proj)
    : m_proj(proj), m_lambda0(lambda0), m_phi0(phi0) {
    if (proj == MapProj::SPStere) m_phi0 = -std::abs(m_phi0);
}

void Mapper::setProjection(MapProj p) {
    m_proj = p;
    if (p == MapProj::SPStere) m_phi0 = -std::abs(m_phi0);
    else if (p == MapProj::NPStere) m_phi0 = std::abs(m_phi0);
}

void Mapper::latBounds(double& lb, double& ub) const {
    lb = minLat(m_proj);
    ub = maxLat(m_proj);
}

QPointF Mapper::toXY(double lat, double lon) const {
    const double mr = m_m * m_radEarth;
    if (m_proj == MapProj::Merc) {
        double x = mr * (rad(lon) - rad(m_lambda0));
        double y = -mr * std::log(std::tan(kPi / 4.0 + rad(lat) / 2.0));
        return QPointF(x, y);
    }
    const bool south = (m_phi0 < 0);
    const double sign = south ? -1.0 : 1.0;
    double sigma = (1.0 + std::sin(rad(sign * m_phi0))) / (1.0 + std::sin(rad(sign * lat)));
    double lam = rad(sign * ((m_lambda0 + 90.0) - lon));
    double x = mr * sigma * std::cos(rad(lat)) * std::cos(lam);
    double y = mr * sigma * std::cos(rad(lat)) * std::sin(lam);
    return QPointF(x, y);
}

void Mapper::toLatLon(double x, double y, double& lat, double& lon) const {
    const double mr = m_m * m_radEarth;
    if (m_proj == MapProj::Merc) {
        lon = deg(rad(m_lambda0) + x / mr);
        lat = -deg(2.0 * std::atan(std::exp(y / mr)) - kPi / 2.0);
        return;
    }
    const double sign = (m_phi0 < 0) ? -1.0 : 1.0;
    lon = m_lambda0 + 90.0 - sign * deg(std::atan2(y, x));
    double r = std::hypot(x, y);
    if (r < 1e-12) r = 1e-12;
    lat = sign * deg(2.0 * std::atan(mr * (1.0 + sign * std::sin(rad(m_phi0))) / r) - kPi / 2.0);
    if (lon < -180) lon += 360;
    else if (lon > 180) lon -= 360;
}

QPainterPath Mapper::coordPaths() const {
    QPainterPath path;
    double lb, ub;
    latBounds(lb, ub);

    auto line = [&](double lat0, double lon0, double lat1, double lon1) {
        QPointF a = toXY(lat0, lon0), b = toXY(lat1, lon1);
        path.moveTo(a);
        path.lineTo(b);
    };
    auto ringThrough4 = [&](double lat) {
        double xmin = 1e300, xmax = -1e300, ymin = 1e300, ymax = -1e300;
        for (int k = 0; k < 4; ++k) {
            QPointF p = toXY(lat, m_lambda0 + 90.0 * k);
            xmin = std::min(xmin, p.x()); xmax = std::max(xmax, p.x());
            ymin = std::min(ymin, p.y()); ymax = std::max(ymax, p.y());
        }
        path.addEllipse(xmin, ymin, xmax - xmin, ymax - ymin);
    };

    if (m_proj == MapProj::NPStere) {
        for (int lon = 0; lon < 360; lon += 20) line(lb, lon, ub, lon);
        for (int lat = static_cast<int>(lb); lat < static_cast<int>(ub); lat += 15) ringThrough4(lat);
    } else if (m_proj == MapProj::Merc) {
        for (int lon = -180; lon < 180 + 20; lon += 20) line(lb, lon, ub, lon);

        const double spc = 10.0;
        double rlb = std::ceil(lb / spc) * spc;
        double rub = std::floor(ub / spc) * spc;
        std::vector<int> lats;
        for (int l = static_cast<int>(rlb); l < static_cast<int>(rub + spc); l += static_cast<int>(spc)) lats.push_back(l);
        if (rlb != lb) lats.insert(lats.begin(), static_cast<int>(lb));
        if (rub != ub) lats.push_back(static_cast<int>(ub));
        for (int lat : lats) line(lat, -180, lat, 180);
    } else { // SPStere
        for (int lon = 0; lon < 360; lon += 20) line(lb, lon, ub, lon);
        for (int lat = static_cast<int>(ub); lat > static_cast<int>(lb); lat -= 15) ringThrough4(lat);
    }
    return path;
}

// ---------------------------------------------------------------------------
// Boundary data (Basemap .dat format, as read by SHARPpy)
// ---------------------------------------------------------------------------

void Mapper::setDataDir(const QString& dir) { g_dataDir = dir; }

QString Mapper::findDataDir() {
    QStringList candidates;
    const QString env = qEnvironmentVariable("SHARPPY_SHAPEFILES");
    if (!env.isEmpty()) candidates << env;
    const QString exe = QCoreApplication::applicationDirPath();
    candidates << exe + "/data/shapefiles"
               << exe + "/shapefiles"
               << exe + "/databases/shapefiles"
               << exe + "/sharppy/databases/shapefiles"
               << QDir::currentPath() + "/data/shapefiles"
               << QDir::currentPath() + "/shapefiles"
               << QDir::currentPath() + "/databases/shapefiles"
               << QDir::currentPath() + "/../data/shapefiles"
               << QDir::homePath() + "/.sharppy/shapefiles";
    for (const QString& c : candidates) {
        if (QFileInfo(c + "/gshhsmeta_i.dat").exists() || QFileInfo(c + "/gshhs_i.dat").exists() ||
            QFileInfo(c + "/statesmeta_h.dat").exists() || QFileInfo(c + "/statesmeta_i.dat").exists()) {
            return c;
        }
    }
    return QString();
}

QString Mapper::dataDir() {
    if (g_dataDir.isEmpty()) g_dataDir = findDataDir();
    return g_dataDir;
}

Mapper::ProjLines Mapper::loadDat(const QString& name, const QString& res) const {
    ProjLines out;
    const MapProj projs[] = {MapProj::NPStere, MapProj::Merc, MapProj::SPStere};
    for (MapProj p : projs) out[p] = {};

    const QString dir = dataDir();
    if (dir.isEmpty()) return out;

    // Prefer the resolution SHARPpy asks for, but fall back to what's installed.
    QStringList order;
    order << res;
    for (const char* r : {"h", "i", "l", "c"}) if (res != r) order << r;
    if (res == "f") order = QStringList{"f", "h", "i", "l", "c"};

    QString datPath, metaPath;
    for (const QString& r : order) {
        QString d = dir + "/" + name + "_" + r + ".dat";
        QString m = dir + "/" + name + "meta_" + r + ".dat";
        if (QFileInfo(d).exists() && QFileInfo(m).exists()) { datPath = d; metaPath = m; break; }
    }
    if (datPath.isEmpty()) return out;

    QFile bdat(datPath), bmeta(metaPath);
    if (!bdat.open(QIODevice::ReadOnly) || !bmeta.open(QIODevice::ReadOnly | QIODevice::Text)) return out;
    const qint64 fileSize = bdat.size();

    while (!bmeta.atEnd()) {
        const QString line = QString::fromUtf8(bmeta.readLine()).trimmed();
        if (line.isEmpty()) continue;
        const QStringList f = line.simplified().split(QChar(' '), Qt::SkipEmptyParts);
        if (f.size() < 7) continue;

        bool ok1, ok2, ok3, ok4;
        double area = f[1].toDouble(&ok1);
        qint64 npts = f[2].toLongLong(&ok2);
        qint64 offset = f[5].toLongLong(&ok3);
        qint64 bytecount = f[6].toLongLong(&ok4);
        if (!(ok1 && ok2 && ok3 && ok4)) continue;
        if (area < 0) area = 1e30;
        if (area <= 1500.0) continue;
        if (npts < 2 || bytecount != npts * 8 || offset < 0 || offset + bytecount > fileSize) continue;

        bdat.seek(offset);
        const QByteArray raw = bdat.read(bytecount);
        if (raw.size() != bytecount) continue;

        std::vector<double> lon(npts), lat(npts);
        bool wrap = false;
        for (qint64 i = 0; i < npts; ++i) {
            quint32 a, b;
            std::memcpy(&a, raw.constData() + i * 8, 4);
            std::memcpy(&b, raw.constData() + i * 8 + 4, 4);
            a = qFromLittleEndian<quint32>(a);
            b = qFromLittleEndian<quint32>(b);
            float fa, fb;
            std::memcpy(&fa, &a, 4);
            std::memcpy(&fb, &b, 4);
            lon[i] = fa;
            lat[i] = fb;
            if (lon[i] > 180) wrap = true;
        }
        // SHARPpy shifts the whole polygon when any point is past 180 E.
        if (wrap) for (double& l : lon) l -= 360.0;

        double latMax = *std::max_element(lat.begin(), lat.end());
        double latMin = *std::min_element(lat.begin(), lat.end());

        for (MapProj p : projs) {
            const double lb = minLat(p), ub = maxLat(p);
            if (latMax <= lb || latMin >= ub) continue;

            // Split into runs of consecutive points inside [lb, ub].
            Line cur;
            auto flush = [&]() {
                if (cur.latlon.size() >= 2) out[p].push_back(std::move(cur));
                cur = Line();
            };
            for (qint64 i = 0; i < npts; ++i) {
                if (lat[i] >= lb && lat[i] <= ub) cur.latlon.emplace_back(lat[i], lon[i]);
                else flush();
            }
            flush();
        }
    }
    return out;
}

QVector<QPainterPath> Mapper::boundary(const QString& nameIn) {
    QString name = nameIn;
    if (name == "coastlines") name = "gshhs";

    QString res;
    if (name == "states") res = "h";
    else if (name == "uscounties") res = "f";
    else res = "i";

    if (!m_bnds.count(name)) m_bnds[name] = loadDat(name, res);

    QVector<QPainterPath> paths;
    const auto& lines = m_bnds[name][m_proj];
    paths.reserve(static_cast<int>(lines.size()));
    for (const Line& ln : lines) {
        QPainterPath path;
        bool first = true;
        for (const auto& ll : ln.latlon) {
            QPointF p = toXY(ll.first, ll.second);
            if (!std::isfinite(p.x()) || !std::isfinite(p.y())) continue;
            if (first) { path.moveTo(p); first = false; }
            else path.lineTo(p);
        }
        if (!first) paths.append(path);
    }
    return paths;
}

} // namespace sharppy
