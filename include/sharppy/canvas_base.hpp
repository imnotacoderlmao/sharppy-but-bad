#pragma once
// Base class of the two big interactive panels (skew-T, hodograph). With SHARPPY_OPENGL they are QOpenGLWidgets, so their
// QPainter drawing runs on Qt's GL paint engine (GPU) instead of the software rasterizer; without it they are plain QWidgets.
// Build switch: CMake -DSHARPPY_OPENGL=ON|OFF, xmake `xmake f --opengl=y|n`.
#ifdef SHARPPY_OPENGL
#include <QOpenGLWidget>
#include <QGuiApplication>
#include <QScreen>
#include <QWindow>
namespace sharppy { using CanvasBase = QOpenGLWidget; }
#define SHARPPY_PAINT_HOOK(Class)                                              \
    void paintGL() override {                                                   \
        static const bool dbg = qEnvironmentVariableIntValue("SHARPPY_GL_DEBUG") != 0;                                       \
        if (dbg) qInfo("GL canvas %dx%d  dpr=%.3f  screen dpr=%.3f  logicalDpi=%.1f  fbo=%dx%d", width(), height(), devicePixelRatioF(), \
                       window() && window()->windowHandle() ? window()->windowHandle()->devicePixelRatio() : -1.0,           \
                       logicalDpiX() * 1.0, int(width() * devicePixelRatioF()), int(height() * devicePixelRatioF()));          \
        QPaintEvent e(rect()); paintContents(&e);                               \
    }                                                                           \
    /* The GL paint device reports its own DPI, which rescaled every point-sized font. Report the screen's, as a QWidget does. */ \
    int metric(QPaintDevice::PaintDeviceMetric m) const override {              \
        QScreen* sc = window() && window()->windowHandle() ? window()->windowHandle()->screen() : QGuiApplication::primaryScreen(); \
        if (sc) switch (m) {                                                    \
            case QPaintDevice::PdmDpiX: return qRound(sc->logicalDotsPerInchX()); \
            case QPaintDevice::PdmDpiY: return qRound(sc->logicalDotsPerInchY()); \
            case QPaintDevice::PdmPhysicalDpiX: return qRound(sc->physicalDotsPerInchX()); \
            case QPaintDevice::PdmPhysicalDpiY: return qRound(sc->physicalDotsPerInchY()); \
            default: break; }                                                   \
        return QOpenGLWidget::metric(m);                                        \
    }                                                                           \
    void paintContents(QPaintEvent* event);
#else
#include <QWidget>
namespace sharppy { using CanvasBase = QWidget; }
#define SHARPPY_PAINT_HOOK(Class)                                              \
    void paintEvent(QPaintEvent* e) override { paintContents(e); }              \
    void paintContents(QPaintEvent* event);
#endif
