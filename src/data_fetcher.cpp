#include "sharppy/data_fetcher.hpp"
#include <QUrl>
#include <QNetworkRequest>
#include <algorithm>
#include <cctype>

namespace sharppy {

static const std::vector<StationInfo> s_knownStations = {
    {"OUN", "Norman", "OK", "72357", 35.18, -97.44, 357.0},
    {"FWD", "Fort Worth", "TX", "72259", 32.83, -97.30, 203.0},
    {"DDC", "Dodge City", "KS", "72451", 37.76, -100.02, 788.0},
    {"TOP", "Topeka", "KS", "72456", 39.07, -95.62, 270.0},
    {"AMA", "Amarillo", "TX", "72363", 35.22, -101.71, 1099.0},
    {"BMX", "Birmingham", "AL", "72230", 33.17, -86.77, 197.0},
    {"JAN", "Jackson", "MS", "72235", 32.32, -90.08, 92.0},
    {"LIT", "Little Rock", "AR", "72340", 34.84, -92.25, 173.0},
    {"SHV", "Shreveport", "LA", "72248", 32.45, -93.84, 82.0},
    {"LCH", "Lake Charles", "LA", "72240", 30.12, -93.22, 9.0},
    {"BRO", "Brownsville", "TX", "72250", 25.91, -97.42, 6.0},
    {"CRP", "Corpus Christi", "TX", "72251", 27.77, -97.51, 13.0},
    {"DRT", "Del Rio", "TX", "72261", 29.37, -100.92, 313.0},
    {"MAF", "Midland", "TX", "72265", 31.95, -102.18, 874.0},
    {"EPZ", "El Paso / Santa Teresa", "NM", "72270", 31.87, -106.70, 1254.0},
    {"ABQ", "Albuquerque", "NM", "72365", 35.04, -106.62, 1619.0},
    {"TUS", "Tucson", "AZ", "72274", 32.12, -110.93, 776.0},
    {"FGZ", "Flagstaff", "AZ", "72376", 35.23, -111.82, 2192.0},
    {"PSR", "Phoenix", "AZ", "74626", 33.43, -112.02, 342.0},
    {"NKX", "San Diego", "CA", "72293", 32.88, -117.15, 146.0},
    {"VBG", "Vandenberg AFB", "CA", "72393", 34.75, -120.57, 112.0},
    {"OAK", "Oakland", "CA", "72493", 37.70, -122.22, 26.0},
    {"MFR", "Medford", "OR", "72597", 42.37, -122.87, 405.0},
    {"SLE", "Salem", "OR", "72694", 44.91, -123.00, 61.0},
    {"OTX", "Spokane", "WA", "72785", 47.68, -117.63, 729.0},
    {"UIL", "Quillayute", "WA", "72797", 47.95, -124.55, 62.0},
    {"TFX", "Great Falls", "MT", "72775", 47.46, -111.38, 1130.0},
    {"GGW", "Glasgow", "MT", "72768", 48.21, -106.63, 693.0},
    {"RIW", "Riverton", "WY", "72572", 43.06, -108.48, 1698.0},
    {"CYS", "Cheyenne", "WY", "72562", 41.15, -104.81, 1872.0},
    {"DNR", "Denver", "CO", "72469", 39.77, -104.88, 1611.0},
    {"GJT", "Grand Junction", "CO", "72476", 39.12, -108.53, 1475.0},
    {"SLC", "Salt Lake City", "UT", "72572", 40.78, -111.97, 1289.0},
    {"LBF", "North Platte", "NE", "72562", 41.13, -100.68, 849.0},
    {"OAX", "Omaha / Valley", "NE", "72558", 41.32, -96.37, 350.0},
    {"ABR", "Aberdeen", "SD", "72659", 45.45, -98.42, 397.0},
    {"RAP", "Rapid City", "SD", "72662", 44.05, -103.05, 966.0},
    {"BIS", "Bismarck", "ND", "72764", 46.77, -100.75, 506.0},
    {"INL", "International Falls", "MN", "72747", 48.56, -93.40, 361.0},
    {"MPX", "Minneapolis / Chanhassen", "MN", "72649", 44.85, -93.56, 290.0},
    {"DVN", "Davenport", "IA", "74455", 41.61, -90.58, 229.0},
    {"GRB", "Green Bay", "WI", "72645", 44.48, -88.13, 214.0},
    {"ILX", "Lincoln", "IL", "74560", 40.15, -89.34, 178.0},
    {"SGF", "Springfield", "MO", "72440", 37.24, -93.38, 387.0},
    {"ILN", "Wilmington", "OH", "72426", 39.42, -83.82, 321.0},
    {"DTX", "Detroit / White Lake", "MI", "72632", 42.70, -83.47, 326.0},
    {"APX", "Gaylord", "MI", "72634", 44.91, -84.72, 442.0},
    {"PIT", "Pittsburgh", "PA", "72520", 40.53, -80.22, 373.0},
    {"BUF", "Buffalo", "NY", "72528", 42.94, -78.73, 215.0},
    {"ALB", "Albany", "NY", "72518", 42.75, -73.80, 89.0},
    {"OKX", "Upton / New York", "NY", "72501", 40.87, -72.86, 20.0},
    {"CHH", "Chatham", "MA", "74494", 41.67, -69.96, 16.0},
    {"GYX", "Gray / Portland", "ME", "74389", 43.89, -70.26, 125.0},
    {"CAR", "Caribou", "ME", "72712", 46.87, -68.01, 191.0},
    {"IAD", "Sterling / Dulles", "VA", "72403", 38.98, -77.46, 95.0},
    {"WAL", "Wallops Island", "VA", "72402", 37.94, -75.47, 12.0},
    {"RNK", "Blacksburg", "VA", "72318", 37.20, -80.41, 642.0},
    {"GSO", "Greensboro", "NC", "72317", 36.08, -79.95, 273.0},
    {"MHX", "Morehead City / Newport", "NC", "72305", 34.78, -76.88, 11.0},
    {"CHS", "Charleston", "SC", "72208", 32.90, -80.03, 15.0},
    {"FFC", "Peachtree City / Atlanta", "GA", "72215", 33.36, -84.57, 245.0},
    {"JAX", "Jacksonville", "FL", "72206", 30.50, -81.70, 9.0},
    {"TBW", "Tampa Bay / Ruskin", "FL", "72210", 27.70, -82.40, 6.0},
    {"MFL", "Miami", "FL", "72202", 25.75, -80.38, 5.0},
    {"KEY", "Key West", "FL", "72201", 24.55, -81.75, 1.0},
    {"TLH", "Tallahassee", "FL", "72214", 30.40, -84.35, 20.0},
    {"BNA", "Nashville", "TN", "72327", 36.12, -86.68, 180.0}
};

DataFetcher::DataFetcher(QObject* parent) : QObject(parent) {
    m_manager = new QNetworkAccessManager(this);
}

DataFetcher::~DataFetcher() {
    cancel();
}

const std::vector<StationInfo>& DataFetcher::getKnownStations() {
    return s_knownStations;
}

const StationInfo* DataFetcher::findStation(const std::string& id) {
    std::string upper_id = id;
    for (char& c : upper_id) c = std::toupper(c);

    // Strip leading 'K' if 4 letters (e.g. KOUN -> OUN)
    if (upper_id.size() == 4 && upper_id[0] == 'K') {
        upper_id = upper_id.substr(1);
    }

    for (const auto& s : s_knownStations) {
        if (s.id == upper_id || s.synop == upper_id || s.id == id) {
            return &s;
        }
    }
    return nullptr;
}

void DataFetcher::cancel() {
    if (!m_currentReply) return;
    // Detach first: abort() emits finished() synchronously, and the handler would
    // otherwise report a spurious failure and null m_currentReply before we use it.
    QNetworkReply* reply = m_currentReply;
    m_currentReply = nullptr;
    reply->abort();
    reply->deleteLater();
}

void DataFetcher::fetchSounding(SoundingSource source, const std::string& stationId, const QDate& date, int cycleHour) {
    cancel();
    m_fallbackUrls.clear();
    m_currentSource = source;

    std::string cleanId = stationId;
    for (char& c : cleanId) c = std::toupper(c);
    if (cleanId.size() == 4 && cleanId[0] == 'K') {
        cleanId = cleanId.substr(1);
    }

    const StationInfo* st = findStation(cleanId);
    std::string synop = st ? st->synop : cleanId;

    QString yyyy = QString::number(date.year());
    QString yy = QString("%1").arg(date.year() % 100, 2, 10, QChar('0'));
    QString mm = QString("%1").arg(date.month(), 2, 10, QChar('0'));
    QString dd = QString("%1").arg(date.day(), 2, 10, QChar('0'));
    QString hh = QString("%1").arg(cycleHour, 2, 10, QChar('0'));

    QString urlStr;
    QString stationLower = QString::fromStdString(cleanId).toLower();

    switch (source) {
        case SoundingSource::SPC_OBSERVED:
            urlStr = QString("https://www.spc.noaa.gov/exper/soundings/%1%2%3%4_OBS/%5.txt")
                         .arg(yy, mm, dd, hh, QString::fromStdString(cleanId));
            m_expectedTitle = QString("%1 %2%3%4/%5Z SPC OBS").arg(QString::fromStdString(cleanId), yyyy, mm, dd, hh);
            break;

        case SoundingSource::IEM_RAOB:
            urlStr = QString("https://mesonet.agron.iastate.edu/archive/raob/%1/%2/%3/%4_%5.txt")
                         .arg(yyyy, mm, dd, hh, QString::fromStdString(cleanId));
            m_expectedTitle = QString("%1 %2%3%4/%5Z IEM RAOB").arg(QString::fromStdString(cleanId), yyyy, mm, dd, hh);
            break;

        case SoundingSource::UWYO_OBSERVED: {
            // weather.uwyo.edu/cgi-bin/sounding is retired; the page now uses wsgi/sounding. Soundings that the site lists
            // are stored as either TEMP (FM35) or BUFR reports, and autodetect does not always find them, so each
            // format is tried in turn, with the old CGI as a last resort.
            const QString dt = QString("%1-%2-%3%%20%4:00:00").arg(yyyy, mm, dd, hh);
            const QString id = QString::fromStdString(synop);
            for (const char* src : {"UNKNOWN", "BUFR", "FM35"})
                m_fallbackUrls << QString("https://weather.uwyo.edu/wsgi/sounding?datetime=%1&id=%2&src=%3&type=TEXT:LIST").arg(dt, id, src);
            m_fallbackUrls << QString("https://weather.uwyo.edu/cgi-bin/sounding?region=nnam&TYPE=TEXT%3ALIST&YEAR=%1&MONTH=%2&FROM=%3%4&TO=%3%4&STNM=%5")
                                  .arg(yyyy, mm, dd, hh, id);
            urlStr = m_fallbackUrls.takeFirst();
            m_expectedTitle = QString("%1 %2%3%4/%5Z UWYO").arg(QString::fromStdString(cleanId), yyyy, mm, dd, hh);
            break;
        }

        case SoundingSource::BUFKIT_RAP:
            urlStr = QString("https://mtarchive.geol.iastate.edu/%1/%2/%3/bufkit/%4/rap/rap_%5.buf")
                         .arg(yyyy, mm, dd, hh, stationLower);
            m_expectedTitle = QString("%1 %2%3%4/%5Z RAP BUFKIT").arg(QString::fromStdString(cleanId), yyyy, mm, dd, hh);
            break;

        case SoundingSource::BUFKIT_HRRR:
            urlStr = QString("https://mtarchive.geol.iastate.edu/%1/%2/%3/bufkit/%4/hrrr/hrrr_%5.buf")
                         .arg(yyyy, mm, dd, hh, stationLower);
            m_expectedTitle = QString("%1 %2%3%4/%5Z HRRR BUFKIT").arg(QString::fromStdString(cleanId), yyyy, mm, dd, hh);
            break;
    }

    startRequest(QUrl(urlStr));
}

void DataFetcher::fetchUrl(const QString& urlStr, const QString& title) {
    cancel();
    m_fallbackUrls.clear();
    m_currentSource = SoundingSource::SPC_OBSERVED;     // generic URL: no UWyo-specific checks
    m_expectedTitle = title;
    startRequest(QUrl(urlStr));
}

void DataFetcher::startRequest(const QUrl& url) {
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "SHARPpy-Cpp/1.0 (Meteorological Analysis Tool)");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    m_currentReply = m_manager->get(request);

