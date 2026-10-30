#include "sharppy/fetch_dialog.hpp"
#include "sharppy/spc.hpp"
#include "sharppy/uwyo.hpp"
#include "sharppy/bufkit.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QMessageBox>

namespace sharppy {

FetchSoundingDialog::FetchSoundingDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Download Sounding / Real-Time Data");
    resize(640, 520);
    m_fetcher = new DataFetcher(this);

    connect(m_fetcher, &DataFetcher::progressChanged, this, &FetchSoundingDialog::onFetchProgress);
    connect(m_fetcher, &DataFetcher::fetchFinished, this, &FetchSoundingDialog::onFetchFinished);

    setupUI();
    populateStations();
}

void FetchSoundingDialog::setupUI() {
    setStyleSheet(
        "QDialog { background-color: #12161f; color: #e2e8f0; font-family: Monospace, sans-serif; }"
        "QGroupBox { border: 1px solid #334155; border-radius: 6px; margin-top: 12px; font-weight: bold; color: #38bdf8; padding-top: 14px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }"
        "QLabel { color: #cbd5e1; font-size: 11px; }"
        "QLineEdit, QComboBox, QDateEdit, QSpinBox, QListWidget { background-color: #1e293b; color: #f8fafc; border: 1px solid #475569; border-radius: 4px; padding: 4px; selection-background-color: #0284c7; }"
        "QListWidget::item:hover { background-color: #334155; }"
        "QListWidget::item:selected { background-color: #0284c7; color: #ffffff; }"
        "QPushButton { background-color: #0284c7; color: #ffffff; border: none; border-radius: 4px; padding: 6px 16px; font-weight: bold; font-size: 11px; }"
        "QPushButton:hover { background-color: #38bdf8; color: #0f172a; }"
        "QPushButton#cancelBtn { background-color: #334155; color: #e2e8f0; }"
        "QPushButton#cancelBtn:hover { background-color: #475569; }"
        "QProgressBar { border: 1px solid #475569; border-radius: 4px; text-align: center; background-color: #1e293b; color: #ffffff; height: 16px; }"
        "QProgressBar::chunk { background-color: #38bdf8; }"
    );

    QVBoxLayout* root = new QVBoxLayout(this);
    root->setSpacing(10);
    root->setContentsMargins(14, 14, 14, 14);

    // Source Selection Group
    QGroupBox* srcGroup = new QGroupBox("1. Sounding Data Source", this);
    QHBoxLayout* srcLayout = new QHBoxLayout(srcGroup);
    m_sourceCombo = new QComboBox(srcGroup);
    m_sourceCombo->addItem("SPC Observed Soundings (.txt archive & realtime)", (int)SoundingSource::SPC_OBSERVED);
    m_sourceCombo->addItem("Iowa State Mesonet (IEM) Upper-Air Archive", (int)SoundingSource::IEM_RAOB);
    m_sourceCombo->addItem("University of Wyoming (UWyo) Upper-Air Archive", (int)SoundingSource::UWYO_OBSERVED);
    m_sourceCombo->addItem("RAP Forecast Model Profile (IEM BUFKIT)", (int)SoundingSource::BUFKIT_RAP);
    m_sourceCombo->addItem("HRRR Forecast Model Profile (IEM BUFKIT)", (int)SoundingSource::BUFKIT_HRRR);
    srcLayout->addWidget(m_sourceCombo);
    root->addWidget(srcGroup);

    // Station Picker Group
    QGroupBox* stnGroup = new QGroupBox("2. Observation Station", this);
    QVBoxLayout* stnLayout = new QVBoxLayout(stnGroup);

    QHBoxLayout* filterLayout = new QHBoxLayout();
    QLabel* searchLbl = new QLabel("Filter Station:", stnGroup);
    m_searchEdit = new QLineEdit(stnGroup);
    m_searchEdit->setPlaceholderText("Type Station ID, Name, or State (e.g. OUN, Norman, OK)...");
    connect(m_searchEdit, &QLineEdit::textChanged, this, &FetchSoundingDialog::onFilterChanged);
    filterLayout->addWidget(searchLbl);
    filterLayout->addWidget(m_searchEdit);
    stnLayout->addLayout(filterLayout);

    m_stationList = new QListWidget(stnGroup);
    m_stationList->setFixedHeight(140);
    connect(m_stationList, &QListWidget::itemClicked, this, &FetchSoundingDialog::onStationSelected);
    stnLayout->addWidget(m_stationList);
    root->addWidget(stnGroup);

    // Date and Cycle Group
    QGroupBox* timeGroup = new QGroupBox("3. Date & Run Cycle", this);
    QGridLayout* timeGrid = new QGridLayout(timeGroup);

    QLabel* dateLbl = new QLabel("Date (UTC):", timeGroup);
    m_dateEdit = new QDateEdit(timeGroup);
    m_dateEdit->setCalendarPopup(true);
    m_dateEdit->setDate(QDate::currentDate()); // default to today
    m_dateEdit->setDisplayFormat("yyyy-MM-dd");

    QLabel* cycleLbl = new QLabel("Cycle Time:", timeGroup);
    m_cycleCombo = new QComboBox(timeGroup);
    m_cycleCombo->addItem("00Z (00:00 UTC Evening)", 0);
    m_cycleCombo->addItem("12Z (12:00 UTC Morning)", 12);
    m_cycleCombo->addItem("Custom Hour...", -1);
    m_cycleCombo->setCurrentIndex(0); // 00Z default

    m_customHourSpin = new QSpinBox(timeGroup);
    m_customHourSpin->setRange(0, 23);
    m_customHourSpin->setSuffix("Z");
    m_customHourSpin->setValue(18);
    m_customHourSpin->setEnabled(false);

    connect(m_cycleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &FetchSoundingDialog::onCycleChanged);

    timeGrid->addWidget(dateLbl, 0, 0);
    timeGrid->addWidget(m_dateEdit, 0, 1);
    timeGrid->addWidget(cycleLbl, 0, 2);
    timeGrid->addWidget(m_cycleCombo, 0, 3);
    timeGrid->addWidget(m_customHourSpin, 0, 4);
    root->addWidget(timeGroup);

    // Progress & Status
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    m_progressBar->setVisible(false);
    root->addWidget(m_progressBar);

    m_statusLabel = new QLabel("Select data source, station, date, and click 'Fetch Sounding'.", this);
    m_statusLabel->setStyleSheet("color: #94a3b8; font-size: 11px;");
    root->addWidget(m_statusLabel);

    // Buttons
    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->addStretch(1);

    m_cancelBtn = new QPushButton("Cancel", this);
    m_cancelBtn->setObjectName("cancelBtn");
    connect(m_cancelBtn, &QPushButton::clicked, this, &FetchSoundingDialog::onCancelClicked);

    m_fetchBtn = new QPushButton("Fetch Sounding", this);
    connect(m_fetchBtn, &QPushButton::clicked, this, &FetchSoundingDialog::onFetchClicked);

    btnLayout->addWidget(m_cancelBtn);
    btnLayout->addWidget(m_fetchBtn);
    root->addLayout(btnLayout);
}

