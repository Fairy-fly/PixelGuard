#pragma once
#include <QImage>
#include <QWidget>
#include <QPointer>

class QDialog;

// Display copy only. Full-resolution BGR images remain in MainWindow.
class ImagePreview : public QWidget {
public:
    explicit ImagePreview(const QString& placeholder, QWidget* parent = nullptr);
    void setImage(const QImage& image, const QString& name = {});
    void openPreview();
    const QImage& image() const { return image_; }
    QSize imageSize() const { return image_.size(); }
    QRect imageRect() const;
    QSize sizeHint() const override { return {440, 320}; }
    QSize minimumSizeHint() const override { return {160, 80}; }
protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
private:
    QImage image_;
    QString placeholder_;
    QString name_;
    QPointer<QDialog> dialog_;
};
