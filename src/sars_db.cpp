#include "sharppy/sars_db.hpp"
#include "sharppy/tab.hpp"
#include "sharppy/db_pack.hpp"
#include <QCoreApplication>
#include <QFile>
#include <QTextStream>
#include <QStringList>
#include <cmath>
#include <algorithm>
#include <map>
#include <mutex>
#include <QFileInfo>
#include <QDateTime>

namespace sharppy {

namespace {
constexpr double KTS2MS = 0.514444;
constexpr double MS2KTS = 1.94384;

// np.loadtxt(skiprows=1, dtype=bytes, comments=...)
// Parsed once and kept: the SARS panel is recomputed on every live-drag step, and re-reading + splitting ~2000 text rows
// each time made it ~75% of the whole recompute. The cache is refreshed when the file changes on disk.
struct Table {
    std::vector<std::vector<std::string>> rows;
    std::vector<std::vector<double>> nums;
    qint64 mtime = -1, size = -1;
};

const Table* getTable(const std::string& fn, const QString& comment) {
    static std::map<std::pair<std::string, QString>, Table> cache;
    static std::mutex mu;
    std::lock_guard<std::mutex> lock(mu);
    const QFileInfo fi(QString::fromStdString(fn));
    Table& t = cache[{fn, comment}];
    const bool loose = fi.isFile();
    if (!t.rows.empty() && (loose ? (t.mtime == fi.lastModified().toMSecsSinceEpoch() && t.size == fi.size()) : t.mtime == 0)) return &t;
    t = Table();
    QByteArray bytes;
    if (loose) { QFile f(fi.filePath()); if (!f.open(QIODevice::ReadOnly)) return nullptr; bytes = f.readAll(); }
    else bytes = data_read("sars/" + fi.fileName());          // sars_hail.txt / sars_supercell.txt inside the data pack
    if (bytes.isEmpty()) return nullptr;
    QTextStream ts(bytes);
    bool first = true;
    const QRegExp ws("\\s+");
    while (!ts.atEnd()) {
        QString line = ts.readLine();
        if (first) { first = false; continue; }
        int c = line.indexOf(comment);
        if (c >= 0) line = line.left(c);
        QStringList tk = line.split(ws, QString::SkipEmptyParts);
        if (tk.isEmpty()) continue;
        std::vector<std::string> r; std::vector<double> nv;
        for (const QString& x : tk) { r.push_back(x.toStdString()); nv.push_back(x.toDouble()); }
        t.rows.push_back(std::move(r));
        t.nums.push_back(std::move(nv));
    }
    if (t.rows.empty()) return nullptr;
    t.mtime = loose ? fi.lastModified().toMSecsSinceEpoch() : 0; t.size = loose ? fi.size() : bytes.size();
    return &t;
}
double num(const std::vector<std::string>& r, size_t i) { return i < r.size() ? std::atof(r[i].c_str()) : std::nan(""); }
}

std::string default_sars_dir() {
    return (QCoreApplication::applicationDirPath() + "/data/sars").toStdString();
}

SarsMatches sars_supercell(const std::string& dbfile, double mlcape, double mllcl, double h5temp, double lr,
                           double shr, double srh, double shr3k, double shr9k, double srh3) {
    SarsMatches out;
    const Table* tb = getTable(dbfile, "%%%%");
    if (!tb) return out;
    const auto& db = tb->rows;

    const double range_mlcape = (mlcape == 0) ? 0 : 1300;
    const double range_mlcape_t1 = mlcape * 0.25;
    const double range_mllcl = 500, range_mllcl_t1 = 200;
    const double range_shr = 14, range_shr_t1 = 10;
    const double range_srh = (std::fabs(srh) < 50) ? 100. : srh;
    const double range_srh_t1 = (std::fabs(srh) < 100) ? 50 : std::fabs(srh) * 0.30;
    const double range_srh3_t1 = (std::fabs(srh3) < 100) ? 50. : std::fabs(srh3) * 0.50;
    const double range_temp = 7, range_temp_t1 = 5;
    const double range_lr = 1.0, range_lr_t1 = 0.8;
    const double range_shr3k_t1 = 15, range_shr9k_t1 = 25;

    int nloose = 0, nmatch = 0;
    for (size_t k = 0; k < db.size(); ++k) {
        const auto& r = db[k];
        const auto& nv = tb->nums[k];
        auto num = [&](const std::vector<std::string>&, size_t i) { return i < nv.size() ? nv[i] : std::nan(""); };
        if (r.size() < 15) continue;
        double cat = num(r, 1), m_cape = num(r, 3), m_lcl = num(r, 5), m_srh = num(r, 6), m_shr = num(r, 7),
               m_h5 = num(r, 9), m_lr = num(r, 11), m_shr3 = num(r, 12), m_shr9 = num(r, 13), m_srh3 = num(r, 14);
        bool loose = (mlcape >= m_cape - range_mlcape) && (mlcape <= m_cape + range_mlcape) &&
                     (mllcl >= m_lcl - range_mllcl) && (mllcl <= m_lcl + range_mllcl) &&
                     (shr >= m_shr - range_shr) && (shr <= m_shr + range_shr) &&
                     (srh >= m_srh - range_srh) && (srh <= m_srh + range_srh) &&
                     (h5temp >= m_h5 - range_temp) && (h5temp <= m_h5 + range_temp) &&
                     (lr >= m_lr - range_lr) && (lr <= m_lr + range_lr);
        if (loose) { ++nloose; if (cat > 0) ++nmatch; }
        bool quality = (mlcape >= m_cape - range_mlcape_t1) && (mlcape <= m_cape + range_mlcape_t1) &&
                       (mllcl >= m_lcl - range_mllcl_t1) && (mllcl <= m_lcl + range_mllcl_t1) &&
                       (shr >= m_shr - range_shr_t1) && (shr <= m_shr + range_shr_t1) &&
                       (srh >= m_srh - range_srh_t1) && (srh <= m_srh + range_srh_t1) &&
                       (h5temp >= m_h5 - range_temp_t1) && (h5temp <= m_h5 + range_temp_t1) &&
                       (lr >= m_lr - range_lr_t1) && (lr <= m_lr + range_lr_t1) &&
                       (shr3k >= m_shr3 - range_shr3k_t1) && (shr3k <= m_shr3 + range_shr3k_t1) &&
                       (shr9k >= m_shr9 - range_shr9k_t1) && (shr9k <= m_shr9 + range_shr9k_t1) &&
                       (srh3 >= m_srh3 - range_srh3_t1) && (srh3 <= m_srh3 + range_srh3_t1);
        if (quality) {
            out.names.push_back(r[0]);
            out.types.push_back(cat == 2 ? "SIGTOR" : cat == 1 ? "WEAKTOR" : cat == 0 ? "NONTOR" : r[1]);
        }
    }
    out.nloose = nloose; out.nmatch = nmatch;
    out.prob = (nloose > 0 && mlcape > 0) ? nmatch / (double)nloose : 0.;
    return out;
}

SarsMatches sars_hail(const std::string& dbfile, double mumr, double mucape, double h5_temp, double lr,
                      double shr6, double shr9, double shr3, double srh) {
    SarsMatches out;
    const Table* tb = getTable(dbfile, "#");
    if (!tb) return out;
    const auto& db = tb->rows;

    const double range_mumr = 2.0, range_mumr_t1 = 2.0;
    const double range_mucape = mucape * .30;
    const double range_mucape_t1 = mucape < 500. ? mucape * .50 : mucape < 2000. ? mucape * .25 : mucape * .20;
    const double range_lr = 2.0, range_lr_t1 = 0.4;
    const double range_temp = 9, range_temp_t1 = 1.5;
    const double range_shr6 = 12, range_shr6_t1 = 6;
    const double range_shr9 = 22, range_shr9_t1 = 15;
    const double range_shr3 = 10, range_shr3_t1 = 8;
    const double range_srh_t1 = srh < 50 ? 25 : srh * 0.5;

    int nloose = 0, nsig = 0;
    for (size_t k = 0; k < db.size(); ++k) {
        const auto& r = db[k];
        const auto& nv = tb->nums[k];
        auto num = [&](const std::vector<std::string>&, size_t i) { return i < nv.size() ? nv[i] : std::nan(""); };
        if (r.size() < 13) continue;
        double size = num(r, 2), m_cape = num(r, 3), m_mr = num(r, 4), m_t = num(r, 5), m_lr = num(r, 7),
               m_shr3 = num(r, 9), m_shr6 = num(r, 10), m_shr9 = num(r, 11), m_srh = num(r, 12);
        bool loose = (mumr >= m_mr - range_mumr) && (mumr <= m_mr + range_mumr) &&
                     (mucape >= m_cape - range_mucape) && (mucape <= m_cape + range_mucape) &&
                     (lr >= m_lr - range_lr) && (lr <= m_lr + range_lr) &&
                     (h5_temp >= m_t - range_temp) && (h5_temp <= m_t + range_temp) &&
                     (shr6 >= m_shr6 - range_shr6) && (shr6 <= m_shr6 + range_shr6) &&
                     (shr9 >= m_shr9 - range_shr9) && (shr9 <= m_shr9 + range_shr9) &&
                     (shr3 >= m_shr3 - range_shr3) && (shr3 <= m_shr3 + range_shr3);
        if (loose) { ++nloose; if (size >= 2.) ++nsig; }
        bool quality = (mumr >= m_mr - range_mumr_t1) && (mumr <= m_mr + range_mumr_t1) &&
                       (mucape >= m_cape - range_mucape_t1) && (mucape <= m_cape + range_mucape_t1) &&
                       (lr >= m_lr - range_lr_t1) && (lr <= m_lr + range_lr_t1) &&
                       (h5_temp >= m_t - range_temp_t1) && (h5_temp <= m_t + range_temp_t1) &&
                       (shr6 >= m_shr6 - range_shr6_t1) && (shr6 <= m_shr6 + range_shr6_t1) &&
                       (shr9 >= m_shr9 - range_shr9_t1) && (shr9 <= m_shr9 + range_shr9_t1) &&
                       (shr3 >= m_shr3 - range_shr3_t1) && (shr3 <= m_shr3 + range_shr3_t1) &&
                       (srh >= m_srh - range_srh_t1) && (srh <= m_srh + range_srh_t1);
        if (quality && out.names.size() < 15) { out.names.push_back(r[0]); out.sizes.push_back(size); }
    }
    out.nloose = nloose; out.nmatch = nsig;
    out.prob = (nloose > 0 && mucape > 0) ? nsig / (double)nloose : 0.;
    return out;
}

SarsAll compute_sars(const Profile& prof, const ParcelResult& mu, const ParcelResult& ml,
                     const BunkersMotion& sr, const std::string& dbdir) {
    SarsAll a;
    if (prof.size() < 2) return a;
    auto presAgl = [&](double h) { return prof.pres_at_hght(prof.to_msl(h)); };
    const double sfc = prof.pres[0];
    auto shearMs = [&](double h) {
        WindVector w = wind_shear_pres(prof, sfc, presAgl(h));
        return w.wspd * KTS2MS;
    };
    const double s6 = shearMs(6000), s3 = shearMs(3000), s9 = shearMs(9000);
    const double h500t = prof.temp_at_pres(500.);

    tab::Env env(prof);
    double lr = std::nan("");
    {   // params.lapse_rate(prof, 700, 500, pres=True)
        double z1 = env.hght_at(700.), z2 = env.hght_at(500.);
        lr = (env.vtmp_at(500.) - env.vtmp_at(700.)) / (z2 - z1) * -1000.;
    }
    const double mucape = mu.cape, mlcape = ml.cape, mllcl = ml.lcl_h_agl;
    const double mumr = tab::mixratio(mu.start_p, mu.start_dp);

    auto srh = [&](double top, const WindVector& st) { return storm_relative_helicity(prof, 0, top, st.u, st.v); };
    const double r1 = srh(1000, sr.right_mover), r3 = srh(3000, sr.right_mover);
    const double l1 = srh(1000, sr.left_mover), l3 = srh(3000, sr.left_mover);

    const std::string hf = dbdir + "/sars_hail.txt", sf = dbdir + "/sars_supercell.txt";
    a.rightHail = sars_hail(hf, mumr, mucape, h500t, lr, s6, s9, s3, r3);
    a.leftHail = sars_hail(hf, mumr, mucape, h500t, lr, s6, s9, s3, -l3);
    a.rightSupercell = sars_supercell(sf, mlcape, mllcl, h500t, lr, s6 * MS2KTS, r1, s3 * MS2KTS, s9 * MS2KTS, r3);
    a.leftSupercell = sars_supercell(sf, mlcape, mllcl, h500t, lr, s6 * MS2KTS, -l1, s3 * MS2KTS, s9 * MS2KTS, -l3);
    return a;
}

} // namespace sharppy
