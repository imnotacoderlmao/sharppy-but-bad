#include "sharppy/map_widget.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QSettings>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

namespace sharppy {

namespace {

QPoint evPos(const QMouseEvent* e) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return e->position().toPoint();
#else
    return e->pos();
#endif
}

QPoint evPos(const QWheelEvent* e) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    return e->position().toPoint();
#else
    return e->pos();
#endif
}

int textWidth(const QFontMetrics& fm, const QString& s) {
    return fm.horizontalAdvance(s);
}

const char* kReadoutStyle16 = "QLabel { background-color:#000000; border-width: 0px; font-size: 16px; color: #FFFFFF; }";
const char* kReadoutStyle18 = "QLabel { background-color:#000000; border-width: 0px; font-size: 18px; color: #FFFFFF; }";
const char* kReadoutStyle32 = "QLabel { background-color:#000000; border-width: 0px; font-size: 32px; color: #FFFFFF; }";

} // namespace

MapPoint mapPointFromStation(const StationInfo& s) {
    MapPoint p;
    p.srcid = s.id;
    p.name = s.name;
    p.state = s.state;
    p.lat = s.lat;
    p.lon = s.lon;
    // US upper-air sites carry a "K" prefix in their ICAO identifier.
    p.icao = (s.state.size() == 2 && s.id.size() == 3) ? "K" + s.id : s.id;
    p.iata = s.id.size() == 3 ? s.id : std::string();
    return p;
}

QString mapConfigPath() {
    return QDir::homePath() + "/.sharppy/sharppy_cpp.ini";
}

MapViewConfig loadMapViewConfig() {
    MapViewConfig c;
    QSettings s(mapConfigPath(), QSettings::IniFormat);
    if (!s.contains("map/proj")) return c;
    MapProj p;
    if (!mapProjFromName(s.value("map/proj").toString(), p)) return c;
    bool ok1, ok2, ok3, ok4;
    double lon = s.value("map/std_lon").toDouble(&ok1);
    double sc = s.value("map/scale").toDouble(&ok2);
    double cx = s.value("map/center_x").toDouble(&ok3);
    double cy = s.value("map/center_y").toDouble(&ok4);
    if (!(ok1 && ok2 && ok3 && ok4) || !(sc > 0.0) || !std::isfinite(sc)) return c;
    c.valid = true;
    c.proj = p;
    c.std_lon = lon;
    c.scale = sc;
    c.center_x = cx;
    c.center_y = cy;
    return c;
}

void saveMapViewConfig(const MapViewConfig& c) {
    QDir().mkpath(QDir::homePath() + "/.sharppy");
    QSettings s(mapConfigPath(), QSettings::IniFormat);
    s.setValue("map/proj", mapProjName(c.proj));
    s.setValue("map/std_lon", c.std_lon);
    s.setValue("map/scale", c.scale);
    s.setValue("map/center_x", c.center_x);
    s.setValue("map/center_y", c.center_y);
    s.sync();
}

// ---------------------------------------------------------------------------