    connect(m_currentReply, &QNetworkReply::finished, this, &DataFetcher::onReplyFinished);
    connect(m_currentReply, &QNetworkReply::downloadProgress, this, &DataFetcher::onDownloadProgress);
}

void DataFetcher::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal) {
    emit progressChanged(bytesReceived, bytesTotal);
}

void DataFetcher::onReplyFinished() {
    if (!m_currentReply) return;

    if (m_currentReply->error() != QNetworkReply::NoError) {
        QString err = m_currentReply->errorString();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
        if (!m_fallbackUrls.isEmpty()) { startRequest(QUrl(m_fallbackUrls.takeFirst())); return; }
        emit fetchFinished(false, "", err, m_expectedTitle);
        return;
    }

    QByteArray data = m_currentReply->readAll();
    QString content = QString::fromUtf8(data);

    m_currentReply->deleteLater();
    m_currentReply = nullptr;

    // UWyo answers 200 with a short message (or an HTML page) when it has no report in that format: try the next one.
    if (!m_fallbackUrls.isEmpty() && !(content.contains("PRES") && content.contains("HGHT"))) {
        startRequest(QUrl(m_fallbackUrls.takeFirst()));
        return;
    }

    if (m_currentSource == SoundingSource::UWYO_OBSERVED && !(content.contains("PRES") && content.contains("HGHT"))) {
        emit fetchFinished(false, "", "University of Wyoming has no sounding for this station/date/cycle (tried autodetect, BUFR and TEMP).", m_expectedTitle);
        return;
    }

    if (content.trimmed().isEmpty()) {
        emit fetchFinished(false, "", "Server returned an empty sounding response.", m_expectedTitle);
        return;
    }

    // Check if error response like HTML 404
    if (content.contains("404 Not Found", Qt::CaseInsensitive) || content.contains("<title>Error</title>", Qt::CaseInsensitive)) {
        emit fetchFinished(false, "", "Sounding not available for this station/date/cycle (404 Not Found).", m_expectedTitle);
        return;
    }

    emit fetchFinished(true, content, "", m_expectedTitle);
}

} // namespace sharppy
