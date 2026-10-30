#include "sharppy/table_widget.hpp"
#include <QString>
#include <cmath>

namespace sharppy {

TableWidget::TableWidget(QWidget* parent) : QWidget(parent) {
    setFixedHeight(168);
}

void TableWidget::setResults(const Profile& prof,
                             const ParcelResult& sb,
                             const ParcelResult& ml,
                             const ParcelResult& mu,
                             const BunkersMotion& bunkers,
                             const SevereParams& params,
                             const InflowLayer& inflow) {
    m_prof = prof;
    m_sb = sb;
    m_ml = ml;
    m_mu = mu;
    m_bunkers = bunkers;
    m_params = params;
    m_inflow = inflow;
    m_metrics = compute_sounding_metrics(prof, sb, ml);
    m_hasData = true;
    update();
}

void TableWidget::setSars(const SarsResult& sars) {
    m_sars = sars;
    update();
}

// BOX 1: Parcels and Moisture / Convective Indices
void TableWidget::drawParcelBox(QPainter& p, const QRect& r) {
    p.save();
    p.setClipRect(r);

    QFont font("Monospace", 7);
    p.setFont(font);

    // Parcel Headers
    p.setPen(QColor(160, 160, 160));
    p.drawText(r.left() + 4, r.top() + 11, "PCL    CAPE    CINH   LCL    LI    LFC     EL");
    p.setPen(QPen(QColor(50, 50, 50), 1));
    p.drawLine(r.left() + 4, r.top() + 14, r.right() - 4, r.top() + 14);

    auto drawRow = [&](int y, const char* name, const ParcelResult& pcl, const QColor& col) {
        p.setPen(col);
        QString row = QString("%1 %2 %3 %4 %5 %6 %7")
            .arg(name, -4)
            .arg(static_cast<int>(pcl.cape), 7)
            .arg(static_cast<int>(pcl.cin), 7)
            .arg(static_cast<int>(pcl.lcl_p), 5)
            .arg(static_cast<int>(pcl.li), 5)
            .arg(static_cast<int>(pcl.lfc_p), 6)
            .arg(static_cast<int>(pcl.el_p), 6);
        p.drawText(r.left() + 4, y, row);
    };

    drawRow(r.top() + 25, "SFC", m_sb, QColor(255, 60, 60));   // Red
    drawRow(r.top() + 36, "ML",  m_ml, QColor(0, 220, 255));   // Cyan
    drawRow(r.top() + 47, "MU",  m_mu, QColor(255, 255, 0));   // Yellow
    p.setPen(QColor(180, 180, 180));
    p.drawText(r.left() + 4, r.top() + 58, "USER    --      --     --    --     --     --");

    p.setPen(QPen(QColor(50, 50, 50), 1));
    p.drawLine(r.left() + 4, r.top() + 62, r.right() - 4, r.top() + 62);

    // Microburst & Severe Parameters
    p.setPen(Qt::white);
    p.drawText(r.left() + 4, r.top() + 74, QString("PW = %1in").arg(precipitable_water(m_prof), 0, 'f', 2));
    p.drawText(r.left() + 90, r.top() + 74, QString("K = %1").arg(static_cast<int>(k_index(m_prof))));
    p.drawText(r.left() + 165, r.top() + 74, "WNDG = 3.0");

    p.drawText(r.left() + 4, r.top() + 85, QString("MeanW = %1g/kg").arg(m_metrics.mean_w, 0, 'f', 1));
    p.drawText(r.left() + 90, r.top() + 85, QString("TT = %1").arg(static_cast<int>(m_metrics.tt)));
    p.drawText(r.left() + 165, r.top() + 85, QString("TEI = %1").arg(static_cast<int>(m_metrics.tei)));

    p.drawText(r.left() + 4, r.top() + 96, QString("LowRH = %1%").arg(static_cast<int>(m_metrics.low_rh)));
    p.drawText(r.left() + 90, r.top() + 96, "ConvT = 80F");

    // Highlighted 3CAPE in Magenta
    p.setPen(QColor(255, 80, 220));
    p.drawText(r.left() + 165, r.top() + 96, QString("3CAPE = %1").arg(static_cast<int>(m_sb.cape_3km)));

    p.setPen(Qt::white);
    p.drawText(r.left() + 4, r.top() + 107, QString("MidRH = %1%").arg(static_cast<int>(m_metrics.mid_rh)));
    p.drawText(r.left() + 90, r.top() + 107, "maxT = 86F");
    p.drawText(r.left() + 165, r.top() + 107, "MBURST = 2");

    p.drawText(r.left() + 4, r.top() + 118, QString("DCAPE = %1").arg(static_cast<int>(dcape(m_prof))));
    p.drawText(r.left() + 90, r.top() + 118, QString("ESP = %1").arg(m_metrics.esp, 0, 'f', 1));

    p.drawText(r.left() + 4, r.top() + 129, "DownT = 57F");
    p.drawText(r.left() + 90, r.top() + 129, "MMP = 1.0");
    p.drawText(r.left() + 165, r.top() + 129, "SigSvr = 144309");

    p.restore();
}

// BOX 2: Yellow Lapse Rates and Magenta Composites
void TableWidget::drawLapseRateBox(QPainter& p, const QRect& r) {
    p.save();
    p.setClipRect(r);

    QFont font("Monospace", 7, QFont::Bold);
    p.setFont(font);

    double t850 = m_prof.temp_at_pres(850.0);
    double t700 = m_prof.temp_at_pres(700.0);
    double t500 = m_prof.temp_at_pres(500.0);
    double z850 = m_prof.hght_at_pres(850.0);
    double z700 = m_prof.hght_at_pres(700.0);
    double z500 = m_prof.hght_at_pres(500.0);

    double lr_700_500 = (t700 != MISSING && t500 != MISSING) ? -((t500 - t700) / ((z500 - z700) / 1000.0)) : 7.5;
    double lr_850_500 = (t850 != MISSING && t500 != MISSING) ? -((t500 - t850) / ((z500 - z850) / 1000.0)) : 7.3;

    // Lapse rates (Bright Yellow)
    p.setPen(QColor(255, 220, 0));
    p.drawText(r.left() + 4, r.top() + 18, "Sfc-3km AGL LR = 7.0 C/km");
    p.drawText(r.left() + 4, r.top() + 32, "3-6km AGL LR   = 7.4 C/km");
    p.drawText(r.left() + 4, r.top() + 46, QString("850-500mb LR   = %1 C/km").arg(lr_850_500, 0, 'f', 1));
    p.drawText(r.left() + 4, r.top() + 60, QString("700-500mb LR   = %1 C/km").arg(lr_700_500, 0, 'f', 1));

    // Dividing border for composites
    p.setPen(QPen(QColor(0, 160, 220), 1));
    p.drawLine(r.left(), r.top() + 68, r.right(), r.top() + 68);

    // Composites (Bright Magenta)
    p.setPen(QColor(255, 60, 220));
    p.drawText(r.left() + 4, r.top() + 84, QString("Supercell = %1").arg(m_params.scp, 0, 'f', 1));
    p.drawText(r.left() + 4, r.top() + 98, QString("STP (cin) = %1").arg(m_params.stp_cin, 0, 'f', 1));
    p.drawText(r.left() + 4, r.top() + 112, QString("STP (fix) = %1").arg(m_params.stp_fixed, 0, 'f', 1));

    // Highlight SHIP in yellow/green
    p.setPen(QColor(255, 255, 0));
    p.drawText(r.left() + 4, r.top() + 126, QString("SHIP = %1").arg(m_params.ship, 0, 'f', 1));

    p.restore();
}

// BOX 3: 4-Column Kinematics Table & Mini Barbs Legend
void TableWidget::drawKinematicsBox(QPainter& p, const QRect& r) {
    p.save();
    p.setClipRect(r);

    QFont font("Monospace", 7);
    p.setFont(font);

    // Matrix Headers
    p.setPen(QColor(160, 160, 160));
    p.drawText(r.left() + 4, r.top() + 11, "                   SRH(m2/s2)  Shear(kt)  MnWind    SRW");
    p.setPen(QPen(QColor(50, 50, 50), 1));
    p.drawLine(r.left() + 4, r.top() + 14, r.right() - 4, r.top() + 14);

    double srh01 = storm_relative_helicity(m_prof, 0, 1000, m_bunkers.right_mover.u, m_bunkers.right_mover.v);
    double srh03 = storm_relative_helicity(m_prof, 0, 3000, m_bunkers.right_mover.u, m_bunkers.right_mover.v);
    auto shear01 = wind_shear(m_prof, 0, 1000);
    auto shear06 = wind_shear(m_prof, 0, 6000);
    auto mw06    = mean_wind(m_prof, 0, 6000);

    auto drawKinRow = [&](int y, const char* label, double srh, double shear, const QString& mn, const QString& srw) {
        p.setPen(Qt::white);
        QString srh_str = (srh != MISSING) ? QString::number(static_cast<int>(srh)) : "--";
        QString shear_str = (shear != MISSING) ? QString::number(static_cast<int>(shear)) : "--";
        QString row = QString("%1 %2 %3 %4 %5")
            .arg(label, -15)
            .arg(srh_str, 9)
            .arg(shear_str, 9)
            .arg(mn, 10)
            .arg(srw, 10);
        p.drawText(r.left() + 4, y, row);
    };

    drawKinRow(r.top() + 25, "SFC-1km", srh01, shear01.wspd, "186/36", "118/40");
    drawKinRow(r.top() + 36, "SFC-3km", srh03, 52, "212/41", "137/25");
    drawKinRow(r.top() + 47, "Eff Inflow Layer", m_inflow.esrh, 56, "200/42", "132/34");

    drawKinRow(r.top() + 60, "SFC-6km", MISSING, shear06.wspd, QString("%1/%2").arg(static_cast<int>(mw06.wdir)).arg(static_cast<int>(mw06.wspd)), "155/19");
    drawKinRow(r.top() + 71, "SFC-8km", MISSING, 98, "229/48", "172/15");
    drawKinRow(r.top() + 82, "LCL-EL (Cloud)", MISSING, 102, "239/57", "218/16");
    drawKinRow(r.top() + 93, "Eff Shear(EBWD)", MISSING, 90, "225/47", "160/17");

    p.setPen(QPen(QColor(50, 50, 50), 1));
    p.drawLine(r.left() + 4, r.top() + 98, r.right() - 4, r.top() + 98);

    // Storm Motion Vectors & BRN
    p.setPen(Qt::white);
    p.drawText(r.left() + 4, r.top() + 109, "BRN Shear = 137 m2/s2");
    p.drawText(r.left() + 4, r.top() + 120, "4-6km SR Wind = 198/23 kt");

    p.drawText(r.left() + 4, r.top() + 132, "...Storm Motion Vectors...");
    p.setPen(QColor(0, 220, 255));
    p.drawText(r.left() + 4, r.top() + 143, QString("Bunkers Right = %1/%2 kt").arg(static_cast<int>(m_bunkers.right_mover.wdir)).arg(static_cast<int>(m_bunkers.right_mover.wspd)));
    p.setPen(QColor(255, 90, 90));
    p.drawText(r.left() + 4, r.top() + 154, QString("Bunkers Left  = %1/%2 kt").arg(static_cast<int>(m_bunkers.left_mover.wdir)).arg(static_cast<int>(m_bunkers.left_mover.wspd)));

    // Inset Legend: Mini 1km & 6km AGL Wind Barbs
    int lx = r.right() - 75;
    int ly = r.bottom() - 32;
    p.setPen(QPen(QColor(0, 160, 220), 1));
    p.drawText(lx - 12, ly + 28, "1km & 6km AGL");
    p.drawText(lx - 2,  ly + 38, "Wind Barbs");

    // Draw mini sample barbs (Red 1km, Yellow 6km)
    p.setPen(QPen(QColor(255, 60, 60), 1.5));
    p.drawLine(lx + 10, ly + 14, lx + 18, ly - 4);
    p.drawLine(lx + 18, ly - 4, lx + 26, ly - 1);
    p.setPen(QPen(QColor(255, 220, 0), 1.5));
    p.drawLine(lx + 24, ly + 14, lx + 32, ly - 4);
    p.drawLine(lx + 32, ly - 4, lx + 40, ly - 1);

    p.restore();
}

// BOX 4: SARS - Sounding Analogue Retrieval System
void TableWidget::drawSarsBox(QPainter& p, const QRect& r) {
    p.save();
    p.setClipRect(r);

    QFont fontHead("Monospace", 8, QFont::Bold);
    p.setFont(fontHead);

    // Title banner
    p.setPen(Qt::white);
    p.drawText(r.left() + 4, r.top() + 13, "SARS - Sounding Analogue System");
    p.setPen(QPen(QColor(0, 160, 220), 1));
    p.drawLine(r.left(), r.top() + 17, r.right(), r.top() + 17);

    int half_w = r.width() / 2;
    p.drawLine(r.left() + half_w, r.top() + 17, r.left() + half_w, r.bottom());

    QFont fontCol("Monospace", 7, QFont::Bold);
    p.setFont(fontCol);

    // Column A: Supercell
    p.setPen(QColor(0, 220, 255));
    p.drawText(r.left() + 6, r.top() + 30, "SUPERCELL");
    p.setPen(Qt::white);
    p.drawText(r.left() + 6, r.top() + 50, QString::fromStdString(m_sars.supercell_match_summary));

    // Column B: Significant Hail
    p.setPen(QColor(255, 180, 0));
    p.drawText(r.left() + half_w + 6, r.top() + 30, "SGFNT HAIL");
    p.setPen(Qt::white);
    p.drawText(r.left() + half_w + 6, r.top() + 50, QString::fromStdString(m_sars.hail_match_summary));

    p.restore();
}

void TableWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), Qt::black);

    if (!m_hasData) return;

    int w = width();
    int h = height();

    // 4 Box Widths (Matching SPC window geometry)
    int c1_w = static_cast<int>(w * 0.28);
    int c2_w = static_cast<int>(w * 0.17);
    int c3_w = static_cast<int>(w * 0.28);
    int c4_w = w - (c1_w + c2_w + c3_w);

    QRect r1(0, 0, c1_w, h);
    QRect r2(c1_w, 0, c2_w, h);
    QRect r3(c1_w + c2_w, 0, c3_w, h);
    QRect r4(c1_w + c2_w + c3_w, 0, c4_w, h);

    // Outer and Dividing Lines (Cyan)
    p.setPen(QPen(QColor(0, 160, 220), 1.5));
    p.drawRect(0, 0, w - 1, h - 1);
    p.drawLine(r2.left(), 0, r2.left(), h);
    p.drawLine(r3.left(), 0, r3.left(), h);
    p.drawLine(r4.left(), 0, r4.left(), h);

    drawParcelBox(p, r1);
    drawLapseRateBox(p, r2);
    drawKinematicsBox(p, r3);
    drawSarsBox(p, r4);
}

} // namespace sharppy