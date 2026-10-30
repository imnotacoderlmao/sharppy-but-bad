#pragma once
// Preference map shared by the ported viz widgets (keys match preferences.py,
// e.g. "bg_color", "stp_box_color", "alert_l1_color"; values are "#rrggbb").
#include <QHash>
#include <QString>
#include <QColor>
namespace sharppy {
using VizPrefs = QHash<QString, QString>;
inline QColor prefColor(const VizPrefs& p, const char* key, const QColor& def) {
    auto it = p.find(QString::fromLatin1(key));
    return it == p.end() ? def : QColor(it.value());
}
}
