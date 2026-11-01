#include "sharppy/picker_window.hpp"
#include <QInputDialog>
#include <QLineEdit>
#include "sharppy/bufkit.hpp"
#include "sharppy/spc.hpp"
#include "sharppy/uwyo.hpp"

#include <QAbstractItemView>
#include <QApplication>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QKeyEvent>
#include <QLineEdit>
#include <QLocale>
#include <QMenuBar>
#include <QMessageBox>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QRegularExpression>
#include <QSet>
#include <QTimer>
#include <QSettings>
#include <QTextCharFormat>
#include <QUrl>
#include <QVBoxLayout>
#include <algorithm>
#include <memory>

namespace sharppy {

namespace {

QDateTime utc(const QDate& d, int hour) {
    return QDateTime(d, QTime(hour, 0), Qt::UTC);
}

const QDateTime kNoRun(QDate(1700, 1, 1), QTime(0, 0), Qt::UTC);

// "Now" for all availability logic; SHARPPY_NOW (ISO-8601) overrides it for testing.
QDateTime nowUtc() {
    const QByteArray e = qgetenv("SHARPPY_NOW");
    if (!e.isEmpty()) {
        const QDateTime t = QDateTime::fromString(QString::fromLatin1(e), Qt::ISODate);
        if (t.isValid()) return t.toUTC();
    }
    return QDateTime::currentDateTimeUtc();
}

MapPoint mapPointFromDs(const DsPoint& d) {
    MapPoint p;
    p.srcid = d.srcid; p.icao = d.icao; p.iata = d.iata; p.synop = d.synop;
    p.name = d.name; p.state = d.state; p.country = d.country;
    p.lat = d.lat; p.lon = d.lon;
    return p;
}

DsPoint dsPointFromMap(const MapPoint& m) {
    DsPoint d;
    d.srcid = m.srcid; d.icao = m.icao; d.iata = m.iata; d.synop = m.synop;
    d.name = m.name; d.state = m.state; d.country = m.country;
    d.lat = m.lat; d.lon = m.lon;
    return d;
}

AppPreferences loadPrefs() { return loadAppPreferences(); }
void savePrefs(const AppPreferences& p) { saveAppPreferences(p); }

struct ParsedSounding {
    QVector<Profile> profiles;
    QStringList titles;
    QVector<int> stim;          // BUFKIT STIM (forecast hour) per profile, -1 when not known
};

// Tries SPC, then BUFKIT (every forecast record), then UWyo text.
bool parseAll(const std::string& text, const QString& fallbackTitle, ParsedSounding& out) {
    try {
        SoundingMetadata meta;
        Profile p;
        if (SpcParser::parse_string(text, p, meta)) {
            out.profiles << p;
            out.stim << -1;
            out.titles << (meta.title.empty() ? fallbackTitle : QString::fromStdString(meta.title));
            return true;
        }

        std::vector<BufkitRecord> recs;
        if (BufkitParser::parse_string(text, recs) && !recs.empty()) {
            for (const auto& r : recs) {
                out.profiles << r.profile;
                out.stim << r.stim;
                out.titles << QString("%1 %2").arg(QString::fromStdString(r.station_id),
                                                   QString::fromStdString(r.valid_time));
            }
            return true;
        }

        meta = SoundingMetadata();
        p = Profile();
        if (UwyoParser::parse_string(text, p, meta)) {
            out.profiles << p;
            out.stim << -1;
            out.titles << (meta.title.empty() ? fallbackTitle : QString::fromStdString(meta.title));
            return true;
        }
    } catch (const std::exception&) {
        // fall through: a malformed download is "could not parse", never a crash
    }
    return false;
}

} // namespace

// ---------------------------------------------------------------------------
// Shared parsing helper (kept for callers that want a single profile)
// ---------------------------------------------------------------------------

bool parse_any_sounding(const std::string& text, Profile& prof, QString& title,
                        const QString& fallbackTitle) {
    ParsedSounding ps;
    if (!parseAll(text, fallbackTitle, ps) || ps.profiles.isEmpty()) return false;
    prof = ps.profiles.first();
    title = ps.titles.first();
    if (ps.profiles.size() > 1) title += " " + fallbackTitle; // BUFKIT: station + valid time + source
    return true;
}

// ---------------------------------------------------------------------------
// PickerCalendar
// ---------------------------------------------------------------------------

PickerCalendar::PickerCalendar(const QDate& earliest, const QDate& latest, QWidget* parent)
    : QCalendarWidget(parent), m_minDate(earliest), m_maxDate(latest) {
    setGridVisible(true);
    setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);
    setHorizontalHeaderFormat(QCalendarWidget::SingleLetterDayNames);

    for (Qt::DayOfWeek day : {Qt::Sunday, Qt::Saturday}) {
        QTextCharFormat fmt = weekdayTextFormat(day);
        fmt.setForeground(QBrush(Qt::black));
        setWeekdayTextFormat(day, fmt);
    }
}

