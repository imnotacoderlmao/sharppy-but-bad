#include "sharppy/sars_archive.hpp"
#include <QDir>
#include <QFile>
#include <QHash>
#include <QtEndian>
#include <QVector>

namespace sharppy {

namespace {
struct Entry { quint8 type; QString name; quint32 off, csize, usize; };

struct Archive {
    QString path;
    QByteArray data;
    QVector<Entry> entries;
    bool ok = false;

    bool open(const QString& p) {
        if (ok && path == p) return true;
        ok = false; entries.clear(); data.clear(); path = p;
        QFile f(p);
        if (!f.open(QIODevice::ReadOnly)) return false;
        data = f.readAll();
        const uchar* d = reinterpret_cast<const uchar*>(data.constData());
        const int n = data.size();
        if (n < 12 || data.left(8) != QByteArray("SARSPK1\0", 8)) return false;
        const quint32 cnt = qFromBigEndian<quint32>(d + 8);
        int pos = 12;
        for (quint32 i = 0; i < cnt; ++i) {
            if (pos + 3 > n) { entries.clear(); return false; }
            Entry e; e.type = d[pos];
            const quint16 nl = qFromBigEndian<quint16>(d + pos + 1); pos += 3;
            if (pos + nl + 12 > n) { entries.clear(); return false; }
            e.name = QString::fromUtf8(data.mid(pos, nl)); pos += nl;
            e.off = qFromBigEndian<quint32>(d + pos); e.csize = qFromBigEndian<quint32>(d + pos + 4);
            e.usize = qFromBigEndian<quint32>(d + pos + 8); pos += 12;
            if (quint64(e.off) + e.csize > quint64(n)) { entries.clear(); return false; }
            entries.push_back(e);
        }
        ok = true;
        return true;
    }
};

bool nameMatches(const QString& file, const QString& date, const QString& loc) {
    return file.startsWith(date) && (file.endsWith(loc.toLower()) || file.endsWith(loc.toUpper()));
}
}

QByteArray sars_read_sounding(const std::string& sars_dir, const QString& type, const QString& match, QString* fileName) {
    const QStringList parts = match.split('.');
    if (parts.size() != 2) return {};
    const QString date = parts[0], loc = parts[1];
    const quint8 tid = type.toLower() == "hail" ? 0 : 1;
    const QString base = QString::fromStdString(sars_dir);

    static Archive arc;
    for (const QString& cand : {base + "/sars.sarspack", base + "/../sars.sarspack"}) {
        if (!QFile::exists(cand) || !arc.open(QDir::cleanPath(cand))) continue;
        for (const Entry& e : arc.entries) {
            if (e.type != tid || !nameMatches(e.name, date, loc)) continue;
            QByteArray raw = qUncompress(arc.data.mid(int(e.off), int(e.csize)));
            if (raw.isEmpty()) continue;
            if (fileName) *fileName = e.name;
            return raw;
        }
    }
    // loose folders: sars/<type>/<file>
    QDir d(base + "/" + type.toLower());
    if (d.exists()) {
        const QStringList files = d.entryList(QDir::Files, QDir::Name);
        for (const QString& fn : files) {
            if (!nameMatches(fn, date, loc)) continue;
            QFile f(d.filePath(fn));
            if (f.open(QIODevice::ReadOnly)) { if (fileName) *fileName = fn; return f.readAll(); }
        }
    }
    return {};
}

} // namespace sharppy
