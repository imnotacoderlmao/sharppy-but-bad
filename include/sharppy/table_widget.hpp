#pragma once

#include <QWidget>
#include <QPainter>
#include "sharppy/profile.hpp"
#include "sharppy/thermo.hpp"
#include "sharppy/winds.hpp"
#include "sharppy/params.hpp"
#include "sharppy/sars.hpp"

namespace sharppy {

class TableWidget : public QWidget {
    Q_OBJECT

public:
    explicit TableWidget(QWidget* parent = nullptr);

    void setResults(const Profile& prof,
                    const ParcelResult& sb,
                    const ParcelResult& ml,
                    const ParcelResult& mu,
                    const BunkersMotion& bunkers,
                    const SevereParams& params,
                    const InflowLayer& inflow);

    void setSars(const SarsResult& sars);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    bool m_hasData = false;
    Profile m_prof;
    ParcelResult m_sb, m_ml, m_mu;
    BunkersMotion m_bunkers;
    SevereParams m_params;
    InflowLayer m_inflow;
    SoundingMetrics m_metrics;
    SarsResult m_sars;

    void drawParcelBox(QPainter& p, const QRect& r);
    void drawLapseRateBox(QPainter& p, const QRect& r);
    void drawKinematicsBox(QPainter& p, const QRect& r);
    void drawSarsBox(QPainter& p, const QRect& r);
};

} // namespace sharppy