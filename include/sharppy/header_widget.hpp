#pragma once

#include <QWidget>
#include <QPainter>
#include <string>

namespace sharppy {

class HeaderWidget : public QWidget {
    Q_OBJECT

public:
    explicit HeaderWidget(QWidget* parent = nullptr);

    void setText(const std::string& title, size_t num_levels);
    void setEditMode(bool enabled);
    void setModified(bool is_modified);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    std::string m_title;
    size_t m_levels = 0;
    bool m_editMode = false;
    bool m_isModified = false;
};

} // namespace sharppy