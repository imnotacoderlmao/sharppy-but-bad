#pragma once
#include <QDialog>
#include "sharppy/cm1_io.hpp"
class QDoubleSpinBox; class QCheckBox; class QPlainTextEdit; class QLabel;
namespace sharppy {
// Options for the CM1 export (the sndmod command-line flags).
class Cm1ExportDialog : public QDialog {
    Q_OBJECT
public:
    explicit Cm1ExportDialog(const Profile& prof, QWidget* parent = nullptr);
    Cm1ExportOptions options() const;
private:
    void updateStability();        // re-runs CM1's Ri / N^2 check on what the current options would export
    Profile m_prof;
    QLabel* m_stabSummary; QPlainTextEdit* m_stabList;
    QDoubleSpinBox *m_dz, *m_desat, *m_dlBot, *m_dlTop, *m_dlPct, *m_fill, *m_adiaH, *m_adiaS;
    QCheckBox *m_layer, *m_sharp;
};
}
