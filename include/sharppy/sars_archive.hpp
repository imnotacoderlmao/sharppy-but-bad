#pragma once
// Reader for the SARSPK1 archive made by tools/pack_sars.py, with the loose sars/<type>/ folders as fallback.
#include <QByteArray>
#include <QString>
#include <string>

namespace sharppy {

// Raw SPC-format text of an analog sounding. type = "hail" | "supercell"; match = "yymmddhh.LOC" (sars.getSounding rules:
// file name starts with the date and ends with the station, either case). Empty when not found.
QByteArray sars_read_sounding(const std::string& sars_dir, const QString& type, const QString& match, QString* fileName = nullptr);

} // namespace sharppy
