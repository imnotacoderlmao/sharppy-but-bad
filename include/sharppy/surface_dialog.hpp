#pragma once

#include <QDialog>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>

namespace sharppy {

class SurfaceModifyDialog : public QDialog {
public:
    SurfaceModifyDialog(double initial_t, double initial_td, QWidget* parent = nullptr) 
        : QDialog(parent) {
        setWindowTitle("Modify Surface Parcel");
        setFixedSize(300, 180);
        setStyleSheet(
            "QDialog { background-color: #1a1a1a; color: #ffffff; font-family: Monospace; }"
            "QLabel { color: #d0d0d0; font-size: 11px; }"
            "QDoubleSpinBox { background-color: #000000; color: #00ffff; border: 1px solid #00a0dc; padding: 2px; }"
            "QCheckBox { color: #d0d0d0; }"
            "QPushButton { background-color: #004060; color: #ffffff; border: 1px solid #00a0dc; padding: 4px 12px; }"
            "QPushButton:hover { background-color: #006090; }"
        );

        QVBoxLayout* layout = new QVBoxLayout(this);

        QHBoxLayout* row1 = new QHBoxLayout();
        row1->addWidget(new QLabel("Surface Temp (°C):", this));
        m_tempSpin = new QDoubleSpinBox(this);
        m_tempSpin->setRange(-60.0, 60.0);
        m_tempSpin->setValue(initial_t);
        row1->addWidget(m_tempSpin);
        layout->addLayout(row1);

        QHBoxLayout* row2 = new QHBoxLayout();
        row2->addWidget(new QLabel("Surface Dewpoint (°C):", this));
        m_dwptSpin = new QDoubleSpinBox(this);
        m_dwptSpin->setRange(-80.0, 50.0);
        m_dwptSpin->setValue(initial_td);
        row2->addWidget(m_dwptSpin);
        layout->addLayout(row2);

        m_mixCheck = new QCheckBox("Dry-Adiabatic Boundary Layer Mixing", this);
        m_mixCheck->setChecked(true);
        layout->addWidget(m_mixCheck);

        QHBoxLayout* btnRow = new QHBoxLayout();
        QPushButton* okBtn = new QPushButton("Apply", this);
        QPushButton* cancelBtn = new QPushButton("Cancel", this);
        btnRow->addWidget(okBtn);
        btnRow->addWidget(cancelBtn);
        layout->addLayout(btnRow);

        connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
        connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    }

    double getTemp() const { return m_tempSpin->value(); }
    double getDewpoint() const { return m_dwptSpin->value(); }
    bool isDryMix() const { return m_mixCheck->isChecked(); }

private:
    QDoubleSpinBox* m_tempSpin;
    QDoubleSpinBox* m_dwptSpin;
    QCheckBox* m_mixCheck;
};

} // namespace sharppy