MapWidget::MapWidget(QWidget* parent)
    : QWidget(parent), m_mapper(-97.5, 60.0, MapProj::NPStere) {
    // SHARPpy creates this widget unparented, so its "default" size is Qt's
    // 640x480 top-level default; the minimum size and zoom limits derive from it.
    resize(640, 480);
    setMouseTracking(true);

    const MapViewConfig cfg = loadMapViewConfig();
    const bool fromConfig = cfg.valid;
    if (fromConfig) {
        m_mapper = Mapper(cfg.std_lon, 60.0, cfg.proj);
        m_scale = cfg.scale;
        m_mapCenterX = cfg.center_x;
        m_mapCenterY = cfg.center_y;
    } else {
        m_scale = m_initScale;
        m_mapCenterX = m_mapCenterY = 0.0;
    }

    m_defaultWidth = width();
    m_defaultHeight = height();
    setMinimumSize(width(), height());

    m_stnReadout = new QLabel(this);
    m_stnReadout->setStyleSheet(kReadoutStyle16);
    m_stnReadout->setText("");
    m_stnReadout->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_stnReadout->show();
    m_stnReadout->move(width(), height());

    m_loadReadout = new QLabel(this);
    m_loadReadout->setStyleSheet(kReadoutStyle18);
    m_loadReadout->setText("Loading ...");
    m_loadReadout->setFixedWidth(100);
    m_loadReadout->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_loadReadout->show();
    m_loadReadout->move(width(), height());

    m_latlonReadout = new QLabel(this);
    m_latlonReadout->setStyleSheet(kReadoutStyle18);
    m_latlonReadout->setText("");
    m_latlonReadout->setFixedWidth(150);
    m_latlonReadout->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_latlonReadout->show();
    m_latlonReadout->move(10, 10);

    m_noInternet = new QLabel(this);
    m_noInternet->setStyleSheet(kReadoutStyle32);
    m_noInternet->setText("No Internet Connection");
    m_noInternet->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    QFont f32 = m_noInternet->font();
    f32.setPixelSize(32);
    m_noInternet->setFixedWidth(QFontMetrics(f32).horizontalAdvance(m_noInternet->text()));
    m_noInternet->show();
    m_noInternet->move(width(), height());

    setWindowTitle("SHARPpy");

    if (!fromConfig) {
        resetViewport();
    }

    initMap();
    initUI();
}

MapWidget::Layer MapWidget::makeLayer(const QVector<QPainterPath>& paths) {
    Layer l;
    l.paths = paths;
    l.bounds.reserve(paths.size());
    for (const QPainterPath& p : paths) l.bounds.append(p.boundingRect());
    return l;
}

void MapWidget::initUI() {
    m_centerX = width() / 2.0;
    m_centerY = height() / 2.0;

    if (width() <= 0 || height() <= 0) return;
    m_plotBitMap = QPixmap(width(), height());
    m_backgroundBitMap = m_plotBitMap.copy();
    drawMap();
}

void MapWidget::initMap() {
    m_coast = makeLayer(m_mapper.boundary("coastlines"));
    m_country = makeLayer(m_mapper.boundary("countries"));
    m_state = makeLayer(m_mapper.boundary("states"));
    m_county = makeLayer(m_mapper.boundary("uscounties"));
    m_grid = m_mapper.coordPaths();
}

void MapWidget::setFreePick(bool on) {
    if (m_freePick == on) return;
    m_freePick = on;
    m_hasPick = false;
    m_cursorIn = false;
    m_clickedStn.clear();
    emit selectionCleared();
    drawMap();
    update();
}

void MapWidget::setStations(const std::vector<MapPoint>& points) {
    if (!m_freePick) {                     // a picked point survives the station refreshes (there are none to show)
        m_clickedStn.clear();
        emit selectionCleared();
    }

    m_points = points;
    m_stnLats.clear();
    m_stnLons.clear();
    m_stnIds.clear();
    m_stnNames.clear();
    for (const MapPoint& p : m_points) {
        m_stnLats.push_back(p.lat);
        m_stnLons.push_back(p.lon);
        m_stnIds.push_back(p.srcid);

        QString idStr = p.icao.empty() ? QString() : QString(" (%1)").arg(QString::fromStdString(p.icao));
        QString polStr;
        if (!p.state.empty()) polStr = ", " + QString::fromStdString(p.state);
        else if (!p.country.empty()) polStr = ", " + QString::fromStdString(p.country);
        QString nm = QString::fromStdString(p.name);
        if (idStr.isEmpty() && polStr.isEmpty()) nm = nm.toUpper();
        m_stnNames << nm + polStr + idStr;
    }

    drawMap();
    update();
}

void MapWidget::setProjection(MapProj proj) {
    m_mapper.setProjection(proj);
    resetViewport();
    showLoading();
    QCoreApplication::processEvents();
    initMap();
    resetViewport();
    drawMap();
    hideLoading();
    update();
}

