#include "sharppy/convective_table_widget.hpp"
#include "sharppy/utils_fmt.hpp"
#include <QFontMetrics>
#include <QDialog>
#include <QCheckBox>
#include <QPushButton>
#include <QMessageBox>
#include <cmath>

namespace sharppy {

namespace {
bool bad(double v) { return !std::isfinite(v) || v == MISSING; }
QString i2s(double v) { return int2str(v); }
QString f2s(double v, int d) { return float2str(v, d); }
const char* kTypes[6] = {"SFC", "ML", "FCST", "MU", "EFF", "USER"};
}

int ConvectiveTableWidget::typeIndex(const QString& t) {
    for (int i = 0; i < 6; ++i) if (t == kTypes[i]) return i;
    return 3;
}

ConvectiveTableWidget::ConvectiveTableWidget(QWidget* parent) : QWidget(parent) {
    setMinimumSize(200, 120);
    initUI();
}

void ConvectiveTableWidget::initUI() {
    const double hgt = height();
    m_brx = width(); m_bry = hgt;
    const double fsize = std::floor(.06 * hgt);
    m_tpad = std::floor(.03 * hgt);
    m_labelFont = QFont("Helvetica");
    m_labelFont.setPixelSize(std::max(1, (int)fsize));
    m_labelHeight = QFontMetrics(m_labelFont).xHeight() + m_tpad;
    m_ylast = m_labelHeight;
}

void ConvectiveTableWidget::resizeEvent(QResizeEvent*) { initUI(); }

void ConvectiveTableWidget::setProfile(const Profile& prof) { m_prof = prof; m_hasProf = !prof.empty(); update(); }

void ConvectiveTableWidget::setParcels(const ParcelResult& sb, const ParcelResult& ml, const ParcelResult& fcst,
                                       const ParcelResult& mu) {
    setParcels(sb, ml, fcst, mu, sb, sb);
}
void ConvectiveTableWidget::setParcels(const ParcelResult& sb, const ParcelResult& ml, const ParcelResult& fcst,
                                       const ParcelResult& mu, const ParcelResult& eff, const ParcelResult& usr) {
    m_parcels["SFC"] = sb; m_parcels["ML"] = ml; m_parcels["FCST"] = fcst;
    m_parcels["MU"] = mu; m_parcels["EFF"] = eff; m_parcels["USER"] = usr;
    update();
}
void ConvectiveTableWidget::setIndices(const tab::ThermoIndices& idx) { m_ix = idx; m_hasIdx = true; update(); }
void ConvectiveTableWidget::setSevere(double ship, double stpFixed, double stpCin, double scp) {
    m_ship = ship; m_stpFixed = stpFixed; m_stpCin = stpCin; m_scp = scp; update();
}

void ConvectiveTableWidget::setPreferences(const VizPrefs& p) {
    auto it = p.find("pw_units"); if (it != p.end()) m_pwUnits = it.value();
    it = p.find("temp_units"); if (it != p.end()) m_tempUnits = it.value();
    m_bg = prefColor(p, "bg_color", m_bg);
    m_fg = prefColor(p, "fg_color", m_fg);
    static const char* pk[7] = {"pwat_m3_color", "pwat_m2_color", "pwat_m1_color", nullptr, "pwat_p1_color", "pwat_p2_color", "pwat_p3_color"};
    for (int i = 0; i < 7; ++i) m_pwat[i] = pk[i] ? prefColor(p, pk[i], m_pwat[i]) : m_fg;
    static const char* ak[7] = {"alert_lscp_color", "alert_l1_color", "alert_l2_color", "alert_l3_color",
                                "alert_l4_color", "alert_l5_color", "alert_l6_color"};
    for (int i = 0; i < 7; ++i) m_alert[i] = prefColor(p, ak[i], m_alert[i]);
    m_pclSel = prefColor(p, "pcl_sel_color", m_pclSel);
    update();
}

void ConvectiveTableWidget::setParcelTypes(const QStringList& t) {
    static const QStringList ok{"SFC", "ML", "FCST", "MU", "EFF", "USER"};
    if (t.size() != 4) return;
    for (const QString& s : t) if (!ok.contains(s)) return;
    m_types = t;
    m_sel = std::min(m_sel, 3);
    update();
}

int ConvectiveTableWidget::selectedParcel() const { return typeIndex(m_types.value(m_sel, "MU")); }

void ConvectiveTableWidget::selectParcelType(const QString& type) {
    int i = m_types.indexOf(type);
    if (i >= 0) { m_sel = i; update(); }
}

void ConvectiveTableWidget::drawFrame(QPainter& qp) {
    qp.setPen(QPen(m_fg, 1, Qt::SolidLine));
    qp.setFont(m_labelFont);
    const double x1 = m_brx / 8;
    int count = 0;
    for (const char* t : {"PCL", "CAPE", "CINH", "LCL", "LI", "LFC", "EL"}) {
        qp.drawText(QRect((int)(x1 * count), 1, (int)(x1 * 2), (int)m_labelHeight), Qt::TextDontClip | Qt::AlignCenter, t);
        ++count;
    }
    qp.drawLine(0, (int)m_labelHeight, (int)m_brx, (int)m_labelHeight);
    m_ylast = m_labelHeight;
}

void ConvectiveTableWidget::drawConvectiveIndices(QPainter& qp) {
    QPen pen(m_fg, 1, Qt::SolidLine);
    qp.setPen(pen);
    qp.setFont(m_labelFont);
    const double x1 = m_brx / 8;
    double y1 = m_ylast + m_tpad;
    const int lh = (int)m_labelHeight;
    const int fl = Qt::TextDontClip | Qt::AlignCenter;

    const int n = std::min<int>(4, m_types.size());
    struct R { QString cape, cin, lcl, li, lfc, el; double capeV, cinV, lclV, liV; } rows[4];
    for (int i = 0; i < n; ++i) {
        const ParcelResult& p = m_parcels.count(m_types[i]) ? m_parcels[m_types[i]] : ParcelResult();
        rows[i] = {i2s(p.cape), i2s(p.cin), i2s(p.lcl_h_agl), i2s(p.li), i2s(p.lfc_h_agl), i2s(p.el_h_agl),
                   bad(p.cape) ? NAN : std::lround(p.cape), bad(p.cin) ? NAN : std::lround(p.cin),
                   bad(p.lcl_h_agl) ? NAN : std::lround(p.lcl_h_agl), bad(p.li) ? NAN : std::lround(p.li)};
    }

    // PCL type
    for (int i = 0; i < n; ++i) {
        m_bounds[i][0] = y1;
        qp.setPen(QPen(i == m_sel ? m_pclSel : m_fg, 1, Qt::SolidLine));
        qp.drawText(QRect(0, (int)y1, (int)(x1 * 2), lh), fl, m_types[i]);
        y1 += m_labelHeight;
        m_bounds[i][1] = y1;
    }
    auto column = [&](int col, auto colorFn, auto textFn) {
        double y = m_ylast + m_tpad;
        for (int i = 0; i < n; ++i) {
            qp.setPen(QPen(colorFn(i), 1, Qt::SolidLine));
            qp.drawText(QRect((int)(x1 * col), (int)y, (int)(x1 * 2), lh), fl, textFn(i));
            y += m_labelHeight;
        }
    };
    const bool selPos = false; (void)selPos;
    auto capeOK = [&](int i) { return !std::isnan(rows[i].capeV) && rows[i].capeV > 0; };

    column(1, [&](int i) -> QColor {
        double c = rows[i].capeV;
        if (std::isnan(c) || i != m_sel) return m_fg;
        return c >= 4000 ? m_alert[6] : c >= 3000 ? m_alert[5] : c >= 2000 ? m_alert[4] : c >= 1000 ? m_alert[3] : c > 0 ? m_alert[2] : m_fg;
    }, [&](int i) { return rows[i].cape; });
    column(2, [&](int i) -> QColor {
        double c = rows[i].cinV;
        if (std::isnan(c) || !capeOK(i) || i != m_sel) return m_fg;
        return c >= -25 ? m_alert[6] : c >= -50 ? m_alert[5] : c >= -75 ? m_alert[4] : c >= -125 ? m_alert[3] : m_alert[2];
    }, [&](int i) { return rows[i].cin; });
    column(3, [&](int i) -> QColor {
        double l = rows[i].lclV;
        if (std::isnan(l) || !capeOK(i) || i != m_sel) return m_fg;
        return l < 750 ? m_alert[6] : l < 1000 ? m_alert[5] : l < 1250 ? m_alert[4] : l < 2000 ? m_alert[3] : m_alert[2];
    }, [&](int i) { return rows[i].lcl; });
    QColor lastPen = m_fg;
    column(4, [&](int i) -> QColor {
        double v = rows[i].liV;
        QColor c = m_fg;
        if (!std::isnan(v) && capeOK(i) && i == m_sel)
            c = v < -13 ? m_alert[6] : v < -10 ? m_alert[5] : v < -7 ? m_alert[4] : v < -4 ? m_alert[3] : m_alert[2];
        lastPen = c;
        return c;
    }, [&](int i) { return rows[i].li; });
    // LFC / EL use the pen left over from the LI loop
    qp.setPen(QPen(lastPen, 1, Qt::SolidLine));
    {
        double y = m_ylast + m_tpad;
        for (int i = 0; i < n; ++i) { qp.drawText(QRect((int)(x1 * 5), (int)y, (int)(x1 * 2), lh), fl, rows[i].lfc); y += m_labelHeight; }
        y = m_ylast + m_tpad;
        for (int i = 0; i < n; ++i) { qp.drawText(QRect((int)(x1 * 6), (int)y, (int)(x1 * 2), lh), fl, rows[i].el); y += m_labelHeight; }
        m_ylast = y;
    }
    qp.drawLine(0, (int)(m_ylast + 2), (int)m_brx, (int)(m_ylast + 2));

    qp.setPen(QPen(QColor("#996633"), 0.5, Qt::SolidLine));
    if (m_sel < n) {
        const double top = m_bounds[m_sel][0], bot = m_bounds[m_sel][1], r = x1 * 6 + x1 * 2 - 1;
        qp.drawLine(QPointF(2, top - 1), QPointF(r, top - 1));
        qp.drawLine(QPointF(2, top - 1), QPointF(2, bot));
        qp.drawLine(QPointF(2, bot), QPointF(r, bot));
        qp.drawLine(QPointF(r, top - 1), QPointF(r, bot));
    }
}

void ConvectiveTableWidget::drawIndices(QPainter& qp) {
    qp.setFont(m_labelFont);
    const double x1 = m_brx / 10;
    const int rpad = 5;
    const int lh = (int)m_labelHeight;
    const int fl = Qt::TextDontClip | Qt::AlignLeft;
    const tab::ThermoIndices& ix = m_ix;

    static const QString sd[7] = {QString::fromUtf8("(<3σ)"), QString::fromUtf8("(2-3σ)"), QString::fromUtf8("(1-2σ)"),
                                  "", QString::fromUtf8("(1-2σ)"), QString::fromUtf8("(2-3σ)"), QString::fromUtf8("(>3σ)")};
    int fi = std::max(0, std::min(6, ix.pwv_flag + 3));
    QColor pwColor = m_pwat[fi];
    QString distString = sd[fi];
    QString pwDisp = (m_pwUnits == "cm") ? f2s(ix.pwat * 2.54, 1) : f2s(ix.pwat, 2);

    QString tU, drush, convT, maxT;
    auto ftoc = [](double f) { return (f - 32.) * 5. / 9.; };
    if (m_tempUnits == "Celsius") {
        tU = "C"; drush = i2s(ftoc(ix.drush_F)); convT = i2s(ftoc(ix.convT_F)); maxT = i2s(ftoc(ix.maxT_F));
    } else {
        tU = "F"; drush = i2s(ix.drush_F); convT = i2s(ix.convT_F); maxT = i2s(ix.maxT_F);
    }

    double y1 = m_ylast + m_tpad;
    {
        QColor cols[6] = {pwColor, m_fg, m_fg, m_fg, m_fg, m_fg};
        QString tx[6] = {"PW = ", "MeanW = ", "LowRH = ", "MidRH = ", "DCAPE = ", "DownT = "};
        QString ixs[6] = {pwDisp + m_pwUnits + " " + distString, f2s(ix.mean_mixr, 1) + "g/kg", i2s(ix.low_rh) + "%",
                          i2s(ix.mid_rh) + "%", i2s(ix.dcape), drush + tU};
        for (int i = 0; i < 6; ++i) {
            qp.setPen(QPen(cols[i], 1, Qt::SolidLine));
            qp.drawText(QRect(rpad, (int)y1, (int)(x1 * 4), lh), fl, tx[i] + ixs[i]);
            y1 += m_labelHeight;
        }
    }
    y1 = m_ylast + m_tpad;
    {
        QString tx[6] = {"K = ", "TT = ", "ConvT = ", "maxT = ", "ESP = ", "MMP = "};
        QString ixs[6] = {i2s(ix.k_idx), i2s(ix.tt), convT + tU, maxT + tU, f2s(ix.esp, 1), f2s(ix.mmp, 2)};
        for (int i = 0; i < 6; ++i) {
            qp.drawText(QRect((int)(x1 * 3.5), (int)y1, (int)(x1 * 4), lh), fl, tx[i] + ixs[i]);
            y1 += m_labelHeight;
        }
    }
    QColor c3 = m_fg;
    const double b3 = ix.mlb3km;
    if (b3 > 125) c3 = QColor("#FF00FF"); else if (b3 > 100) c3 = QColor("#FF0000");
    else if (b3 > 75) c3 = QColor("#FFA500"); else if (b3 > 50) c3 = QColor("#FFFF00");
    else if (b3 > 25) c3 = QColor("#00FF00");
    y1 = m_ylast + m_tpad;
    {
        QString tx[6] = {"WNDG = ", "TEI = ", "3CAPE = ", "MBURST = ", "", "SigSvr = "};
        QColor cols[6] = {m_fg, m_fg, c3, m_fg, m_fg, m_fg};
        QString ixs[6] = {f2s(ix.wndg, 1), i2s(ix.tei), i2s(ix.mlb3km), i2s(ix.mburst), "", i2s(ix.sig_severe) + " m3/s3"};
        for (int i = 0; i < 6; ++i) {
            qp.setPen(QPen(cols[i], 1, Qt::SolidLine));
            qp.drawText(QRect((int)(x1 * 6), (int)y1, (int)(x1 * 4), lh), fl, tx[i] + ixs[i]);
            y1 += m_labelHeight;
            m_ylast = y1;
        }
    }
    qp.drawLine(0, (int)(y1 + 2), (int)m_brx, (int)(y1 + 2));
    qp.drawLine((int)(x1 * 7 - 5), (int)(y1 + 2), (int)(x1 * 7 - 5), (int)m_bry);

    y1 = m_ylast + m_tpad;
    {
        QString tx[4] = {"Sfc-3km AGL LR = ", "3-6km AGL LR = ", "850-500mb LR = ", "700-500mb LR = "};
        double v[4] = {ix.lr3, ix.lr36, ix.lr850_500, ix.lr700_500};
        for (int i = 0; i < 4; ++i) {
            // FLOAT2STR(x, 1) then compare the rounded value, as thermo.py does
            QString s = f2s(v[i], 1);
            QColor c("#FFFFFF");
            if (s != "--") {
                double f = s.toDouble();
                c = f <= 6.0 ? QColor("#00FF00") : f <= 7.0 ? QColor("#FFFF00") : f <= 8.0 ? QColor("#FFA500")
                  : f <= 9.0 ? QColor("#FF0000") : QColor("#FF00FF");
            }
            qp.setPen(QPen(c, 1, Qt::SolidLine));
            qp.drawText(QRect(rpad, (int)y1, (int)(x1 * 8), lh), fl, tx[i] + s + " C/km");
            y1 += m_labelHeight;
        }
    }
}

void ConvectiveTableWidget::drawSevere(QPainter& qp) {
    qp.setFont(m_labelFont);
    const double x1 = m_brx / 10;
    double y1 = m_ylast + m_tpad;
    const int lh = (int)m_labelHeight;

    QString strs[4] = {f2s(m_scp, 1), f2s(m_stpCin, 1), f2s(m_stpFixed, 1), f2s(m_ship, 1)};
    double vals[4] = {m_scp, m_stpCin, m_stpFixed, m_ship};
    if (!bad(m_prof.latitude) && m_prof.latitude < 0) { vals[0] = -vals[0]; vals[1] = -vals[1]; vals[2] = -vals[2]; }
    const QString labels[4] = {"Supercell = ", "STP (cin) = ", "STP (fix) = ", "SHIP = "};

    QColor pen = QColor(Qt::yellow);
    for (int i = 0; i < 4; ++i) {
        const double v = vals[i];
        if (bad(v)) pen = m_alert[0];
        else if (i == 0 || i == 1) {
            pen = v >= 15 ? m_alert[6] : v >= 10 ? m_alert[5] : v >= 5 ? m_alert[4] : v >= 2 ? m_alert[3]
                : v >= .5 ? m_alert[2] : v >= -.5 ? m_alert[1] : m_alert[0];
        } else if (i == 2) {
            pen = v >= 7 ? m_alert[6] : v >= 5 ? m_alert[5] : v >= 2 ? m_alert[4] : v >= 1 ? m_alert[3]
                : v > 0 ? m_alert[2] : m_alert[1];
        } else {
            pen = v >= 5 ? m_alert[6] : v >= 3 ? m_alert[5] : v >= 2 ? m_alert[4] : v >= 1 ? m_alert[3]
                : v >= 0.1 ? m_alert[2] : m_alert[1];
        }
        qp.setPen(QPen(pen, 1, Qt::SolidLine));
        qp.drawText(QRect((int)(x1 * 7), (int)y1, (int)(x1 * 8), lh), Qt::TextDontClip | Qt::AlignLeft, labels[i] + strs[i]);
        y1 += m_labelHeight;
    }
}

void ConvectiveTableWidget::paintEvent(QPaintEvent*) {
    QPainter qp(this);
    qp.fillRect(rect(), m_bg);
    qp.setPen(QPen(QColor("#3399CC"), 1));
    qp.drawRect(0, 0, width() - 1, height() - 1);
    qp.translate(1, 1);
    qp.setRenderHint(QPainter::TextAntialiasing);
    drawFrame(qp);
    if (!m_hasProf || !m_hasIdx || m_parcels.empty()) return;
    drawConvectiveIndices(qp);
    drawIndices(qp);
    drawSevere(qp);
}

void ConvectiveTableWidget::mousePressEvent(QMouseEvent* e) {
    const QPointF pos = e->localPos();
    const int n = std::min<int>(4, m_types.size());
    for (int i = 0; i < n; ++i) {
        if (m_bounds[i][0] < pos.y() && m_bounds[i][1] > pos.y()) {
            m_sel = i;
            update();
            emit parcelSelected(typeIndex(m_types[i]));
            if (parentWidget()) parentWidget()->setFocus();
            break;
        }
    }
}

void ConvectiveTableWidget::mouseDoubleClickEvent(QMouseEvent*) { showParcelDialog(); }

// SelectParcels
void ConvectiveTableWidget::showParcelDialog() {
    QDialog* dlg = new QDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle("Show Parcels");
    dlg->setGeometry(300, 300, 250, 180);
    const char* names[6] = {"Surface-Based Parcel", "100 mb Mixed Layer Parcel", "Forecast Surface Parcel",
                            "Most Unstable Parcel", "Effective Inflow Layer Parcel", "User Defined Parcel"};
    QCheckBox* cb[6];
    for (int i = 0; i < 6; ++i) {
        cb[i] = new QCheckBox(names[i], dlg);
        cb[i]->move(20, 20 + 20 * i);
        cb[i]->setChecked(m_types.contains(kTypes[i]));
    }
    QPushButton* ok = new QPushButton("Ok", dlg);
    ok->move(20, 150);
    connect(ok, &QPushButton::clicked, dlg, [this, dlg, cb]() {
        int count = 0;
        for (int i = 0; i < 6; ++i) if (cb[i]->isChecked()) ++count;
        if (count > 4) {
            QMessageBox::information(dlg, "", QString("You can only show 4 parcels at a time. You selected %1.\nUnselect some parcels.").arg(count));
        } else if (count != 4) {
            QMessageBox::information(dlg, "", QString("You need to select 4 parcels to show. You selected %1. Please Select some more.").arg(count));
        } else {
            m_types.clear();
            for (int i = 0; i < 6; ++i) if (cb[i]->isChecked()) m_types << kTypes[i];
            m_sel = 0;
            update();
            emit parcelSelected(typeIndex(m_types[0]));
            if (parentWidget()) parentWidget()->setFocus();
            dlg->close();
        }
    });
    dlg->show();
}

} // namespace sharppy
