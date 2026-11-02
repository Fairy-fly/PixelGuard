#include "ui/ImageViewer.h"
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QImage>
#include <QLabel>
#include <QPainter>
#include <QToolButton>
#include <QFrame>
#include "ui/DesignTokens.h"
#include "ui/WorkbenchWidgets.h"
#include <QVBoxLayout>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

namespace {
// QImage's shared storage keeps the display snapshot alive without a second
// full-resolution pixmap or any access to the detector's cv::Mat buffers.
class ImageItem : public QGraphicsItem {
public:
    explicit ImageItem(const QImage& image) : image_(image) {}
    QRectF boundingRect() const override { return QRectF(QPointF(), image_.size()); }
    void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) override
    { painter->drawImage(QPointF(), image_); }
private:
    QImage image_;
};
}

ImageView::ImageView(QWidget* parent) : QGraphicsView(parent)
{
    setObjectName("imageView");
    setDragMode(QGraphicsView::ScrollHandDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setRenderHint(QPainter::SmoothPixmapTransform);
    setBackgroundBrush(QColor(design::Canvas));
    setFrameShape(QFrame::NoFrame);
    setAccessibleName(QStringLiteral("图片预览：滚轮缩放，拖动查看"));
}
void ImageView::fitImage()
{
    fitting_ = true;
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    fitInView(sceneRect(), Qt::KeepAspectRatio);
    if (zoomChanged) zoomChanged();
}
void ImageView::actualSize()
{
    fitting_ = false;
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    resetTransform();
    if (zoomChanged) zoomChanged();
}
void ImageView::wheelEvent(QWheelEvent* event)
{
    if (!event->angleDelta().y()) { event->accept(); return; }
    fitting_ = false;
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    const double target = std::clamp(zoom() * std::pow(1.2, event->angleDelta().y() / 120.0), 0.001, 32.0);
    scale(target / zoom(), target / zoom());
    if (zoomChanged) zoomChanged();
    event->accept();
}
void ImageView::resizeEvent(QResizeEvent* event)
{
    QGraphicsView::resizeEvent(event);
    if (fitting_) fitImage();
}
void ImageView::drawBackground(QPainter* painter, const QRectF& rect)
{
    painter->fillRect(rect, QColor(design::Canvas));
    painter->setPen(QPen(QColor(design::ImageEdge), 0));
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(sceneRect());
}
ImageViewer::ImageViewer(const QImage& image, const QString& name, QWidget* parent) : QDialog(parent)
{
    setObjectName("imageViewer");
    setWindowTitle(QStringLiteral("%1 — PixelGuard 图片预览").arg(name));
    resize(1100, 800);
    setMinimumSize(560, 380);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0,0,0,0);
    layout->setSpacing(0);
    auto* header = new QFrame;
    header->setObjectName("viewerHeader");
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(design::Margin, design::Small, design::Margin, design::Small);
    headerLayout->setSpacing(design::Gap);
    auto* file = new ElidedLabel(name);
    file->setObjectName("viewerName");
    file->setToolTip(name);
    file->setTextFormat(Qt::PlainText);
    headerLayout->addWidget(file, 1);
    auto* info = new QLabel(QStringLiteral("%1 × %2 px").arg(image.width()).arg(image.height()));
    info->setObjectName("viewerInfo");
    headerLayout->addWidget(info);
    auto* closeButton = new QToolButton;
    closeButton->setObjectName("closePreviewButton");
    closeButton->setText(QStringLiteral("关闭"));
    closeButton->setIcon(workbenchIcon(Glyph::Close));
    closeButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    closeButton->setProperty("kind", "quiet");
    closeButton->setFixedHeight(design::Compact);
    closeButton->setToolTip(QStringLiteral("关闭预览 · Esc"));
    closeButton->setAccessibleName(QStringLiteral("关闭图片预览"));
    connect(closeButton, &QToolButton::clicked, this, &QDialog::close);
    headerLayout->addWidget(closeButton);
    layout->addWidget(header);
    auto* toolbarFrame = new QFrame;
    toolbarFrame->setObjectName("viewerToolbar");
    auto* toolbar = new QHBoxLayout(toolbarFrame);
    toolbar->setContentsMargins(design::Margin, design::Small, design::Margin, design::Small);
    toolbar->setSpacing(design::Small);
    auto addButton = [&](const QString& text, Glyph glyph, const char* objectName, auto action) {
        auto* button = new QToolButton;
        button->setText(text);
        button->setIcon(workbenchIcon(glyph));
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->setObjectName(objectName);
        button->setCheckable(true);
        button->setFixedHeight(design::Compact);
        connect(button, &QToolButton::clicked, this, action);
        toolbar->addWidget(button);
        return button;
    };
    view_ = new ImageView(this);
    auto* scene = new QGraphicsScene(view_);
    scene->addItem(new ImageItem(image));
    scene->setSceneRect(QRectF(QPointF(), image.size()));
    view_->setScene(scene);
    fitButton_ = addButton(QStringLiteral("适应窗口"), Glyph::Fit, "fitButton", [this] { view_->fitImage(); });
    actualButton_ = addButton(QStringLiteral("100%"), Glyph::Actual, "actualSizeButton", [this] { view_->actualSize(); });
    fitButton_->setAccessibleName(QStringLiteral("适应窗口"));
    actualButton_->setAccessibleName(QStringLiteral("100% 原始比例"));
    toolbar->addStretch();
    auto* zoomCaption = new QLabel(QStringLiteral("缩放"));
    zoomCaption->setObjectName("viewerInfo");
    toolbar->addWidget(zoomCaption);
    zoomLabel_ = new QLabel;
    zoomLabel_->setObjectName("zoomLabel");
    zoomLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    toolbar->addWidget(zoomLabel_);
    layout->addWidget(toolbarFrame);
    layout->addWidget(view_, 1);
    auto* footer = new QFrame;
    footer->setObjectName("viewerFooter");
    auto* footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(design::Margin, design::Small, design::Margin, design::Small);
    auto* hint = new QLabel(QStringLiteral("滚轮缩放   ·   按住左键拖动查看"));
    hint->setObjectName("viewerHint");
    footerLayout->addWidget(hint);
    footerLayout->addStretch();
    footerLayout->addWidget(new QLabel(QStringLiteral("Esc 关闭")));
    layout->addWidget(footer);
    view_->zoomChanged = [this] {
        zoomLabel_->setText(QStringLiteral("%1%").arg(view_->zoom() * 100, 0, 'f', 1));
        fitButton_->setChecked(view_->isFitting());
        actualButton_->setChecked(!view_->isFitting() && std::abs(view_->zoom() - 1.0) < 0.000001);
    };
}
void ImageViewer::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    if (firstShow_) { firstShow_ = false; view_->fitImage(); }
}
