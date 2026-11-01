#include "sharppy/openmeteo.hpp"
#include "sharppy/era5.hpp"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>
#include <algorithm>
#include <cmath>

namespace sharppy {

namespace {
const int kLevels[] = {1000, 975, 950, 925, 900, 875, 850, 825, 800, 750, 700, 650, 600, 550, 500, 450, 400, 350, 300,
                       250, 200, 175, 150, 125, 100, 70, 50, 30, 20, 10};

QStringList variables() {
    QStringList v;
    for (int l : kLevels)
        v << QString("temperature_%1hPa").arg(l) << QString("relative_humidity_%1hPa").arg(l) << QString("geopotential_height_%1hPa").arg(l)
          << QString("wind_speed_%1hPa").arg(l) << QString("wind_direction_%1hPa").arg(l);
    v << "surface_pressure" << "temperature_2m" << "dew_point_2m" << "wind_speed_10m" << "wind_direction_10m";
    return v;
}
}

QUrl openmeteo_url(const QString& model, double lat, double lon, const QVector<QDateTime>& valid, const QDateTime& nowUtc) {
    QDate d0 = valid.first().date(), d1 = valid.first().date();
    for (const QDateTime& t : valid) { d0 = std::min(d0, t.date()); d1 = std::max(d1, t.date()); }
    const bool recent = d0 >= nowUtc.date().addDays(-80);
    const QString envBase = qEnvironmentVariable("SHARPPY_OPENMETEO_URL");     // test hook: a local mock server
    QUrl url(!envBase.isEmpty() ? envBase : recent ? "https://api.open-meteo.com/v1/forecast" : "https://historical-forecast-api.open-meteo.com/v1/forecast");
    QUrlQuery q;
    q.addQueryItem("latitude", QString::number(lat, 'f', 4));
    q.addQueryItem("longitude", QString::number(lon, 'f', 4));
    q.addQueryItem("models", model);
    q.addQueryItem("hourly", variables().join(','));
    q.addQueryItem("wind_speed_unit", "kn");
    q.addQueryItem("timezone", "GMT");
    q.addQueryItem("start_date", d0.toString("yyyy-MM-dd"));
    q.addQueryItem("end_date", d1.toString("yyyy-MM-dd"));
    url.setQuery(q);
    return url;
}

bool openmeteo_build_texts(const QByteArray& json, const QString& label, double lat, double lon, const QVector<QDateTime>& valid,
                           const QDateTime& run, QStringList& texts, QStringList& titles, QVector<int>& okIdx, QString* err) {
    texts.clear(); titles.clear(); okIdx.clear();
    const QJsonObject root = QJsonDocument::fromJson(json).object();
    if (root.isEmpty()) { if (err) *err = "the reply is not JSON"; return false; }
    if (root.value("error").toBool()) { if (err) *err = root.value("reason").toString("the service reported an error"); return false; }
    const QJsonObject h = root.value("hourly").toObject();
    const QJsonArray times = h.value("time").toArray();
    if (times.isEmpty()) { if (err) *err = "the reply has no hourly data"; return false; }
    auto at = [&](const QString& key, int i, double& out) {
        const QJsonArray a = h.value(key).toArray();
        if (i >= a.size() || a.at(i).isNull() || !a.at(i).isDouble()) return false;
        out = a.at(i).toDouble();
        return std::isfinite(out);
    };
    const double elev = root.value("elevation").toDouble(0.0);
    const QString tag = QString("%1%2,%3%4").arg(lat >= 0 ? "N" : "S").arg(std::fabs(lat), 0, 'f', 2).arg(lon >= 0 ? "E" : "W").arg(std::fabs(lon), 0, 'f', 2);

    for (int vi = 0; vi < valid.size(); ++vi) {
        const QString key = valid[vi].toUTC().toString("yyyy-MM-dd'T'HH:mm");
        int i = -1;
        for (int k = 0; k < times.size(); ++k) if (times.at(k).toString() == key) { i = k; break; }
        if (i < 0) continue;

        struct Row { double p, z, t, td, wd, ws; };
        std::vector<Row> rows;
        double psfc = NAN;
        double sp, t2, d2, ws10, wd10;
        if (at("surface_pressure", i, sp) && at("temperature_2m", i, t2) && at("dew_point_2m", i, d2) &&
            at("wind_speed_10m", i, ws10) && at("wind_direction_10m", i, wd10)) {
            psfc = sp;
            rows.push_back({sp, elev, t2, d2, wd10, ws10});
        }
        for (int l : kLevels) {
            double t, r, z, ws, wd;
            if (!at(QString("temperature_%1hPa").arg(l), i, t) || !at(QString("relative_humidity_%1hPa").arg(l), i, r) ||
                !at(QString("geopotential_height_%1hPa").arg(l), i, z) || !at(QString("wind_speed_%1hPa").arg(l), i, ws) ||
                !at(QString("wind_direction_%1hPa").arg(l), i, wd)) continue;
            if (std::isfinite(psfc) && l >= psfc - 0.5) continue;               // at or below the ground
            rows.push_back({double(l), z, t, era5_dewpoint(t, std::max(r, 0.01)), wd, ws});
        }
        if (rows.size() < 4) continue;
        const int fh = int(run.secsTo(valid[vi]) / 3600);
        const QString title = QString("%1_(%2)   %3/%4  F%5").arg(label, tag, valid[vi].toString("yyMMdd"), valid[vi].toString("HHmm"))
                                  .arg(fh, 3, 10, QChar('0'));
        QString text = QString("%TITLE%\n%1\n-------------------------------------------------------------------\n%RAW%\n").arg(title);
        for (const Row& r : rows)
            text += QString("%1, %2,  %3, %4, %5, %6\n").arg(r.p, 0, 'f', 2).arg(r.z, 0, 'f', 2).arg(r.t, 0, 'f', 2).arg(r.td, 0, 'f', 2)
                        .arg(r.wd, 0, 'f', 2).arg(r.ws, 0, 'f', 2);
        text += "%END%\n";
        texts << text; titles << title; okIdx << vi;
    }
    if (texts.isEmpty()) { if (err) *err = "the model has no data for this point and time (outside the model's domain or coverage?)"; return false; }
    return true;
}

} // namespace sharppy
