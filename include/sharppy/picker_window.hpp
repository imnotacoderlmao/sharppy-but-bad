#pragma once

#include <QCalendarWidget>
#include <QComboBox>
#include <QDate>
#include <QDateTime>
#include <QLabel>
#include <QListWidget>
#include <QMainWindow>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QNetworkReply>
#include <QProgressDialog>
#include <QPushButton>
#include <QSet>
#include <QStringList>
#include <QVector>
#include <QWidget>
#include <string>
#include <vector>

#include "sharppy/data_fetcher.hpp"
#include "sharppy/free_pick.hpp"
#include "sharppy/datasource.hpp"
#include "sharppy/map_widget.hpp"
#include "sharppy/preferences_dialog.hpp"
#include "sharppy/profile.hpp"

namespace sharppy {

// Parses any supported sounding text (SPC, BUFKIT, UWyo). On success fills
// `prof` and `title` (falling back to `fallbackTitle` when the data has none).
bool parse_any_sounding(const std::string& text, Profile& prof, QString& title,
                        const QString& fallbackTitle);

// Calendar that greys out dates outside the available span (port of full_gui.Calendar).
class PickerCalendar : public QCalendarWidget {
    Q_OBJECT

public:
    PickerCalendar(const QDate& earliest, const QDate& latest, QWidget* parent = nullptr);
    void setLatestAvailable(const QDate& d);
    void setEarliestAvailable(const QDate& d);

protected:
    void paintCell(QPainter* painter, const QRect& rect, const QDate& date) const override;

private:
    QDate m_minDate;
    QDate m_maxDate;
};

// Port of full_gui.Picker: the interactive widget for choosing which sounding(s) to view.
class Picker : public QWidget {
    Q_OBJECT

public:
    explicit Picker(QWidget* parent = nullptr);

signals:
    // One entry per profile (several for model forecast hours).
    void profilesReady(const QVector<Profile>& profiles, const QStringList& titles);

protected:
    void keyPressEvent(QKeyEvent* e) override;

private slots:
    void onGetModel(int index);
    void onGetRun(int index);
    void onGetMap(int index);
    void onCalendarClicked(const QDate& date);
    void onSaveView();
    void onSelectAll();
    void onMapClicked(const MapPoint& point);
    void onSelectionCleared();
    void onPointPicked(double lat, double lon);
    void onFreePickFinished(bool ok, const QStringList& texts, const QStringList& titles, const QString& error, const QString& warning);
    void onGenerate();
    void onFetchFinished(bool success, const QString& content, const QString& errorMsg, const QString& title);

private:
    // --- state (mirrors the Python attributes) ---
    DataSourceSet m_ds;
    DataSource m_emptySource;          // stands in when no datasource files were found
    int m_sourceIdx = 0;
    QDateTime m_run;                   // selected run; 1700-01-01 means "none"
    QVector<QDateTime> m_runTimes;     // times currently listed in the run dropdown
    QDate m_calDate;
    bool m_hasLoc = false;
    MapPoint m_loc;
    QString m_dispName;
    std::vector<int> m_profIdx;
    bool m_selectFlag = false;
    bool m_hasConnection = true;

    // --- fetch state ---
    DataFetcher* m_fetcher = nullptr;
    FreePickFetcher* m_free = nullptr;     // gridded sources: ERA5 (CDS) and model point queries
    QProgressDialog* m_progress = nullptr;
    bool m_busy = false;
    size_t m_tries = 0;
    QString m_lastError;
    QStringList m_attemptLog;      // one line per failed outlet, shown when every source fails
    DsPoint m_fetchStn;                // station being fetched
    std::vector<int> m_outletOrder;    // outlets that have the profile, in fetch order
    QVector<int> m_fetchIdx;           // forecast list rows requested

    // --- per-outlet station listings (which stations really have data at m_run) ---
    enum class ListState { None, Pending, Ready, Failed };
    std::vector<ListState> m_listState;
    std::vector<QSet<QString>> m_listing;
    QVector<QPointer<QNetworkReply>> m_listReplies;
    QNetworkAccessManager* m_listMgr = nullptr;
    int m_listGen = 0;

    // --- widgets ---
    QLabel* m_typeLabel = nullptr;
    QLabel* m_dateLabel = nullptr;
    QLabel* m_mapLabel = nullptr;
    QLabel* m_runLabel = nullptr;
    QComboBox* m_modelDropdown = nullptr;
    QComboBox* m_mapDropdown = nullptr;
    QComboBox* m_runDropdown = nullptr;
    PickerCalendar* m_cal = nullptr;
    QListWidget* m_profileList = nullptr;
    QPushButton* m_allProfs = nullptr;
    QPushButton* m_button = nullptr;
    QPushButton* m_saveViewButton = nullptr;
    MapWidget* m_view = nullptr;
    QNetworkAccessManager* m_pingMgr = nullptr;

    void initSources();
    void initUI();
    static QComboBox* dropdownMenu(const QStringList& items, QWidget* parent);
    const DataSource& source() const {
        return m_ds.sources.empty() ? m_emptySource : m_ds.sources[static_cast<size_t>(m_sourceIdx)];
    }

    QDateTime defaultRun() const;
    static QString runText(const QDateTime& t);
    std::vector<const QSet<QString>*> listingPointers() const;
    void applyStations();
    void abortListings();

    void updateFromCal(bool updatedModel = false);
    void updateRunDropdown(bool updatedModel = false);
    void updateList();
    void refreshStations();
    void pingConnections();

    void startFetchAttempt();
    void finishFetch();
    void fail(const QString& message);
};

// Port of full_gui.Main: the "SHARPpy Sounding Picker" window.
class PickerWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit PickerWindow(QWidget* parent = nullptr);
    Picker* picker() const { return m_picker; }

signals:
    void profilesReady(const QVector<Profile>& profiles, const QStringList& titles);
    void fileRequested(const QString& path);
    void focusSkewRequested();
    void preferencesChanged(const AppPreferences& prefs);

protected:
    void keyPressEvent(QKeyEvent* e) override;

private:
    Picker* m_picker = nullptr;

    void createMenuBar();
    void openFile();
    void preferencesBox();
    void aboutBox();
};

} // namespace sharppy
