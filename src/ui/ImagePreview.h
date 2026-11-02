#pragma once
#include <QImage>
#include <QWidget>

// Display copy only. Full-resolution BGR images remain in MainWindow.
class ImagePreview : public QWidget {
public:
    explicit ImagePreview(const QString& placeholder, QWidget* parent = nullptr);
    void setImage(const QImage& image);
    QSize imageSize() const { return image_.size(); }
    QRect imageRect() const;
    QSize sizeHint() const override { return {440, 320}; }
    QSize minimumSizeHint() const override { return {160, 160}; }
protected:
    void paintEvent(QPaintEvent* event) override;
private:
    QImage image_;
    QString placeholder_;
};