void PickerCalendar::setLatestAvailable(const QDate& d) {
    m_maxDate = d;
    setSelectedDate(selectedDate());
}

void PickerCalendar::setEarliestAvailable(const QDate& d) {
    m_minDate = d;
}

void PickerCalendar::paintCell(QPainter* painter, const QRect& rect, const QDate& date) const {
    QCalendarWidget::paintCell(painter, rect, date);
    if (date > m_maxDate || date < m_minDate) {
        QColor color("#808080");
        color.setAlphaF(0.5);
        painter->fillRect(rect, color);
    }
}

// ---------------------------------------------------------------------------
// Picker
// ---------------------------------------------------------------------------

Picker::Picker(QWidget* parent) : QWidget(parent) {
    m_fetcher = new DataFetcher(this);
    connect(m_fetcher, &DataFetcher::fetchFinished, this, &Picker::onFetchFinished);
    m_free = new FreePickFetcher(this);
    connect(m_free, &FreePickFetcher::finished, this, &Picker::onFreePickFinished);
    connect(m_free, &FreePickFetcher::status, this, [this](const QString& t) { if (m_progress && m_busy) m_progress->setLabelText(t); });

    initSources();

    // Default sounding source is "Observed"; default run is the most recent 00Z/12Z.
    const int obs = m_ds.indexOf("Observed");
    m_sourceIdx = obs >= 0 ? obs : 0;
    m_run = defaultRun();

    initUI();
    pingConnections();

    for (const QString& w : m_ds.warnings) qWarning("datasources: %s", qPrintable(w));
    if (m_ds.sources.empty()) {
        const QString detail = m_ds.warnings.isEmpty() ? QString() : "\n\n" + m_ds.warnings.join("\n");
        QTimer::singleShot(0, this, [this, detail]() {
            QMessageBox::warning(this, "SHARPpy",
                                 "No data sources were found, so the picker has nothing to list.\n\n"
                                 "Copy the datasource .xml and .csv files into a \"data/datasources\" folder next to "
                                 "the program (or set SHARPPY_DATASOURCES)." + detail);
        });
    }
}

void Picker::initSources() {
    m_ds = loadDataSources();
    m_emptySource.name = "No data sources";
    m_emptySource.observed = true;
}

QComboBox* Picker::dropdownMenu(const QStringList& items, QWidget* parent) {
    QComboBox* dropdown = new QComboBox(parent);
    // Editable + read-only so the text can be centred.
    dropdown->setEditable(true);
    dropdown->lineEdit()->setReadOnly(true);
    dropdown->lineEdit()->setAlignment(Qt::AlignCenter);
    for (const QString& it : items) dropdown->addItem(it);
    return dropdown;
}

QString Picker::runText(const QDateTime& t) {
    return QLocale::c().toString(t, "dd MMMM yyyy / HHmm 'UTC'");
}

QDateTime Picker::defaultRun() const {
    // self.run = [t for t in self.all_times if t.hour in [0, 12]][-1]
    const QVector<QDateTime> all = source().availableTimes(QDate(), nowUtc());
    QDateTime best = kNoRun;
    for (const QDateTime& t : all) {
        const int h = t.time().hour();
        if (h == 0 || h == 12) best = t;
    }
    return best;
}