void MapWidget::resetViewport(bool hasCtr, double ctrLat, double ctrLon) {
    m_mapCenterX = width() / 2.0;
    if (hasCtr) {
        QPointF c = m_mapper.toXY(ctrLat, ctrLon);
        m_mapCenterY = -c.y() + height() / 2.0;
        m_mapCenterY = m_centerY - (m_centerY - m_mapCenterY) / m_scale;
    } else {
        m_scale = m_initScale;
        m_mapRot = 0.0;
        switch (m_mapper.projection()) {
            case MapProj::NPStere: m_mapCenterY = -13.0 * height() / 10.0 + height() / 2.0; break;
            case MapProj::Merc:    m_mapCenterY = height() / 2.0; break;
            case MapProj::SPStere: m_mapCenterY = 13.0 * height() / 10.0 + height() / 2.0; break;
        }
    }
}

void MapWidget::drawMap() {
    if (m_plotBitMap.isNull()) return;

    QPainter qp(&m_plotBitMap);
    m_plotBitMap.fill(Qt::black);

    const double mapCenterX = m_mapCenterX + m_transX;
    const double mapCenterY = m_mapCenterY + m_transY;

    qp.translate(mapCenterX, mapCenterY);

    switch (m_mapper.projection()) {
        case MapProj::NPStere:
            qp.rotate(m_mapRot);
            break;
        case MapProj::Merc: {
            double clat, clon;
            m_mapper.toLatLon(mapCenterX - width() / 2.0, mapCenterY - height() / 2.0, clat, clon);
            clon -= m_mapRot;
            QPointF nc = m_mapper.toXY(clat, clon);
            double ncx = nc.x() + width() / 2.0;
            double ncy = nc.y() + height() / 2.0;
            qp.translate((ncx - mapCenterX) / m_scale, (ncy - mapCenterY) / m_scale);
            break;
        }
        case MapProj::SPStere:
            qp.rotate(-m_mapRot);
            break;
    }

    qp.scale(1.0 / m_scale, 1.0 / m_scale);

    drawPolitical(qp);
    m_backgroundBitMap = m_plotBitMap.copy();
    drawStations(qp);

    qp.end();
    m_drawnTransX = m_transX; m_drawnTransY = m_transY;
}

void MapWidget::drawPolitical(QPainter& qp) {
    m_transform = qp.transform();
    const QRectF windowRect(0, 0, width(), height());

    qp.setPen(QPen(QColor("#333333"), m_scale));
    qp.drawPath(m_grid);

    // Scale thresholds follow the ratio of the current area to the default area.
    const double defaultArea = double(m_defaultWidth) * m_defaultHeight;
    const double actualArea = double(width()) * height();
    const double scaledArea = std::sqrt(defaultArea / actualArea);

    auto drawLayer = [&](const Layer& l) {
        for (int i = 0; i < l.paths.size(); ++i) {
            if (m_transform.mapRect(l.bounds[i]).intersects(windowRect)) qp.drawPath(l.paths[i]);
        }
    };

    if (m_scale < 0.15 * scaledArea) {
        const double maxComp = 102.0;
        const double fullScale = 0.10 * scaledArea;
        const double zeroScale = 0.15 * scaledArea;
        double comp = maxComp * std::min(std::max((zeroScale - m_scale) / (zeroScale - fullScale), 0.0), 1.0);
        int c = int(std::lround(comp));
        QColor color(c, c, c);
        qp.setPen(QPen(color, m_scale));
        drawLayer(m_county);
    }

    qp.setPen(QPen(QColor("#999999"), m_scale));
    drawLayer(m_state);

    qp.setPen(QPen(Qt::white, m_scale));
    drawLayer(m_coast);
    drawLayer(m_country);
}

