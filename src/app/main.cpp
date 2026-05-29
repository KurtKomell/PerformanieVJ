#include <QApplication>
#include <QPalette>
#include <QSurfaceFormat>
#include <QStyleFactory>

#include "AppUserPaths.h"
#include "MainWindow.h"

#include "render/maxine/MaxineFilterBackend.h"

namespace {

/// Cohesive dark “studio” skin: Fusion palette + light QSS polish (tabs, docks, inputs).
void applyPerformanieVjTheme(QApplication& app)
{
    if (QStyle* fusion = QStyleFactory::create(QStringLiteral("Fusion"))) {
        app.setStyle(fusion);
    }

    QPalette pal;
    const QColor windowBg(22, 24, 30);
    const QColor base(30, 34, 42);
    const QColor alt(38, 43, 54);
    const QColor text(235, 238, 245);
    const QColor muted(140, 145, 160);
    const QColor accent(91, 156, 245);
    const QColor button(42, 47, 58);

    pal.setColor(QPalette::Window, windowBg);
    pal.setColor(QPalette::WindowText, text);
    pal.setColor(QPalette::Base, base);
    pal.setColor(QPalette::AlternateBase, alt);
    pal.setColor(QPalette::ToolTipBase, QColor(40, 44, 56));
    pal.setColor(QPalette::ToolTipText, text);
    pal.setColor(QPalette::Text, text);
    pal.setColor(QPalette::Button, button);
    pal.setColor(QPalette::ButtonText, text);
    pal.setColor(QPalette::BrightText, QColor(255, 130, 130));
    pal.setColor(QPalette::Highlight, accent);
    pal.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    pal.setColor(QPalette::Link, QColor(125, 196, 255));
    pal.setColor(QPalette::LinkVisited, QColor(188, 154, 255));
    pal.setColor(QPalette::PlaceholderText, muted);
    pal.setColor(QPalette::Light, QColor(62, 68, 84));
    pal.setColor(QPalette::Midlight, QColor(52, 58, 72));
    pal.setColor(QPalette::Mid, QColor(36, 40, 50));
    pal.setColor(QPalette::Dark, QColor(18, 20, 26));
    pal.setColor(QPalette::Shadow, QColor(10, 11, 14));
    app.setPalette(pal);

    app.setStyleSheet(QStringLiteral(
        "QToolTip { color: #eceef3; background-color: #2c313c; border: 1px solid #454d60; padding: 6px; }"

        "QMenuBar { background-color: #1c1f26; border-bottom: 1px solid #343b4a; padding: 2px 4px; }"
        "QMenuBar::item { padding: 6px 12px; border-radius: 4px; }"
        "QMenuBar::item:selected { background-color: #343b4a; }"

        "QMenu { background-color: #252a33; border: 1px solid #3d4453; padding: 4px; }"
        "QMenu::item { padding: 6px 28px; border-radius: 4px; }"
        "QMenu::item:selected { background-color: #3d4f73; }"
        "QMenu::separator { height: 1px; background: #343b4a; margin: 4px 8px; }"

        "QStatusBar { background-color: #1c1f26; border-top: 1px solid #343b4a; }"
        "QStatusBar::item { border: none; }"

        "QDockWidget::title {"
        "  text-align: left;"
        "  background-color: #252a33;"
        "  padding: 8px 10px;"
        "  border: 1px solid #343b4a;"
        "  border-bottom: none;"
        "  border-top-left-radius: 6px;"
        "  border-top-right-radius: 6px;"
        "}"

        "QTabWidget::pane {"
        "  border: 1px solid #343b4a;"
        "  border-radius: 6px;"
        "  top: -1px;"
        "  background-color: #1e2229;"
        "}"

        "QTabBar::tab {"
        "  background-color: #252a33;"
        "  color: #b4bac8;"
        "  border: 1px solid #343b4a;"
        "  border-bottom-color: #343b4a;"
        "  border-top-left-radius: 6px;"
        "  border-top-right-radius: 6px;"
        "  min-width: 8ex;"
        "  padding: 8px 16px;"
        "  margin-right: 2px;"
        "}"
        "QTabBar::tab:selected {"
        "  background-color: #1e2229;"
        "  color: #eef1f7;"
        "  border-bottom-color: #1e2229;"
        "}"
        "QTabBar::tab:hover:!selected { background-color: #2c323d; color: #dce0e8; }"

        "QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox {"
        "  background-color: #262b35;"
        "  border: 1px solid #3d4453;"
        "  border-radius: 5px;"
        "  padding: 5px 8px;"
        "  selection-background-color: #5b9cf5;"
        "}"
        "QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus {"
        "  border: 1px solid #5b9cf5;"
        "}"

        "QAbstractItemView {"
        "  background-color: #1e2229;"
        "  alternate-background-color: #252a33;"
        "  border: 1px solid #343b4a;"
        "  border-radius: 6px;"
        "}"
        "QAbstractItemView::item:selected { background-color: #3d4f73; }"
        "QAbstractItemView::item:hover:!selected { background-color: #2c323d; }"

        "QScrollBar:vertical { width: 11px; margin: 0; background: #1a1d23; border-radius: 5px; }"
        "QScrollBar::handle:vertical {"
        "  min-height: 28px;"
        "  background-color: #3d4453;"
        "  border-radius: 5px;"
        "  margin: 2px;"
        "}"
        "QScrollBar::handle:vertical:hover { background-color: #4d566b; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"

        "QScrollBar:horizontal { height: 11px; margin: 0; background: #1a1d23; border-radius: 5px; }"
        "QScrollBar::handle:horizontal {"
        "  min-width: 28px;"
        "  background-color: #3d4453;"
        "  border-radius: 5px;"
        "  margin: 2px;"
        "}"
        "QScrollBar::handle:horizontal:hover { background-color: #4d566b; }"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }"

        "QSplitter::handle { background-color: #2a2f3a; }"
        "QSplitter::handle:hover { background-color: #3d4453; }"
        ));
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication::setOrganizationName("PerformanieVJ");
    QCoreApplication::setApplicationName("PerformanieVJ");
    QCoreApplication::setApplicationVersion("0.1.0");
    pvj::app::configureUserStorage();

    // Request a modern OpenGL context up front so QRhi/QOpenGLWidget can use it
    // consistently across platforms once the render pipeline (M5) is wired in.
    QSurfaceFormat fmt;
    fmt.setVersion(3, 3);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setDepthBufferSize(24);
    fmt.setStencilBufferSize(8);
    fmt.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    QSurfaceFormat::setDefaultFormat(fmt);

    QApplication app(argc, argv);
    applyPerformanieVjTheme(app);

    pvj::render::initializeMaxineRuntime();

    pvj::app::MainWindow window;
    window.show();

    return app.exec();
}