void Picker::initUI() {
    QGridLayout* layout = new QGridLayout(this);

    m_view = new MapWidget(this);

    m_button = new QPushButton("Generate Profiles", this);
    m_button->setDisabled(true);
    connect(m_button, &QPushButton::clicked, this, &Picker::onGenerate);

    m_allProfs = new QPushButton("Select All", this);
    m_allProfs->setDisabled(true);
    connect(m_allProfs, &QPushButton::clicked, this, &Picker::onSelectAll);

    m_saveViewButton = new QPushButton("Save Map View as Default", this);
    connect(m_saveViewButton, &QPushButton::clicked, this, &Picker::onSaveView);

    m_profileList = new QListWidget(this);
    m_profileList->setSelectionMode(QAbstractItemView::MultiSelection);
    m_profileList->setDisabled(true);

    QWidget* leftFrame = new QWidget(this);
    QWidget* rightFrame = new QWidget(this);
    QVBoxLayout* leftLayout = new QVBoxLayout(leftFrame);
    QGridLayout* rightLayout = new QGridLayout(rightFrame);

    // Calendar(self, dt_avail=self.run): earliest defaults to 1946-01-01.
    const QDate calMax = (m_run != kNoRun) ? m_run.date() : QDate::currentDate();
    m_cal = new PickerCalendar(QDate(1946, 1, 1), calMax, this);
    m_cal->setSelectedDate(calMax);
    connect(m_cal, &QCalendarWidget::clicked, this, &Picker::onCalendarClicked);
    m_calDate = m_cal->selectedDate();

    // Source dropdown (sorted by name, like the Python)
    QStringList models;
    for (const DataSource& s : m_ds.sources) models << s.name;
    if (models.isEmpty()) models << m_emptySource.name;
    m_modelDropdown = dropdownMenu(models, this);
    m_modelDropdown->setCurrentIndex(m_sourceIdx);

    // Map projection dropdown
    const MapViewConfig cfg = loadMapViewConfig();
    int projIdx = 0;
    if (cfg.valid) projIdx = (cfg.proj == MapProj::NPStere) ? 0 : (cfg.proj == MapProj::Merc) ? 1 : 2;
    m_mapDropdown = dropdownMenu({"Northern Hemisphere", "Tropics", "Southern Hemisphere"}, this);
    m_mapDropdown->setCurrentIndex(projIdx);

    // Run (cycle) dropdown: the times of the initial day.
    QVector<QDateTime> filt;
    for (const QDateTime& t : source().availableTimes(QDate(), nowUtc())) {
        if (t.date() == m_calDate) filt.append(t);
    }
    QStringList runItems;
    for (const QDateTime& t : filt) runItems << runText(t);
    m_runDropdown = dropdownMenu(runItems, this);
    m_runTimes = filt;
    const int runIdx = static_cast<int>(filt.indexOf(m_run));
    if (runIdx >= 0) m_runDropdown->setCurrentIndex(runIdx);

    connect(m_modelDropdown, QOverload<int>::of(&QComboBox::activated), this, &Picker::onGetModel);
    connect(m_mapDropdown, QOverload<int>::of(&QComboBox::activated), this, &Picker::onGetMap);
    connect(m_runDropdown, QOverload<int>::of(&QComboBox::activated), this, &Picker::onGetRun);

    m_typeLabel = new QLabel("Select Sounding Source", this);
    m_dateLabel = new QLabel("Select Forecast Time", this);
    m_mapLabel = new QLabel("Select Map Area", this);
    m_runLabel = new QLabel("Select Cycle", this);
    m_dateLabel->setDisabled(true);

    leftLayout->addWidget(m_typeLabel);
    leftLayout->addWidget(m_modelDropdown);
    leftLayout->addWidget(m_runLabel);
    leftLayout->addWidget(m_cal);
    leftLayout->addWidget(m_runDropdown);
    leftLayout->addWidget(m_dateLabel);
    leftLayout->addWidget(m_profileList);
    leftLayout->addWidget(m_allProfs);
    leftLayout->addWidget(m_button);

    rightLayout->setColumnMinimumWidth(0, 500);
    rightLayout->addWidget(m_mapLabel, 0, 0, 1, 1);
    rightLayout->addWidget(m_saveViewButton, 0, 1, 1, 1);
    rightLayout->addWidget(m_mapDropdown, 1, 0, 1, 2);
    rightLayout->addWidget(m_view, 2, 0, 1, 2);

    layout->addWidget(leftFrame, 0, 0, 1, 1);
    layout->addWidget(rightFrame, 0, 1, 1, 1);
    leftFrame->setMaximumWidth(280);

    connect(m_view, &MapWidget::stationClicked, this, &Picker::onMapClicked);
    connect(m_view, &MapWidget::pointPicked, this, &Picker::onPointPicked);
    connect(m_view, &MapWidget::selectionCleared, this, &Picker::onSelectionCleared);

    refreshStations();
    m_view->hasInternet(m_hasConnection);
    updateList();
}

// --- stations on the map -------------------------------------------------------

std::vector<const QSet<QString>*> Picker::listingPointers() const {
    static const QSet<QString> kNothing;
    std::vector<const QSet<QString>*> out(m_listState.size(), nullptr);
    for (size_t i = 0; i < m_listState.size(); ++i) {
        if (m_listState[i] == ListState::Ready) out[i] = &m_listing[i];
        else if (m_listState[i] == ListState::Pending) out[i] = &kNothing;   // not known yet: hide
    }
    return out;
}

void Picker::applyStations() {
    std::vector<MapPoint> pts;
    if (m_run != kNoRun) {
        for (const DsPoint& p : source().availableAtTime(listingPointers())) pts.push_back(mapPointFromDs(p));
    }
    m_view->setStations(pts);
}

void Picker::abortListings() {
    ++m_listGen;
    for (const QPointer<QNetworkReply>& r : m_listReplies) {
        if (!r) continue;
        r->disconnect(this);   // abort() emits finished() synchronously
        r->abort();
        r->deleteLater();
    }
    m_listReplies.clear();
}

