#pragma once
// ERA5 reanalysis soundings: request bodies for the Copernicus CDS and the conversion of the returned GRIB files into a
// SHARPpy text sounding. The conversion is a port of the netCDF/MetPy script this feature is based on
// (nearest grid point, z/9.81, t-273.15, dewpoint from relative humidity, wdir = atan2(u,v)+180, wspd = hypot*1.944).
#include <QByteArray>
#include <QDateTime>
#include <QJsonObject>
#include <QString>

namespace sharppy {

// "inputs" of the CDS retrieve job for the pressure-level fields (z, t, u, v, r on all 37 levels) around the point.
QJsonObject era5_pressure_inputs(const QDateTime& utc, double lat, double lon);
// Surface fields (surface pressure, orography, 2 m temperature and dewpoint, 10 m wind) so the profile starts at the ground.
QJsonObject era5_surface_inputs(const QDateTime& utc, double lat, double lon);

// MetPy 1.x dewpoint_from_relative_humidity: temperature in degC, relative humidity in percent -> degC.
double era5_dewpoint(double tempC, double rhPercent);

// Builds the sounding text (%TITLE% / %RAW% / %END%). `surfaceGrib` may be empty: the profile then starts at the lowest
// pressure level (1000 hPa), which can lie below the real ground. `note` receives a warning when something was skipped.
bool era5_build_text(const QByteArray& pressureGrib, const QByteArray& surfaceGrib, double lat, double lon,
                     QString& text, QString& title, QString* err = nullptr, QString* note = nullptr);

} // namespace sharppy
