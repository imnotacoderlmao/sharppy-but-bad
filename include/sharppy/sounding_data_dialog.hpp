#pragma once

#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include "sharppy/profile.hpp"

namespace sharppy {

class SoundingDataDialog : public QDialog {
    Q_OBJECT

public:
    explicit SoundingDataDialog(const Profile& prof, const QString& title, QWidget* parent = nullptr);

private slots:
    void onCopyClicked();
    void onExportCsvClicked();
    void onFilterChanged(const QString& text);

private:
    Profile m_prof;
    QString m_title;
    QTableWidget* m_table;
    QLineEdit* m_filterEdit;
    QLabel* m_statusLabel;

    void setupUI();
    void populateTable(double minPres = 0.0, double maxPres = 1100.0);
};

} // namespace sharppy
