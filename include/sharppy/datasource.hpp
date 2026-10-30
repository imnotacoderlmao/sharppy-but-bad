#pragma once

// Port of SHARPpy's datasources/data_source.py (+ the offline parts of available.py).
//
// Data sources are described by XML files (<datasource><outlet><time .../><points csv=.../>)
// next to CSV station lists. Both are read at run time from a "datasources" directory, the same
// files the Python application keeps in ~/.sharppy/datasources.

#include <QDate>
#include <QDateTime>
#include <QRegularExpression>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>
#include <memory>
#include <string>
#include <vector>

namespace sharppy {

// One row of a station csv (the dict SHARPpy keeps per point).
struct DsPoint {
    std::string icao, iata, synop, name, state, country, srcid;
    double lat = 0.0;
    double lon = 0.0;
    int elev = 0;
    std::string priority;
};

using DsPoints = std::shared_ptr<const std::vector<DsPoint>>;

// Calendar limits derived from a source (Picker.update_run_dropdown.update()).
struct DsTimeSpan {
    bool valid = false;
    QDate earliest;
    QDate latest;
};

class DsOutlet {
public:
    QString dsName;       // owning datasource, e.g. "GFS"
    QString name;         // "PSU", "IEM", "SPC", "SHARP", ...
    QString urlTemplate;  // with {srcid} {cycle} {date} {year} {month} {day}
    QString format;       // "spc", "bufkit", "pecan", "nucaps"
    QString csvName;
    int first = 0, range = 0, delta = 0, offset = 0, delay = 0, cycle = 24, archive = 0;
    QString startAttr = "-", endAttr = "-";
    DsPoints points;

    std::vector<int> forecastHours() const;
    std::vector<int> cycles() const;

    // [start, end]; an invalid date means "-" (open).
    void timeSpan(const QDateTime& nowUtc, QDate& start, QDate& end) const;

    // True when available.py has a custom time lookup for this outlet/datasource pair.
    bool customAvail() const;

    QDateTime mostRecentCycle(const QDateTime& nowUtc) const;
    QVector<QDateTime> archivedCycles(const QDateTime& start, int maxCycles = 100) const;

    // Times this outlet can serve. `dt` (when valid) is the calendar day being looked at.
    QVector<QDateTime> availableTimes(const QDate& dt, const QDateTime& nowUtc, int maxCycles = 100) const;

    // Port of Outlet.hasProfile's station test (icao, then iata, then synop).
    static bool sameStation(const DsPoint& wanted, const std::vector<DsPoint>& candidates);

    QString url(const DsPoint& stn, const QDateTime& cycle) const;

    // Directory listing that tells which stations really have data at `cycle`
    // (available.availableat). Empty URL when the outlet has no such listing here.
    QString stationListingUrl(const QDateTime& cycle) const;
    QRegularExpression stationListingRegex() const;
};

class DataSource {
public:
    QString name;
    bool ensemble = false;
    bool observed = false;
    std::vector<DsOutlet> outlets;

    std::vector<int> forecastHours() const;   // union over outlets, sorted
    bool hasForecast() const;                 // forecastHours() != [0]

    // Union of every outlet's times for the day `dt` (invalid QDate = "latest"), sorted.
    QVector<QDateTime> availableTimes(const QDate& dt, const QDateTime& nowUtc, int maxCycles = 100) const;

    DsTimeSpan calendarSpan(const QVector<QDateTime>& times, const QDateTime& nowUtc) const;

    // Outlets that can serve `stn` at `cycle` (order = fetch order). `listings[i]`, when
    // non-null, is the set of station ids really present at that time for outlet i.
    std::vector<int> outletsWithProfile(const DsPoint& stn, const QDateTime& cycle, const QDateTime& nowUtc,
                                        const std::vector<const QSet<QString>*>& listings) const;

    // Stations to show on the map: every outlet's points (filtered by `listings`), de-duplicated
    // by position, earlier outlets winning.
    std::vector<DsPoint> availableAtTime(const std::vector<const QSet<QString>*>& listings) const;

    // True when every outlet uses a format this port can decode.
    bool supported() const;
};

struct DataSourceSet {
    std::vector<DataSource> sources;   // sorted by name, like sorted(ds.keys())
    QString dir;
    QStringList warnings;

    const DataSource* find(const QString& name) const;
    int indexOf(const QString& name) const;
};

// Folder holding the *.xml / *.csv files, or an empty string when none is found.
// Search order: $SHARPPY_DATASOURCES, then data/datasources (or datasources) next to the
// executable, its parent folders (so build/ trees work), the current folder, and finally
// ~/.sharppy/datasources.
QString findDataSourceDir();

// Reads every *.xml in `dir` (default: findDataSourceDir()). Sources that cannot be read are
// skipped and reported in `warnings`; sources with unsupported formats are dropped too.
DataSourceSet loadDataSources(const QString& dir = QString());

} // namespace sharppy
