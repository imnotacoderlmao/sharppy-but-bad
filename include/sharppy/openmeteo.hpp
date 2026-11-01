#pragma once
// Model soundings for an arbitrary lat/lon from Open-Meteo's gridded point API (pressure-level variables, JSON, no key).
// The service returns the model's best data for each valid time (the newest run covering it), not one specific run.
#include <QDateTime>
#include <QStringList>
#include <QUrl>
#include <QVector>

namespace sharppy {

// Request url for the valid times `valid` (UTC). Recent times use the forecast API, older ones the historical-forecast API.
QUrl openmeteo_url(const QString& model, double lat, double lon, const QVector<QDateTime>& valid, const QDateTime& nowUtc);

// Turns the JSON reply into one SHARPpy text sounding per entry of `valid` (same order). Entries whose time is missing or
// has no data are skipped; their indexes are left out of `okIdx`. `label` titles the profile ("GFS", ...).
bool openmeteo_build_texts(const QByteArray& json, const QString& label, double lat, double lon, const QVector<QDateTime>& valid,
                           const QDateTime& run, QStringList& texts, QStringList& titles, QVector<int>& okIdx, QString* err = nullptr);

} // namespace sharppy