void MapWidget::drawStations(QPainter& qp) {
    double lb, ub;
    m_mapper.latBounds(lb, ub);
    const double size = 3.0 * m_scale;
    const QRect windowRect(0, 0, width(), height());

    auto inWindow = [&](const QPointF& p) {
        QPointF t = m_transform.map(p);
        return windowRect.contains(int(t.x()), int(t.y()));
    };

    bool haveClicked = false;
    QPointF clickedPt;
    double clickedLat = 0.0;

    qp.setPen(QPen(Qt::red, m_scale));
    qp.setBrush(QBrush(Qt::red));
    for (size_t i = 0; i < m_stnLats.size(); ++i) {
        QPointF p = m_mapper.toXY(m_stnLats[i], m_stnLons[i] + m_mapRot);
        if (!m_clickedStn.isEmpty() && m_clickedStn.toStdString() == m_stnIds[i]) {
            haveClicked = true;
            clickedPt = p;
            clickedLat = m_stnLats[i];
            continue;
        }
        if (lb <= m_stnLats[i] && m_stnLats[i] <= ub && inWindow(p)) {
            qp.drawEllipse(p, size, size);
        }
    }

    if (m_freePick && m_hasPick) {
        const QPointF p = m_mapper.toXY(m_pickLat, m_pickLon + m_mapRot);
        if (inWindow(p)) {
            const double r = 5.0 * m_scale;
            qp.setPen(QPen(Qt::green, 1.5 * m_scale));
            qp.setBrush(Qt::NoBrush);
            qp.drawEllipse(p, r, r);
            qp.drawLine(QPointF(p.x() - 2 * r, p.y()), QPointF(p.x() + 2 * r, p.y()));
            qp.drawLine(QPointF(p.x(), p.y() - 2 * r), QPointF(p.x(), p.y() + 2 * r));
        }
    }

    if (haveClicked && lb <= clickedLat && clickedLat <= ub && inWindow(clickedPt)) {
        qp.setPen(QPen(Qt::green, m_scale));
        qp.setBrush(QBrush(Qt::green));
        qp.drawEllipse(clickedPt, size, size);
    }
}

void MapWidget::paintEvent(QPaintEvent*) {
    QPainter qp(this);
    // While dragging the cached map is just slid along with the mouse; the full redraw happens when the drag pauses or ends.
    const int dx = int(std::lround(m_transX - m_drawnTransX)), dy = int(std::lround(m_transY - m_drawnTransY));
    if (dx || dy) qp.fillRect(rect(), Qt::black);
    qp.drawPixmap(dx, dy, m_plotBitMap);
    if (m_freePick && m_cursorIn && !m_hasDrag) {            // crosshair that follows the mouse before the point is clicked
        qp.setPen(QPen(QColor(255, 255, 255, 200), 1));
        qp.drawLine(m_cursor.x(), 0, m_cursor.x(), height());
        qp.drawLine(0, m_cursor.y(), width(), m_cursor.y());
    }
}

void MapWidget::leaveEvent(QEvent* e) {
    QWidget::leaveEvent(e);
    if (m_cursorIn) { m_cursorIn = false; update(); }
}

void MapWidget::resizeEvent(QResizeEvent* e) {
    QSize oldSize = e->oldSize();
    const QSize newSize = e->size();
    if (oldSize.width() == -1 && oldSize.height() == -1) oldSize = size();

    m_mapCenterX += (newSize.width() - oldSize.width()) / 2.0;
    m_mapCenterY += (newSize.height() - oldSize.height()) / 2.0;
    hideLoading();
    hasInternet(m_hasInternet);

    initUI();
}

void MapWidget::zoomBy(double scale_fac) {
    const double scaledSize = double(std::min(m_defaultWidth, m_defaultHeight)) / std::min(width(), height());
    if (m_scale * scale_fac > 2.5 * scaledSize) scale_fac = 2.5 * scaledSize / m_scale;

    m_scale *= scale_fac;
    m_mapCenterX = m_centerX - (m_centerX - m_mapCenterX) / scale_fac;
    m_mapCenterY = m_centerY - (m_centerY - m_mapCenterY) / scale_fac;
}

