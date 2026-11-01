#include "sharppy/free_pick.hpp"
#include "sharppy/era5.hpp"
#include "sharppy/openmeteo.hpp"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSettings>
#include <QTextStream>

namespace sharppy {

namespace {
const char* kDefaultCdsUrl = "https://cds.climate.copernicus.eu/api";

// ~/.cdsapirc: "url: ...\nkey: ..."
QString rcValue(const QString& key) {
    QFile f(QDir::homePath() + "/.cdsapirc");
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    QTextStream ts(&f);
    while (!ts.atEnd()) {
        const QString line = ts.readLine().trimmed();
        if (line.startsWith(key + ":")) return line.mid(key.size() + 1).trimmed();
    }
    return {};
}
}

FreePickFetcher::FreePickFetcher(QObject* parent) : QObject(parent) {
    m_net = new QNetworkAccessManager(this);
    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &FreePickFetcher::poll);
}

QString FreePickFetcher::cdsToken() {
    QString t = QSettings("sharppy_cpp", "sharppy").value("cds/token").toString().trimmed();
    if (t.isEmpty()) t = qEnvironmentVariable("CDSAPI_KEY").trimmed();
    if (t.isEmpty()) t = rcValue("key");
    return t;
}
void FreePickFetcher::setCdsToken(const QString& token) {
    QSettings("sharppy_cpp", "sharppy").setValue("cds/token", token.trimmed());
}
QString FreePickFetcher::cdsUrl() {
    QString u = QSettings("sharppy_cpp", "sharppy").value("cds/url").toString().trimmed();
    if (u.isEmpty()) u = qEnvironmentVariable("CDSAPI_URL").trimmed();
    if (u.isEmpty()) u = rcValue("url");
    if (u.isEmpty()) u = kDefaultCdsUrl;
    while (u.endsWith('/')) u.chop(1);
    return u;
}

void FreePickFetcher::cancel() {
    ++m_gen;
    m_busy = false;
    m_timer->stop();
    for (const QPointer<QNetworkReply>& r : m_replies) {
        if (!r) continue;
        r->disconnect(this);
        r->abort();
        r->deleteLater();
    }
    m_replies.clear();
    m_jobs.clear();
}

void FreePickFetcher::start(const PickRequest& req) {
    if (m_busy) return;
    cancel();
    m_busy = true;
    m_req = req;
    if (req.provider == "era5") startEra5(req);
    else if (req.provider == "openmeteo") startOpenMeteo(req);
    else fail("Unknown free-pick provider '" + req.provider + "'");
}

void FreePickFetcher::fail(const QString& why) {
    cancel();
    emit finished(false, {}, {}, why, QString());
}

QNetworkRequest FreePickFetcher::cdsRequest(const QString& path) const {
    QNetworkRequest rq(QUrl(cdsUrl() + "/retrieve/v1/" + path));
    rq.setRawHeader("PRIVATE-TOKEN", cdsToken().toUtf8());
    rq.setRawHeader("User-Agent", "sharppy-cpp");
    rq.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    return rq;
}

// The CDS answers errors as {"type":..., "title":..., "detail":...}
QString FreePickFetcher::apiError(const QByteArray& body, int httpStatus, const QString& fallback) {
    const QJsonObject o = QJsonDocument::fromJson(body).object();
    QString msg = o.value("title").toString();
    const QString detail = o.value("detail").toString();
    if (!detail.isEmpty() && detail != msg) msg += msg.isEmpty() ? detail : ": " + detail;
    if (msg.isEmpty()) msg = fallback;
    if (httpStatus == 401 || httpStatus == 403) msg += "\n(check the CDS token, and that the ERA5 licence was accepted on the dataset page)";
    return msg;
}

// ---------------------------------------------------------------------------------------------------------------
// ERA5 via the CDS
// ---------------------------------------------------------------------------------------------------------------

