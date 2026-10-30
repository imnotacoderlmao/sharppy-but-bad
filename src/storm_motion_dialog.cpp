#include "sharppy/storm_motion_dialog.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <cmath>
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace sharppy {

StormMotionDialog::StormMotionDialog(const BunkersMotion& bunkers,
                                     const CorfidiMotion& corfidi,
                                     double meanU, double meanV,
                                     double currentU, double currentV,
                                     QWidget* parent)
    : QDialog(parent), m_bunkers(bunkers), m_corfidi(corfidi),
      m_meanU(meanU), m_meanV(meanV), m_u(currentU), m_v(currentV) {

    setWindowTitle("Custom Storm Motion Editor");
    resize(420, 360);
    setupUI();
    updateInputsFromUV();
}

void StormMotionDialog::setupUI() {
    setStyleSheet(
        "QDialog { background-color: #12161f; color: #e2e8f0; font-family: Monospace, sans-serif; }"
        "QGroupBox { border: 1px solid #334155; border-radius: 6px; margin-top: 12px; font-weight: bold; color: #38bdf8; padding-top: 14px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }"
        "QRadioButton { color: #cbd5e1; font-size: 11px; spacing: 6px; }"
        "QRadioButton:hover { color: #ffffff; }"
        "QLabel { color: #94a3b8; font-size: 11px; }"
        "QDoubleSpinBox { background-color: #1e293b; color: #f8fafc; border: 1px solid #475569; border-radius: 4px; padding: 4px; }"
        "QPushButton { background-color: #0284c7; color: #ffffff; border: none; border-radius: 4px; padding: 6px 16px; font-weight: bold; font-size: 11px; }"
        "QPushButton:hover { background-color: #38bdf8; color: #0f172a; }"
        "QPushButton#cancelBtn { background-color: #334155; color: #e2e8f0; }"
        "QPushButton#cancelBtn:hover { background-color: #475569; }"
    );

    QVBoxLayout* root = new QVBoxLayout(this);
    root->setSpacing(12);

    // Presets Group
    QGroupBox* presetGroup = new QGroupBox("Presets", this);
    QVBoxLayout* presetLayout = new QVBoxLayout(presetGroup);

    auto formatPreset = [](const QString& name, double u, double v) {
        double spd = std::hypot(u, v);
        double dir = std::atan2(-u, -v) * 180.0 / M_PI;
        if (dir < 0) dir += 360.0;
        return QString("%1: %2° / %3 kt  (u=%4, v=%5)")
            .arg(name, -16)
            .arg((int)std::round(dir), 3, 10, QChar('0'))
            .arg((int)std::round(spd), 2)
            .arg(u, 4, 'f', 1)
            .arg(v, 4, 'f', 1);
    };

    m_radRM = new QRadioButton(formatPreset("Bunkers Right", m_bunkers.right_mover.u, m_bunkers.right_mover.v), presetGroup);
    m_radLM = new QRadioButton(formatPreset("Bunkers Left", m_bunkers.left_mover.u, m_bunkers.left_mover.v), presetGroup);
    m_radCorfidiUp = new QRadioButton(formatPreset("Corfidi Upwind", m_corfidi.upwind.u, m_corfidi.upwind.v), presetGroup);
    m_radCorfidiDown = new QRadioButton(formatPreset("Corfidi Downwind", m_corfidi.downwind.u, m_corfidi.downwind.v), presetGroup);
    m_radMeanWind = new QRadioButton(formatPreset("0-6km Mean Wind", m_meanU, m_meanV), presetGroup);
    m_radCustom = new QRadioButton("Custom Vector", presetGroup);

    presetLayout->addWidget(m_radRM);
    presetLayout->addWidget(m_radLM);
    presetLayout->addWidget(m_radCorfidiUp);
    presetLayout->addWidget(m_radCorfidiDown);
    presetLayout->addWidget(m_radMeanWind);
    presetLayout->addWidget(m_radCustom);
    root->addWidget(presetGroup);

    connect(m_radRM, &QRadioButton::toggled, this, &StormMotionDialog::onPresetSelected);
    connect(m_radLM, &QRadioButton::toggled, this, &StormMotionDialog::onPresetSelected);
    connect(m_radCorfidiUp, &QRadioButton::toggled, this, &StormMotionDialog::onPresetSelected);
    connect(m_radCorfidiDown, &QRadioButton::toggled, this, &StormMotionDialog::onPresetSelected);
    connect(m_radMeanWind, &QRadioButton::toggled, this, &StormMotionDialog::onPresetSelected);
    connect(m_radCustom, &QRadioButton::toggled, this, &StormMotionDialog::onPresetSelected);

    // Vector Editor Group
    QGroupBox* editGroup = new QGroupBox("Vector Parameters", this);
    QGridLayout* editGrid = new QGridLayout(editGroup);

    m_spinDir = new QDoubleSpinBox(editGroup);
    m_spinDir->setRange(0.0, 360.0);
    m_spinDir->setSuffix("°");
    m_spinDir->setDecimals(1);

    m_spinSpd = new QDoubleSpinBox(editGroup);
    m_spinSpd->setRange(0.0, 150.0);
    m_spinSpd->setSuffix(" kt");
    m_spinSpd->setDecimals(1);

    m_spinU = new QDoubleSpinBox(editGroup);
    m_spinU->setRange(-150.0, 150.0);
    m_spinU->setSuffix(" kt");
    m_spinU->setDecimals(1);

    m_spinV = new QDoubleSpinBox(editGroup);
    m_spinV->setRange(-150.0, 150.0);
    m_spinV->setSuffix(" kt");
    m_spinV->setDecimals(1);

    editGrid->addWidget(new QLabel("Direction:", editGroup), 0, 0);
    editGrid->addWidget(m_spinDir, 0, 1);
    editGrid->addWidget(new QLabel("Speed:", editGroup), 0, 2);
    editGrid->addWidget(m_spinSpd, 0, 3);

    editGrid->addWidget(new QLabel("U (East):", editGroup), 1, 0);
    editGrid->addWidget(m_spinU, 1, 1);
    editGrid->addWidget(new QLabel("V (North):", editGroup), 1, 2);
    editGrid->addWidget(m_spinV, 1, 3);

    root->addWidget(editGroup);

    connect(m_spinDir, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &StormMotionDialog::onDirSpdChanged);
    connect(m_spinSpd, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &StormMotionDialog::onDirSpdChanged);
    connect(m_spinU, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &StormMotionDialog::onUVChanged);
    connect(m_spinV, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &StormMotionDialog::onUVChanged);

    // Buttons
    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->addStretch(1);

    QPushButton* cancelBtn = new QPushButton("Cancel", this);
    cancelBtn->setObjectName("cancelBtn");
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    QPushButton* okBtn = new QPushButton("Apply Storm Motion", this);
    connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);

    btnLayout->addWidget(cancelBtn);
    btnLayout->addWidget(okBtn);
    root->addLayout(btnLayout);

    m_radCustom->setChecked(true);
}

