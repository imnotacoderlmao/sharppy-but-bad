#include "sharppy/analogues_widget.hpp"
#include "sharppy/utils_fmt.hpp"
#include <QPainter>
#include <QFontMetrics>
#include <QMouseEvent>
#include <cmath>

namespace sharppy {

namespace {
const QColor RED_("#FF0000"), LBLUE_("#06B5FF"), LBROWN_("#996600");   // sharptab constants
QString i2s(double v) { return int2str(v); }

// datetime.strptime(mdate, "%y%m%d%H").strftime("%d %b %y %HZ")
QString fmtDate(const QString& s) {
    static const char* M[12] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
    if (s.size() < 8) return s;
    int mo = s.mid(2, 2).toInt();
    if (mo < 1 || mo > 12) return s;
    return QString("%1 %2 %3 %4Z").arg(s.mid(4, 2)).arg(M[mo - 1]).arg(s.left(2)).arg(s.mid(6, 2));
}
}

AnaloguesWidget::AnaloguesWidget(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(140);
    initUI();
}

void AnaloguesWidget::initUI() {
    const double hgt = height();
    m_brx = width(); m_bry = hgt;
    const double f1 = std::floor(.09 * hgt), f2 = std::floor(.07 * hgt), f3 = std::floor(.06 * hgt);
    m_tpadF = std::floor(.03 * hgt);
    m_titleFont = QFont("Helvetica"); m_titleFont.setPointSize(std::max(1, (int)f1));
    m_plotFont = QFont("Helvetica");  m_plotFont.setPointSize(std::max(1, (int)f2));
    m_matchFont = QFont("Helvetica"); m_matchFont.setPointSize(std::max(1, (int)f3));
#ifdef Q_OS_WIN
    {   // analogues.py: on Windows the point sizes are reduced by the font descent
        const int d1 = QFontMetrics(m_titleFont).descent(), d2 = QFontMetrics(m_plotFont).descent(), d3 = QFontMetrics(m_matchFont).descent();
        m_titleFont.setPointSize(std::max(1, (int)(f1 - d1)));
        m_plotFont.setPointSize(std::max(1, (int)(f2 - d2)));
        m_matchFont.setPointSize(std::max(1, (int)(f3 - d3)));
    }
#endif
    m_titleHeight = QFontMetrics(m_titleFont).xHeight() + m_tpadF;
    m_plotHeight = QFontMetrics(m_plotFont).xHeight() + m_tpadF;
    m_matchHeight = QFontMetrics(m_matchFont).xHeight() + m_tpadF;
    m_ylast = m_tpadF;
}

void AnaloguesWidget::resizeEvent(QResizeEvent*) { initUI(); }

void AnaloguesWidget::setPreferences(const VizPrefs& p) {
    m_bg = prefColor(p, "bg_color", m_bg);
    m_fg = prefColor(p, "fg_color", m_fg);
    update();
}

void AnaloguesWidget::setSars(const SarsAll& s) {
    m_sars = s; m_hasData = true;
    m_yHail.assign(hail().names.size(), {0, 0});
    m_ySup.assign(sup().names.size(), {0, 0});
    update();
}

void AnaloguesWidget::setDeviant(bool use_left) {
    m_useLeft = use_left;
    if (m_hasData) {
        m_yHail.assign(hail().names.size(), {0, 0});
        m_ySup.assign(sup().names.size(), {0, 0});
    }
    update();
}

void AnaloguesWidget::drawFrame(QPainter& qp) {
    qp.setPen(QPen(m_fg, 1, Qt::SolidLine));
    const double x1 = m_brx / 6;
    m_ylast = m_tpadF;
    qp.setFont(m_titleFont);
    qp.drawText(QRect(0, (int)m_ylast, (int)m_brx, (int)m_titleHeight), Qt::TextDontClip | Qt::AlignCenter,
                "SARS - Sounding Analogue System");
    m_ylast += m_titleHeight + m_tpadF;
    qp.drawLine(0, (int)m_ylast, (int)m_brx, (int)m_ylast);
    qp.drawLine((int)(m_brx / 2), (int)m_ylast, (int)(m_brx / 2), (int)m_bry);
    m_ylast += m_tpadF;
    qp.setFont(m_plotFont);
    qp.drawText(QRect((int)x1, (int)m_ylast, (int)x1, (int)m_plotHeight), Qt::TextDontClip | Qt::AlignCenter, "SUPERCELL");
    qp.drawText(QRect((int)(x1 * 4), (int)m_ylast, (int)x1, (int)m_plotHeight), Qt::TextDontClip | Qt::AlignCenter, "SGFNT HAIL");
    m_ylast += m_titleHeight;
    m_textStart = m_ylast;
}

void AnaloguesWidget::drawSARS(QPainter& qp, bool tor) {
    const double x1 = m_brx / 6;
    const SarsMatches& M = tor ? sup() : hail();
    auto& yb = tor ? m_ySup : m_yHail;
    const QString sigstr = tor ? "TOR" : "SIG";
    const int place = tor ? 1 : 4;
    const double p20 = tor ? m_lpad : x1 * 3 + 7;
    const double p21 = tor ? (m_brx / 2.) - x1 * 3. / 4. + 2 : x1 * 5.5 - 5;

    QPen pen(m_fg, 1, Qt::SolidLine);
    auto noMatches = [&]() {
        pen.setColor(m_fg); qp.setPen(pen); qp.setFont(m_matchFont);
        qp.drawText(QRect((int)(x1 * place), (int)(m_bry * (2. / 5.)), (int)x1, (int)m_matchHeight),
                    Qt::TextDontClip | Qt::AlignCenter, "No Quality Matches");
    };
    if (M.names.empty()) { noMatches(); return; }   // .size == 0 branch: no stats drawn either

    qp.setPen(pen);
    qp.setFont(m_plotFont);
    m_ylast = m_bry - m_bpad * 3;

    const QString sigStr = "SARS: " + i2s(std::round(M.prob * 100)) + "% " + sigstr;
    const QString matchStr = "(" + i2s(M.nloose) + " loose matches)";
    if (M.nloose > 0) {
        qp.setFont(m_matchFont);
        pen.setColor(M.prob * 100. >= 50. ? QColor(Qt::magenta) : m_fg);
        qp.setPen(pen);
        qp.drawText(QRect((int)(x1 * place), (int)m_ylast, (int)x1, (int)m_matchHeight),
                    Qt::TextDontClip | Qt::AlignCenter, sigStr);
        m_ylast -= (m_matchHeight + m_bpad);
        qp.drawText(QRect((int)(x1 * place), (int)m_ylast, (int)x1, (int)m_matchHeight),
                    Qt::TextDontClip | Qt::AlignCenter, matchStr);
    }

    if (M.names.empty()) { noMatches(); }
    else {
        pen.setColor(m_fg); qp.setPen(pen); qp.setFont(m_matchFont);
        m_ylast = m_textStart;
        if (yb.size() != M.names.size()) yb.assign(M.names.size(), {0, 0});
        for (size_t i = 0; i < M.names.size(); ++i) {
            const QString nm = QString::fromStdString(M.names[i]);
            const int dot = nm.indexOf('.');
            const QString mdate = dot < 0 ? nm : nm.left(dot), mloc = dot < 0 ? QString() : nm.mid(dot + 1);
            const QString ms = QString("%1 (%2)").arg(fmtDate(mdate)).arg(mloc);
            QRect rect3((int)p20, (int)m_ylast, (int)x1, (int)m_matchHeight);
            QRect rect4((int)p21, (int)m_ylast, (int)x1, (int)m_matchHeight);
            yb[i] = {m_ylast, m_ylast + m_matchHeight};
            QString sizeStr;
            if (tor) {
                const QString t = QString::fromStdString(M.types[i]);
                sizeStr = t.left(t.size() - 3);
                if (t.startsWith("SIG")) pen.setColor(RED_);
                else if (t.startsWith("WEAK")) pen.setColor(LBLUE_);
                else if (t.startsWith("NON")) pen.setColor(LBROWN_);
                qp.setPen(pen);
            } else {
                const double sz = M.sizes[i];
                sizeStr = QString::number(sz, 'f', 2);
                pen.setColor(sz >= 2.0 ? RED_ : LBLUE_);
                qp.setPen(pen);
            }
            qp.drawText(rect3, Qt::TextDontClip | Qt::AlignLeft, ms);
            qp.drawText(rect4, Qt::TextDontClip | Qt::AlignLeft, sizeStr);
            if (m_hasSel) {
                pen.setColor(LBLUE_); qp.setPen(pen);
                qp.drawLine(m_selectRect.topLeft(), m_selectRect.topRight());
                qp.drawLine(m_selectRect.bottomLeft(), m_selectRect.bottomRight());
            }
            m_ylast += m_matchHeight;
        }
    }
    m_ylast = m_tpadF;
}

void AnaloguesWidget::paintEvent(QPaintEvent*) {
    QPainter qp(this);
    qp.fillRect(rect(), m_bg);
    qp.setPen(QPen(QColor("#3399CC"), 1));
    qp.drawRect(0, 0, width() - 1, height() - 1);
    qp.translate(1, 1);
    drawFrame(qp);
    if (!m_hasData) return;
    drawSARS(qp, false);
    drawSARS(qp, true);
}

void AnaloguesWidget::mousePressEvent(QMouseEvent* e) {
    if (!m_hasData || (sup().names.empty() && hail().names.empty())) return;
    const QPointF pos = e->localPos();
    const double px = pos.x(), py = pos.y();
    if (px < m_brx / 2.) {
        for (size_t i = 0; i < m_ySup.size(); ++i)
            if (m_ySup[i].first < py && m_ySup[i].second > py) {
                emit updatematch(QString::fromStdString(sup().names[i]), "supercell");
                m_selectRect = QRectF(0, m_ySup[i].first, m_brx / 2., m_ySup[i].second - m_ySup[i].first);
                m_hasSel = true;
                break;
            }
    } else if (px > m_brx / 2.) {
        for (size_t i = 0; i < m_yHail.size(); ++i)
            if (m_yHail[i].first < py && m_yHail[i].second > py) {
                emit updatematch(QString::fromStdString(hail().names[i]), "hail");
                m_selectRect = QRectF(m_brx / 2., m_yHail[i].first, m_brx / 2., m_yHail[i].second - m_yHail[i].first);
                m_hasSel = true;
                break;
            }
    }
    update();
    if (parentWidget()) parentWidget()->setFocus();
}

void AnaloguesWidget::setSelection(const QString& matchString, const QString& type) {
    const bool isSup = type.toLower() == "supercell";
    const SarsMatches& M = isSup ? sup() : hail();
    auto& yb = isSup ? m_ySup : m_yHail;
    for (size_t i = 0; i < M.names.size() && i < yb.size(); ++i)
        if (QString::fromStdString(M.names[i]) == matchString) {
            m_selectRect = QRectF(isSup ? 0. : m_brx / 2., yb[i].first, m_brx / 2., yb[i].second - yb[i].first);
            m_hasSel = true;
            break;
        }
    update();
    if (parentWidget()) parentWidget()->setFocus();
}

void AnaloguesWidget::clearSelection() {
    m_hasSel = false;
    update();
    if (parentWidget()) parentWidget()->setFocus();
}

} // namespace sharppy