void MapWidget::keyPressEvent(QKeyEvent* e) {
    double delta;
    if (e->key() == Qt::Key_Equal) delta = -100.0;
    else if (e->key() == Qt::Key_Minus) delta = 100.0;
    else { QWidget::keyPressEvent(e); return; }

    zoomBy(std::pow(10.0, delta / 1000.0));
    drawMap();
    update();
}

void MapWidget::mousePressEvent(QMouseEvent* e) {
    const QPoint p = evPos(e);
    m_hasDrag = true;
    m_initDragX = p.x();
    m_initDragY = p.y();
}

void MapWidget::nearestStation(const QPoint& pos, int& outIdx, double& outDist, QPointF* outPos) const {
    outIdx = -1;
    outDist = 1e300;
    double lb, ub;
    m_mapper.latBounds(lb, ub);
    for (size_t i = 0; i < m_stnLats.size(); ++i) {
        if (!(lb <= m_stnLats[i] && m_stnLats[i] <= ub)) continue;
        QPointF p = m_mapper.toXY(m_stnLats[i], m_stnLons[i] + m_mapRot);
        QPointF t = m_transform.map(p);
        double d = std::hypot(t.x() - pos.x(), t.y() - pos.y());
        if (d < outDist) {
            outDist = d;
            outIdx = static_cast<int>(i);
            if (outPos) *outPos = t;
        }
    }
}

void MapWidget::mouseMoveEvent(QMouseEvent* e) {
    const QPoint pos = evPos(e);

    if (m_hasDrag) {
        m_dragging = true;
        m_transX = pos.x() - m_initDragX;
        m_transY = pos.y() - m_initDragY;
        if (!m_dragRedraw) {
            m_dragRedraw = new QTimer(this);
            m_dragRedraw->setSingleShot(true);
            m_dragRedraw->setInterval(120);
            connect(m_dragRedraw, &QTimer::timeout, this, [this]() { if (m_hasDrag) { drawMap(); update(); } });
        }
        m_dragRedraw->start();
        update();                                   // paintEvent slides the cached pixmap; no full redraw per mouse move
        return;                                     // hover / readout resume when the drag ends
    }
    if (m_freePick) { m_cursor = pos; m_cursorIn = true; update(); }   // the crosshair is painted over the cached map
    checkStations(pos);

    bool invertible = false;
    QTransform inv = m_transform.inverted(&invertible);
    if (invertible) {
        QPointF m = inv.map(QPointF(pos));
        double lat, lon;
        m_mapper.toLatLon(m.x(), m.y(), lat, lon);
        lon -= m_mapRot;
        if (lon > 180) lon -= 360.0;
        else if (lon <= -180) lon += 360.0;
        m_latlonReadout->setText(QString("%1; %2").arg(lat, 0, 'f', 3).arg(lon, 0, 'f', 3));
    }
}

void MapWidget::mouseReleaseEvent(QMouseEvent* e) {
    const QPoint pos = evPos(e);

    m_hasDrag = false;
    if (m_dragRedraw) m_dragRedraw->stop();
    const bool moved = (m_transX != 0.0 || m_transY != 0.0);
    m_mapCenterX += m_transX;
    m_mapCenterY += m_transY;
    m_transX = m_transY = 0.0;
    if (moved) { drawMap(); update(); }            // one real redraw at the final position

    if (!m_dragging && m_freePick) {
        bool invertible = false;
        const QTransform inv = m_transform.inverted(&invertible);
        if (invertible) {
            const QPointF m = inv.map(QPointF(pos));
            double lat, lon;
            m_mapper.toLatLon(m.x(), m.y(), lat, lon);
            lon -= m_mapRot;
            if (lon > 180) lon -= 360.0;
            else if (lon <= -180) lon += 360.0;
            double lb, ub;
            m_mapper.latBounds(lb, ub);
            if (std::isfinite(lat) && std::isfinite(lon) && std::fabs(lat) <= 90.0) {
                m_hasPick = true; m_pickLat = lat; m_pickLon = lon;
                emit pointPicked(lat, lon);
                drawMap();
                update();
            }
        }
    } else if (!m_dragging && !m_stnLats.empty()) {
        int idx;
        double dist;
        nearestStation(pos, idx, dist);
        if (idx >= 0 && dist <= 5.0) {
            m_clickedStn = QString::fromStdString(m_stnIds[idx]);
            emit stationClicked(m_points[idx]);
            drawMap();
            update();
        }
    }
    m_dragging = false;
}