void Picker::refreshStations() {
    abortListings();

    const DataSource& s = source();
    const size_t n = s.outlets.size();
    m_listState.assign(n, ListState::None);
    m_listing.assign(n, QSet<QString>());

    // Ask each outlet's archive index which stations really have data at this run
    // (available.availableat). Until the answer arrives those outlets contribute no points.
    if (m_run != kNoRun && m_hasConnection && qEnvironmentVariableIsEmpty("SHARPPY_SKIP_LISTING")) {
        if (!m_listMgr) m_listMgr = new QNetworkAccessManager(this);
        const int gen = m_listGen;
        for (size_t i = 0; i < n; ++i) {
            const QString url = s.outlets[i].stationListingUrl(m_run);
            const QRegularExpression rx = s.outlets[i].stationListingRegex();
            if (url.isEmpty() || !rx.isValid() || rx.pattern().isEmpty()) continue;

            m_listState[i] = ListState::Pending;
            QNetworkRequest req{QUrl(url)};
            req.setHeader(QNetworkRequest::UserAgentHeader, "SHARPpy-Cpp/1.0 (Meteorological Analysis Tool)");
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
            req.setTransferTimeout(8000);
#endif
            QNetworkReply* reply = m_listMgr->get(req);
            m_listReplies.append(reply);

            connect(reply, &QNetworkReply::finished, this, [this, reply, rx, i, gen]() {
                reply->deleteLater();
                if (gen != m_listGen || i >= m_listState.size()) return;   // superseded

                const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                if (reply->error() == QNetworkReply::NoError) {
                    QSet<QString> ids;
                    QRegularExpressionMatchIterator it = rx.globalMatch(QString::fromUtf8(reply->readAll()));
                    while (it.hasNext()) ids.insert(it.next().captured(1));
                    m_listing[i] = ids;
                    m_listState[i] = ListState::Ready;
                } else if (status == 404 || reply->error() == QNetworkReply::ContentNotFoundError) {
                    // The archive has no folder for that hour: nothing is available there.
                    m_listing[i].clear();
                    m_listState[i] = ListState::Ready;
                } else {
                    // Offline or timed out: fall back to listing every station of the outlet.
                    m_listState[i] = ListState::Failed;
                }
                applyStations();
            });
        }
    }
    applyStations();
}

// --- connectivity ------------------------------------------------------------

void Picker::pingConnections() {
    if (!qEnvironmentVariableIsEmpty("SHARPPY_SKIP_PING")) return;

    // data_source.pingURLs: one request per distinct host named in the datasources.
    QStringList urls;
    for (const DataSource& ds : m_ds.sources) {
        for (const DsOutlet& o : ds.outlets) {
            const QUrl u(o.urlTemplate.left(o.urlTemplate.indexOf('{') > 0 ? o.urlTemplate.indexOf('{') : -1));
            if (u.scheme() != "http" && u.scheme() != "https") continue;   // ftp is not probed
            const QString base = u.scheme() + "://" + u.host() + "/";
            if (!urls.contains(base)) urls << base;
        }
    }
    if (urls.isEmpty()) {
        m_hasConnection = false;
        return;
    }

    m_pingMgr = new QNetworkAccessManager(this);
    auto remaining = std::make_shared<int>(urls.size());
    auto anyOk = std::make_shared<bool>(false);

    for (const QString& u : urls) {
        QNetworkRequest req{QUrl(u)};
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
        req.setTransferTimeout(5000);
#endif
        QNetworkReply* reply = m_pingMgr->head(req);
        connect(reply, &QNetworkReply::finished, this, [this, reply, remaining, anyOk]() {
            // Any HTTP response at all (even an error status) means the host is reachable.
            if (reply->error() == QNetworkReply::NoError ||
                reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).isValid()) {
                *anyOk = true;
            }
            reply->deleteLater();
            if (--(*remaining) == 0) {
                const bool changed = (m_hasConnection != *anyOk);
                m_hasConnection = *anyOk;
                m_view->hasInternet(m_hasConnection);
                if (!m_hasConnection) m_button->setDisabled(true);
                else if (m_hasLoc) m_button->setEnabled(true);
                if (changed && m_hasConnection) refreshStations();   // listings were skipped while offline
            }
        });
    }
}

// --- date / cycle / forecast list ---------------------------------------------

void Picker::updateFromCal(bool updatedModel) {
    updateRunDropdown(updatedModel);
    refreshStations();   // view.setDataSource(source, run)
    updateList();
}

