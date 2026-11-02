#include "ui/Theme.h"
#include <QApplication>
#include <QFile>
#include <QFont>

bool applyTheme(QApplication& app)
{
    Q_INIT_RESOURCE(pixelguard_theme);
    QFile file(":/resources/theme.qss");
    if (!file.open(QIODevice::ReadOnly)) return false;
    app.setStyle("Fusion");
    app.setFont(QFont(QStringLiteral("Microsoft YaHei UI"), 10));
    app.setStyleSheet(QString::fromUtf8(file.readAll()));
    return true;
}
