#pragma once

#include <QDialog>
#include <QComboBox>
#include <QLineEdit>
#include <QListWidget>
#include <QDateEdit>
#include <QSpinBox>
#include <QProgressBar>
#include <QLabel>
#include <QPushButton>
#include "sharppy/data_fetcher.hpp"
#include "sharppy/profile.hpp"

namespace sharppy {

class FetchSoundingDialog : public QDialog {
    Q_OBJECT

public:
    explicit FetchSoundingDialog(QWidget* parent = nullptr);

    const Profile& getProfile() const { return m_profile; }
    const QString& getTitle() const { return m_title; }

signals:
    void soundingLoaded(const Profile& prof, const QString& title);

private slots:
    void onFilterChanged(const QString& text);
    void onStationSelected(QListWidgetItem* item);
    void onCycleChanged(int index);
    void onFetchClicked();
    void onCancelClicked();
    void onFetchProgress(qint64 received, qint64 total);
    void onFetchFinished(bool success, const QString& content, const QString& errorMsg, const QString& title);

private:
    DataFetcher* m_fetcher;
    QComboBox* m_sourceCombo;
    QLineEdit* m_searchEdit;
    QListWidget* m_stationList;
    QDateEdit* m_dateEdit;
    QComboBox* m_cycleCombo;
    QSpinBox* m_customHourSpin;
    QProgressBar* m_progressBar;
    QLabel* m_statusLabel;
    QPushButton* m_fetchBtn;
    QPushButton* m_cancelBtn;

    Profile m_profile;
    QString m_title;

    void setupUI();
    void populateStations(const QString& filter = "");
    SoundingSource getCurrentSource() const;
    int getCurrentCycleHour() const;
    std::string getSelectedStationId() const;
};

} // namespace sharppy
