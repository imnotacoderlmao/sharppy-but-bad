#pragma once
#include <QDialog>
#include "sharppy/cm1_io.hpp"
class QDoubleSpinBox; class QCheckBox;
namespace sharppy {
// Options for the CM1 export (the sndmod command-line flags).
class Cm1ExportDialog : public QDialog {
    Q_OBJECT
public:
    explicit Cm1ExportDialog(QWidget* parent = nullptr);
    Cm1ExportOptions options() const;
private:
    QDoubleSpinBox *m_dz, *m_desat, *m_dlBot, *m_dlTop, *m_dlPct, *m_fill, *m_adiaH, *m_adiaS;
    QCheckBox *m_layer, *m_sharp;
};
}