void StormMotionDialog::updateInputsFromUV() {
    m_updating = true;
    double spd = std::hypot(m_u, m_v);
    double dir = std::atan2(-m_u, -m_v) * 180.0 / M_PI;
    if (dir < 0) dir += 360.0;

    m_spinU->setValue(m_u);
    m_spinV->setValue(m_v);
    m_spinDir->setValue(dir);
    m_spinSpd->setValue(spd);
    m_updating = false;
}

void StormMotionDialog::onPresetSelected() {
    if (m_radRM->isChecked()) {
        m_u = m_bunkers.right_mover.u;
        m_v = m_bunkers.right_mover.v;
        m_isCustom = false;
        updateInputsFromUV();
    } else if (m_radLM->isChecked()) {
        m_u = m_bunkers.left_mover.u;
        m_v = m_bunkers.left_mover.v;
        m_isCustom = true;
        updateInputsFromUV();
    } else if (m_radCorfidiUp->isChecked()) {
        m_u = m_corfidi.upwind.u;
        m_v = m_corfidi.upwind.v;
        m_isCustom = true;
        updateInputsFromUV();
    } else if (m_radCorfidiDown->isChecked()) {
        m_u = m_corfidi.downwind.u;
        m_v = m_corfidi.downwind.v;
        m_isCustom = true;
        updateInputsFromUV();
    } else if (m_radMeanWind->isChecked()) {
        m_u = m_meanU;
        m_v = m_meanV;
        m_isCustom = true;
        updateInputsFromUV();
    } else if (m_radCustom->isChecked()) {
        m_isCustom = true;
    }
}

void StormMotionDialog::onDirSpdChanged() {
    if (m_updating) return;
    m_updating = true;
    double dir = m_spinDir->value();
    double spd = m_spinSpd->value();

    double rad = dir * M_PI / 180.0;
    m_u = -spd * std::sin(rad);
    m_v = -spd * std::cos(rad);

    m_spinU->setValue(m_u);
    m_spinV->setValue(m_v);
    m_radCustom->setChecked(true);
    m_isCustom = true;
    m_updating = false;
}

void StormMotionDialog::onUVChanged() {
    if (m_updating) return;
    m_updating = true;
    m_u = m_spinU->value();
    m_v = m_spinV->value();

    double spd = std::hypot(m_u, m_v);
    double dir = std::atan2(-m_u, -m_v) * 180.0 / M_PI;
    if (dir < 0) dir += 360.0;

    m_spinDir->setValue(dir);
    m_spinSpd->setValue(spd);
    m_radCustom->setChecked(true);
    m_isCustom = true;
    m_updating = false;
}

} // namespace sharppy
