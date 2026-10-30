#include "sharppy/preferences_dialog.hpp"
#include "sharppy/viz_styles.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QTabWidget>
#include <QSettings>
#include <QPainter>

namespace sharppy {

static const char* const kReadoutNames[7] = {"Temperature (C)", "Dewpoint (C)", "Equiv. Potential Temp. (K)",
    "Wetbulb Temperature (C)", "Potential Temperature (K)", "Water Vapor Mixing Ratio (g/kg)", "Vertical Velocity (mb/hr)"};
static const char* const kReadoutKeys[7] = {"tmpc", "dwpc", "thetae", "wetbulb", "theta", "wvmr", "omeg"};

AppPreferences loadAppPreferences() {
    QSettings s("sharppy", "cppsharp");
    AppPreferences p;
    auto g = [&](const char* k, std::string& v) { v = s.value(QString("prefs/") + k, QString::fromStdString(v)).toString().toStdString(); };
    g("color_style", p.color_style); g("temp_units", p.temp_units); g("wind_units", p.wind_units);
    g("pw_units", p.pw_units); g("readout_tr", p.readout_tr); g("readout_br", p.readout_br);
    return p;
}

void saveAppPreferences(const AppPreferences& p) {
    QSettings s("sharppy", "cppsharp");
    auto w = [&](const char* k, const std::string& v) { s.setValue(QString("prefs/") + k, QString::fromStdString(v)); };
    w("color_style", p.color_style); w("temp_units", p.temp_units); w("wind_units", p.wind_units);
    w("pw_units", p.pw_units); w("readout_tr", p.readout_tr); w("readout_br", p.readout_br);
}

VizPrefs toVizPrefs(const AppPreferences& a) {
    VizPrefs p = vizStyle(a.color_style);
    p["color_style"] = QString::fromStdString(a.color_style);
    p["temp_units"] = QString::fromStdString(a.temp_units);
    p["wind_units"] = QString::fromStdString(a.wind_units);
    p["pw_units"] = QString::fromStdString(a.pw_units);
    p["readout_tr"] = QString::fromStdString(a.readout_tr);
    p["readout_br"] = QString::fromStdString(a.readout_br);
    return p;
}

// stands in for ColorPreview's sample_*.png (those images were not provided): a few profile-colored strokes
class StylePreview : public QWidget {
public:
    explicit StylePreview(QWidget* parent) : QWidget(parent) { setMinimumSize(236, 160); }
    void setStyleName(const std::string& n) { m_p = vizStyle(n); update(); }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter qp(this);
        auto c = [&](const char* k) { return QColor(m_p.value(k)); };
        qp.fillRect(rect(), c("bg_color"));
        const int w = width(), h = height();
        qp.setPen(QPen(c("skew_itherm_color"), 1));
        for (int i = -2; i < 8; ++i) qp.drawLine(i * w / 6, h, i * w / 6 + h / 2, 0);
        qp.setPen(QPen(c("skew_mixr_color"), 1, Qt::DashLine)); qp.drawLine(w / 8, h, w / 3, h * 2 / 3);
        qp.setPen(QPen(c("wetb_color"), 2));  qp.drawLine(w / 2 - 8, h, w / 2 + 14, 0);
        qp.setPen(QPen(c("temp_color"), 3));  qp.drawLine(w / 2 + 10, h, w / 2 + 44, 0);
        qp.setPen(QPen(c("dewp_color"), 3));  qp.drawLine(w / 2 - 20, h, w / 2 - 6, 0);
        QColor hs[5] = {c("0_1_color"), c("1_3_color"), c("3_6_color"), c("6_9_color"), c("9_12_color")};
        for (int i = 0; i < 5; ++i) { qp.setPen(QPen(hs[i], 3)); qp.drawLine(w - 60 + i * 11, h - 10 - i * 12, w - 49 + i * 11, h - 22 - i * 12); }
        qp.setPen(c("fg_color")); qp.drawText(6, 14, "SHARPpy"); qp.drawRect(0, 0, w - 1, h - 1);
    }
private:
    VizPrefs m_p;
};

static QGroupBox* radioSet(const QString& title, const QStringList& names, const QString& cur, QRadioButton** out, QWidget* parent) {
    QGroupBox* box = new QGroupBox(title, parent);
    QHBoxLayout* l = new QHBoxLayout(box);
    for (int i = 0; i < names.size(); ++i) {
        out[i] = new QRadioButton(names[i], box);
        out[i]->setChecked(names[i] == cur);
        l->addWidget(out[i]);
    }
    return box;
}

