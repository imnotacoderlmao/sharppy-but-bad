#include <QApplication>
#include <QSurfaceFormat>
#include <QPointer>
#include <QString>
#include "sharppy/picker_window.hpp"
#include "sharppy/spc_window.hpp"

namespace {

QPointer<sharppy::SPCWindow> g_lastWindow;

// Each generated sounding opens in its own SPCWindow; the picker stays around so
// more can be pulled up, and File > Sounding Picker... brings it back to the front.
sharppy::SPCWindow* newSpcWindow(sharppy::PickerWindow* picker) {
    auto* win = new sharppy::SPCWindow;
    win->setAttribute(Qt::WA_DeleteOnClose);
    QObject::connect(win, &sharppy::SPCWindow::pickerRequested, picker, [picker]() {
        picker->show();
        picker->raise();
        picker->activateWindow();
    });
    g_lastWindow = win;
    return win;
}

} // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setOrganizationName("sharppy");
    app.setApplicationName("cppsharp");

    sharppy::PickerWindow picker;

    QObject::connect(&picker, &sharppy::PickerWindow::profilesReady, &picker,
                     [&picker](const QVector<sharppy::Profile>& profs, const QStringList& titles) {
                         auto* win = newSpcWindow(&picker);
                         win->show();
                         win->loadProfileCollection(profs, titles);
                     });

    QObject::connect(&picker, &sharppy::PickerWindow::fileRequested, &picker,
                     [&picker](const QString& path) {
                         auto* win = newSpcWindow(&picker);
                         win->show();
                         if (!win->loadSoundingFile(path.toStdString())) win->close();
                     });

    QObject::connect(&picker, &sharppy::PickerWindow::focusSkewRequested, &picker, []() {
        if (g_lastWindow) {
            g_lastWindow->activateWindow();
            g_lastWindow->raise();
            g_lastWindow->setFocus();
        }
    });

    if (argc > 1) {
        // Opened with a file: go straight to the analysis window; the picker stays
        // hidden until requested.
        auto* win = newSpcWindow(&picker);
        win->show();
        if (!win->loadSoundingFile(argv[1])) {
            win->close();
            picker.show();
        }
    } else {
        picker.show();
    }

    return app.exec();
}
