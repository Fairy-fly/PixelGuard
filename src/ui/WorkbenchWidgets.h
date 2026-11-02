#pragma once
#include "ui/DesignTokens.h"
#include <QIcon>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

// Native QLabel semantics, with paint-time elision instead of layout growth.
class ElidedLabel : public QLabel {
public:
    using QLabel::QLabel;
    QSize minimumSizeHint() const override { return {0, fontMetrics().height()}; }
    QSize sizeHint() const override { return {120, fontMetrics().height()}; }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setPen(palette().color(isEnabled() ? QPalette::Active : QPalette::Disabled, QPalette::WindowText));
        p.setFont(font());
        p.drawText(contentsRect(), Qt::AlignVCenter | Qt::AlignLeft,
                   fontMetrics().elidedText(text(), Qt::ElideMiddle, contentsRect().width()));
    }
};

enum class Glyph { Import, Expand, Fit, Actual, Close, Export, Compare };
inline QIcon workbenchIcon(Glyph glyph, const QColor& color = QColor(design::Secondary))
{
    QPixmap pixmap(32, 32);
    pixmap.setDevicePixelRatio(2);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(color, 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    if (glyph == Glyph::Close) {
        p.drawLine(QPointF(4, 4), QPointF(12, 12)); p.drawLine(QPointF(12, 4), QPointF(4, 12));
    } else if (glyph == Glyph::Import || glyph == Glyph::Export) {
        p.drawLine(QPointF(3, 10), QPointF(3, 13)); p.drawLine(QPointF(3, 13), QPointF(13, 13));
        p.drawLine(QPointF(13, 13), QPointF(13, 10)); p.drawLine(QPointF(8, 2), QPointF(8, 10));
        const int tip = glyph == Glyph::Import ? 10 : 2;
        const int tail = glyph == Glyph::Import ? 7 : 5;
        p.drawLine(QPointF(5, tail), QPointF(8, tip)); p.drawLine(QPointF(8, tip), QPointF(11, tail));
    } else if (glyph == Glyph::Actual) {
        p.drawRect(QRectF(3, 3, 10, 10)); p.drawPoint(QPointF(8, 8));
    } else if (glyph == Glyph::Compare) {
        p.drawRoundedRect(QRectF(2, 3, 12, 10), 1, 1); p.drawLine(QPointF(8, 3), QPointF(8, 13));
        p.drawLine(QPointF(4.5, 6), QPointF(6, 8)); p.drawLine(QPointF(6, 8), QPointF(4.5, 10));
        p.drawLine(QPointF(11.5, 6), QPointF(10, 8)); p.drawLine(QPointF(10, 8), QPointF(11.5, 10));
    } else {
        for (const QPointF& c : {QPointF(3,3), QPointF(13,3), QPointF(3,13), QPointF(13,13)}) {
            p.drawLine(c, c + QPointF(c.x() < 8 ? 3 : -3, 0));
            p.drawLine(c, c + QPointF(0, c.y() < 8 ? 3 : -3));
        }
        if (glyph == Glyph::Fit) p.drawRect(QRectF(6, 6, 4, 4));
    }
    return QIcon(pixmap);
}
