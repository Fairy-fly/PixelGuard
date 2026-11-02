#include "ui/ImagePreview.h"
#include <QPainter>
#include "ui/DesignTokens.h"
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPainterPath>
#include "ui/ImageViewer.h"

ImagePreview::ImagePreview(const QString& placeholder, QWidget* parent)
    : QWidget(parent), placeholder_(placeholder)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(160, 80);
    setAccessibleName(placeholder);
}
void ImagePreview::setImage(const QImage& image, const QString& name)
{
    if (dialog_) { dialog_->close(); dialog_.clear(); }
    image_ = image;
    name_ = name;
    setCursor(image.isNull() ? Qt::ArrowCursor : Qt::PointingHandCursor);
    setFocusPolicy(image.isNull() ? Qt::NoFocus : Qt::StrongFocus);
    setToolTip(image.isNull() ? QString() : QStringLiteral("点击放大查看 · 滚轮缩放 · 拖动查看"));
    update();
}
void ImagePreview::openPreview()
{
    if (image_.isNull()) return;
    if (dialog_) { dialog_->show(); dialog_->raise(); dialog_->activateWindow(); return; }
    dialog_ = new ImageViewer(image_, name_, window());
    dialog_->setAttribute(Qt::WA_DeleteOnClose);
    dialog_->show();
}
void ImagePreview::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && rect().contains(event->position().toPoint())) openPreview();
    QWidget::mouseReleaseEvent(event);
}
void ImagePreview::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Space) { openPreview(); event->accept(); }
    else QWidget::keyPressEvent(event);
}
void ImagePreview::enterEvent(QEnterEvent* event) { QWidget::enterEvent(event); update(); }
void ImagePreview::leaveEvent(QEvent* event) { QWidget::leaveEvent(event); update(); }
void ImagePreview::focusInEvent(QFocusEvent* event) { QWidget::focusInEvent(event); update(); }
void ImagePreview::focusOutEvent(QFocusEvent* event) { QWidget::focusOutEvent(event); update(); }
QRect ImagePreview::imageRect() const
{
    if (image_.isNull()) return {};
    const QRect area = contentsRect().adjusted(design::CanvasInset, design::CanvasInset, -design::CanvasInset, -design::CanvasInset);
    if (area.isEmpty()) return {};
    const QSize fitted = image_.size().scaled(area.size(), Qt::KeepAspectRatio);
    return {QPoint(area.center().x() - fitted.width() / 2,
                   area.center().y() - fitted.height() / 2), fitted};
}
void ImagePreview::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor(design::Canvas));
    if (image_.isNull()) {
        const QPointF center(width() / 2.0, height() / 2.0 - 38);
        painter.setPen(QPen(QColor(design::CanvasLine), 1.5));
        painter.setBrush(QColor(design::CanvasDeep));
        painter.drawRoundedRect(QRectF(center.x() - 30, center.y() - 25, 60, 50), design::Small, design::Small);
        painter.setPen(QPen(QColor(design::CanvasMuted), 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawEllipse(QPointF(center.x() + 13, center.y() - 11), 4, 4);
        QPainterPath mountains;
        mountains.moveTo(center.x() - 20, center.y() + 14);
        mountains.lineTo(center.x() - 5, center.y() - 3);
        mountains.lineTo(center.x() + 7, center.y() + 10);
        mountains.lineTo(center.x() + 14, center.y() + 3);
        mountains.lineTo(center.x() + 23, center.y() + 14);
        painter.drawPath(mountains);
        QFont title = font(); title.setPixelSize(design::BodyFont); title.setWeight(QFont::Medium);
        painter.setFont(title); painter.setPen(QColor(design::CanvasText));
        painter.drawText(QRect(12, height()/2 + 3, width()-24, 24), Qt::AlignCenter, placeholder_);
        QFont detail = font(); detail.setPixelSize(design::SmallFont);
        painter.setFont(detail); painter.setPen(QColor(design::CanvasMuted));
        painter.drawText(QRect(12, height()/2 + 34, width()-24, 20), Qt::AlignCenter,
            property("side") == "baseline" ? QStringLiteral("选择预期界面的截图") : QStringLiteral("选择需要与基准比较的截图"));
        painter.drawText(QRect(12, height()/2 + 61, width()-24, 20), Qt::AlignCenter, QStringLiteral("PNG / JPG · 使用上方导入按钮"));
        return;
    }
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.setPen(QPen(QColor(design::ImageEdge), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(imageRect().adjusted(-1, -1, 1, 1));
    painter.drawImage(imageRect(), image_);
    if (underMouse() || hasFocus()) {
        painter.setPen(QPen(QColor(design::Accent), hasFocus() ? 2 : 1));
        painter.drawRect(rect().adjusted(1,1,-2,-2));
    }
}
