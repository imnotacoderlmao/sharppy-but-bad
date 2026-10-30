#pragma once

#include <QWidget>
#include <QPainter>
#include "sharppy/profile.hpp"

namespace sharppy {

class WindBarWidget : public QWidget {
    Q_OBJECT

public:
    explicit WindBarWidget(QWidget* parent = nullptr);
    void setProfile(const Profile& prof);
    void setPresBounds(double pBot, double pTop) {
        m_pBot = pBot;
        m_pTop = pTop;
        update();
    }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    Profile m_prof;
    bool m_hasData = false;

    double m_pBot = 1050.0;
    double m_pTop = 100.0;
    double m_maxSpd = 120.0;

    double presToY(double pres) const;
};

} // namespace sharppy
