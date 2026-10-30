#pragma once
// Port of viz/fire.py (backgroundFire / plotFire): the FIRE inset.
#include <QWidget>
#include <QColor>
#include "sharppy/profile.hpp"
#include "sharppy/tab_params.hpp"
#include "sharppy/viz_prefs.hpp"

namespace sharppy {

class FireWidget : public QWidget {
    Q_OBJECT
public:
    explicit FireWidget(QWidget* parent = nullptr);
    void setProfile(const Profile& prof);
    void setPreferences(const VizPrefs& prefs);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;

private:
    struct Layout {
        int lh = 0, osMod = 0, tpad = 3, startDataY = 0, fosbergY = 0, hainesY = 0;
        QFont label, fosberg;
    };
    Layout layout() const;

    tab::FireData m_d;
    bool m_hasData = false;
    int m_hainesHght = 0;
    QColor m_bg{"#000000"}, m_fg{"#ffffff"};
};

} // namespace sharppy
