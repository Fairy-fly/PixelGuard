#include "ui/MainWindow.h"
#include "ui/Theme.h"
#include <QApplication>
#include <QDir>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("PixelGuard");
    QApplication::setApplicationVersion("0.2.0");
    if (!applyTheme(app)) {
        QMessageBox::critical(nullptr, QStringLiteral("启动失败"), QStringLiteral("无法加载界面主题资源。"));
        return 1;
    }
    MainWindow window;
    window.show();
    if (app.arguments().contains("--smoke-test")) {
        QTimer::singleShot(800, &app, [&] {
            const auto* detect = window.findChild<QPushButton*>("detectButton");
            const auto* save = window.findChild<QPushButton*>("saveButton");
            const bool screenshotSaved = window.grab().save(
                QDir(QCoreApplication::applicationDirPath()).filePath("startup-smoke.png"));
            app.exit(window.isVisible() && detect && !detect->isEnabled() && save &&
                     !save->isEnabled() && screenshotSaved ? 0 : 1);
        });
    }
    return app.exec();
}
