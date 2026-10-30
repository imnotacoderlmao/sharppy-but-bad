#include "sharppy/pwv.hpp"
#include "sharppy/db_pack.hpp"
#include "sharppy/thermo.hpp"
#include "sharppy/tab.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QMap>
#include <QStringList>
#include <QTextStream>
#include <cmath>

namespace sharppy {

double precip_water(const Profile& prof, double ptop, double pbot) {
    // params.precip_water, non-exact path: p = np.arange(pbot, ptop-1, -1); masked dwpt -> masked w;
    // sum of ((w[i]+w[i+1])/2 * (p[i]-p[i+1])) * 0.00040173 over pairs where both ends are valid.
    if (prof.size() < 2) return 0.0;
    if (!(pbot > 0)) pbot = prof.sfc_pres > 0 ? prof.sfc_pres : prof.pres.front();
    if (prof.pres.back() > ptop) ptop = prof.pres.back();
    const long n = static_cast<long>(std::ceil(pbot - ptop + 1.0));
    if (n < 2) return 0.0;
    double sum = 0.0, pprev = 0.0, wprev = 0.0; bool prevOk = false;
    for (long k = 0; k < n; ++k) {
        const double pk = pbot - static_cast<double>(k);
        const double d = prof.dwpt_at_pres(pk);
        const bool ok = (d != MISSING);
        const double w = ok ? tab::mixratio(pk, d) : 0.0;
        if (k > 0 && ok && prevOk) sum += (((wprev + w) / 2.0) * (pprev - pk)) * 0.00040173;
        pprev = pk; wprev = w; prevOk = ok;
    }
    return sum;
}

namespace {
using Table = QMap<QString, QVector<double>>;   // key: ICAO(upper), WMO, iata(lower)

Table load(const QString& name, int idIndex) {
    Table t;
    // data/sharptab/<name> (or data/<name>) as a loose file, else from the data pack
    QByteArray bytes = data_read("sharptab/" + name);
    if (bytes.isEmpty()) bytes = data_read(name);
    if (bytes.isEmpty()) return t;
    QTextStream in(bytes);
    in.readLine();   // header
    while (!in.atEnd()) {
        QStringList c = in.readLine().split(',');
        if (c.size() < 15) continue;
        QVector<double> v;
        for (int i = 3; i < 15; ++i) v << c[i].trimmed().toDouble();
        t.insert(c[idIndex].trimmed(), v);
    }
    return t;
}
} // namespace

int pwv_climo(const Profile& prof, const QString& station, int month) {
    if (station.isEmpty() || month < 1 || month > 12) return 0;
    int idIndex; QString key = station;
    if (station.size() == 4) { idIndex = 0; key = station.toUpper(); }
    else if (station.size() == 3) { idIndex = 2; key = station.toLower(); }
    else if (station.size() == 5) idIndex = 1;
    else return 0;

    static QMap<int, Table> means, stds;
    if (!means.contains(idIndex)) {
        means[idIndex] = load("PW-mean-inches.txt", idIndex);
        stds[idIndex] = load("PW-stdev-inches.txt", idIndex);
    }
    if (!means[idIndex].contains(key) || !stds[idIndex].contains(key)) return 0;
    double mean = means[idIndex][key][month - 1], sd = stds[idIndex][key][month - 1];
    double pw = precip_water(prof, 300.0);
    if (pw > mean + 3 * sd) return 3;
    if (pw < mean - 3 * sd) return -3;
    if (pw > mean + 2 * sd) return 2;
    if (pw < mean - 2 * sd) return -2;
    if (pw > mean + sd) return 1;
    if (pw < mean - sd) return -1;
    return 0;
}

} // namespace sharppy
