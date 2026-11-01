#include "sharppy/db_pack.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QtEndian>
#include <QVector>

namespace sharppy {

namespace {
// A program made by `pack_db.py embed` carries the archive at the end of its own file: [archive]["DBPKEND1"][u64 length].
constexpr const char* kEmbedded = "embedded:";
QByteArray readEmbedded() {
    QFile f(QCoreApplication::applicationFilePath());
    if (!f.open(QIODevice::ReadOnly)) return {};
    const qint64 size = f.size();
    if (size < 16 || !f.seek(size - 16)) return {};
    const QByteArray tail = f.read(16);
    if (tail.size() != 16 || tail.left(8) != QByteArray("DBPKEND1", 8)) return {};
    const quint64 len = qFromBigEndian<quint64>(reinterpret_cast<const uchar*>(tail.constData()) + 8);
    if (len == 0 || len + 16 > quint64(size) || len > 0x7fffffffULL) return {};
    if (!f.seek(size - 16 - qint64(len))) return {};
    const QByteArray d = f.read(qint64(len));
    return quint64(d.size()) == len ? d : QByteArray();
}

struct Entry { QString path; quint32 off, csize, usize; };

struct Archive {
    QString file;
    QByteArray data;
    QVector<Entry> entries;
    QHash<QString, int> index;
    bool ok = false;

    static QString legacyPath(quint8 type, const QString& name) {
        return (type == 0 ? "sars/hail/" : type == 1 ? "sars/supercell/" : "datasources/") + name;
    }