void FreePickFetcher::startEra5(const PickRequest& req) {
    if (cdsToken().isEmpty()) {
        fail("No CDS access token is set.\n\nCreate a free account at cds.climate.copernicus.eu, copy the personal access token "
             "from your profile page and enter it under Settings > Set CDS token.");
        return;
    }
    m_jobs.clear();
    Job p; p.dataset = "reanalysis-era5-pressure-levels"; p.label = "pressure levels"; p.inputs = era5_pressure_inputs(req.run, req.lat, req.lon);
    Job s; s.dataset = "reanalysis-era5-single-levels"; s.label = "surface"; s.inputs = era5_surface_inputs(req.run, req.lat, req.lon);
    m_jobs << p << s;
    m_pollMs = 2500;
    emit status("ERA5: submitting the request to the Copernicus CDS ...");
    for (int j = 0; j < m_jobs.size(); ++j) submit(j);
}

void FreePickFetcher::submit(int j) {
    const int gen = m_gen;
    QNetworkRequest rq = cdsRequest("processes/" + m_jobs[j].dataset + "/execution");
    rq.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QJsonObject body; body["inputs"] = m_jobs[j].inputs;
    QNetworkReply* r = m_net->post(rq, QJsonDocument(body).toJson(QJsonDocument::Compact));
    track(r);
    connect(r, &QNetworkReply::finished, this, [this, r, j, gen]() {
        r->deleteLater();
        if (gen != m_gen) return;
        const QByteArray b = r->readAll();
        const int http = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (r->error() != QNetworkReply::NoError && http == 0) { jobFailed(j, "network error: " + r->errorString()); return; }
        const QJsonObject o = QJsonDocument::fromJson(b).object();
        if (http >= 400 || o.value("jobID").toString().isEmpty()) { jobFailed(j, apiError(b, http, QString("the CDS rejected the request (HTTP %1)").arg(http))); return; }
        m_jobs[j].id = o.value("jobID").toString();
        m_jobs[j].state = o.value("status").toString("accepted");
        m_jobs[j].submitted = true;
        poll();
    });
}

void FreePickFetcher::poll() {
    if (!m_busy) return;
    const int gen = m_gen;
    bool waiting = false;
    for (int j = 0; j < m_jobs.size(); ++j) {
        Job& jb = m_jobs[j];
        if (!jb.submitted || jb.done || jb.resultsAsked) { waiting = waiting || !jb.done; continue; }
        waiting = true;
        QNetworkReply* r = m_net->get(cdsRequest("jobs/" + jb.id));
        track(r);
        connect(r, &QNetworkReply::finished, this, [this, r, j, gen]() {
            r->deleteLater();
            if (gen != m_gen || j >= m_jobs.size()) return;
            const QByteArray b = r->readAll();
            const int http = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (http >= 400) { jobFailed(j, apiError(b, http, QString("status request failed (HTTP %1)").arg(http))); return; }
            Job& jb = m_jobs[j];
            jb.state = QJsonDocument::fromJson(b).object().value("status").toString();
            if (jb.state == "successful") { fetchResults(j); return; }
            if (jb.state == "failed" || jb.state == "rejected" || jb.state == "dismissed" || jb.state == "deleted") {
                jobFailed(j, QString("the CDS job %1").arg(jb.state)); return;
            }
            QStringList st;
            for (const Job& x : m_jobs) st << QString("%1: %2").arg(x.label, x.done ? "done" : (x.state.isEmpty() ? "submitting" : (x.state == "accepted" ? "queued" : x.state)));
            emit status("ERA5 (CDS) - " + st.join(", ") + "  (queue times vary; this can take a minute or more)");
        });
    }
    if (waiting) {
        m_timer->start(m_pollMs);
        m_pollMs = std::min(m_pollMs + 500, 8000);
    }
}

