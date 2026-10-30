#pragma once

#include <QPainterPath>
#include <QPointF>
#include <QString>
#include <QVector>
#include <map>
#include <utility>
#include <vector>

namespace sharppy {

enum class MapProj { NPStere, Merc, SPStere };

QString mapProjName(MapProj p);            // "npstere" / "merc" / "spstere"
bool mapProjFromName(const QString& s, MapProj& out);

// Port of SHARPpy's viz/map.py Mapper: lat/lon <-> map x/y for a north polar
// stereographic, Mercator, or south polar stereographic projection, the lat/lon
// grid, and the Basemap-format boundary .dat files (coastlines, countries,
// states, US counties).
class Mapper {
public:
    Mapper(double lambda0, double phi0, MapProj proj = MapProj::NPStere);

    double lambda0() const { return m_lambda0; }
    void setLambda0(double l) { m_lambda0 = l; }
    double phi0() const { return m_phi0; }

    MapProj projection() const { return m_proj; }
    void setProjection(MapProj p);

    void latBounds(double& lb, double& ub) const;

    // lat/lon (deg) -> map units
    QPointF toXY(double lat, double lon) const;
    // map units -> lat/lon (deg)
    void toLatLon(double x, double y, double& lat, double& lon) const;

    // Lat/lon grid
    QPainterPath coordPaths() const;

    // Boundary line work for the current projection. `name` is one of
    // "coastlines", "countries", "states", "uscounties". Missing data files
    // yield an empty list (the map simply has no such layer).
    QVector<QPainterPath> boundary(const QString& name);

    // Directory searched for <name>_<res>.dat / <name>meta_<res>.dat.
    static void setDataDir(const QString& dir);
    static QString dataDir();
    static QString findDataDir();   // resolves the default search order

private:
    struct Line { std::vector<std::pair<double, double>> latlon; };  // (lat, lon)
    using ProjLines = std::map<MapProj, std::vector<Line>>;

    MapProj m_proj;
    double m_lambda0;
    double m_phi0;
    static constexpr double m_m = 6.6667e-7;
    static constexpr double m_radEarth = 6.371e8;
    std::map<QString, ProjLines> m_bnds;

    ProjLines loadDat(const QString& name, const QString& res) const;
};

} // namespace sharppy
