#pragma once

#include <QWidget>
#include <QPainter>
#include "sharppy/params.hpp"

namespace sharppy {

class HazardWidget : public QWidget {
    Q_OBJECT

public:
    explicit HazardWidget(QWidget* parent = nullptr);
    void setParams(const SevereParams& params);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    SevereParams m_params;
    bool m_hasData = false;
};

} // namespace sharppy