void FetchSoundingDialog::populateStations(const QString& filter) {
    m_stationList->clear();
    const auto& stations = DataFetcher::getKnownStations();
    QString f = filter.trimmed().toLower();

    int selectRow = -1;
    int curRow = 0;

    for (const auto& s : stations) {
        QString itemText = QString("%1 - %2, %3 (%4°N, %5°W, %6m)")
            .arg(QString::fromStdString(s.id), -4)
            .arg(QString::fromStdString(s.name))
            .arg(QString::fromStdString(s.state))
            .arg(s.lat, 0, 'f', 2)
            .arg(-s.lon, 0, 'f', 2)
            .arg(s.elev_m, 0, 'f', 0);

        if (f.isEmpty() ||
            QString::fromStdString(s.id).toLower().contains(f) ||
            QString::fromStdString(s.name).toLower().contains(f) ||
            QString::fromStdString(s.state).toLower().contains(f) ||
            QString::fromStdString(s.synop).contains(f)) {

            QListWidgetItem* item = new QListWidgetItem(itemText, m_stationList);
            item->setData(Qt::UserRole, QString::fromStdString(s.id));

            if (s.id == "OUN" && selectRow < 0) {
                selectRow = curRow;
            }
            curRow++;
        }
    }

    if (m_stationList->count() > 0) {
        m_stationList->setCurrentRow(selectRow >= 0 ? selectRow : 0);
    }
}

void FetchSoundingDialog::onFilterChanged(const QString& text) {
    populateStations(text);
}

