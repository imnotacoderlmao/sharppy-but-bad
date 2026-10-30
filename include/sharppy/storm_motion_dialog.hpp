#pragma once

#include <QDialog>
#include <QRadioButton>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include "sharppy/winds.hpp"

namespace sharppy {

class StormMotionDialog : public QDialog {
    Q_OBJECT

public:
    explicit StormMotionDialog(const BunkersMotion& bunkers,
                               const CorfidiMotion& corfidi,
                               double meanU, double meanV,
                               double currentU, double currentV,
                               QWidget* parent = nullptr);

    double getMotionU() const { return m_u; }
    double getMotionV() const { return m_v; }
    bool isCustom() const { return m_isCustom; }

private slots:
    void onPresetSelected();
    void onDirSpdChanged();
    void onUVChanged();

private:
    BunkersMotion m_bunkers;
    CorfidiMotion m_corfidi;
    double m_meanU, m_meanV;
    double m_u, m_v;
    bool m_isCustom = false;
    bool m_updating = false;

    QRadioButton* m_radRM;
    QRadioButton* m_radLM;
    QRadioButton* m_radCorfidiUp;
    QRadioButton* m_radCorfidiDown;
    QRadioButton* m_radMeanWind;
    QRadioButton* m_radCustom;

    QDoubleSpinBox* m_spinDir;
    QDoubleSpinBox* m_spinSpd;
    QDoubleSpinBox* m_spinU;
    QDoubleSpinBox* m_spinV;

    void setupUI();
    void updateInputsFromUV();
};

} // namespace sharppy