void MapWidget::mouseDoubleClickEvent(QMouseEvent* e) {
    const QPoint pos = evPos(e);
    bool invertible = false;
    QTransform inv = m_transform.inverted(&invertible);
    if (!invertible) return;
    QPointF m = inv.map(QPointF(pos));
    double lat, lon;
    m_mapper.toLatLon(m.x(), m.y(), lat, lon);
    lon -= m_mapRot;

    m_mapRot += (lon - m_mapper.lambda0());
    m_mapper.setLambda0(lon);
    showLoading();

    resetViewport(true, lat, lon);
    drawMap();
    hideLoading();
    update();
}

void MapWidget::wheelEvent(QWheelEvent* e) {
    const double maxSpeed = 75.0;
    double delta = std::max(std::min(double(-e->angleDelta().y()), maxSpeed), -maxSpeed);
    zoomBy(std::pow(10.0, delta / 1000.0));

    drawMap();
    checkStations(evPos(e));
    update();
}

void MapWidget::saveProjection() {
    MapViewConfig c;
    c.valid = true;
    c.proj = m_mapper.projection();
    c.std_lon = m_mapper.lambda0();
    c.scale = m_scale;
    c.center_x = m_mapCenterX + (m_defaultWidth - width()) / 2.0;
    c.center_y = m_mapCenterY + (m_defaultHeight - height()) / 2.0;
    saveMapViewConfig(c);
}

void MapWidget::hasInternet(bool has_connection) {
    m_hasInternet = has_connection;
    if (has_connection) {
        m_noInternet->move(width(), height());
    } else {
        QFont f = m_noInternet->font();
        f.setPixelSize(32);
        QFontMetrics fm(f);
        m_noInternet->move(int((width() - textWidth(fm, m_noInternet->text())) / 2.0),
                           int((height() - fm.height()) / 2.0));
    }
}

void MapWidget::showLoading() {
    m_loadReadout->move(10, height() - 25);
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
}

void MapWidget::hideLoading() {
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    m_loadReadout->move(width(), height());
}

void MapWidget::checkStations(const QPoint& pos) {
    int idx;
    double dist;
    QPointF stnPos;
    nearestStation(pos, idx, dist, &stnPos);
    if (idx < 0) return;

    if (dist <= 5.0) {
        QFont font = this->font();
        font.setPointSize(16);
        QFontMetrics fm(font);
        const QString name = m_stnNames[idx];

        const int labelOffset = 5;
        Qt::Alignment align;
        int sgnX, sgnY;
        double labelX, labelY;

        if (stnPos.x() > width() / 2.0) {
            sgnX = -1;
            labelX = stnPos.x() - textWidth(fm, name);
            align |= Qt::AlignRight;
        } else {
            sgnX = 1;
            labelX = stnPos.x();
            align |= Qt::AlignLeft;
        }
        if (stnPos.y() > height() / 2.0) {
            sgnY = -1;
            labelY = stnPos.y() - fm.height();
            align |= Qt::AlignBottom;
        } else {
            sgnY = 1;
            labelY = stnPos.y();
            align |= Qt::AlignTop;
        }

        m_stnReadout->setText(name);
        m_stnReadout->move(int(labelX + sgnX * labelOffset), int(labelY + sgnY * labelOffset));
        m_stnReadout->setFixedWidth(textWidth(fm, name));
        m_stnReadout->setAlignment(align);
        setCursor(Qt::PointingHandCursor);
    } else {
        m_stnReadout->setText("");
        m_stnReadout->setFixedWidth(0);
        m_stnReadout->move(width(), height());
        m_stnReadout->setAlignment(Qt::AlignLeft);
        unsetCursor();
    }
}

} // namespace sharppy
