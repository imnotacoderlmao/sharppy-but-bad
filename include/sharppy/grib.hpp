#pragma once
// Minimal GRIB1 reader: regular lat/lon grids with simple packing, which is what ECMWF's ERA5 downloads from the CDS use
// (data_format "grib"). Anything else (GRIB2, complex packing, bitmaps, reduced grids) is reported as unsupported.
#include <QByteArray>
#include <QString>
#include <vector>

namespace sharppy {

struct GribField {
    int centre = 0, table = 0, param = 0;     // PDS: originating centre, parameter table version, indicator of parameter
    int levelType = 0, level = 0;             // e.g. 100 = isobaric (hPa), 1 = surface, 105 = height above ground (m)
    int date = 0, time = 0;                   // yyyymmdd, hhmm (reference time)
    int ni = 0, nj = 0;
    double la1 = 0, lo1 = 0, la2 = 0, lo2 = 0, di = 0, dj = 0;   // degrees
    bool iNegative = false, jPositive = false, jConsecutive = false;
    std::vector<double> v;                    // ni*nj values in the file's scan order
    // value at grid column i (0..ni-1 along longitude) and row j (0..nj-1 along latitude), both in scan order
    double at(int i, int j) const { return jConsecutive ? v[size_t(i) * nj + j] : v[size_t(j) * ni + i]; }
    double lat(int j) const { return jPositive ? la1 + j * dj : la1 - j * dj; }
    double lon(int i) const { return iNegative ? lo1 - i * di : lo1 + i * di; }
};

// Decodes every message in the buffer. False (with err) when nothing could be decoded.
bool grib1_decode(const QByteArray& data, std::vector<GribField>& out, QString* err = nullptr);

} // namespace sharppy
