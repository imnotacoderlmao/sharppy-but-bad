#pragma once

#include <QLabel>
#include <QPixmap>
#include <QTimer>
#include <QTransform>
#include <QVector>
#include <QWidget>
#include <string>
#include <vector>

#include "sharppy/data_fetcher.hpp"
#include "sharppy/map_mapper.hpp"

namespace sharppy {

// One clickable point on the map (mirrors the dicts SHARPpy's datasources return).
struct MapPoint {
    std::string srcid;
    std::string icao;
    std::string iata;
    std::string synop;
    std::string name;
    std::string state;
    std::string country;
    double lat = 0.0;
    double lon = 0.0;
};

MapPoint mapPointFromStation(const StationInfo& s);

// Saved map view ("Save Map View as Default"), kept in an ini file.
struct MapViewConfig {
    bool valid = false;
    MapProj proj = MapProj::NPStere;
    double std_lon = -97.5;
    double scale = 0.6;
    double center_x = 0.0;
    double center_y = 0.0;
};
MapViewConfig loadMapViewConfig();
void saveMapViewConfig(const MapViewConfig& cfg);
QString mapConfigPath();

// Port of SHARPpy's MapWidget: pannable/zoomable station map in a polar
// stereographic or Mercator projection with coast, country, state and county lines.
class MapWidget : public QWidget {
    Q_OBJECT

public:
    explicit MapWidget(QWidget* parent = nullptr);

    void setStations(const std::vector<MapPoint>& points);
    void setProjection(MapProj proj);
    MapProj projection() const { return m_mapper.projection(); }
    void saveProjection();
    void hasInternet(bool has_connection);
    QString selectedStation() const { return m_clickedStn; }

signals:
    void stationClicked(const MapPoint& point);
    void selectionCleared();

protected:
    void paintEvent(QPaintEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void mouseDoubleClickEvent(QMouseEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;

private:
    struct Layer {
        QVector<QPainterPath> paths;
        QVector<QRectF> bounds;
    };

    Mapper m_mapper;

    double m_transX = 0.0, m_transY = 0.0;
    double m_drawnTransX = 0.0, m_drawnTransY = 0.0;   // drag offset baked into m_plotBitMap; the rest is applied by shifting the pixmap
    QTimer* m_dragRedraw = nullptr;                    // repaints the real map when the drag pauses
    double m_centerX = 0.0, m_centerY = 0.0;
    bool m_hasDrag = false;
    int m_initDragX = 0, m_initDragY = 0;
    bool m_dragging = false;
    double m_mapRot = 0.0;
    double m_initScale = 0.6;
    double m_scale = 0.6;
    double m_mapCenterX = 0.0, m_mapCenterY = 0.0;
    int m_defaultWidth = 640, m_defaultHeight = 480;
    bool m_hasInternet = true;

    std::vector<MapPoint> m_points;
    std::vector<double> m_stnLats, m_stnLons;
    std::vector<std::string> m_stnIds;
    QStringList m_stnNames;
    QString m_clickedStn;

    QLabel* m_stnReadout = nullptr;
    QLabel* m_loadReadout = nullptr;
    QLabel* m_latlonReadout = nullptr;
    QLabel* m_noInternet = nullptr;

    QPixmap m_plotBitMap;
    QPixmap m_backgroundBitMap;
    QTransform m_transform;

    Layer m_coast, m_country, m_state, m_county;
    QPainterPath m_grid;

    void initUI();
    void initMap();
    void resetViewport(bool hasCtr = false, double ctrLat = 0.0, double ctrLon = 0.0);
    void drawMap();
    void drawPolitical(QPainter& qp);
    void drawStations(QPainter& qp);
    void checkStations(const QPoint& pos);
    void zoomBy(double scale_fac);
    void showLoading();
    void hideLoading();
    static Layer makeLayer(const QVector<QPainterPath>& paths);
    void nearestStation(const QPoint& pos, int& outIdx, double& outDist, QPointF* outPos = nullptr) const;
};

} // namespace sharppy
