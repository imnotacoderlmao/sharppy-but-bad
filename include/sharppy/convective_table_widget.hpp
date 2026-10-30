#pragma once

#include <QWidget>
#include <QPainter>
#include <QMouseEvent>
#include <QColor>
#include <QFont>
#include <QString>
#include <QStringList>
#include <map>
#include "sharppy/profile.hpp"
#include "sharppy/thermo.hpp"
#include "sharppy/tab_params.hpp"
#include "sharppy/viz_prefs.hpp"

namespace sharppy {

// Faithful port of sharppy/viz/thermo.py (plotText + SelectParcels).
class ConvectiveTableWidget : public QWidget {
    Q_OBJECT

public:
    // global parcel type indices used by parcelSelected(): SFC=0 ML=1 FCST=2 MU=3 EFF=4 USER=5
    explicit ConvectiveTableWidget(QWidget* parent = nullptr);

    void setProfile(const Profile& prof);
    void setParcels(const ParcelResult& sb, const ParcelResult& ml, const ParcelResult& fcst, const ParcelResult& mu);
    void setParcels(const ParcelResult& sb, const ParcelResult& ml, const ParcelResult& fcst, const ParcelResult& mu,
                    const ParcelResult& eff, const ParcelResult& usr);
    void setIndices(const tab::ThermoIndices& idx);
    // ship / stp_fixed / stp_cin / scp for the active storm mover (left or right), unflipped
    void setSevere(double ship, double stpFixed, double stpCin, double scp);
    void setPreferences(const VizPrefs& prefs);
    QStringList parcelTypes() const { return m_types; }
    void setParcelTypes(const QStringList& t);          // exactly four of SFC/ML/FCST/MU/EFF/USER
    int selectedParcel() const;                         // global type index of the highlighted parcel
    void selectParcelType(const QString& type);         // keeps the table in sync with outside changes

signals:
    void parcelSelected(int index);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    void initUI();
    void drawFrame(QPainter& qp);
    void drawConvectiveIndices(QPainter& qp);
    void drawIndices(QPainter& qp);
    void drawSevere(QPainter& qp);
    void showParcelDialog();
    static int typeIndex(const QString& t);

    Profile m_prof;
    std::map<QString, ParcelResult> m_parcels;
    QStringList m_types{"SFC", "ML", "FCST", "MU"};
    int m_sel = 3;                                       // index into m_types (default "MU")
    tab::ThermoIndices m_ix;
    double m_ship = NAN, m_stpFixed = NAN, m_stpCin = NAN, m_scp = NAN;
    bool m_hasProf = false, m_hasIdx = false;

    QString m_pwUnits = "in", m_tempUnits = "Fahrenheit";
    QColor m_bg{"#000000"}, m_fg{"#ffffff"};
    QColor m_alert[7] = {"#00ffff", "#775000", "#996600", "#ffffff", "#ffff00", "#ff0000", "#e700df"};
    QColor m_pwat[7] = {"#ff7f00", "#ee9a00", "#ffdab9", "#ffffff", "#98fb98", "#66cd00", "#00ff00"};
    QColor m_pclSel{"#00ffff"};

    double m_bounds[4][2] = {};
    double m_brx = 0, m_bry = 0, m_tpad = 0, m_labelHeight = 0, m_ylast = 0;
    QFont m_labelFont;
};

} // namespace sharppy