void Picker::updateRunDropdown(bool) {
    const DataSource& s = source();
    const QDateTime now = nowUtc();
    m_calDate = m_cal->selectedDate();

    // times = source.getAvailableTimes(dt=cal_date)
    QVector<QDateTime> times = s.availableTimes(m_calDate, now);

    const DsTimeSpan span = s.calendarSpan(times, now);
    if (span.valid) {
        m_cal->setLatestAvailable(span.latest);
        m_cal->setEarliestAvailable(span.earliest);
    }
    m_calDate = m_cal->selectedDate();
    if (s.freePick() && span.valid && m_calDate > span.latest) {   // gridded sources lag behind now: jump to the newest day with data
        m_cal->setSelectedDate(span.latest);
        m_calDate = m_cal->selectedDate();
        times = s.availableTimes(m_calDate, now);
    }
    m_cal->update();

    // Keep only the times of the selected day.
    QVector<QDateTime> filtered;
    for (const QDateTime& t : times) {
        if (t.date() == m_calDate) filtered.append(t);
    }

    m_runDropdown->clear();
    m_runTimes = filtered;

    if (!filtered.isEmpty()) {
        for (const QDateTime& t : filtered) m_runDropdown->addItem(runText(t));

        if (s.observed && s.name == "Observed") {
            // Prefer the 00Z/12Z launch (03Z/15Z before May 1957) for this day.
            const bool modern = m_calDate >= QDate(1957, 5, 1);
            QDateTime pick;
            for (const QDateTime& t : filtered) {
                const int h = t.time().hour();
                if (modern ? (h == 0 || h == 12) : (h == 3 || h == 15)) pick = t;
            }
            m_run = pick.isValid() ? pick : filtered.last();
        } else {
            m_run = filtered.last();
        }
        m_runDropdown->setCurrentIndex(static_cast<int>(filtered.indexOf(m_run)));
        m_runDropdown->setEnabled(true);
    } else {
        m_run = kNoRun;
        const bool obs = s.name == "Observed";
        m_runDropdown->addItem(obs ? "- No obs available -" : "- No runs available -");
        m_runDropdown->setCurrentIndex(0);
        m_runDropdown->setEnabled(false);
    }
    m_runDropdown->update();
}

void Picker::updateList() {
    if (m_selectFlag) onSelectAll();
    m_profileList->clear();
    m_profIdx.clear();

    const DataSource& s = source();
    QStringList timelist;

    if (m_run == kNoRun) {
        m_profileList->setDisabled(true);
        m_allProfs->setDisabled(true);
        m_dateLabel->setDisabled(true);
    } else if (s.hasForecast()) {
        m_profileList->setEnabled(true);
        m_allProfs->setEnabled(true);
        m_dateLabel->setEnabled(true);
        for (int fh : s.forecastHours()) {
            const QDateTime valid = m_run.addSecs(static_cast<qint64>(fh) * 3600);
            timelist << QString("%1   (F%2)")
                            .arg(valid.toString("yyyy-MM-dd HH'Z'"))
                            .arg(fh, 3, 10, QChar('0'));
        }
    } else {
        m_profileList->setDisabled(true);
        m_allProfs->setDisabled(true);
        m_dateLabel->setDisabled(true);
    }

    for (const QString& it : timelist) m_profileList->addItem(it);
    m_profileList->update();
    m_allProfs->setText("Select All");
    m_selectFlag = false;
}

// --- slots ----------------------------------------------------------------------

void Picker::onGetModel(int) {
    const int idx = m_ds.indexOf(m_modelDropdown->currentText());
    if (idx >= 0) m_sourceIdx = idx;
    m_view->setFreePick(source().freePick());
    updateFromCal(true);
    m_runLabel->setEnabled(true);
    m_cal->setEnabled(true);
}

void Picker::onGetRun(int index) {
    if (index < 0 || index >= m_runTimes.size()) return;
    m_run = m_runTimes[index];
    refreshStations();   // view.setCurrentTime(run): clears the station selection
    updateList();
}

void Picker::onGetMap(int) {
    const QString txt = m_mapDropdown->currentText();
    MapProj p = MapProj::NPStere;
    if (txt == "Tropics") p = MapProj::Merc;
    else if (txt == "Southern Hemisphere") p = MapProj::SPStere;
    m_view->setProjection(p);
}

void Picker::onCalendarClicked(const QDate&) {
    updateFromCal(false);
}

void Picker::onSaveView() {
    m_view->saveProjection();
}

void Picker::onSelectAll() {
    const int items = m_profileList->count();
    if (!m_selectFlag) {
        for (int i = 0; i < items; ++i) m_profileList->item(i)->setSelected(true);
        m_allProfs->setText("Deselect All");
        m_selectFlag = true;
    } else {
        for (int i = 0; i < items; ++i) m_profileList->item(i)->setSelected(false);
        m_allProfs->setText("Select All");
        m_selectFlag = false;
    }
}

void Picker::onSelectionCleared() {
    m_hasLoc = false;
    m_dispName.clear();
    m_button->setText("Generate Profiles");
    m_button->setDisabled(true);
}

