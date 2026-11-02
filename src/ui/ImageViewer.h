#pragma once
#include <QDialog>
#include <QGraphicsView>
#include <functional>

class QLabel;
class QToolButton;
class ImageView : public QGraphicsView {
public:
    explicit ImageView(QWidget* parent = nullptr);
    void fitImage();
    void actualSize();
    double zoom() const { return transform().m11(); }
    bool isFitting() const { return fitting_; }
    std::function<void()> zoomChanged;
protected:
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void drawBackground(QPainter* painter, const QRectF& rect) override;
private:
    bool fitting_ = true;
};

class ImageViewer : public QDialog {
public:
    ImageViewer(const QImage& image, const QString& name, QWidget* parent = nullptr);
protected:
    void showEvent(QShowEvent* event) override;
private:
    ImageView* view_;
    QLabel* zoomLabel_;
    QToolButton* fitButton_;
    QToolButton* actualButton_;
    bool firstShow_ = true;
};
