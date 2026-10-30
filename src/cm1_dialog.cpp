#include "sharppy/cm1_dialog.hpp"
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QSettings>
#include <QVBoxLayout>

namespace sharppy {

static QDoubleSpinBox* spin(double v, double lo, double hi, int dec, const QString& suffix = {}) {
    auto* s = new QDoubleSpinBox; s->setRange(lo, hi); s->setDecimals(dec); s->setValue(v); s->setSuffix(suffix);
    return s;
}

Cm1ExportDialog::Cm1ExportDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Export CM1 Sounding");
    QSettings st("sharppy_cpp", "sharppy");
    auto* form = new QFormLayout;
    m_dz = spin(st.value("cm1/dz", 100.).toDouble(), 1, 5000, 1, " m");
    m_desat = spin(st.value("cm1/desat", 0.).toDouble(), 0, 100, 2, " % RH");
    m_layer = new QCheckBox("Desaturate a height layer");
    m_dlBot = spin(st.value("cm1/dlbot", 1000.).toDouble(), 0, 30000, 0, " m");
    m_dlTop = spin(st.value("cm1/dltop", 3000.).toDouble(), 0, 30000, 0, " m");
    m_dlPct = spin(st.value("cm1/dlpct", 5.5).toDouble(), 0, 100, 2, " % RH");
    m_fill = spin(st.value("cm1/fill", 0.).toDouble(), 0, 100000, 0, " m");
    m_adiaH = spin(st.value("cm1/adiah", 0.).toDouble(), 0, 30000, 0, " m");
    m_adiaS = spin(st.value("cm1/adias", 0.04).toDouble(), -10, 10, 3, " K/km");
    m_sharp = new QCheckBox("Also write a SHARPpy (.sharppy) file");
    m_sharp->setChecked(st.value("cm1/sharp", false).toBool());
    m_layer->setChecked(st.value("cm1/layer", false).toBool());
    form->addRow("Output spacing (dz)", m_dz);
    form->addRow("Desaturate where RH ≥ 97%", m_desat);
    form->addRow(m_layer);
    form->addRow("  Layer bottom", m_dlBot);
    form->addRow("  Layer top", m_dlTop);
    form->addRow("  RH reduction", m_dlPct);
    form->addRow("Fill above data top to (isothermal)", m_fill);
    form->addRow("Limit superadiabatic layers up to (0 = off)", m_adiaH);
    form->addRow("  Stability parameter", m_adiaS);
    form->addRow(m_sharp);
    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(bb, &QDialogButtonBox::accepted, this, [this]() {
        QSettings s("sharppy_cpp", "sharppy");
        s.setValue("cm1/dz", m_dz->value()); s.setValue("cm1/desat", m_desat->value());
        s.setValue("cm1/layer", m_layer->isChecked()); s.setValue("cm1/dlbot", m_dlBot->value());
        s.setValue("cm1/dltop", m_dlTop->value()); s.setValue("cm1/dlpct", m_dlPct->value());
        s.setValue("cm1/fill", m_fill->value()); s.setValue("cm1/adiah", m_adiaH->value());
        s.setValue("cm1/adias", m_adiaS->value()); s.setValue("cm1/sharp", m_sharp->isChecked());
        accept();
    });
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    auto* lay = new QVBoxLayout(this);
    lay->addLayout(form);
    lay->addWidget(bb);
}

Cm1ExportOptions Cm1ExportDialog::options() const {
    Cm1ExportOptions o;
    o.dz = m_dz->value(); o.desat = m_desat->value();
    o.desatLayer = m_layer->isChecked();
    o.dlBottom = m_dlBot->value(); o.dlTop = m_dlTop->value(); o.dlPct = m_dlPct->value();
    o.fillTop = m_fill->value(); o.adiabaticHeight = m_adiaH->value(); o.adiabaticStability = m_adiaS->value();
    o.alsoSharppy = m_sharp->isChecked();
    return o;
}

} // namespace sharppy
