#include "sharppy/cm1_dialog.hpp"
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QFontDatabase>
#include <QSettings>
#include <QVBoxLayout>

namespace sharppy {

static QDoubleSpinBox* spin(double v, double lo, double hi, int dec, const QString& suffix = {}) {
    auto* s = new QDoubleSpinBox; s->setRange(lo, hi); s->setDecimals(dec); s->setValue(v); s->setSuffix(suffix);
    return s;
}

Cm1ExportDialog::Cm1ExportDialog(const Profile& prof, QWidget* parent) : QDialog(parent), m_prof(prof) {
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
    m_stabSummary = new QLabel;
    m_stabSummary->setWordWrap(true);
    m_stabList = new QPlainTextEdit; m_stabList->setReadOnly(true); m_stabList->setMinimumHeight(140);
    m_stabList->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_stabList->setLineWrapMode(QPlainTextEdit::NoWrap);
    auto* lay = new QVBoxLayout(this);
    setMinimumWidth(640);
    lay->addLayout(form);
    lay->addWidget(new QLabel("<b>CM1 stability check</b> (base.F: needs N² &gt; 0 and Ri &gt; 0.25)"));
    lay->addWidget(m_stabSummary);
    lay->addWidget(m_stabList);
    lay->addWidget(bb);
    for (QDoubleSpinBox* b : {m_dz, m_desat, m_dlBot, m_dlTop, m_dlPct, m_fill, m_adiaH, m_adiaS})
        connect(b, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this]() { updateStability(); });
    connect(m_layer, &QCheckBox::toggled, this, [this]() { updateStability(); });
    updateStability();
}

void Cm1ExportDialog::updateStability() {
    Cm1Sounding snd; std::string err;
    if (!build_cm1(m_prof, options(), snd, &err)) {
        m_stabSummary->setText("Cannot check: " + QString::fromStdString(err));
        m_stabList->clear();
        return;
    }
    const Cm1Stability st = cm1_stability(snd);
    const int total = (int)st.rows.size();
    if (st.nN2bad == 0 && st.nRibad == 0) {
        m_stabSummary->setText(QString("All %1 level pairs pass (N² > 0 and Ri > 0.25).").arg(total));
        m_stabList->setPlainText("No problem levels.");
        return;
    }
    m_stabSummary->setText(QString("N² ≤ 0 at %1 of %2 level pairs; Ri ≤ 0.25 at %3. Line numbers are lines of the exported file "
                                   "(line 1 is the surface line); each row is the layer between that line and the one above it. "
                                   "N² uses virtual potential temperature (moisture included), so the superadiabatic limit, which acts on θ only, may not clear it; edit the sounding near those heights.")
                           .arg(st.nN2bad).arg(total).arg(st.nRibad));
    QString t = "  line   z (m)       N² (1/s²)      Ri        issue\n";
    int shown = 0;
    for (const Cm1StabRow& r : st.rows) {
        if (!r.n2bad && !r.ribad) continue;
        if (++shown > 400) { t += "  ... (more levels not shown)\n"; break; }
        QString issue;
        if (r.n2bad) issue += (r.n2 < -1e-12 ? "N² < 0 (CM1 warns) " : "N² = 0 ");
        if (r.ribad) issue += "Ri < 0.25";
        t += QString("%1 %2 %3 %4  %5\n").arg(r.line, 6).arg(r.z, 9, 'f', 1).arg(r.n2, 14, 'e', 3).arg(r.ri, 11, 'g', 4).arg(issue.trimmed());
    }
    m_stabList->setPlainText(t);
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
