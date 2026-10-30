#pragma once
// Reader for the single-file data archive made by tools/pack_db.py ("DBPACK1"). Everything under data/ (datasources, SARS
// analog soundings and tables, shapefiles, sharptab climatology) can live inside it so a repository carries one file instead
// of thousands. Loose files next to the program always win, so any of them can still be overridden by hand.
// Legacy SARSPK1 archives (sars.sarspack) are read too: hail/ -> sars/hail/, supercell/ -> sars/supercell/, datasources/.
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <string>

namespace sharppy {

// Locates the archive: SHARPPY_DBPACK, data/data.dbpack next to / above the program, the working directory,
// ~/.sharppy, then the legacy data/sars/sars.sarspack. Empty when there is none.
QString dbpack_find();

// Entry paths are relative with '/' separators, e.g. "datasources/standard.xml", "sars/hail/95052300.DDC".
QStringList dbpack_list(const QString& archive, const QString& prefix);
QByteArray  dbpack_read(const QString& archive, const QString& path);

// Reads a data file by its path relative to data/ ("shapefiles/gshhs_i.dat"): a loose file under any data root first, else
// the archive. Empty when neither has it.
QByteArray data_read(const QString& relPath);
bool       data_exists(const QString& relPath);
// Entry names (not paths) directly inside a data folder, merged from loose files and the archive.
QStringList data_list(const QString& relDir);

// Raw SPC-format text of an analog sounding. type = "hail" | "supercell"; match = "yymmddhh.LOC" (sars.getSounding rules:
// file name starts with the date and ends with the station, either case). Empty when not found.
QByteArray sars_read_sounding(const std::string& sars_dir, const QString& type, const QString& match, QString* fileName = nullptr);

} // namespace sharppy