PreferencesDialog::PreferencesDialog(const AppPreferences& current, QWidget* parent)
    : QDialog(parent), m_prefs(current) {
    setWindowTitle("SHARPpy Preferences");

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    QTabWidget* tabs = new QTabWidget(this);

    // Colors tab
    QWidget* colors = new QWidget;
    QVBoxLayout* cl = new QVBoxLayout(colors);
    m_colorStyleCombo = new QComboBox(colors);
    m_colorStyleCombo->addItems({"Standard", "Inverted", "Protanopia"});
    int ci = m_prefs.color_style == "inverted" ? 1 : m_prefs.color_style == "protanopia" ? 2 : 0;
    m_colorStyleCombo->setCurrentIndex(ci);
    StylePreview* prev = new StylePreview(colors);
    prev->setStyleName(m_prefs.color_style);
    cl->addWidget(m_colorStyleCombo);
    cl->addWidget(prev, 1);
    connect(m_colorStyleCombo, QOverload<int>::of(&QComboBox::activated), this, [prev](int i) {
        prev->setStyleName(i == 1 ? "inverted" : i == 2 ? "protanopia" : "standard");
    });
    tabs->addTab(colors, "Colors");

    // Units tab
    QWidget* units = new QWidget;
    QVBoxLayout* ul = new QVBoxLayout(units);
    QRadioButton* t[2]; QRadioButton* w[2]; QRadioButton* pw[2];
    ul->addWidget(radioSet("Surface Temperature Units", {"Fahrenheit", "Celsius"}, QString::fromStdString(m_prefs.temp_units), t, units));
    ul->addWidget(radioSet("Wind Units", {"knots", "m/s"}, QString::fromStdString(m_prefs.wind_units), w, units));
    ul->addWidget(radioSet("Precipitable Water Vapor Units", {"in", "cm"}, QString::fromStdString(m_prefs.pw_units), pw, units));
    m_tempF = t[0]; m_tempC = t[1]; m_windKts = w[0]; m_windMs = w[1]; m_pwIn = pw[0]; m_pwCm = pw[1];
    ul->addStretch(1);
    tabs->addTab(units, "Units");

    // Readout tab
    QWidget* ro = new QWidget;
    QVBoxLayout* rl = new QVBoxLayout(ro);
    m_readTr = new QComboBox(ro); m_readBr = new QComboBox(ro);
    for (int i = 0; i < 7; ++i) { m_readTr->addItem(kReadoutNames[i]); m_readBr->addItem(kReadoutNames[i]); }
    for (int i = 0; i < 7; ++i) {
        if (m_prefs.readout_tr == kReadoutKeys[i]) m_readTr->setCurrentIndex(i);
        if (m_prefs.readout_br == kReadoutKeys[i]) m_readBr->setCurrentIndex(i);
    }
    rl->addWidget(new QLabel("Top Right Readout Variable:"));
    rl->addWidget(m_readTr);
    rl->addWidget(new QLabel("Bottom Right Readout Variable:"));
    rl->addWidget(m_readBr);
    rl->addStretch(1);
    tabs->addTab(ro, "Readout");
    mainLayout->addWidget(tabs);

    QHBoxLayout* btnLayout = new QHBoxLayout();
    QPushButton* okBtn = new QPushButton("Accept", this);
    QPushButton* cancelBtn = new QPushButton("Cancel", this);
    okBtn->setDefault(true);
    connect(okBtn, &QPushButton::clicked, this, &PreferencesDialog::onAccept);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(okBtn);
    btnLayout->addWidget(cancelBtn);
    mainLayout->addLayout(btnLayout);
}

void PreferencesDialog::onAccept() {
    int idx = m_colorStyleCombo->currentIndex();
    m_prefs.color_style = idx == 1 ? "inverted" : idx == 2 ? "protanopia" : "standard";
    m_prefs.temp_units = m_tempC->isChecked() ? "Celsius" : "Fahrenheit";
    m_prefs.wind_units = m_windMs->isChecked() ? "m/s" : "knots";
    m_prefs.pw_units = m_pwCm->isChecked() ? "cm" : "in";
    m_prefs.readout_tr = kReadoutKeys[m_readTr->currentIndex()];
    m_prefs.readout_br = kReadoutKeys[m_readBr->currentIndex()];
    accept();
}

} // namespace sharppy