void Picker::onPointPicked(double lat, double lon) {
    MapPoint p;
    p.srcid = "pick"; p.lat = lat; p.lon = lon;
    m_hasLoc = true;
    m_loc = p;
    m_dispName = QString("%1%2,%3%4").arg(lat >= 0 ? "N" : "S").arg(std::fabs(lat), 0, 'f', 2).arg(lon >= 0 ? "E" : "W").arg(std::fabs(lon), 0, 'f', 2);
    m_button->setText(m_dispName + " | Generate Profiles");
    if (m_hasConnection) m_button->setEnabled(true);
}

void Picker::onMapClicked(const MapPoint& point) {
    m_hasLoc = true;
    m_loc = point;
    if (!point.icao.empty()) m_dispName = QString::fromStdString(point.icao);
    else if (!point.iata.empty()) m_dispName = QString::fromStdString(point.iata);
    else m_dispName = QString::fromStdString(point.srcid).toUpper();

    m_button->setText(m_dispName + " | Generate Profiles");
    if (m_hasConnection) m_button->setEnabled(true);
}

void Picker::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_Equal || e->key() == Qt::Key_Minus) {
        QCoreApplication::sendEvent(m_view, e);
    } else {
        QWidget::keyPressEvent(e);
    }
}

// --- fetching -----------------------------------------------------------------

void Picker::onGenerate() {
    if (!m_hasLoc || m_busy) return;

    m_profIdx.clear();
    for (QListWidgetItem* item : m_profileList->selectedItems()) {
        const int idx = m_profileList->row(item);
        if (std::find(m_profIdx.begin(), m_profIdx.end(), idx) == m_profIdx.end()) m_profIdx.push_back(idx);
    }
    std::sort(m_profIdx.begin(), m_profIdx.end());

    // Forecast sources need at least one forecast hour picked; observed ones don't.
    if (source().hasForecast() && m_profIdx.empty()) return;
    if (m_run == kNoRun) return;

    // Outlets that really have this station at this cycle, in the order they are tried
    // (DataSource._getOutletWithProfile).
    m_fetchStn = dsPointFromMap(m_loc);
    if (source().freePick()) {                  // any map point works: every provider outlet that serves this time
        m_outletOrder.clear();
        for (size_t i = 0; i < source().outlets.size(); ++i) {
            const DsOutlet& o = source().outlets[i];
            if (!o.provider.isEmpty() && o.availableTimes(m_run.date(), nowUtc()).contains(m_run)) m_outletOrder.push_back(int(i));
        }
    } else {
        m_outletOrder = source().outletsWithProfile(m_fetchStn, m_run, nowUtc(), listingPointers());
    }

    m_tries = 0;
    m_lastError.clear();
    m_attemptLog.clear();
    m_busy = true;
    startFetchAttempt();
}

void Picker::startFetchAttempt() {
    if (m_tries >= m_outletOrder.size()) {
        fail(m_lastError);
        return;
    }

    if (!m_progress) {
        m_progress = new QProgressDialog("Downloading Profile", "Cancel", 0, 0, this);
        m_progress->setWindowTitle("SHARPpy");
        m_progress->setWindowModality(Qt::WindowModal);
        m_progress->setMinimumDuration(0);
        m_progress->setAutoClose(false);
        m_progress->setAutoReset(false);
        connect(m_progress, &QProgressDialog::canceled, this, [this]() {
            if (!m_busy) return;
            m_busy = false;            // before cancel(): the abort emits fetchFinished(false)
            m_fetcher->cancel();
            m_free->cancel();
            m_button->setEnabled(m_hasLoc && m_hasConnection);
        });
    }
    m_progress->setLabelText(QString("Downloading Profile\n%1 %2/%3Z from %4 (source %5 of %6)")
                                 .arg(m_dispName, m_run.toString("yyyy-MM-dd"), m_run.toString("HH"),
                                      source().outlets[static_cast<size_t>(m_outletOrder[m_tries])].name)
                                 .arg(m_tries + 1).arg(m_outletOrder.size()));
    m_progress->setRange(0, 0);
    m_progress->show();
    m_button->setDisabled(true);

    const DsOutlet& o = source().outlets[static_cast<size_t>(m_outletOrder[m_tries])];
    if (!o.provider.isEmpty()) {
        PickRequest rq;
        rq.provider = o.provider; rq.model = o.modelId; rq.label = source().name.section(" (", 0, 0); rq.lat = m_loc.lat; rq.lon = m_loc.lon; rq.run = m_run;
        for (int idx : m_profIdx) { const std::vector<int> hrs = source().forecastHours(); if (idx >= 0 && idx < int(hrs.size())) rq.hours.push_back(hrs[size_t(idx)]); }
        if (rq.hours.isEmpty()) rq.hours.push_back(0);
        m_free->start(rq);
        return;
    }
    const QString title = QString("%1 %2/%3Z (%4: %5)")
                              .arg(m_dispName, m_run.toString("yyyyMMdd"), m_run.toString("HH"), source().name, o.name);
    DsPoint stn = m_fetchStn;
    if (const DsPoint* own = o.matchStation(m_fetchStn)) stn.srcid = own->srcid;   // per-outlet station id
    m_fetcher->fetchUrl(o.url(stn, m_run), title);
}

