#include "sharppy/grib.hpp"
#include <cmath>

namespace sharppy {

namespace {
struct Rd {
    const uchar* p; size_t n;
    unsigned u1(size_t o) const { return p[o]; }
    unsigned u2(size_t o) const { return (p[o] << 8) | p[o + 1]; }
    unsigned u3(size_t o) const { return (p[o] << 16) | (p[o + 1] << 8) | p[o + 2]; }
    quint32 u4(size_t o) const { return (quint32(p[o]) << 24) | (quint32(p[o + 1]) << 16) | (quint32(p[o + 2]) << 8) | p[o + 3]; }
    // GRIB sign-magnitude integers (top bit is the sign)
    int s2(size_t o) const { int v = int(u2(o) & 0x7fff); return (p[o] & 0x80) ? -v : v; }
    int s3(size_t o) const { int v = int(u3(o) & 0x7fffff); return (p[o] & 0x80) ? -v : v; }
    // IBM System/360 single precision
    double ibm(size_t o) const {
        const quint32 w = u4(o);
        if ((w & 0x7fffffff) == 0) return 0.0;
        const double mant = double(w & 0x00ffffff) / 16777216.0;
        const int e = int((w >> 24) & 0x7f) - 64;
        const double v = mant * std::pow(16.0, e);
        return (w & 0x80000000u) ? -v : v;
    }
};
}

bool grib1_decode(const QByteArray& data, std::vector<GribField>& out, QString* err) {
    out.clear();
    const uchar* d = reinterpret_cast<const uchar*>(data.constData());
    const size_t n = size_t(data.size());
    size_t pos = 0;
    QString why;
    while (pos + 8 <= n) {
        if (!(d[pos] == 'G' && d[pos + 1] == 'R' && d[pos + 2] == 'I' && d[pos + 3] == 'B')) { ++pos; continue; }
        const int edition = d[pos + 7];
        if (edition != 1) { why = QString("GRIB edition %1 is not supported (only GRIB1)").arg(edition); break; }
        Rd r0{d + pos, n - pos};
        size_t total = r0.u3(4);
        if (total < 8 + 28 + 4 || pos + total > n) { why = "truncated GRIB message"; break; }
        // messages larger than 8388607 bytes use a scaled length in the ECMWF convention; ERA5 point boxes never get there
        Rd r{d + pos, total};
        size_t o = 8;
        // ---- PDS ----
        const size_t pdsLen = r.u3(o);
        if (pdsLen < 28 || o + pdsLen > total) { why = "bad PDS"; break; }
        GribField f;
        f.table = int(r.u1(o + 3)); f.centre = int(r.u1(o + 4));
        const unsigned flags = r.u1(o + 7);
        f.param = int(r.u1(o + 8));
        f.levelType = int(r.u1(o + 9));
        f.level = (f.levelType == 100 || f.levelType == 105 || f.levelType == 103 || f.levelType == 109) ? int(r.u2(o + 10)) : int(r.u1(o + 11));
        const int yoc = int(r.u1(o + 12)), mon = int(r.u1(o + 13)), day = int(r.u1(o + 14)), hr = int(r.u1(o + 15)), mn = int(r.u1(o + 16));
        const int century = pdsLen >= 25 ? int(r.u1(o + 24)) : 21;
        f.date = ((century - 1) * 100 + yoc) * 10000 + mon * 100 + day;
        f.time = hr * 100 + mn;
        const int dscale = pdsLen >= 28 ? r.s2(o + 26) : 0;
        o += pdsLen;
        // ---- GDS ----
        if (!(flags & 0x80)) { why = "message has no grid description"; break; }
        const size_t gdsLen = r.u3(o);
        if (o + gdsLen > total) { why = "bad GDS"; break; }
        const int rep = int(r.u1(o + 5));
        if (rep != 0) { why = QString("grid type %1 is not supported (only regular lat/lon)").arg(rep); break; }
        f.ni = int(r.u2(o + 6)); f.nj = int(r.u2(o + 8));
        f.la1 = r.s3(o + 10) / 1000.0; f.lo1 = r.s3(o + 13) / 1000.0;
        f.la2 = r.s3(o + 17) / 1000.0; f.lo2 = r.s3(o + 20) / 1000.0;
        f.di = r.u2(o + 23) / 1000.0; f.dj = r.u2(o + 25) / 1000.0;
        const unsigned scan = r.u1(o + 27);
        f.iNegative = scan & 0x80; f.jPositive = scan & 0x40; f.jConsecutive = scan & 0x20;
        o += gdsLen;
        // ---- BMS ----
        if (flags & 0x40) { why = "bitmaps are not supported"; break; }
        // ---- BDS ----
        const size_t bdsLen = r.u3(o);
        if (o + bdsLen > total || bdsLen < 11) { why = "bad BDS"; break; }
        const unsigned bflags = r.u1(o + 3);
        if (bflags & 0xf0) { why = "only simple grid-point packing is supported (no spherical harmonics, complex packing or integer data)"; break; }
        const int E = r.s2(o + 4);
        const double R = r.ibm(o + 6);
        const int bits = int(r.u1(o + 10));
        const size_t np = size_t(f.ni) * size_t(f.nj);
        f.v.resize(np);
        const double sc = std::pow(2.0, E), dec = std::pow(10.0, -dscale);
        if (bits == 0) {
            for (size_t k = 0; k < np; ++k) f.v[k] = R * dec;
        } else {
            if (bits > 32 || (bdsLen - 11) * 8 < np * size_t(bits)) { why = "BDS too short for the grid"; break; }
            const uchar* b = r.p + o + 11;
            size_t bitpos = 0;
            for (size_t k = 0; k < np; ++k) {
                quint64 x = 0;
                for (int q = 0; q < bits; ++q, ++bitpos) x = (x << 1) | ((b[bitpos >> 3] >> (7 - (bitpos & 7))) & 1u);
                f.v[k] = (R + double(x) * sc) * dec;
            }
        }
        out.push_back(std::move(f));
        pos += total;
    }
    if (out.empty()) { if (err) *err = why.isEmpty() ? "no GRIB messages found" : why; return false; }
    if (err && !why.isEmpty()) *err = why;
    return true;
}

} // namespace sharppy
