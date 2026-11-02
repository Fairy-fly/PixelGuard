#include "ui/Theme.h"
#include <QApplication>
#include <QFile>
#include <QFont>
#include <QPalette>
#include "ui/DesignTokens.h"

bool applyTheme(QApplication& app)
{
    Q_INIT_RESOURCE(pixelguard_theme);
    QFile file(":/resources/theme.qss");
    if (!file.open(QIODevice::ReadOnly)) return false;
    app.setStyle("Fusion");
    QFont font;
    font.setFamilies({QStringLiteral("Microsoft YaHei UI"), QStringLiteral("Microsoft YaHei"), QStringLiteral("Segoe UI")});
    font.setPixelSize(design::BodyFont);
    app.setFont(font);
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(design::Background));
    palette.setColor(QPalette::WindowText, QColor(design::Text));
    palette.setColor(QPalette::Base, QColor(design::Raised));
    palette.setColor(QPalette::Text, QColor(design::Text));
    palette.setColor(QPalette::Highlight, QColor(design::Accent));
    palette.setColor(QPalette::HighlightedText, QColor(design::Raised));
    app.setPalette(palette);
    QString qss = QString::fromUtf8(file.readAll());
    for (const auto& token : design::colors)
        qss.replace("@" + QString::fromLatin1(token.first) + "@", QString::fromLatin1(token.second));
    const std::pair<const char*, int> dimensions[] = {
        {"Unit", design::Unit}, {"Small", design::Small}, {"Gap", design::Gap}, {"Margin", design::Margin},
        {"Radius", design::Radius}, {"BodyFont", design::BodyFont}, {"SmallFont", design::SmallFont},
        {"TitleFont", design::TitleFont}, {"MetricFont", design::MetricFont}
    };
    for (const auto& token : dimensions)
        qss.replace("@" + QString::fromLatin1(token.first) + "@", QString::number(token.second));
    if (qss.contains('@')) return false;
    app.setStyleSheet(qss);
    return true;
}