void Picker::onFetchFinished(bool success, const QString& content, const QString& errorMsg,
                             const QString& title) {
    if (!m_busy) return; // cancelled or stale

    const QString outletName = source().outlets[static_cast<size_t>(m_outletOrder[m_tries])].name;
    if (!success) {
        m_lastError = errorMsg;
        m_attemptLog << outletName + ": " + errorMsg;
        ++m_tries;
        startFetchAttempt();
        return;
    }

    if (m_progress) m_progress->setLabelText("Creating Profiles");

    ParsedSounding ps;
    if (!parseAll(content.toStdString(), title, ps) || ps.profiles.isEmpty()) {
        m_lastError = "The downloaded data could not be parsed as an SPC, BUFKIT or UWyo sounding.";
        m_attemptLog << outletName + ": no usable sounding in the reply";
        ++m_tries;
        startFetchAttempt();
        return;
    }

    const DataSource& s = source();
    QVector<Profile> outProfs;
    QStringList outTitles;

    if (s.hasForecast()) {
        // BUFKIT files hold one record per forecast time. Match each requested hour to the record whose STIM
        // says so (files can skip hours or start late); only fall back to the list position when STIM is absent.
        const std::vector<int> hours = s.forecastHours();
        for (int idx : m_profIdx) {
            if (idx < 0 || idx >= static_cast<int>(hours.size())) continue;
            const int fh = hours[static_cast<size_t>(idx)];
            int rec = ps.stim.indexOf(fh);
            if (rec < 0 && !ps.stim.contains(0) && idx < ps.profiles.size() && ps.stim.value(idx, -1) < 0) rec = idx;
            if (rec < 0) continue;
            outProfs << ps.profiles[rec];
            outTitles << QString("%1 %2  %3Z %4  F%5")
                             .arg(m_dispName, s.name, m_run.toString("yyyyMMdd/HH"),
                                  ps.titles[rec].section(' ', 1), QString("%1").arg(fh, 3, 10, QChar('0')));
        }
        if (outProfs.isEmpty()) {
            m_lastError = "The model file has none of the requested forecast hours.";
            ++m_tries;
            startFetchAttempt();
            return;
        }
    } else {
        outProfs << ps.profiles.first();
        outTitles << title;                    // "STN yyyymmdd/hhZ (Observed: SPC)"
    }

    for (Profile& pr : outProfs) {
        if (pr.latitude == MISSING) pr.latitude = m_fetchStn.lat;
        pr.station_id = !m_fetchStn.icao.empty() ? m_fetchStn.icao : (!m_fetchStn.iata.empty() ? m_fetchStn.iata : m_fetchStn.synop);
    }
    finishFetch();
    emit profilesReady(outProfs, outTitles);
}

void Picker::onFreePickFinished(bool ok, const QStringList& texts, const QStringList& titles, const QString& error, const QString& warning) {
    if (!m_busy) return;                       // cancelled
    const QString outletName = source().outlets[static_cast<size_t>(m_outletOrder[m_tries])].name;
    QVector<Profile> outProfs;
    QStringList outTitles;
    QString why = error;
    if (ok) {
        for (int i = 0; i < texts.size(); ++i) {
            ParsedSounding ps;
            if (!parseAll(texts[i].toStdString(), titles.value(i), ps) || ps.profiles.isEmpty()) continue;
            Profile pr = ps.profiles.first();
            pr.latitude = m_loc.lat;
            pr.station_id = source().name.toStdString();
            outProfs << pr;
            outTitles << titles.value(i);
        }
        if (outProfs.isEmpty()) why = "the generated sounding could not be parsed";
    }
    if (outProfs.isEmpty()) {
        m_lastError = why;
        m_attemptLog << outletName + ": " + why;
        ++m_tries;
        startFetchAttempt();
        return;
    }
    finishFetch();
    if (!warning.isEmpty()) QMessageBox::information(this, "SHARPpy", warning);
    emit profilesReady(outProfs, outTitles);
}

void Picker::finishFetch() {
    m_busy = false;
    if (m_progress) m_progress->hide();
    m_button->setEnabled(m_hasLoc && m_hasConnection);
}

void Picker::fail(const QString& message) {
    finishFetch();
    QString text;
    if (m_outletOrder.empty()) {
        text = "No outlet found with the requested profile!\n\nNone of this source's outlets lists this station at this time.";
    } else {
        QStringList names;
        for (int i : m_outletOrder) names << source().outlets[static_cast<size_t>(i)].name;
        text = QString("Could not find a sounding for %1 at %2 %3Z.\n\nTried %4 source%5: %6.")
                   .arg(m_dispName, m_run.toString("yyyy-MM-dd"), m_run.toString("HH"))
                   .arg(names.size()).arg(names.size() == 1 ? "" : "s").arg(names.join(", "));
        if (!m_attemptLog.isEmpty()) text += "\n\n" + m_attemptLog.join("\n");
    }
    (void)message;
    QMessageBox::critical(this, "SHARPpy", text);
}

