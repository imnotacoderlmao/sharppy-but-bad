#pragma once

#include <QWidget>
#include <QPainter>
#include "sharppy/profile.hpp"
#include "sharppy/winds.hpp"
#include "sharppy/params.hpp"

namespace sharppy {

class SRWindWidget : public QWidget {
    Q_OBJECT

public:
    explicit SRWindWidget(QWidget* parent = nullptr);

    void setResults(const Profile& prof, const BunkersMotion& bunkers, const SevereParams& params);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    Profile m_prof;
    BunkersMotion m_bunkers;
    SevereParams m_params;
    bool m_hasData = false;

    void drawSRWindPlot(QPainter& p, const QRect& r);
    void drawHazardBox(QPainter& p, const QRect& r);
};

} // namespace sharppy
