#include "sharppy/datasource.hpp"
#include "sharppy/db_pack.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QUrl>
#include <QXmlStreamReader>
#include <algorithm>
#include <map>
#include <set>

namespace sharppy {

namespace {

QDateTime at(const QDate& d, int hour) { return QDateTime(d, QTime(hour, 0), Qt::UTC); }

// Python's datetime.replace(hour=...) keeps minutes zeroed in all our uses.
QDateTime withHour(const QDateTime& t, int hour) {
    QDateTime r(t.date(), QTime(0, 0), Qt::UTC);
    return r.addSecs(static_cast<qint64>(hour) * 3600);
}

// "YYYY/MM/DD" | "now" | "-"
bool parseSpanDate(const QString& s, const QDateTime& nowUtc, QDate& out) {
    const QString v = s.trimmed();
    if (v.isEmpty() || v == "-") return false;
    if (v == "now") { out = nowUtc.date(); return true; }
    const QDate d = QDate::fromString(v, "yyyy/MM/dd");
    if (!d.isValid()) return false;
    out = d;
    return true;
}

QString iemModelName(const QString& dsLower) {
    if (dsLower == "4km nam" || dsLower == "nam nest") return "nam4km";
    return dsLower;
}

// available._available_iem: IEM keeps whole-day bufkit archives older than 24 hours.
QVector<QDateTime> iemTimes(const QString& dsLower, const QDate& day, const QDateTime& nowUtc) {
    QVector<QDateTime> out;
    if (!day.isValid()) return out;   // dt=None -> "now", which is inside the 24 h blackout

    const QString model = iemModelName(dsLower);
    const QDateTime dt = at(day, 0);

    if (dt < at(QDate(2010, 12, 30), 0)) return out;
    if (dt > nowUtc.addSecs(-3600 * 24)) return out;
    if (model == "ruc" && dt > at(QDate(2012, 5, 1), 11)) return out;
    if (model == "nam4km" && dt < at(QDate(2013, 3, 25), 0)) return out;
    if (model == "rap" && dt < at(QDate(2012, 5, 1), 0)) return out;
    if (model == "hrrr" && dt < at(QDate(2019, 8, 24), 0)) return out;

    int start = 0, end = 24, inc = 1;
    if (model == "ruc" || model == "rap") {
        if (day == QDate(2012, 5, 1)) {   // RUC -> RAP switchover day
            if (model == "ruc") { start = 0; end = 12; }
            else { start = 12; end = 24; }
        }
        inc = 1;
    } else if (model == "gfs" || model == "nam" || model == "nam4km") {
        inc = 6;
    } else if (model == "hrrr") {
        start = (day == QDate(2019, 8, 24)) ? 1 : 0;
        inc = 1;
    } else {
        return out;
    }
    for (int h = start; h < end; h += inc) out.append(at(day, h));
    return out;
}

// RFC-4180-ish csv line splitter (Python's csv module semantics, enough for station lists).
QStringList splitCsvLine(const QString& line) {
    QStringList f;
    QString cur;
    bool q = false;
    for (int i = 0; i < line.size(); ++i) {
        const QChar c = line[i];
        if (q) {
            if (c == '"') {
                if (i + 1 < line.size() && line[i + 1] == '"') { cur += '"'; ++i; }
                else q = false;
            } else cur += c;
        } else if (c == '"') q = true;
        else if (c == ',') { f << cur; cur.clear(); }
        else cur += c;
    }
    f << cur;
    return f;
}

DsPoints loadPointsCsv(const QString& label, const QByteArray& bytes, QString* err) {
    if (bytes.isEmpty()) {
        if (err) *err = "cannot open " + label;
        return nullptr;
    }
    const QStringList lines = QString::fromUtf8(bytes).split('\n');

    auto pts = std::make_shared<std::vector<DsPoint>>();
    QStringList header;
    bool first = true;
    for (QString line : lines) {
        if (line.endsWith('\r')) line.chop(1);
        if (line.trimmed().isEmpty()) continue;
        const QStringList f = splitCsvLine(line);
        if (first) {
            for (const QString& h : f) header << h.trimmed();
            first = false;
            continue;
        }
        auto col = [&](const char* nm) -> QString {
            const int i = header.indexOf(QString::fromLatin1(nm));
            return (i >= 0 && i < f.size()) ? f[i].trimmed() : QString();
        };
        DsPoint p;
        p.icao = col("icao").toStdString();
        p.iata = col("iata").toStdString();
        p.synop = col("synop").toStdString();
        p.name = col("name").toStdString();
        p.state = col("state").toStdString();
        p.country = col("country").toStdString();
        p.srcid = col("srcid").toStdString();
        p.priority = col("priority").toStdString();
        bool ok1 = false, ok2 = false;
        p.lat = col("lat").toDouble(&ok1);
        p.lon = col("lon").toDouble(&ok2);
        if (!ok1 || !ok2) continue;   // a point without a position cannot be drawn
        p.elev = static_cast<int>(col("elev").toDouble());
        pts->push_back(std::move(p));
    }
    return pts;
}

bool formatSupported(const QString& f) {
    const QString l = f.toLower();
    return l == "spc" || l == "bufkit" || l == "uwyo";
}

} // namespace

// ---------------------------------------------------------------------------
// DsOutlet
// ---------------------------------------------------------------------------

std::vector<int> DsOutlet::forecastHours() const {
    std::vector<int> t;
    if (delta > 0) {
        for (int h = first; h < range + delta; h += delta) t.push_back(h);
    } else {
        t.push_back(0);
    }
    return t;
}

std::vector<int> DsOutlet::cycles() const {
    std::vector<int> t;
    const int step = cycle > 0 ? cycle : 24;
    for (int h = 0; h < 24; h += step) t.push_back(h + offset);
    return t;
}

void DsOutlet::timeSpan(const QDateTime& nowUtc, QDate& start, QDate& end) const {
    start = QDate();
    end = QDate();
    QDate s, e;
    if (parseSpanDate(startAttr, nowUtc, s)) start = s;
    if (parseSpanDate(endAttr, nowUtc, e)) end = e;
}

bool DsOutlet::customAvail() const {
    const QString o = name.toLower();
    const QString d = dsName.toLower();
    static const QSet<QString> psu = {"gfs", "nam", "rap", "hrrr", "4km nam", "sref"};
    static const QSet<QString> iem = {"gfs", "nam", "rap", "ruc", "nam nest", "hrrr"};
    if (o == "psu") return psu.contains(d);
    if (o == "iem") return iem.contains(d);
    if (o == "sharp") return d == "observed" || d == "ncar ensemble" || d == "goes";
    if (o.startsWith("uwyo")) return d.startsWith("observed");
    return false;
}

QDateTime DsOutlet::mostRecentCycle(const QDateTime& nowUtc) const {
    // Outlet.getMostRecentCycle without the network look-ups: newest cycle that has had
    // `delay` hours to be published.
    const std::vector<int> cyc = cycles();
    QDateTime best;
    for (int hr : cyc) {
        QDateTime run = withHour(nowUtc, hr).addSecs(static_cast<qint64>(delay) * 3600);
        if (run > nowUtc) run = run.addDays(-1);
        if (!best.isValid() || run > best) best = run;
    }
    if (!best.isValid()) return nowUtc;
    return best.addSecs(-static_cast<qint64>(delay) * 3600);
}

QVector<QDateTime> DsOutlet::archivedCycles(const QDateTime& start, int maxCycles) const {
    QVector<QDateTime> out;
    const std::vector<int> daily = cycles();
    const auto it = std::find(daily.begin(), daily.end(), start.time().hour());
    if (it == daily.end() || daily.empty()) {   // Python raises ValueError here
        out.append(start);
        return out;
    }
    int counter = static_cast<int>(it - daily.begin());
    const int n = static_cast<int>(daily.size());

    QDateTime cur = start;
    const QDateTime limit = start.addSecs(-static_cast<qint64>(archive) * 3600);
    while (cur > limit) {
        out.append(cur);
        if (out.size() >= maxCycles) break;

        counter = ((counter - 1) % n + n) % n;
        const int cyc = daily[static_cast<size_t>(counter)];
        cur = withHour(cur, cyc);
        if (cyc == daily.back()) cur = cur.addDays(-1);
    }
    return out;
}

QVector<QDateTime> DsOutlet::availableTimes(const QDate& dt, const QDateTime& nowUtc, int maxCycles) const {
    QVector<QDateTime> times;
    if (!provider.isEmpty()) {
        // Gridded free-pick source (ERA5, model point queries): every cycle of any day between start and now - delay.
        QDate s, e;
        if (!parseSpanDate(startAttr, nowUtc, s)) s = QDate(1940, 1, 1);
        const QDateTime limit = nowUtc.addSecs(-static_cast<qint64>(delay) * 3600);
        QDate lastDay = limit.date();
        if (parseSpanDate(endAttr, nowUtc, e) && e < lastDay) lastDay = e;
        const QDate day = dt.isValid() ? dt : lastDay;
        if (day < s || day > lastDay) return times;
        for (int h : cycles()) {
            const QDateTime t = at(day, h);
            if (t <= limit) times.append(t);
        }
        return times;
    }
    const QString o = name.toLower();
    const QString d = dsName.toLower();
    bool useFallback = !customAvail();

    if (!useFallback) {
        if (o == "iem") {
            times = iemTimes(d, dt, nowUtc);
        } else if ((o == "sharp" && d == "observed") || (o.startsWith("uwyo") && d.startsWith("observed"))) {
            // The Python scrapes the OU archive index. Offline we assume the standard
            // synoptic launches exist for every archived day (03/15Z before May 1957).
            if (!dt.isValid()) {
                useFallback = true;
            } else {
                QDate s;
                if (!parseSpanDate(startAttr, nowUtc, s)) s = QDate(1946, 1, 1);
                if (dt >= s && dt <= nowUtc.date()) {
                    std::vector<int> hrs = cycles();
                    if (o == "sharp" && dt < QDate(1957, 5, 1)) hrs = {3, 15};
                    const QDateTime limit = nowUtc.addSecs(-static_cast<qint64>(delay) * 3600);
                    for (int h : hrs) {
                        const QDateTime t = at(dt, h);
                        if (t <= limit) times.append(t);
                    }
                }
            }
        } else {
            // PSU (ftp index) and the remaining custom sources need the network: use the
            // same fallback the Python takes when the lookup raises URLError.
            useFallback = true;
        }
        if (!useFallback && times.size() == 1) times = archivedCycles(times[0], maxCycles);
    }

    if (useFallback) times = archivedCycles(mostRecentCycle(nowUtc), maxCycles);

    if (times.size() > maxCycles) times = times.mid(times.size() - maxCycles);
    return times;
}

bool DsOutlet::sameStation(const DsPoint& wanted, const std::vector<DsPoint>& candidates) {
    for (const DsPoint& s : candidates) {
        if (!wanted.icao.empty()) {
            if (s.icao == wanted.icao) return true;
        } else if (!wanted.iata.empty()) {
            if (s.iata == wanted.iata) return true;
        } else {
            if (s.synop == wanted.synop) return true;
        }
    }
    return false;
}

const DsPoint* DsOutlet::matchStation(const DsPoint& wanted) const {
    if (!points) return nullptr;
    for (const DsPoint& s : *points) {
        if (!wanted.icao.empty()) { if (s.icao == wanted.icao) return &s; }
        else if (!wanted.iata.empty()) { if (s.iata == wanted.iata) return &s; }
        else if (s.synop == wanted.synop) return &s;
    }
    return nullptr;
}

QString DsOutlet::url(const DsPoint& stn, const QDateTime& c) const {
    QString u = urlTemplate;
    u.replace("{srcid}", QString::fromLatin1(QUrl::toPercentEncoding(QString::fromStdString(stn.srcid), "/")));
    u.replace("{cycle}", QString("%1").arg(c.time().hour(), 2, 10, QChar('0')));
    u.replace("{date}", c.toString("yyMMdd"));
    u.replace("{year}", c.toString("yyyy"));
    u.replace("{month}", c.toString("MM"));
    u.replace("{day}", c.toString("dd"));
    return u;
}

QString DsOutlet::stationListingUrl(const QDateTime& c) const {
    const QString o = name.toLower();
    const QString d = dsName.toLower();
    if (!customAvail()) return QString();
    if (o == "iem") {
        return QString("http://mtarchive.geol.iastate.edu/%1/bufkit/%2/%3/")
            .arg(c.toString("yyyy/MM/dd"), c.toString("HH"), iemModelName(d));
    }
    if (o == "sharp" && d == "observed") {
        return QString("http://sharp.weather.ou.edu/soundings/archive/%1/%2/").arg(c.toString("yyyy/MM/dd"), c.toString("HH"));
    }
    return QString();
}

QRegularExpression DsOutlet::stationListingRegex() const {
    const QString o = name.toLower();
    const QString d = dsName.toLower();
    if (o == "iem") {
        static const std::map<QString, QString> prefix = {{"gfs", "gfs3"},     {"nam", "namm?"}, {"rap", "rap"},
                                                          {"nam4km", "nam4kmm?"}, {"hrrr", "hrrr"}, {"ruc", "ruc"}};
        const auto it = prefix.find(iemModelName(d));
        if (it == prefix.end()) return QRegularExpression();
        return QRegularExpression(it->second + R"(_([^"<>\s]+?)\.buf")");
    }
    if (o == "sharp") return QRegularExpression(R"(a href="([^"]+?)\.txt")");
    return QRegularExpression();
}

// ---------------------------------------------------------------------------
// DataSource
// ---------------------------------------------------------------------------

std::vector<int> DataSource::forecastHours() const {
    std::set<int> s;
    for (const DsOutlet& o : outlets) {
        for (int h : o.forecastHours()) s.insert(h);
    }
    return std::vector<int>(s.begin(), s.end());
}

bool DataSource::hasForecast() const {
    const std::vector<int> f = forecastHours();
    return !(f.size() == 1 && f[0] == 0);
}

QVector<QDateTime> DataSource::availableTimes(const QDate& dt, const QDateTime& nowUtc, int maxCycles) const {
    QSet<qint64> seen;
    QVector<QDateTime> all;
    for (const DsOutlet& o : outlets) {
        for (const QDateTime& t : o.availableTimes(dt, nowUtc, maxCycles)) {
            if (seen.contains(t.toSecsSinceEpoch())) continue;
            seen.insert(t.toSecsSinceEpoch());
            all.append(t);
        }
    }
    std::sort(all.begin(), all.end());
    if (all.size() > maxCycles) all = all.mid(all.size() - maxCycles);
    return all;
}

DsTimeSpan DataSource::calendarSpan(const QVector<QDateTime>& times, const QDateTime& nowUtc) const {
    // Mirrors the loop in Picker.update_run_dropdown: the last outlet decides, and only an
    // outlet with both a start and an end date gives explicit limits.
    DsTimeSpan r;
    bool span = true;
    for (const DsOutlet& o : outlets) {
        QDate s, e;
        o.timeSpan(nowUtc, s, e);
        if (!o.provider.isEmpty() && e.isValid()) e = std::min(e, nowUtc.addSecs(-static_cast<qint64>(o.delay) * 3600).date());   // data lags by `delay`
        if (s.isValid() && e.isValid()) {
            r.earliest = s;
            r.latest = e;
            r.valid = true;
            span = false;
        } else {
            span = true;
        }
    }
    if (span && !times.isEmpty()) {
        r.latest = times.last().date();
        r.earliest = times.first().date();
        r.valid = true;
    }
    return r;
}

std::vector<int> DataSource::outletsWithProfile(const DsPoint& stn, const QDateTime& cycle, const QDateTime& nowUtc,
                                                const std::vector<const QSet<QString>*>& listings) const {
    std::vector<int> use;
    for (size_t i = 0; i < outlets.size(); ++i) {
        const DsOutlet& o = outlets[i];
        if (!o.points) continue;
        if (!o.availableTimes(cycle.date(), nowUtc).contains(cycle)) continue;

        const QSet<QString>* listing = i < listings.size() ? listings[i] : nullptr;
        std::vector<DsPoint> pts;
        pts.reserve(o.points->size());
        for (const DsPoint& p : *o.points) {
            if (listing && !listing->contains(QString::fromStdString(p.srcid))) continue;
            pts.push_back(p);
        }
        if (DsOutlet::sameStation(stn, pts)) use.push_back(static_cast<int>(i));
    }
    return use;
}

std::vector<DsPoint> DataSource::availableAtTime(const std::vector<const QSet<QString>*>& listings) const {
    std::vector<DsPoint> out;
    std::set<std::pair<double, double>> seen;
    for (size_t i = 0; i < outlets.size(); ++i) {
        const DsOutlet& o = outlets[i];
        if (!o.points) continue;
        const QSet<QString>* listing = i < listings.size() ? listings[i] : nullptr;
        for (const DsPoint& p : *o.points) {
            if (listing && !listing->contains(QString::fromStdString(p.srcid))) continue;
            if (!seen.insert({p.lat, p.lon}).second) continue;
            out.push_back(p);
        }
    }
    return out;
}

bool DataSource::freePick() const {
    for (const DsOutlet& o : outlets) if (!o.provider.isEmpty()) return true;
    return false;
}

bool DataSource::supported() const {
    if (outlets.empty()) return false;
    for (const DsOutlet& o : outlets) {
        if (!formatSupported(o.format)) return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Loading
// ---------------------------------------------------------------------------

const DataSource* DataSourceSet::find(const QString& n) const {
    const int i = indexOf(n);
    return i < 0 ? nullptr : &sources[static_cast<size_t>(i)];
}

int DataSourceSet::indexOf(const QString& n) const {
    for (size_t i = 0; i < sources.size(); ++i) {
        if (sources[i].name == n) return static_cast<int>(i);
    }
    return -1;
}

namespace {

bool dirHasXml(const QString& path) {
    const QDir d(path);
    return d.exists() && !d.entryList(QStringList() << "*.xml", QDir::Files).isEmpty();
}

void addWithParents(QStringList& out, QDir dir, int levels) {
    for (int i = 0; i <= levels; ++i) {
        out << dir.absoluteFilePath("data/datasources") << dir.absoluteFilePath("datasources");
        if (!dir.cdUp()) break;
    }
}

} // namespace

QString findDataSourceDir() {
    QStringList cands;
    const QString env = qEnvironmentVariable("SHARPPY_DATASOURCES");
    if (!env.isEmpty()) cands << env;

    addWithParents(cands, QDir(QCoreApplication::applicationDirPath()), 6);
    addWithParents(cands, QDir::current(), 2);
    cands << QDir::homePath() + "/.sharppy/datasources";

    for (const QString& c : cands) {
        if (dirHasXml(c)) return QDir(c).absolutePath();
    }
    return QString();
}

DataSourceSet loadDataSources(const QString& dirIn) {
    DataSourceSet set;
    set.dir = dirIn.isEmpty() ? findDataSourceDir() : dirIn;
    // Datasource files may also live inside the data pack (datasources/ entries); loose files win so they can be edited.
    const QString pack = dbpack_find();
    const QStringList packed = pack.isEmpty() ? QStringList() : dbpack_list(pack, "datasources");
    if (set.dir.isEmpty() && !packed.isEmpty()) set.dir = pack;
    auto readAsset = [&](const QString& name) -> QByteArray {
        QFile lf(QDir(set.dir).absoluteFilePath(name));
        if (set.dir != pack && lf.open(QIODevice::ReadOnly)) return lf.readAll();
        return packed.contains(name) ? dbpack_read(pack, "datasources/" + name) : QByteArray();
    };
    if (set.dir.isEmpty()) {
        set.warnings << "No datasource folder found (looked for data/datasources next to the program, "
                        "SHARPPY_DATASOURCES and ~/.sharppy/datasources).";
        return set;
    }

    const QDir dir(set.dir);
    QStringList files = set.dir == pack ? QStringList() : dir.entryList(QStringList() << "*.xml", QDir::Files, QDir::Name);
    for (const QString& n : packed) if (n.endsWith(".xml", Qt::CaseInsensitive) && !files.contains(n)) files << n;
    files.sort();

    std::map<QString, DsPoints> csvCache;
    std::map<QString, DataSource> byName;

    for (const QString& fn : files) {
        const QByteArray xmlBytes = readAsset(fn);
        if (xmlBytes.isEmpty()) {
            set.warnings << "Unable to open " + fn;
            continue;
        }
        QXmlStreamReader xml(xmlBytes);

        DataSource cur;
        DsOutlet* curOutlet = nullptr;
        bool inSource = false;
        bool broken = false;

        auto finishSource = [&]() {
            if (!inSource) return;
            inSource = false;
            if (broken) {
                set.warnings << QString("Unable to process data source '%1' (%2)").arg(cur.name, fn);
            } else {
                byName[cur.name] = cur;
            }
        };

        while (!xml.atEnd()) {
            xml.readNext();
            if (xml.isStartElement()) {
                const QString tag = xml.name().toString();
                const auto a = xml.attributes();
                if (tag == "datasource") {
                    cur = DataSource();
                    broken = false;
                    inSource = true;
                    cur.name = a.value("name").toString();
                    cur.ensemble = a.value("ensemble").toString().toLower() == "true";
                    cur.observed = a.value("observed").toString().toLower() == "true";
                    curOutlet = nullptr;
                } else if (tag == "outlet" && inSource) {
                    DsOutlet o;
                    o.dsName = cur.name;
                    o.name = a.value("name").toString();
                    o.urlTemplate = a.value("url").toString();
                    o.format = a.value("format").toString();
                    o.provider = a.value("provider").toString().toLower();
                    o.modelId = a.value("model").toString();
                    cur.outlets.push_back(o);
                    curOutlet = &cur.outlets.back();
                } else if (tag == "time" && curOutlet) {
                    auto num = [&](const char* k, int def) {
                        bool ok = false;
                        const int v = a.value(QLatin1String(k)).toString().toInt(&ok);
                        return ok ? v : def;
                    };
                    curOutlet->first = num("first", 0);
                    curOutlet->range = num("range", 0);
                    curOutlet->delta = num("delta", 0);
                    curOutlet->offset = num("offset", 0);
                    curOutlet->delay = num("delay", 0);
                    curOutlet->cycle = num("cycle", 24);
                    curOutlet->archive = num("archive", 0);
                    if (a.hasAttribute("start")) curOutlet->startAttr = a.value("start").toString();
                    if (a.hasAttribute("end")) curOutlet->endAttr = a.value("end").toString();
                } else if (tag == "points" && curOutlet) {
                    curOutlet->csvName = a.value("csv").toString();
                    auto it = csvCache.find(curOutlet->csvName);
                    if (it == csvCache.end()) {
                        QString err;
                        DsPoints pts = loadPointsCsv(curOutlet->csvName, readAsset(curOutlet->csvName), &err);
                        if (!pts) {
                            set.warnings << QString("Data source '%1': %2").arg(cur.name, err);
                            broken = true;
                        }
                        it = csvCache.emplace(curOutlet->csvName, pts).first;
                    }
                    curOutlet->points = it->second;
                    if (!curOutlet->points) broken = true;
                }
            } else if (xml.isEndElement()) {
                const QString tag = xml.name().toString();
                if (tag == "outlet") curOutlet = nullptr;
                else if (tag == "datasource") finishSource();
            }
        }
        if (xml.hasError()) set.warnings << QString("%1: %2").arg(fn, xml.errorString());
    }

    for (auto& kv : byName) {
        if (!kv.second.supported()) {
            set.warnings << QString("Data source '%1' uses a format this port cannot decode yet; hidden.").arg(kv.first);
            continue;
        }
        set.sources.push_back(kv.second);
    }
    std::sort(set.sources.begin(), set.sources.end(),
              [](const DataSource& a, const DataSource& b) { return a.name < b.name; });
    return set;
}

} // namespace sharppy