void FreePickFetcher::fetchResults(int j) {
    const int gen = m_gen;
    m_jobs[j].resultsAsked = true;
    QNetworkReply* r = m_net->get(cdsRequest("jobs/" + m_jobs[j].id + "/results"));
    track(r);
    connect(r, &QNetworkReply::finished, this, [this, r, j, gen]() {
        r->deleteLater();
        if (gen != m_gen) return;
        const QByteArray b = r->readAll();
        const int http = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (http >= 400) { jobFailed(j, apiError(b, http, QString("could not get the results (HTTP %1)").arg(http))); return; }
        const QString href = QJsonDocument::fromJson(b).object().value("asset").toObject().value("value").toObject().value("href").toString();
        if (href.isEmpty()) { jobFailed(j, "the CDS reply has no download link"); return; }
        download(j, QUrl(cdsUrl() + "/").resolved(QUrl(href)));
    });
}

void FreePickFetcher::download(int j, const QUrl& url) {
    const int gen = m_gen;
    emit status(QString("ERA5 (CDS) - downloading %1 ...").arg(m_jobs[j].label));
    QNetworkRequest rq(url);
    rq.setRawHeader("User-Agent", "sharppy-cpp");
    rq.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* r = m_net->get(rq);
    track(r);
    connect(r, &QNetworkReply::finished, this, [this, r, j, gen]() {
        r->deleteLater();
        if (gen != m_gen) return;
        if (r->error() != QNetworkReply::NoError) { jobFailed(j, "download failed: " + r->errorString()); return; }
        m_jobs[j].data = r->readAll();
        m_jobs[j].done = true;
        for (const Job& x : m_jobs) if (!x.done) return;
        finishEra5();
    });
}

void FreePickFetcher::jobFailed(int j, const QString& why) {
    if (!m_busy) return;
    fail(QString("ERA5 %1 request: %2").arg(m_jobs[j].label, why));
}

void FreePickFetcher::finishEra5() {
    QString text, title, err, note;
    const QByteArray pres = m_jobs[0].data, sfc = m_jobs[1].data;
    const PickRequest req = m_req;
    if (!era5_build_text(pres, sfc, req.lat, req.lon, text, title, &err, &note)) { fail("Could not decode the ERA5 data: " + err); return; }
    m_busy = false;
    m_jobs.clear();
    emit finished(true, {text}, {title}, QString(), note);
}

// ---------------------------------------------------------------------------------------------------------------
// gridded model point queries (implemented in the next step)
// ---------------------------------------------------------------------------------------------------------------

void FreePickFetcher::startOpenMeteo(const PickRequest& req) {
    if (req.model.isEmpty()) { fail("This source has no model id (model=\"...\" in its datasource file)."); return; }
    QVector<QDateTime> valid;
    for (int h : req.hours) valid << req.run.toUTC().addSecs(qint64(h) * 3600);
    const QUrl url = openmeteo_url(req.model, req.lat, req.lon, valid, QDateTime::currentDateTimeUtc());
    emit status(QString("Requesting %1 data from Open-Meteo ...").arg(req.model));
    const int gen = m_gen;
    QNetworkRequest rq(url);
    rq.setRawHeader("User-Agent", "sharppy-cpp");
    rq.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* r = m_net->get(rq);
    track(r);
    connect(r, &QNetworkReply::finished, this, [this, r, gen, req, valid]() {
        r->deleteLater();
        if (gen != m_gen) return;
        const QByteArray b = r->readAll();
        const int http = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (r->error() != QNetworkReply::NoError && http == 0) { fail("network error: " + r->errorString()); return; }
        if (http >= 400) {
            const QString reason = QJsonDocument::fromJson(b).object().value("reason").toString();
            fail(QString("Open-Meteo answered HTTP %1%2").arg(http).arg(reason.isEmpty() ? QString() : ": " + reason));
            return;
        }
        QStringList texts, titles; QVector<int> ok; QString err;
        if (!openmeteo_build_texts(b, req.label.isEmpty() ? req.model : req.label, req.lat, req.lon, valid, req.run, texts, titles, ok, &err)) { fail(err); return; }
        QString warn;
        if (ok.size() < valid.size()) warn = QString("%1 of the %2 requested times had no data and were skipped.").arg(valid.size() - ok.size()).arg(valid.size());
        m_busy = false;
        emit finished(true, texts, titles, QString(), warn);
    });
}

} // namespace sharppy
