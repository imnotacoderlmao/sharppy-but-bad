#pragma once
#include <QWidget>
#include <QColor>
#include <QString>
#include <QHash>
#include "sharppy/viz_prefs.hpp"

namespace sharppy {

// Port of viz/watch.py (plotWatch): shows the top possible-hazard string for the active mover.
class WatchWidget : public QWidget {
    Q_OBJECT
public:
    explicit WatchWidget(QWidget* parent = nullptr);
    void setWatchType(const QString& type);
    void setPreferences(const VizPrefs& prefs);
protected:
    void paintEvent(QPaintEvent*) override;
private:
    QColor colorFor(const QString& t) const;
    QString m_type; bool m_has = false;
    QColor m_bg{"#000000"}, m_fg{"#ffffff"};
    QColor m_wtf{"#ffffff"}, m_pds{"#ff00ff"}, m_tor{"#ff0000"}, m_svr{"#ffff00"}, m_mrgl{"#0099cc"},
           m_flood{"#5ffb17"}, m_blizz{"#3366ff"}, m_fire{"#ff9900"}, m_heat{"#c85a17"}, m_none{"#ffcc33"};
};

} // namespace sharppy