void FetchSoundingDialog::onStationSelected(QListWidgetItem* item) {
    Q_UNUSED(item);
}

void FetchSoundingDialog::onCycleChanged(int index) {
    int hour = m_cycleCombo->itemData(index).toInt();
    m_customHourSpin->setEnabled(hour < 0);
}

SoundingSource FetchSoundingDialog::getCurrentSource() const {
    return (SoundingSource)m_sourceCombo->currentData().toInt();
}

int FetchSoundingDialog::getCurrentCycleHour() const {
    int hour = m_cycleCombo->currentData().toInt();
    if (hour < 0) {
        return m_customHourSpin->value();
    }
    return hour;
}

std::string FetchSoundingDialog::getSelectedStationId() const {
    QListWidgetItem* item = m_stationList->currentItem();
    if (item) {
        return item->data(Qt::UserRole).toString().toStdString();
    }
    return "OUN";
}

void FetchSoundingDialog::onFetchClicked() {
    std::string stationId = getSelectedStationId();
    if (stationId.empty()) {
        m_statusLabel->setText("Please select a station from the list.");
        m_statusLabel->setStyleSheet("color: #ef4444; font-weight: bold;");
        return;
    }

    SoundingSource src = getCurrentSource();
    QDate dt = m_dateEdit->date();
    int cycle = getCurrentCycleHour();

    m_progressBar->setVisible(true);
    m_progressBar->setRange(0, 0); // Indeterminate pulsing
    m_fetchBtn->setEnabled(false);
    m_cancelBtn->setText("Cancel");

    m_statusLabel->setText(QString("Connecting to server and fetching %1 for %2 %3Z...")
                               .arg(QString::fromStdString(stationId), dt.toString("yyyy-MM-dd"), QString::number(cycle)));
    m_statusLabel->setStyleSheet("color: #38bdf8; font-weight: normal;");

    m_fetcher->fetchSounding(src, stationId, dt, cycle);
}

void FetchSoundingDialog::onCancelClicked() {
    m_fetcher->cancel();
    reject();
}

void FetchSoundingDialog::onFetchProgress(qint64 received, qint64 total) {
    if (total > 0) {
        m_progressBar->setRange(0, (int)total);
        m_progressBar->setValue((int)received);
        m_statusLabel->setText(QString("Downloading sounding: %1 of %2 bytes...").arg(received).arg(total));
    }
}

void FetchSoundingDialog::onFetchFinished(bool success, const QString& content, const QString& errorMsg, const QString& title) {
    m_progressBar->setVisible(false);
    m_fetchBtn->setEnabled(true);

    if (!success) {
        m_statusLabel->setText(QString("Error fetching sounding: %1").arg(errorMsg));
        m_statusLabel->setStyleSheet("color: #ef4444; font-weight: bold;");
        return;
    }

    m_statusLabel->setText("Parsing sounding data...");
    m_statusLabel->setStyleSheet("color: #38bdf8; font-weight: bold;");

    std::string text = content.toStdString();
    SoundingMetadata meta;

    // 1. Try SPC format
    if (SpcParser::parse_string(text, m_profile, meta)) {
        m_title = QString::fromStdString(meta.title.empty() ? title.toStdString() : meta.title);
        emit soundingLoaded(m_profile, m_title);
        accept();
        return;
    }

    // 2. Try BUFKIT format
    std::vector<BufkitRecord> buf_recs;
    if (BufkitParser::parse_string(text, buf_recs) && !buf_recs.empty()) {
        m_profile = buf_recs.front().profile;
        m_title = QString("%1 %2 %3").arg(QString::fromStdString(buf_recs.front().station_id),
                                           QString::fromStdString(buf_recs.front().valid_time),
                                           title);
        emit soundingLoaded(m_profile, m_title);
        accept();
        return;
    }

    // 3. Try UWyo format
    if (UwyoParser::parse_string(text, m_profile, meta)) {
        m_title = QString::fromStdString(meta.title.empty() ? title.toStdString() : meta.title);
        emit soundingLoaded(m_profile, m_title);
        accept();
        return;
    }

    m_statusLabel->setText("Downloaded file could not be parsed as SPC, BUFKIT, or UWyo sounding.");
    m_statusLabel->setStyleSheet("color: #ef4444; font-weight: bold;");
}

} // namespace sharppy
