#pragma once

#include <QDialog>
#include <QComboBox>
#include <QRadioButton>
#include <string>
#include <QRadioButton>
#include "sharppy/viz_prefs.hpp"

namespace sharppy {

struct AppPreferences {
    std::string color_style = "standard"; // "standard", "inverted", "protanopia"
    std::string temp_units  = "Fahrenheit"; // "Fahrenheit", "Celsius"
    std::string wind_units  = "knots";      // "knots", "m/s"
    std::string pw_units    = "in";         // "in", "cm"
    std::string readout_tr  = "tmpc";       // skew-T cursor readout, top right
    std::string readout_br  = "dwpc";       // bottom right
};

// Persistent storage (QSettings "sharppy"/"cppsharp") shared by the picker and the sounding window.
AppPreferences loadAppPreferences();
void saveAppPreferences(const AppPreferences& p);
// Full VizPrefs map (style colors + units + readout vars) as handed to every widget's setPreferences().
VizPrefs toVizPrefs(const AppPreferences& p);

class PreferencesDialog : public QDialog {
    Q_OBJECT

public:
    explicit PreferencesDialog(const AppPreferences& current, QWidget* parent = nullptr);
    AppPreferences getPreferences() const { return m_prefs; }

private slots:
    void onAccept();

private:
    AppPreferences m_prefs;
    QComboBox* m_colorStyleCombo;
    QRadioButton* m_tempF;
    QRadioButton* m_tempC;
    QRadioButton* m_windKts;
    QRadioButton* m_windMs;
    QRadioButton* m_pwIn;
    QRadioButton* m_pwCm;
    QComboBox* m_readTr;
    QComboBox* m_readBr;
};

} // namespace sharppy