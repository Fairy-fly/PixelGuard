#include "ui/ImagePreview.h"
#include <QPainter>

ImagePreview::ImagePreview(const QString& placeholder, QWidget* parent)
    : QWidget(parent), placeholder_(placeholder)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(160, 160);
    setAccessibleName(placeholder);
}
void ImagePreview::setImage(const QImage& image)
{
    image_ = image;
    update();
}
QRect ImagePreview::imageRect() const
{
    if (image_.isNull()) return {};
    const QRect area = contentsRect().adjusted(12, 12, -12, -12);
    if (area.isEmpty()) return {};
    const QSize fitted = image_.size().scaled(area.size(), Qt::KeepAspectRatio);
    return {QPoint(area.center().x() - fitted.width() / 2,
                   area.center().y() - fitted.height() / 2), fitted};
}
void ImagePreview::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    if (image_.isNull()) {
        painter.setPen(palette().color(QPalette::WindowText));
        painter.drawText(rect().adjusted(20, 20, -20, -20),
                         Qt::AlignCenter | Qt::TextWordWrap, placeholder_);
        return;
    }
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawImage(imageRect(), image_);
}
