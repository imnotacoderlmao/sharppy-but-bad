#pragma once
// "Free pick" sources: instead of a fixed list of station soundings, the user clicks any point on the map and the
// sounding is built for that point from gridded data. Providers:
//   era5      Copernicus CDS ERA5 reanalysis (GRIB, needs a CDS personal access token), see era5.hpp
//   openmeteo gridded model point queries (JSON, no key)
#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVector>

namespace sharppy {

struct PickRequest {
    QString provider;          // "era5" | "openmeteo"
    QString model;             // provider specific id (open-meteo "models=" value)
    QString label;             // short name for titles ("GFS")
    double lat = 0, lon = 0;
    QDateTime run;             // ERA5: the valid time; models: the run (cycle) time
    QVector<int> hours;        // forecast hours wanted (models); {0} for analyses
};

class FreePickFetcher : public QObject {
    Q_OBJECT
public:
    explicit FreePickFetcher(QObject* parent = nullptr);
    void start(const PickRequest& req);
    void cancel();
    bool busy() const { return m_busy; }

    // CDS access: the personal access token (from Settings, else $CDSAPI_KEY, else ~/.cdsapirc) and API base url.
    static QString cdsToken();
    static void setCdsToken(const QString& token);
    static QString cdsUrl();

signals:
    void status(const QString& text);
    // One text/title per requested profile. `warning` carries non-fatal notes (e.g. surface data missing).
    void finished(bool ok, const QStringList& texts, const QStringList& titles, const QString& error, const QString& warning);

private:
    struct Job {
        QString dataset, label;
        QJsonObject inputs;
        QString id, state, error;
        QByteArray data;
        bool submitted = false, resultsAsked = false, downloading = false, done = false;
    };

    void startEra5(const PickRequest& req);
    void startOpenMeteo(const PickRequest& req);
    void submit(int j);
    void poll();
    void fetchResults(int j);
    void download(int j, const QUrl& url);
    void jobFailed(int j, const QString& why);
    void finishEra5();
    void fail(const QString& why);
    QNetworkRequest cdsRequest(const QString& path) const;
    static QString apiError(const QByteArray& body, int httpStatus, const QString& fallback);
    void track(QNetworkReply* r) { m_replies.append(QPointer<QNetworkReply>(r)); }

    QNetworkAccessManager* m_net;
    QTimer* m_timer;
    QVector<QPointer<QNetworkReply>> m_replies;
    QVector<Job> m_jobs;
    PickRequest m_req;
    int m_gen = 0, m_pollMs = 2500;
    bool m_busy = false;
};

} // namespace sharppy
