#pragma once
// Reader for the SARSPK1 archive made by tools/pack_sars.py, with the loose sars/<type>/ folders as fallback.
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <string>

namespace sharppy {

// Raw SPC-format text of an analog sounding. type = "hail" | "supercell"; match = "yymmddhh.LOC" (sars.getSounding rules:
// file name starts with the date and ends with the station, either case). Empty when not found.
QByteArray sars_read_sounding(const std::string& sars_dir, const QString& type, const QString& match, QString* fileName = nullptr);

// Generic access to archive entries. type: 0 hail, 1 supercell, 2 datasource files (*.xml / *.csv).
QStringList sarspack_list(const QString& archivePath, int type);
QByteArray sarspack_read(const QString& archivePath, int type, const QString& name);
// Locates sars.sarspack (SHARPPY_SARSPACK, data/sars next to / above the program, cwd, ~/.sharppy). Empty if none.
// requireType >= 0 skips archives that hold no entries of that type.
QString sarspack_find(int requireType = -1);

} // namespace sharppy
