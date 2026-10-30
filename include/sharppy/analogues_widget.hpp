#pragma once

#include <QWidget>
#include <QColor>
#include <QFont>
#include <QRect>
#include <vector>
#include "sharppy/sars_db.hpp"
#include "sharppy/viz_prefs.hpp"

namespace sharppy {

// Faithful port of sharppy/viz/analogues.py (plotAnalogues).
class AnaloguesWidget : public QWidget {
    Q_OBJECT

public:
    explicit AnaloguesWidget(QWidget* parent = nullptr);

    void setSars(const SarsAll& sars);                 // profile.*_matches / *_supercell_matches
    void setDeviant(bool use_left);
    void setPreferences(const VizPrefs& prefs);
    void setSelection(const QString& matchString, const QString& type);   // type: "supercell" | "hail"
    void clearSelection();

signals:
    // sars.getSounding() key: match string ("yymmddhh.LOC") and type ("supercell"/"hail")
    void updatematch(const QString& matchString, const QString& type);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    void initUI();
    void drawFrame(QPainter& qp);
    void drawSARS(QPainter& qp, bool tor);
    const SarsMatches& hail() const { return m_useLeft ? m_sars.leftHail : m_sars.rightHail; }
    const SarsMatches& sup() const { return m_useLeft ? m_sars.leftSupercell : m_sars.rightSupercell; }

    SarsAll m_sars;
    bool m_hasData = false;
    bool m_useLeft = false;
    QColor m_bg{"#000000"}, m_fg{"#ffffff"};

    std::vector<std::pair<double, double>> m_yHail, m_ySup;
    bool m_hasSel = false;
    QRectF m_selectRect;

    int m_lpad = 3, m_rpad = 5, m_tpad = 5, m_bpad = 5;
    double m_brx = 0, m_bry = 0, m_tpadF = 0;
    double m_titleHeight = 0, m_plotHeight = 0, m_matchHeight = 0, m_textStart = 0, m_ylast = 0;
    QFont m_titleFont, m_plotFont, m_matchFont;
};

} // namespace sharppy
