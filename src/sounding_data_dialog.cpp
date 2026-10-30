#include "sharppy/sounding_data_dialog.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFileDialog>
#include <QApplication>
#include <QClipboard>
#include <QTextStream>
#include <QFile>
#include <cmath>

namespace sharppy {

SoundingDataDialog::SoundingDataDialog(const Profile& prof, const QString& title, QWidget* parent)
    : QDialog(parent), m_prof(prof), m_title(title) {

    setWindowTitle(QString("Sounding Level Data: %1").arg(title));
    resize(860, 560);
    setupUI();
    populateTable();
}

void SoundingDataDialog::setupUI() {
    setStyleSheet(
        "QDialog { background-color: #12161f; color: #e2e8f0; font-family: Monospace, sans-serif; }"
        "QLabel { color: #94a3b8; font-size: 11px; }"
        "QLineEdit { background-color: #1e293b; color: #f8fafc; border: 1px solid #475569; border-radius: 4px; padding: 4px; }"
        "QTableWidget { background-color: #0f172a; color: #f1f5f9; border: 1px solid #334155; gridline-color: #1e293b; selection-background-color: #0284c7; font-size: 11px; }"
        "QHeaderView::section { background-color: #1e293b; color: #38bdf8; font-weight: bold; padding: 4px; border: 1px solid #334155; }"
        "QPushButton { background-color: #0284c7; color: #ffffff; border: none; border-radius: 4px; padding: 6px 14px; font-weight: bold; font-size: 11px; }"
        "QPushButton:hover { background-color: #38bdf8; color: #0f172a; }"
        "QPushButton#closeBtn { background-color: #334155; color: #e2e8f0; }"
        "QPushButton#closeBtn:hover { background-color: #475569; }"
    );

    QVBoxLayout* root = new QVBoxLayout(this);
    root->setSpacing(10);
    root->setContentsMargins(12, 12, 12, 12);

    QHBoxLayout* topLayout = new QHBoxLayout();
    QLabel* infoLbl = new QLabel(QString("Profile: <b>%1</b> (%2 levels)").arg(m_title).arg(m_prof.size()), this);
    infoLbl->setStyleSheet("color: #38bdf8; font-size: 12px;");

    QLabel* filterLbl = new QLabel("Filter Pres (e.g. 850 or 500-200):", this);
    m_filterEdit = new QLineEdit(this);
    m_filterEdit->setPlaceholderText("Filter by mb...");
    m_filterEdit->setFixedWidth(160);
    connect(m_filterEdit, &QLineEdit::textChanged, this, &SoundingDataDialog::onFilterChanged);

    topLayout->addWidget(infoLbl);
    topLayout->addStretch(1);
    topLayout->addWidget(filterLbl);
    topLayout->addWidget(m_filterEdit);
    root->addLayout(topLayout);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(12);
    m_table->setHorizontalHeaderLabels({
        "#", "Pres (mb)", "Hght (m)", "Hght AGL", "Temp (°C)", "Dewpt (°C)",
        "RH (%)", "Dir (°)", "Spd (kt)", "U (kt)", "V (kt)", "Theta (K)"
    });
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_table->setAlternatingRowColors(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    root->addWidget(m_table);

    QHBoxLayout* btmLayout = new QHBoxLayout();
    m_statusLabel = new QLabel(QString("Total levels: %1").arg(m_prof.size()), this);

    QPushButton* copyBtn = new QPushButton("Copy Table (TSV)", this);
    connect(copyBtn, &QPushButton::clicked, this, &SoundingDataDialog::onCopyClicked);

    QPushButton* csvBtn = new QPushButton("Export CSV...", this);
    connect(csvBtn, &QPushButton::clicked, this, &SoundingDataDialog::onExportCsvClicked);

    QPushButton* closeBtn = new QPushButton("Close", this);
    closeBtn->setObjectName("closeBtn");
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    btmLayout->addWidget(m_statusLabel);
    btmLayout->addStretch(1);
    btmLayout->addWidget(copyBtn);
    btmLayout->addWidget(csvBtn);
    btmLayout->addWidget(closeBtn);
    root->addLayout(btmLayout);
}

void SoundingDataDialog::populateTable(double minPres, double maxPres) {
    m_table->setRowCount(0);
    if (m_prof.empty()) return;

    double sfc_hght = m_prof.hght.empty() ? 0.0 : m_prof.hght.front();
    int row = 0;

    for (size_t i = 0; i < m_prof.size(); ++i) {
        double p = m_prof.pres[i];
        if (p < minPres || p > maxPres) continue;

        double z = m_prof.hght[i];
        double zagl = z - sfc_hght;
        double t = m_prof.temp[i];
        double td = m_prof.dwpt[i];
        double dir = m_prof.wdir[i];
        double spd = m_prof.wspd[i];
        double u = m_prof.u[i];
        double v = m_prof.v[i];

        double theta = (t + 273.15) * std::pow(1000.0 / p, 0.286);
        double es = 6.112 * std::exp((17.67 * t) / (t + 243.5));
        double e = 6.112 * std::exp((17.67 * td) / (td + 243.5));
        double rh = std::clamp((e / es) * 100.0, 0.0, 100.0);

        m_table->insertRow(row);

        auto setCell = [this, row](int col, const QString& txt, Qt::Alignment align = Qt::AlignRight) {
            QTableWidgetItem* item = new QTableWidgetItem(txt);
            item->setTextAlignment(align | Qt::AlignVCenter);
            m_table->setItem(row, col, item);
        };

        setCell(0, QString::number(i + 1), Qt::AlignCenter);
        setCell(1, QString::number(p, 'f', 1));
        setCell(2, QString::number((int)std::round(z)));
        setCell(3, QString::number((int)std::round(zagl)));
        setCell(4, QString::number(t, 'f', 1));
        setCell(5, QString::number(td, 'f', 1));
        setCell(6, QString::number((int)std::round(rh)));
        setCell(7, QString("%1°").arg((int)std::round(dir), 3, 10, QChar('0')));
        setCell(8, QString::number((int)std::round(spd)));
        setCell(9, QString::number(u, 'f', 1));
        setCell(10, QString::number(v, 'f', 1));
        setCell(11, QString::number(theta, 'f', 1));

        row++;
    }

    m_statusLabel->setText(QString("Showing %1 of %2 levels").arg(row).arg(m_prof.size()));
}

void SoundingDataDialog::onFilterChanged(const QString& text) {
    QString t = text.trimmed();
    if (t.isEmpty()) {
        populateTable(0.0, 1100.0);
        return;
    }

    if (t.contains("-")) {
        QStringList parts = t.split("-");
        if (parts.size() == 2) {
            double p1 = parts[0].toDouble();
            double p2 = parts[1].toDouble();
            populateTable(std::min(p1, p2), std::max(p1, p2));
            return;
        }
    }

    double center = t.toDouble();
    if (center > 0) {
        populateTable(center - 50.0, center + 50.0);
    }
}

void SoundingDataDialog::onCopyClicked() {
    QString buffer;
    QTextStream ts(&buffer);

    // Header
    for (int c = 0; c < m_table->columnCount(); ++c) {
        ts << m_table->horizontalHeaderItem(c)->text() << (c == m_table->columnCount() - 1 ? "\n" : "\t");
    }

    // Rows
    for (int r = 0; r < m_table->rowCount(); ++r) {
        for (int c = 0; c < m_table->columnCount(); ++c) {
            QTableWidgetItem* item = m_table->item(r, c);
            ts << (item ? item->text() : "") << (c == m_table->columnCount() - 1 ? "\n" : "\t");
        }
    }

    QApplication::clipboard()->setText(buffer);
    m_statusLabel->setText("Copied table to clipboard!");
    m_statusLabel->setStyleSheet("color: #38bdf8; font-weight: bold;");
}

void SoundingDataDialog::onExportCsvClicked() {
    QString f = QFileDialog::getSaveFileName(this, "Export Sounding CSV", "sounding_levels.csv", "CSV Files (*.csv)");
    if (f.isEmpty()) return;

    QFile file(f);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream ts(&file);
        // Header
        for (int c = 0; c < m_table->columnCount(); ++c) {
            ts << "\"" << m_table->horizontalHeaderItem(c)->text() << "\"" << (c == m_table->columnCount() - 1 ? "\n" : ",");
        }
        // Rows
        for (int r = 0; r < m_table->rowCount(); ++r) {
            for (int c = 0; c < m_table->columnCount(); ++c) {
                QTableWidgetItem* item = m_table->item(r, c);
                ts << "\"" << (item ? item->text() : "") << "\"" << (c == m_table->columnCount() - 1 ? "\n" : ",");
            }
        }
        file.close();
        m_statusLabel->setText(QString("Saved CSV to %1").arg(f));
        m_statusLabel->setStyleSheet("color: #38bdf8; font-weight: bold;");
    }
}

} // namespace sharppy