// ---------------------------------------------------------------------------
// PickerWindow
// ---------------------------------------------------------------------------

PickerWindow::PickerWindow(QWidget* parent) : QMainWindow(parent) {
    m_picker = new Picker(this);
    setCentralWidget(m_picker);
    connect(m_picker, &Picker::profilesReady, this, &PickerWindow::profilesReady);
    createMenuBar();
    setWindowTitle("SHARPpy Sounding Picker");
}

void PickerWindow::createMenuBar() {
    QMenu* fileMenu = menuBar()->addMenu("File");

    QAction* open = fileMenu->addAction("Open");
    open->setShortcut(QKeySequence("Ctrl+O"));
    connect(open, &QAction::triggered, this, &PickerWindow::openFile);

    QAction* pref = fileMenu->addAction("Preferences");
    connect(pref, &QAction::triggered, this, &PickerWindow::preferencesBox);

    QMenu* setMenu = menuBar()->addMenu("Settings");
    QAction* cds = setMenu->addAction("Set CDS token (ERA5)...");
    connect(cds, &QAction::triggered, this, [this]() {
        bool ok = false;
        const QString t = QInputDialog::getText(this, "Copernicus CDS token",
            "Personal access token from your profile page at cds.climate.copernicus.eu\n(also accept the ERA5 licence on the dataset page once).\nLeave empty to clear.",
            QLineEdit::Password, FreePickFetcher::cdsToken(), &ok);
        if (ok) FreePickFetcher::setCdsToken(t);
    });

    QMenu* helpMenu = menuBar()->addMenu("Help");
    QAction* about = helpMenu->addAction("About");
    connect(about, &QAction::triggered, this, &PickerWindow::aboutBox);
}

void PickerWindow::openFile() {
    QSettings s("sharppy", "cppsharp");
    const QString path = s.value("paths/load_txt", QDir::homePath()).toString();

    const QStringList links = QFileDialog::getOpenFileNames(this, "Open file", path);
    if (links.isEmpty() || links.first().isEmpty()) return;

    s.setValue("paths/load_txt", QFileInfo(links.first()).absolutePath());
    for (const QString& l : links) emit fileRequested(l);
}

void PickerWindow::preferencesBox() {
    PreferencesDialog dlg(loadPrefs(), this);
    if (dlg.exec() == QDialog::Accepted) {
        savePrefs(dlg.getPreferences());
        emit preferencesChanged(dlg.getPreferences());
    }
}

void PickerWindow::aboutBox() {
    QMessageBox box(this);
    QPushButton* docs = box.addButton("Online Docs", QMessageBox::ActionRole);
    QPushButton* bug = box.addButton("Report Bug", QMessageBox::ActionRole);
    QPushButton* gh = box.addButton("Github", QMessageBox::ActionRole);
    box.addButton(QMessageBox::Close);
    box.setDefaultButton(QMessageBox::Close);

    QString txt = "SHARPpy v0+cpp\n\n";
    txt += "Sounding and Hodograph Analysis and Research Program for Python (C++/Qt port)\n\n";
    txt += "(C) 2014-" + QString::number(QDate::currentDate().year()) +
           " by Patrick Marsh, John Hart, Kelton Halbert, Greg Blumberg, and Tim Supinie.";
    txt += "\n\nSHARPpy is a collection of open source sounding and hodograph analysis routines, a sounding "
           "plotting package, and an interactive application for analyzing real-time soundings. It was "
           "developed to provide the atmospheric science community a free and consistent source of routines "
           "for analyzing sounding data.\n\n";
    txt += "Qt version: " + QString(qVersion());
    txt += "\n\nContribute: https://github.com/sharppy/SHARPpy/";
    box.setText(txt);
    box.exec();

    if (box.clickedButton() == docs) QDesktopServices::openUrl(QUrl("http://sharppy.github.io/SHARPpy/"));
    else if (box.clickedButton() == gh) QDesktopServices::openUrl(QUrl("https://github.com/sharppy/SHARPpy"));
    else if (box.clickedButton() == bug) QDesktopServices::openUrl(QUrl("https://github.com/sharppy/SHARPpy/issues"));
}

void PickerWindow::keyPressEvent(QKeyEvent* e) {
    if (e->matches(QKeySequence::Open)) openFile();
    if (e->matches(QKeySequence::Quit)) close();
    if (e->key() == Qt::Key_W) emit focusSkewRequested();
    QMainWindow::keyPressEvent(e);
}

} // namespace sharppy
