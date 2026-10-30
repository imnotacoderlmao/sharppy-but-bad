#pragma once

#include <QObject>
#include <QString>
#include <QDate>
#include <QTime>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <vector>
#include <string>

namespace sharppy {

enum class SoundingSource {
    SPC_OBSERVED,
    IEM_RAOB,
    UWYO_OBSERVED,
    BUFKIT_RAP,
    BUFKIT_HRRR
};

struct StationInfo {
    std::string id;
    std::string name;
    std::string state;
    std::string synop;
    double lat = 0.0;
    double lon = 0.0;
    double elev_m = 0.0;
};

class DataFetcher : public QObject {
    Q_OBJECT

public:
    explicit DataFetcher(QObject* parent = nullptr);
    ~DataFetcher() override;

    static const std::vector<StationInfo>& getKnownStations();
    static const StationInfo* findStation(const std::string& id);

    void fetchSounding(SoundingSource source, const std::string& stationId, const QDate& date, int cycleHour);
    // Downloads an arbitrary url (built from a datasource xml template).
    void fetchUrl(const QString& url, const QString& title);
    void cancel();

signals:
    void progressChanged(qint64 bytesReceived, qint64 bytesTotal);
    void fetchFinished(bool success, const QString& content, const QString& errorMsg, const QString& title);

private slots:
    void onReplyFinished();
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);

private:
    void startRequest(const QUrl& url);
    QNetworkAccessManager* m_manager;
    QNetworkReply* m_currentReply = nullptr;
    QString m_expectedTitle;
    SoundingSource m_currentSource = SoundingSource::SPC_OBSERVED;
};

} // namespace sharppy