    bool open(const QString& p) {
        if (ok && file == p) return true;
        ok = false; entries.clear(); index.clear(); data.clear(); file = p;
        if (p == kEmbedded) data = readEmbedded();
        else { QFile f(p); if (!f.open(QIODevice::ReadOnly)) return false; data = f.readAll(); }
        const uchar* d = reinterpret_cast<const uchar*>(data.constData());
        const int n = data.size();
        const bool legacy = data.left(8) == QByteArray("SARSPK1\0", 8);
        if (n < 12 || (!legacy && data.left(8) != QByteArray("DBPACK1\0", 8))) return false;
        const quint32 cnt = qFromBigEndian<quint32>(d + 8);
        int pos = 12;
        for (quint32 i = 0; i < cnt; ++i) {
            quint8 type = 0;
            if (legacy) { if (pos + 3 > n) return fail(); type = d[pos]; ++pos; }
            if (pos + 2 > n) return fail();
            const quint16 nl = qFromBigEndian<quint16>(d + pos); pos += 2;
            if (pos + nl + 12 > n) return fail();
            Entry e;
            const QString name = QString::fromUtf8(data.mid(pos, nl)); pos += nl;
            e.path = legacy ? legacyPath(type, name) : name;
            e.off = qFromBigEndian<quint32>(d + pos); e.csize = qFromBigEndian<quint32>(d + pos + 4);
            e.usize = qFromBigEndian<quint32>(d + pos + 8); pos += 12;
            if (quint64(e.off) + e.csize > quint64(n)) return fail();
            index.insert(e.path, entries.size());
            entries.push_back(e);
        }
        ok = true;
        return true;
    }
    bool fail() { entries.clear(); index.clear(); return false; }
};

QMutex g_mu;
Archive g_arc;

bool nameMatches(const QString& file, const QString& date, const QString& loc) {
    return file.startsWith(date) && (file.endsWith(loc.toLower()) || file.endsWith(loc.toUpper()));
}

QStringList dataRoots() {
    static QStringList roots;
    if (roots.isEmpty()) {
        const QString env = qEnvironmentVariable("SHARPPY_DATA");
        if (!env.isEmpty()) roots << env;
        QDir d(QCoreApplication::applicationDirPath());
        for (int i = 0; i < 7; ++i) { roots << d.absoluteFilePath("data"); if (!d.cdUp()) break; }
        roots << QDir::current().absoluteFilePath("data") << QDir::homePath() + "/.sharppy";
    }
    return roots;
}
}

QString dbpack_find() {
    static QString found; static bool done = false;
    QMutexLocker lock(&g_mu);
    if (done) return found;
    done = true;
    QStringList c;
    const QString env = qEnvironmentVariable("SHARPPY_DBPACK");
    if (!env.isEmpty() && QFileInfo::exists(env)) { found = QDir::cleanPath(env); return found; }
    if (!readEmbedded().isEmpty()) { found = kEmbedded; return found; }       // self-contained build
    for (const QString& r : dataRoots()) c << r + "/data.dbpack";
    c << QDir::homePath() + "/.sharppy/data.dbpack";
    for (const QString& r : dataRoots()) c << r + "/sars/sars.sarspack";     // legacy
    for (const QString& f : c) if (QFileInfo::exists(f)) { found = QDir::cleanPath(f); break; }
    return found;
}

QStringList dbpack_list(const QString& archive, const QString& prefix) {
    QStringList out;
    if (archive.isEmpty()) return out;
    QMutexLocker lock(&g_mu);
    if (!g_arc.open(QDir::cleanPath(archive))) return out;
    const QString pre = prefix.isEmpty() || prefix.endsWith('/') ? prefix : prefix + "/";
    for (const Entry& e : g_arc.entries) if (e.path.startsWith(pre)) out << e.path.mid(pre.size());
    return out;
}

QByteArray dbpack_read(const QString& archive, const QString& path) {
    if (archive.isEmpty()) return {};
    QMutexLocker lock(&g_mu);
    if (!g_arc.open(QDir::cleanPath(archive))) return {};
    const auto it = g_arc.index.constFind(path);
    if (it == g_arc.index.constEnd()) return {};
    const Entry& e = g_arc.entries[it.value()];
    return qUncompress(g_arc.data.mid(int(e.off), int(e.csize)));
}

QByteArray data_read(const QString& relPath) {
    for (const QString& r : dataRoots()) {
        QFile f(r + "/" + relPath);
        if (f.open(QIODevice::ReadOnly)) return f.readAll();
    }
    return dbpack_read(dbpack_find(), relPath);
}

bool data_exists(const QString& relPath) {
    for (const QString& r : dataRoots()) if (QFileInfo(r + "/" + relPath).isFile()) return true;
    return dbpack_list(dbpack_find(), QString()).contains(relPath);
}

QStringList data_list(const QString& relDir) {
    QStringList out;
    for (const QString& r : dataRoots()) {
        const QDir d(r + "/" + relDir);
        if (d.exists()) for (const QString& f : d.entryList(QDir::Files, QDir::Name)) if (!out.contains(f)) out << f;
    }
    for (const QString& f : dbpack_list(dbpack_find(), relDir))
        if (!f.contains('/') && !out.contains(f)) out << f;
    return out;
}

QByteArray sars_read_sounding(const std::string& sars_dir, const QString& type, const QString& match, QString* fileName) {
    const QStringList parts = match.split('.');
    if (parts.size() != 2) return {};
    const QString date = parts[0], loc = parts[1];
    const QString t = type.toLower();

    // loose folders given explicitly (sars_dir/<type>/<file>) first, then everything data_read() can see
    const QString base = QString::fromStdString(sars_dir);
    QDir d(base + "/" + t);
    if (d.exists()) {
        for (const QString& fn : d.entryList(QDir::Files, QDir::Name)) {
            if (!nameMatches(fn, date, loc)) continue;
            QFile f(d.filePath(fn));
            if (f.open(QIODevice::ReadOnly)) { if (fileName) *fileName = fn; return f.readAll(); }
        }
    }
    for (const QString& fn : data_list("sars/" + t)) {
        if (!nameMatches(fn, date, loc)) continue;
        const QByteArray raw = data_read("sars/" + t + "/" + fn);
        if (!raw.isEmpty()) { if (fileName) *fileName = fn; return raw; }
    }
    return {};
}

} // namespace sharppy
